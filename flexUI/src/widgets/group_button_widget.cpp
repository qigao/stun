/*
 * flexUI - GroupButtonWidget Implementation
 */

#include <flexUI/widgets/group_button_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>

namespace flexUI {

GroupButtonWidget::GroupButtonWidget() {
    // Initial shapes will be created in first render when we know dimensions
}

void GroupButtonWidget::set_label(const std::string& label) {
    label_ = label;
    if (text_) {
        text_->set_text(label);
    }
    dirty_ = true;
}

void GroupButtonWidget::rebuild_shapes(float width, float height) {
    if (width == cached_width_ && height == cached_height_ && background_ != nullptr) {
        return;  // No rebuild needed
    }

    root_.clear();
    cached_width_ = width;
    cached_height_ = height;

    // Background rectangle (fills entire button)
    background_ = root_.add<RectShape>(0, 0, width, height, 6.0f);

    // Text centered in button
    text_ = root_.add<TextShape>(width / 2, height / 2 + 5, label_);
    text_->set_font_size(14.0f);

    update_colors();
}

void GroupButtonWidget::update_colors() {
    if (!background_ || !text_) return;

    Color bg_color;
    Color text_color{1.0f, 1.0f, 1.0f, 1.0f};  // white

    if (pressed_) {
        // Pressed: darker blue
        bg_color = Color{0.18f, 0.41f, 0.78f, 1.0f};
    } else if (hovered_) {
        // Hovered: lighter blue
        bg_color = Color{0.33f, 0.56f, 0.92f, 1.0f};
    } else {
        // Normal: blue
        bg_color = Color{0.23f, 0.51f, 0.97f, 1.0f};
    }

    background_->set_fill(bg_color);
    text_->set_color(text_color);
}

void GroupButtonWidget::render(const Element& elem, Renderer& renderer) {
    float width = elem.width();
    float height = elem.height();

    // Rebuild shapes if size changed
    rebuild_shapes(width, height);

    // Update colors based on state
    if (hovered_ != elem.is_hover() || pressed_ != elem.is_active()) {
        hovered_ = elem.is_hover();
        pressed_ = elem.is_active();
        update_colors();
    }

    // Draw using flex::Renderer
    // Position comes from element's absolute position
    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    // Get opacity from style
    float opacity = 1.0f;
    if (elem.computed_style) {
        opacity = elem.computed_style->opacity;
    }

    // Draw the group
    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool GroupButtonWidget::handle_event(const Event& event, Element& elem) {
    float local_x = event.x - elem.absolute_x();
    float local_y = event.y - elem.absolute_y();

    bool inside = local_x >= 0 && local_x <= elem.width() &&
                  local_y >= 0 && local_y <= elem.height();

    switch (event.type) {
        case EventType::MouseMove:
            // Handle hover state based on position
            if (inside && !elem.is_hover()) {
                elem.set_hover(true);
                elem.mark_paint_dirty();
            } else if (!inside && elem.is_hover()) {
                elem.set_hover(false);
                elem.set_active(false);
                elem.mark_paint_dirty();
            }
            return inside;

        case EventType::MouseDown:
            if (inside && event.button == MouseButton::Left) {
                elem.set_active(true);
                elem.mark_paint_dirty();
                return true;
            }
            break;

        case EventType::MouseUp:
            if (elem.is_active() && inside && event.button == MouseButton::Left) {
                elem.set_active(false);
                elem.mark_paint_dirty();
                if (on_click_) {
                    on_click_();
                }
                return true;
            }
            elem.set_active(false);
            break;

        default:
            break;
    }

    return false;
}

} // namespace flexUI
