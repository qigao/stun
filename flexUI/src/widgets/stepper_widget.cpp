/*
 * flexUI - StepperWidget Implementation
 */

#include <flexUI/widgets/stepper_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <stb_sprintf.h>

namespace flexUI {

StepperWidget::StepperWidget(int value, int min_value, int max_value, int step)
    : value_(value), min_value_(min_value), max_value_(max_value), step_(step) {
    value_ = std::clamp(value_, min_value_, max_value_);
}

void StepperWidget::set_value(int v) {
    int new_value = std::clamp(v, min_value_, max_value_);
    if (new_value != value_) {
        value_ = new_value;
        dirty_ = true;
        if (callback_) {
            callback_(value_);
        }
    }
}

void StepperWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        minus_bg_ != nullptr) {
        return;
    }

    root_.clear();
    cached_width_ = elem.width();
    cached_height_ = elem.height();

    float height = style->get_variable_float("--stepper-height", elem.height());
    float btn_width = style->get_variable_float("--stepper-button-width", 32.0f);
    float radius = style->get_variable_float("--stepper-radius", 4.0f);
    float value_width = elem.width() - btn_width * 2;

    float y = (elem.height() - height) / 2;

    // Minus button (left)
    minus_bg_ = root_.add<RectShape>(0, y, btn_width, height, radius);
    float minus_x = btn_width / 2 - style->font_size * 0.3f;
    float text_y = y + (height - style->font_size) / 2;
    minus_text_ = root_.add<TextShape>(minus_x, text_y, "-");
    minus_text_->set_font_family(style->font_family);
    minus_text_->set_font_size(style->font_size);

    // Value display (center)
    value_bg_ = root_.add<RectShape>(btn_width, y, value_width, height, 0);

    char buf[32];
    stbsp_snprintf(buf, sizeof(buf), "%d", value_);
    float char_width = style->font_size * 0.6f;
    float text_width = strlen(buf) * char_width;
    float value_x = btn_width + (value_width - text_width) / 2;
    value_text_ = root_.add<TextShape>(value_x, text_y, buf);
    value_text_->set_font_family(style->font_family);
    value_text_->set_font_size(style->font_size);

    // Plus button (right)
    float plus_x_pos = btn_width + value_width;
    plus_bg_ = root_.add<RectShape>(plus_x_pos, y, btn_width, height, radius);
    float plus_x = plus_x_pos + btn_width / 2 - style->font_size * 0.3f;
    plus_text_ = root_.add<TextShape>(plus_x, text_y, "+");
    plus_text_->set_font_family(style->font_family);
    plus_text_->set_font_size(style->font_size);
}

void StepperWidget::update_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style || !minus_bg_) return;

    Color bg = style->get_variable_color("--stepper-bg", color_from_u8(255, 255, 255, 255));
    Color btn_bg = style->get_variable_color("--stepper-button-bg", color_from_u8(243, 244, 246, 255));
    Color btn_hover = style->get_variable_color("--stepper-button-bg-hover", color_from_u8(229, 231, 235, 255));
    Color text_color = style->get_variable_color("--stepper-text", color_from_u8(17, 24, 39, 255));
    Color border = style->get_variable_color("--stepper-border", color_from_u8(209, 213, 219, 255));
    Color disabled = style->get_variable_color("--stepper-disabled", color_from_u8(156, 163, 175, 255));

    bool can_decrease = value_ > min_value_;
    bool can_increase = value_ < max_value_;

    // Minus button
    Color minus_color = (hovered_button_ == 0 && can_decrease) ? btn_hover : btn_bg;
    minus_bg_->set_fill(minus_color);
    minus_bg_->set_stroke(border, 1.0f);
    minus_text_->set_color(can_decrease ? text_color : disabled);

    // Value display
    value_bg_->set_fill(bg);
    value_bg_->set_stroke(border, 1.0f);
    value_text_->set_color(text_color);

    // Update value text
    char buf[32];
    stbsp_snprintf(buf, sizeof(buf), "%d", value_);
    value_text_->set_text(buf);

    // Plus button
    Color plus_color = (hovered_button_ == 1 && can_increase) ? btn_hover : btn_bg;
    plus_bg_->set_fill(plus_color);
    plus_bg_->set_stroke(border, 1.0f);
    plus_text_->set_color(can_increase ? text_color : disabled);
}

int StepperWidget::hit_test(float x, float y, const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return -1;

    float height = style->get_variable_float("--stepper-height", elem.height());
    float btn_width = style->get_variable_float("--stepper-button-width", 32.0f);
    float btn_y = (elem.height() - height) / 2;

    if (y < btn_y || y > btn_y + height) {
        return -1;
    }

    if (x >= 0 && x < btn_width) {
        return 0;  // Minus button
    }

    float plus_x = elem.width() - btn_width;
    if (x >= plus_x && x < elem.width()) {
        return 1;  // Plus button
    }

    return -1;
}

void StepperWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    rebuild_shapes(elem);
    update_shapes(elem);

    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());
    float opacity = style->opacity;

    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool StepperWidget::handle_event(const Event& event, Element& elem) {
    if (elem.has_state("disabled")) return false;

    switch (event.type) {
        case EventType::MouseMove: {
            float local_x = event.x - elem.absolute_x();
            float local_y = event.y - elem.absolute_y();
            int button = hit_test(local_x, local_y, elem);

            if (button != hovered_button_) {
                hovered_button_ = button;
                elem.mark_paint_dirty();
            }
            break;
        }

        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                if (hovered_button_ == 0 && value_ > min_value_) {
                    set_value(value_ - step_);
                    elem.mark_paint_dirty();
                    return true;
                } else if (hovered_button_ == 1 && value_ < max_value_) {
                    set_value(value_ + step_);
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

void StepperWidget::update(float delta_ms, Element& elem) {
    (void)delta_ms;
    (void)elem;
}

} // namespace flexUI
