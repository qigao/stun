/*
 * flexUI - SliderWidget Implementation
 */

#include <flexUI/widgets/slider_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace flexUI {

namespace {

std::string format_slider_aria_number(float value) {
    if (std::fabs(value) < 0.0000005f) {
        value = 0.0f;
    }

    std::ostringstream out;
    out.setf(std::ios::fixed, std::ios::floatfield);
    out.precision(6);
    out << value;
    std::string text = out.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text == "-0" ? "0" : text;
}

} // namespace

SliderWidget::SliderWidget(float min, float max, float value, float step)
    : min_(min), max_(max), value_(value), step_(step) {
    update_value_position();
}

void SliderWidget::build_semantic_tree() {
    track_ = create_part("track", "track");
    fill_ = create_part("fill", "fill");
    thumb_ = create_part("thumb", "thumb");
    value_label_ = create_part("value", "value");
}

void SliderWidget::sync_host_semantics() {
    set_host_attribute("role", "slider");
    set_host_attribute("aria-orientation", "horizontal");
    set_host_attribute("aria-valuemin", format_slider_aria_number(min_));
    set_host_attribute("aria-valuemax", format_slider_aria_number(max_));
    set_host_attribute("aria-valuenow", format_slider_aria_number(value_));
    set_host_boolean_attribute("aria-disabled", disabled_);
    set_host_presence_attribute("disabled", disabled_);
    set_host_attribute("data-state", disabled_ ? "disabled" : "enabled");
    set_host_state("disabled", disabled_);
}

void SliderWidget::set_value(float value) {
    float clamped = std::max(min_, std::min(max_, value));

    if (step_ > 0) {
        clamped = snap_to_step(clamped);
    }

    if (value_ != clamped) {
        value_ = clamped;
        update_value_position();
        sync_host_semantics();
        invalidate_parts();

        if (change_callback_) {
            change_callback_(value_);
        }
    }
}

void SliderWidget::set_min(float min) {
    if (min_ == min) {
        return;
    }
    min_ = min;
    update_value_position();
    sync_host_semantics();
    invalidate_parts();
}

void SliderWidget::set_max(float max) {
    if (max_ == max) {
        return;
    }
    max_ = max;
    update_value_position();
    sync_host_semantics();
    invalidate_parts();
}

void SliderWidget::set_disabled(bool disabled) {
    if (disabled_ == disabled) {
        return;
    }
    disabled_ = disabled;
    sync_host_semantics();
    invalidate_parts();
}

void SliderWidget::invalidate_parts() {
    dirty_ = true;
    if (auto* host = host_element()) {
        host->mark_paint_dirty();
    }
}

void SliderWidget::update_part_geometry(const Element& elem) {
    if (!track_ || !fill_ || !thumb_ || !value_label_ ||
        !track_->computed_style || !fill_->computed_style ||
        !thumb_->computed_style || !value_label_->computed_style) {
        return;
    }

    const auto resolved_height = [](const Element& part, float fallback) {
        return part.computed_style->height_size.kind == CssSizeKind::Auto
                   ? fallback
                   : std::max(part.computed_style->height, 0.0f);
    };
    const auto resolved_width = [](const Element& part, float fallback) {
        return part.computed_style->width_size.kind == CssSizeKind::Auto
                   ? fallback
                   : std::max(part.computed_style->width, 0.0f);
    };

    const float legacy_track_height = elem.computed_style->get_variable_float(
        "--track-height", 4.0f);
    const float track_height = resolved_height(*track_, legacy_track_height);
    const float fill_height = resolved_height(*fill_, track_height);
    const float track_width = resolved_width(*track_, elem.width());
    const float track_x = (elem.width() - track_width) * 0.5f;
    const float track_y = (elem.height() - track_height) * 0.5f;
    track_->set_layout_bounds(track_x, track_y, track_width, track_height);

    const float fill_width = track_width * value_position_;
    const float fill_y = (elem.height() - fill_height) * 0.5f;
    fill_->set_layout_bounds(track_x, fill_y, fill_width, fill_height);

    const float legacy_thumb_size = elem.computed_style->get_variable_float(
        "--thumb-size", 20.0f);
    const float thumb_width =
        resolved_width(*thumb_, legacy_thumb_size) * current_thumb_scale_;
    const float thumb_height =
        resolved_height(*thumb_, legacy_thumb_size) * current_thumb_scale_;
    const float thumb_center_x = track_x + track_width * value_position_;
    thumb_->set_layout_bounds(thumb_center_x - thumb_width * 0.5f,
                              (elem.height() - thumb_height) * 0.5f,
                              thumb_width, thumb_height);

    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value_;
    value_label_->set_text(out.str());
    const bool show_value = elem.computed_style->get_variable(
                                "--show-value", "false") == "true";
    value_label_->set_visible(show_value);
    const float label_width = resolved_width(*value_label_, 64.0f);
    const float label_height = resolved_height(
        *value_label_, std::max(value_label_->computed_style->font_size * 1.2f,
                                1.0f));
    value_label_->set_layout_bounds(thumb_center_x - label_width * 0.5f,
                                    track_y - label_height - 5.0f,
                                    label_width, label_height);
}

void SliderWidget::sync_host_semantics_for_layout(Element& elem) {
    sync_host_semantics();
    update_part_geometry(elem);
}

void SliderWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    (void)elem;
    (void)commands;
    dirty_ = false;
}

bool SliderWidget::handle_event(const Event& event, Element& elem) {
    if (disabled_ || elem.has_state("disabled")) {
        return false;
    }

    switch (event.type) {
        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                is_dragging_ = true;
                target_thumb_scale_ = 1.2f;
                update_value_from_position(event.x, event.y, elem);
                elem.mark_paint_dirty();
                return true;
            }
            break;

        case EventType::MouseMove:
            if (is_dragging_) {
                update_value_from_position(event.x, event.y, elem);
                elem.mark_paint_dirty();
                return true;
            }
            break;

        case EventType::MouseUp:
            if (is_dragging_) {
                is_dragging_ = false;
                target_thumb_scale_ = 1.0f;
                elem.mark_paint_dirty();
                return true;
            }
            break;

        case EventType::KeyDown:
            {
                float delta = (max_ - min_) * 0.01f;
                if (step_ > 0) {
                    delta = step_;
                }

                bool changed = false;
                if (event.key == KeyCode::Left) {
                    set_value(value_ - delta);
                    changed = true;
                } else if (event.key == KeyCode::Right) {
                    set_value(value_ + delta);
                    changed = true;
                }

                if (changed) {
                    elem.mark_paint_dirty();
                    return true;
                }
            }
            break;

        default:
            break;
    }

    return false;
}

void SliderWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--transition-duration", 0.0f);
    if (duration <= 0) {
        if (std::abs(current_thumb_scale_ - target_thumb_scale_) > 0.01f) {
            current_thumb_scale_ = target_thumb_scale_;
            elem.mark_paint_dirty();
        }
        return;
    }

    float speed = 1000.0f / duration;
    float delta = speed * (delta_ms / 1000.0f);

    if (std::abs(current_thumb_scale_ - target_thumb_scale_) > 0.01f) {
        if (current_thumb_scale_ < target_thumb_scale_) {
            current_thumb_scale_ = std::min(current_thumb_scale_ + delta, target_thumb_scale_);
        } else {
            current_thumb_scale_ = std::max(current_thumb_scale_ - delta, target_thumb_scale_);
        }
        elem.mark_paint_dirty();
    }
}

void SliderWidget::update_value_from_position(float x, float y,
                                              const Element& elem) {
    const flex::Vec2 local_pos =
        detail::css_render_to_local(&elem, flex::Vec2(x, y));
    float relative_x = local_pos.x;
    float position = relative_x / elem.width();
    position = std::max(0.0f, std::min(1.0f, position));

    float new_value = min_ + position * (max_ - min_);
    set_value(new_value);
}

void SliderWidget::update_value_position() {
    if (max_ > min_) {
        value_position_ = (value_ - min_) / (max_ - min_);
    } else {
        value_position_ = 0.0f;
    }
}

float SliderWidget::snap_to_step(float value) {
    if (step_ <= 0) return value;

    float steps = std::round((value - min_) / step_);
    return min_ + steps * step_;
}

} // namespace flexUI
