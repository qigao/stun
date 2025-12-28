/*
 * flexUI - SwitchWidget Implementation
 */

#include <flexUI/widgets/switch_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

SwitchWidget::SwitchWidget(const std::string& label, bool checked)
    : label_(label), checked_(checked) {
    target_thumb_position_ = checked ? 1.0f : 0.0f;
    thumb_position_ = target_thumb_position_;
}

void SwitchWidget::set_checked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        target_thumb_position_ = checked ? 1.0f : 0.0f;
        dirty_ = true;

        if (change_callback_) {
            change_callback_(checked_);
        }
    }
}

void SwitchWidget::set_label(const std::string& label) {
    label_ = label;
    if (text_) {
        text_->set_text(label);
    }
    dirty_ = true;
}

void SwitchWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float switch_width = style->get_variable_float("--switch-width", 50.0f);
    float switch_height = style->get_variable_float("--switch-height", 28.0f);

    // Check if rebuild needed
    if (switch_width == cached_switch_width_ &&
        switch_height == cached_switch_height_ &&
        track_ != nullptr) {
        return;
    }

    root_.clear();
    cached_switch_width_ = switch_width;
    cached_switch_height_ = switch_height;

    float track_radius = switch_height / 2;

    // Track (pill-shaped background)
    track_ = root_.add<RectShape>(0, 0, switch_width, switch_height, track_radius);

    // Thumb (circular, position will be updated in update_shapes)
    float thumb_size = switch_height - 4;
    float margin = 2;
    thumb_ = root_.add<CircleShape>(margin + thumb_size / 2, switch_height / 2, thumb_size / 2);

    // Label text
    if (!label_.empty()) {
        float label_spacing = style->get_variable_float("--label-spacing", 8.0f);
        float text_x = switch_width + label_spacing;
        float text_y = switch_height / 2 - style->font_size / 2;
        text_ = root_.add<TextShape>(text_x, text_y, label_);
        text_->set_font_family(style->font_family);
        text_->set_font_size(style->font_size);
    } else {
        text_ = nullptr;
    }
}

void SwitchWidget::update_shapes(const Element& elem) {
    if (!track_ || !thumb_) return;

    auto* style = elem.computed_style;
    if (!style) return;

    float switch_width = style->get_variable_float("--switch-width", 50.0f);
    float switch_height = style->get_variable_float("--switch-height", 28.0f);

    // Track color based on state
    Color track_color;
    if (checked_ || elem.has_state("checked")) {
        track_color = style->get_variable_color("--switch-bg-on", color_from_u8(34, 197, 94, 255));
    } else {
        track_color = style->get_variable_color("--switch-bg-off", color_from_u8(200, 200, 200, 255));
    }
    track_->set_fill(track_color);

    // Thumb color and position
    Color thumb_color = style->get_variable_color("--switch-thumb", color_from_u8(255, 255, 255, 255));
    thumb_->set_fill(thumb_color);

    float thumb_size = switch_height - 4;
    float margin = 2;
    float travel_distance = switch_width - thumb_size - 2 * margin;
    float thumb_x = margin + thumb_size / 2 + travel_distance * thumb_position_;
    float thumb_y = switch_height / 2;
    thumb_->set_position(thumb_x, thumb_y);

    // Label text color
    if (text_) {
        text_->set_color(style->text_color);
    }
}

void SwitchWidget::render(const Element& elem, Renderer& renderer) {
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

bool SwitchWidget::handle_event(const Event& event, Element& elem) {
    if (disabled_ || elem.has_state("disabled")) {
        return false;
    }

    switch (event.type) {
        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                set_checked(!checked_);

                if (checked_) {
                    elem.add_state("checked");
                } else {
                    elem.remove_state("checked");
                }

                elem.mark_paint_dirty();
                return true;
            }
            break;

        case EventType::KeyDown:
            if (event.key == KeyCode::Enter || event.key == KeyCode::Num0) {
                set_checked(!checked_);

                if (checked_) {
                    elem.add_state("checked");
                } else {
                    elem.remove_state("checked");
                }

                elem.mark_paint_dirty();
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

void SwitchWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--transition-duration", 200.0f);
    if (duration <= 0) {
        thumb_position_ = target_thumb_position_;
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
