/*
 * tvgbox2 - CheckboxWidget Implementation
 */

#include <tvgbox2/widgets/checkbox_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tvgbox2 {

CheckboxWidget::CheckboxWidget(const std::string& label, bool checked)
    : label_(label), checked_(checked) {
    target_checkmark_scale_ = checked ? 1.0f : 0.0f;
    checkmark_scale_ = target_checkmark_scale_;
}

void CheckboxWidget::set_checked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        target_checkmark_scale_ = checked ? 1.0f : 0.0f;
        dirty_ = true;

        if (change_callback_) {
            change_callback_(checked_);
        }
    }
}

void CheckboxWidget::set_label(const std::string& label) {
    label_ = label;
    if (text_) {
        text_->set_text(label);
    }
    dirty_ = true;
}

void CheckboxWidget::rebuild_shapes(float checkbox_size, float label_spacing,
                                     const std::string& font_family, float font_size) {
    if (checkbox_size == cached_checkbox_size_ &&
        label_spacing == cached_label_spacing_ && box_ != nullptr) {
        return;
    }

    root_.clear();
    cached_checkbox_size_ = checkbox_size;
    cached_label_spacing_ = label_spacing;

    float radius = checkbox_size * 0.15f;

    // Box background with border
    box_ = root_.add<RectShape>(0, 0, checkbox_size, checkbox_size, radius);

    // Checkmark path (will be updated with scale)
    checkmark_ = root_.add<PathShape>();
    update_checkmark_path(checkbox_size);

    // Label text
    if (!label_.empty()) {
        float text_x = checkbox_size + label_spacing;
        float text_y = checkbox_size / 2 + font_size / 3;
        text_ = root_.add<TextShape>(text_x, text_y, label_);
        text_->set_font_family(font_family);
        text_->set_font_size(font_size);
    }
}

void CheckboxWidget::update_colors(const Element& elem) {
    if (!box_ || !checkmark_) return;

    auto* style = elem.computed_style;
    if (!style) return;

    // Background color based on checked state
    Color bg_color;
    if (checked_ || elem.has_state("checked")) {
        bg_color = style->get_variable_color("--checkbox-bg-checked", color_from_u8(59, 130, 246, 255));
    } else {
        bg_color = style->get_variable_color("--checkbox-bg", color_from_u8(255, 255, 255, 255));
    }

    // Border color
    Color border_color = style->get_variable_color("--checkbox-border", color_from_u8(200, 200, 200, 255));

    // Checkmark color
    Color checkmark_color = style->get_variable_color("--checkbox-checkmark", color_from_u8(255, 255, 255, 255));

    box_->set_fill(bg_color);
    box_->set_stroke(border_color, 2.0f);

    checkmark_->set_stroke(checkmark_color, 2.0f);
    checkmark_->set_visible(checkmark_scale_ > 0.01f);

    // Label text color
    if (text_) {
        text_->set_color(style->text_color);
    }
}

void CheckboxWidget::update_checkmark_path(float checkbox_size) {
    if (!checkmark_) return;

    float center_x = checkbox_size / 2;
    float center_y = checkbox_size / 2;
    float size = checkbox_size * 0.4f * checkmark_scale_;

    if (size < 0.1f) {
        checkmark_->set_path_data("");
        return;
    }

    // L-shaped checkmark path
    char path[128];
    snprintf(path, sizeof(path),
             "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
             center_x - size * 0.5f, center_y,
             center_x - size * 0.2f, center_y + size * 0.4f,
             center_x + size * 0.5f, center_y - size * 0.4f);

    checkmark_->set_path_data(path);
}

void CheckboxWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    float checkbox_size = style->get_variable_float("--checkbox-size", 20.0f);
    float label_spacing = style->get_variable_float("--label-spacing", 8.0f);

    // Rebuild shapes if dimensions changed
    rebuild_shapes(checkbox_size, label_spacing, style->font_family, style->font_size);

    // Update colors based on state
    update_colors(elem);

    // Update checkmark path for animation
    update_checkmark_path(checkbox_size);

    // Draw using flex::Renderer
    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    float opacity = style->opacity;
    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool CheckboxWidget::handle_event(const Event& event, Element& elem) {
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

void CheckboxWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--transition-duration", 200.0f);
    if (duration <= 0) {
        checkmark_scale_ = target_checkmark_scale_;
        return;
    }

    float speed = 1000.0f / duration;
    float delta = speed * (delta_ms / 1000.0f);

    if (std::abs(checkmark_scale_ - target_checkmark_scale_) > 0.01f) {
        if (checkmark_scale_ < target_checkmark_scale_) {
            checkmark_scale_ = std::min(checkmark_scale_ + delta, target_checkmark_scale_);
        } else {
            checkmark_scale_ = std::max(checkmark_scale_ - delta, target_checkmark_scale_);
        }
        elem.mark_paint_dirty();
    }
}

} // namespace tvgbox2
