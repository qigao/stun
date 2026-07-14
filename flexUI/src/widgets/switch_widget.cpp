/*
 * flexUI - SwitchWidget Implementation
 */

#include <flexUI/widgets/switch_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

SwitchWidget::SwitchWidget(const std::string& label, bool checked)
    : label_(label), checked_(checked) {
    target_thumb_position_ = checked ? 1.0f : 0.0f;
    thumb_position_ = target_thumb_position_;
}

void SwitchWidget::build_semantic_tree() {
    track_ = create_part("track", "track");
    thumb_ = create_part("thumb", "thumb");
    label_element_ = create_part("label", "label");
}

bool SwitchWidget::measure_intrinsic_size(const Element& elem, float available_width,
                                          float available_height, float& out_width,
                                          float& out_height) const {
    (void)available_width;
    (void)available_height;
    auto* style = elem.computed_style;
    const float switch_width =
        style ? style->get_variable_float("--switch-width", 50.0f) : 50.0f;
    const float switch_height =
        style ? style->get_variable_float("--switch-height", 28.0f) : 28.0f;
    const float label_spacing =
        style ? style->get_variable_float("--label-spacing", 8.0f) : 8.0f;
    const float font_size = style && style->font_size > 0.0f ? style->font_size : 14.0f;
    ComputedStyle measure_style;
    if (style) {
        measure_style = *style;
    }
    measure_style.font_size = font_size;
    const float label_width = label_.empty()
                                  ? 0.0f
                                  : approximate_segmented_text_width(
                                        &measure_style, label_);
    out_width = switch_width + (label_.empty() ? 0.0f : label_spacing + label_width);
    out_height = std::max(switch_height, font_size);
    return true;
}

void SwitchWidget::sync_host_semantics() {
    set_host_attribute("role", "switch");
    set_host_data_state("checked", "unchecked", checked_);
    set_host_boolean_attribute("aria-checked", checked_);
    set_host_boolean_attribute("aria-disabled", disabled_);
    set_host_presence_attribute("disabled", disabled_);
    set_host_state("checked", checked_);
}

void SwitchWidget::set_checked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        target_thumb_position_ = checked ? 1.0f : 0.0f;
        invalidate_render_cache();
        sync_host_semantics();

        if (change_callback_) {
            change_callback_(checked_);
        }
    }
}

void SwitchWidget::set_label(const std::string& label) {
    label_ = label;
    if (label_element_) {
        label_element_->set_text(label);
    }
    invalidate_render_cache();
}

void SwitchWidget::set_disabled(bool disabled) {
    if (disabled_ == disabled) {
        return;
    }
    disabled_ = disabled;
    invalidate_render_cache();
    sync_host_semantics();
}

void SwitchWidget::update_part_geometry(const Element& elem) {
    if (!elem.computed_style || !track_ || !thumb_ || !label_element_ ||
        !track_->computed_style || !thumb_->computed_style ||
        !label_element_->computed_style) {
        return;
    }

    const auto resolved_size = [](const Element& part, bool width, float fallback) {
        const auto& size = width ? part.computed_style->width_size
                                 : part.computed_style->height_size;
        if (size.kind == CssSizeKind::Auto) {
            return fallback;
        }
        return std::max(width ? part.computed_style->width
                              : part.computed_style->height,
                        0.0f);
    };
    const float legacy_width =
        elem.computed_style->get_variable_float("--switch-width", 50.0f);
    const float legacy_height =
        elem.computed_style->get_variable_float("--switch-height", 28.0f);
    const float track_width = resolved_size(*track_, true, legacy_width);
    const float track_height = resolved_size(*track_, false, legacy_height);
    const float track_y = (elem.height() - track_height) * 0.5f;
    track_->set_layout_bounds(0.0f, track_y, track_width, track_height);

    const float fallback_thumb_size = std::max(track_height - 4.0f, 0.0f);
    const float thumb_width = resolved_size(*thumb_, true, fallback_thumb_size);
    const float thumb_height = resolved_size(*thumb_, false, fallback_thumb_size);
    const float margin = std::max((track_height - thumb_height) * 0.5f, 0.0f);
    const float travel = std::max(track_width - thumb_width - margin * 2.0f, 0.0f);
    thumb_->set_layout_bounds(margin + travel * thumb_position_,
                              track_y + (track_height - thumb_height) * 0.5f,
                              thumb_width, thumb_height);

    const float spacing =
        elem.computed_style->get_variable_float("--label-spacing", 8.0f);
    label_element_->set_text(label_);
    label_element_->set_visible(!label_.empty());
    const float label_width = label_.empty()
                                  ? 0.0f
                                  : approximate_segmented_text_width(
                                        label_element_->computed_style, label_);
    label_element_->set_layout_bounds(
        track_width + spacing, 0.0f, label_width, elem.height());
}

void SwitchWidget::sync_host_semantics_for_layout(Element& elem) {
    sync_host_semantics();
    update_part_geometry(elem);
}

void SwitchWidget::invalidate_render_cache() {
    dirty_ = true;
    if (auto* host = host_element()) {
        host->mark_paint_dirty();
    }
}

void SwitchWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    if (!host_element() && elem.computed_style) {
        const auto* style = elem.computed_style;
        const float width = style->get_variable_float("--switch-width", 50.0f);
        const float height = style->get_variable_float("--switch-height", 28.0f);
        const float thumb_size = std::max(height - 4.0f, 0.0f);
        const float margin = 2.0f;
        const float travel = std::max(width - thumb_size - margin * 2.0f, 0.0f);
        const Color track = checked_ || elem.has_state("checked")
                                ? style->get_variable_color(
                                      "--switch-bg-on",
                                      style->get_variable_color(
                                          "--accent-color",
                                          color_from_u8(34, 197, 94, 255)))
                                : style->get_variable_color(
                                      "--switch-bg-off",
                                      color_from_u8(200, 200, 200, 255));
        const Color thumb = style->get_variable_color(
            "--switch-thumb", color_from_u8(255, 255, 255, 255));
        commands.draw_rect(0.0f, 0.0f, width, height, height * 0.5f,
                           Paint::solid(track), Paint::none(), 0.0f);
        commands.draw_circle(margin + thumb_size * 0.5f + travel * thumb_position_,
                             height * 0.5f, thumb_size * 0.5f,
                             Paint::solid(thumb), Paint::none(), 0.0f);
        if (!label_.empty()) {
            const float spacing =
                style->get_variable_float("--label-spacing", 8.0f);
            commands.draw_text(label_, width + spacing,
                               height * 0.5f - style->font_size * 0.5f,
                               style->font_family, style->font_size,
                               style->font_weight >= FontWeight::Bold,
                               style->text_color);
        }
    }
    dirty_ = false;
}

bool SwitchWidget::handle_event(const Event& event, Element& elem) {
    if (disabled_ || elem.has_state("disabled")) {
        return false;
    }

    switch (event.type) {
        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                set_checked(!checked_);

                elem.mark_paint_dirty();
                return true;
            }
            break;

        case EventType::KeyDown:
            if (event.key == KeyCode::Enter || event.key == KeyCode::Num0) {
                set_checked(!checked_);

                elem.mark_paint_dirty();
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

bool SwitchWidget::needs_frame_update(const Element& elem) const {
    (void)elem;
    return std::abs(thumb_position_ - target_thumb_position_) > 0.01f;
}

void SwitchWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--transition-duration", 0.0f);
    if (duration <= 0) {
        if (std::abs(thumb_position_ - target_thumb_position_) > 0.01f) {
            thumb_position_ = target_thumb_position_;
            elem.mark_paint_dirty();
        }
        return;
    }

    float speed = 1000.0f / duration;
    float delta = speed * (delta_ms / 1000.0f);

    if (std::abs(thumb_position_ - target_thumb_position_) > 0.01f) {
        if (thumb_position_ < target_thumb_position_) {
            thumb_position_ = std::min(thumb_position_ + delta, target_thumb_position_);
        } else {
            thumb_position_ = std::max(thumb_position_ - delta, target_thumb_position_);
        }
        elem.mark_paint_dirty();
    }
}

} // namespace flexUI
