/*
 * flexUI - RadioWidget Implementation
 */

#include <flexUI/widgets/radio_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

RadioWidget::RadioWidget(const std::string& label, const std::string& value,
                         const std::string& group, bool checked)
    : label_(label), value_(value), group_(group), checked_(checked) {
    target_dot_scale_ = checked ? 1.0f : 0.0f;
    dot_scale_ = target_dot_scale_;
}

void RadioWidget::set_checked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        target_dot_scale_ = checked ? 1.0f : 0.0f;
        dirty_ = true;

        if (checked && change_callback_) {
            change_callback_(value_, group_);
        }
    }
}

void RadioWidget::set_label(const std::string& label) {
    label_ = label;
    if (text_) {
        text_->set_text(label);
    }
    dirty_ = true;
}

void RadioWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float radio_size = style->get_variable_float("--radio-size", 20.0f);

    // Check if rebuild needed
    if (radio_size == cached_radio_size_ && circle_ != nullptr) {
        return;
    }

    root_.clear();
    cached_radio_size_ = radio_size;

    float radius = radio_size / 2;
    float center_x = radius;
    float center_y = radius;

    // Outer circle (background + border)
    circle_ = root_.add<CircleShape>(center_x, center_y, radius);

    // Inner dot (will be scaled based on animation)
    float dot_radius = radius * 0.5f;
    dot_ = root_.add<CircleShape>(center_x, center_y, dot_radius);

    // Label text
    if (!label_.empty()) {
        float label_spacing = style->get_variable_float("--label-spacing", 8.0f);
        float text_x = radio_size + label_spacing;
        float text_y = radio_size / 2 - style->font_size / 2;
        text_ = root_.add<TextShape>(text_x, text_y, label_);
        text_->set_font_family(style->font_family);
        text_->set_font_size(style->font_size);
    } else {
        text_ = nullptr;
    }
}

void RadioWidget::update_shapes(const Element& elem) {
    if (!circle_ || !dot_) return;

    auto* style = elem.computed_style;
    if (!style) return;

    float radio_size = style->get_variable_float("--radio-size", 20.0f);

    // Circle color based on state
    Color bg_color;
    if (checked_ || elem.has_state("checked")) {
        bg_color = style->get_variable_color("--radio-bg-checked", color_from_u8(59, 130, 246, 255));
    } else {
        bg_color = style->get_variable_color("--radio-bg", color_from_u8(255, 255, 255, 255));
    }

    Color border_color = style->get_variable_color("--radio-border", color_from_u8(200, 200, 200, 255));

    circle_->set_fill(bg_color);
    circle_->set_stroke(border_color, 2.0f);

    // Dot color and size (animated)
    Color dot_color = style->get_variable_color("--radio-dot", color_from_u8(255, 255, 255, 255));
    float dot_radius = (radio_size / 2) * 0.5f * dot_scale_;

    dot_->set_radius(dot_radius);
    dot_->set_fill(dot_color);
    dot_->set_visible(dot_scale_ > 0.01f);

    // Label text color
    if (text_) {
        text_->set_color(style->text_color);
    }
}

void RadioWidget::render(const Element& elem, Renderer& renderer) {
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

bool RadioWidget::handle_event(const Event& event, Element& elem) {
    if (disabled_ || elem.has_state("disabled")) {
        return false;
    }

    switch (event.type) {
        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                if (!checked_) {
                    set_checked(true);
                    elem.add_state("checked");
                    elem.mark_paint_dirty();
                }
                return true;
            }
            break;

        case EventType::KeyDown:
            if (event.key == KeyCode::Enter || event.key == KeyCode::Num0) {
                if (!checked_) {
                    set_checked(true);
                    elem.add_state("checked");
                    elem.mark_paint_dirty();
                }
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

void RadioWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--transition-duration", 200.0f);
    if (duration <= 0) {
        dot_scale_ = target_dot_scale_;
        return;
    }

    float speed = 1000.0f / duration;
    float delta = speed * (delta_ms / 1000.0f);

    if (std::abs(dot_scale_ - target_dot_scale_) > 0.01f) {
        if (dot_scale_ < target_dot_scale_) {
            dot_scale_ = std::min(dot_scale_ + delta, target_dot_scale_);
        } else {
            dot_scale_ = std::max(dot_scale_ - delta, target_dot_scale_);
        }
        elem.mark_paint_dirty();
    }
}

} // namespace flexUI
