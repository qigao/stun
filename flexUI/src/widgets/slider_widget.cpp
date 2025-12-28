/*
 * flexUI - SliderWidget Implementation
 */

#include <flexUI/widgets/slider_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace flexUI {

SliderWidget::SliderWidget(float min, float max, float value, float step)
    : min_(min), max_(max), value_(value), step_(step) {
    update_value_position();
}

void SliderWidget::set_value(float value) {
    float clamped = std::max(min_, std::min(max_, value));

    if (step_ > 0) {
        clamped = snap_to_step(clamped);
    }

    if (value_ != clamped) {
        value_ = clamped;
        update_value_position();
        dirty_ = true;

        if (change_callback_) {
            change_callback_(value_);
        }
    }
}

void SliderWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float track_height = style->get_variable_float("--track-height", 4.0f);
    float thumb_size = style->get_variable_float("--thumb-size", 20.0f);
    bool show_value = style->get_variable("--show-value", "false") == "true";

    // Check if rebuild needed
    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        track_height == cached_track_height_ &&
        thumb_size == cached_thumb_size_ &&
        show_value == cached_show_value_ &&
        track_ != nullptr) {
        return;
    }

    root_.clear();
    cached_width_ = elem.width();
    cached_height_ = elem.height();
    cached_track_height_ = track_height;
    cached_thumb_size_ = thumb_size;
    cached_show_value_ = show_value;

    float track_y = (elem.height() - track_height) / 2;
    float track_radius = track_height / 2;

    // Track background
    track_ = root_.add<RectShape>(0, track_y, elem.width(), track_height, track_radius);

    // Track fill (will be updated in update_shapes)
    fill_ = root_.add<RectShape>(0, track_y, 0, track_height, track_radius);

    // Thumb (position will be updated in update_shapes)
    thumb_ = root_.add<CircleShape>(0, elem.height() / 2, thumb_size / 2);

    // Value label (optional)
    if (show_value) {
        value_label_ = root_.add<TextShape>(0, 0, "");
        value_label_->set_font_family(style->font_family);
        value_label_->set_font_size(style->font_size * 0.8f);
    } else {
        value_label_ = nullptr;
    }
}

void SliderWidget::update_shapes(const Element& elem) {
    if (!track_ || !fill_ || !thumb_) return;

    auto* style = elem.computed_style;
    if (!style) return;

    float track_height = style->get_variable_float("--track-height", 4.0f);
    float thumb_size = style->get_variable_float("--thumb-size", 20.0f);

    // Colors
    Color track_bg = style->get_variable_color("--track-bg", color_from_u8(229, 231, 235, 255));
    Color track_fill = style->get_variable_color("--track-fill", color_from_u8(59, 130, 246, 255));
    Color thumb_bg = style->get_variable_color("--thumb-bg", color_from_u8(255, 255, 255, 255));
    Color thumb_border = style->get_variable_color("--thumb-border", color_from_u8(200, 200, 200, 255));

    // Track
    track_->set_fill(track_bg);

    // Fill width based on value position
    float fill_width = elem.width() * value_position_;
    fill_->set_size(fill_width, track_height);
    fill_->set_fill(track_fill);

    // Thumb position and size with scale animation
    float actual_size = thumb_size * current_thumb_scale_;
    float thumb_x = elem.width() * value_position_;
    float thumb_y = elem.height() / 2;
    thumb_->set_position(thumb_x, thumb_y);
    thumb_->set_radius(actual_size / 2);
    thumb_->set_fill(thumb_bg);
    thumb_->set_stroke(thumb_border, 2.0f);

    // Value label
    if (value_label_) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << value_;
        std::string value_text = oss.str();

        value_label_->set_text(value_text);
        value_label_->set_color(style->text_color);

        float text_x = thumb_x - value_text.size() * style->font_size * 0.3f;
        float text_y = (elem.height() - thumb_size) / 2 - 5;
        value_label_->set_position(text_x, text_y);
    }
}

void SliderWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Rebuild shapes if needed
    rebuild_shapes(elem);

    // Update shape properties
    update_shapes(elem);

    // Draw using flex::Renderer
    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    float opacity = style->opacity;
    root_.draw(renderer.flex(), world_transform, opacity);

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
                update_value_from_x(event.x, elem);
                elem.mark_paint_dirty();
                return true;
            }
            break;

        case EventType::MouseMove:
            if (is_dragging_) {
                update_value_from_x(event.x, elem);
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

    float duration = style->get_variable_float("--transition-duration", 100.0f);
    if (duration <= 0) {
        current_thumb_scale_ = target_thumb_scale_;
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

void SliderWidget::update_value_from_x(float x, const Element& elem) {
    float relative_x = x - elem.absolute_x();
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
