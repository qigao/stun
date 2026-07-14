/*
 * flexUI - CheckboxWidget Implementation
 */

#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

CheckboxWidget::CheckboxWidget(const std::string& label, bool checked)
    : label_(label), checked_(checked) {
    target_checkmark_scale_ = checked ? 1.0f : 0.0f;
    checkmark_scale_ = target_checkmark_scale_;
}

void CheckboxWidget::build_semantic_tree() {
    box_ = create_part("box", "box");
    indicator_ = create_part("indicator", "indicator");
    label_element_ = create_part("label", "label");
}

bool CheckboxWidget::paints_part_box(std::string_view part_name) const {
    return part_name == "indicator";
}

bool CheckboxWidget::emit_part_render_commands(
    const Element& host, const Element& part, std::string_view part_name,
    RenderCommandList& commands) {
    (void)host;
    if (part_name != "indicator" || !part.computed_style ||
        part.width() <= 0.0f || part.height() <= 0.0f) {
        return part_name == "indicator";
    }

    const Color color = part.computed_style->text_color;
    const float stroke_width = std::max(std::min(part.width(), part.height()) * 0.12f, 1.5f);
    commands.draw_line(part.width() * 0.20f, part.height() * 0.52f,
                       part.width() * 0.42f, part.height() * 0.73f,
                       Paint::solid(color), stroke_width);
    commands.draw_line(part.width() * 0.42f, part.height() * 0.73f,
                       part.width() * 0.82f, part.height() * 0.27f,
                       Paint::solid(color), stroke_width);
    return true;
}

bool CheckboxWidget::measure_intrinsic_size(const Element& elem, float available_width,
                                            float available_height, float& out_width,
                                            float& out_height) const {
    (void)available_width;
    (void)available_height;
    auto* style = elem.computed_style;
    const float checkbox_size =
        style ? style->get_variable_float("--checkbox-size", 20.0f) : 20.0f;
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
    out_width = checkbox_size + (label_.empty() ? 0.0f : label_spacing + label_width);
    out_height = std::max(checkbox_size, font_size);
    return true;
}

void CheckboxWidget::sync_host_semantics() {
    set_host_attribute("role", "checkbox");
    set_host_data_state("checked", "unchecked", checked_);
    set_host_boolean_attribute("aria-checked", checked_);
    set_host_boolean_attribute("aria-disabled", disabled_);
    set_host_presence_attribute("disabled", disabled_);
    set_host_state("checked", checked_);
}

void CheckboxWidget::set_checked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        target_checkmark_scale_ = checked ? 1.0f : 0.0f;
        invalidate_render_cache();
        sync_host_semantics();

        if (change_callback_) {
            change_callback_(checked_);
        }
    }
}

void CheckboxWidget::set_label(const std::string& label) {
    label_ = label;
    if (label_element_) {
        label_element_->set_text(label);
    }
    invalidate_render_cache();
}

void CheckboxWidget::set_disabled(bool disabled) {
    if (disabled_ == disabled) {
        return;
    }
    disabled_ = disabled;
    invalidate_render_cache();
    sync_host_semantics();
}

void CheckboxWidget::update_part_geometry(const Element& elem) {
    if (!elem.computed_style || !box_ || !indicator_ || !label_element_ ||
        !box_->computed_style || !indicator_->computed_style ||
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
    const float legacy_size =
        elem.computed_style->get_variable_float("--checkbox-size", 20.0f);
    const float box_width = resolved_size(*box_, true, legacy_size);
    const float box_height = resolved_size(*box_, false, legacy_size);
    const float box_y = (elem.height() - box_height) * 0.5f;
    box_->set_layout_bounds(0.0f, box_y, box_width, box_height);

    const float indicator_width = resolved_size(*indicator_, true, box_width);
    const float indicator_height = resolved_size(*indicator_, false, box_height);
    const float scaled_width = indicator_width * checkmark_scale_;
    const float scaled_height = indicator_height * checkmark_scale_;
    indicator_->set_layout_bounds((box_width - scaled_width) * 0.5f,
                                  box_y + (box_height - scaled_height) * 0.5f,
                                  scaled_width, scaled_height);
    indicator_->set_visible(checkmark_scale_ > 0.01f);

    const float spacing =
        elem.computed_style->get_variable_float("--label-spacing", 8.0f);
    label_element_->set_text(label_);
    label_element_->set_visible(!label_.empty());
    const float label_width = label_.empty()
                                  ? 0.0f
                                  : approximate_segmented_text_width(
                                        label_element_->computed_style, label_);
    label_element_->set_layout_bounds(
        box_width + spacing, 0.0f, label_width, elem.height());
}

void CheckboxWidget::sync_host_semantics_for_layout(Element& elem) {
    sync_host_semantics();
    update_part_geometry(elem);
}

void CheckboxWidget::invalidate_render_cache() {
    dirty_ = true;
    if (auto* host = host_element()) {
        host->mark_paint_dirty();
    }
}

void CheckboxWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    if (!host_element() && elem.computed_style) {
        const auto* style = elem.computed_style;
        const float size = style->get_variable_float("--checkbox-size", 20.0f);
        const float spacing = style->get_variable_float("--label-spacing", 8.0f);
        const Color background = checked_ || elem.has_state("checked")
                                     ? style->get_variable_color(
                                           "--checkbox-bg-checked",
                                           style->get_variable_color(
                                               "--accent-color",
                                               color_from_u8(59, 130, 246, 255)))
                                     : style->get_variable_color(
                                           "--checkbox-bg",
                                           color_from_u8(255, 255, 255, 255));
        const Color border = style->get_variable_color(
            "--checkbox-border", color_from_u8(200, 200, 200, 255));
        commands.draw_rect(0.0f, 0.0f, size, size, size * 0.15f,
                           Paint::solid(background), Paint::solid(border), 2.0f);

        if (checkmark_scale_ > 0.01f) {
            const float center = size * 0.5f;
            const float mark_size = size * 0.4f * checkmark_scale_;
            const Color mark = style->get_variable_color(
                "--checkbox-checkmark", color_from_u8(255, 255, 255, 255));
            commands.draw_line(center - mark_size * 0.5f, center,
                               center - mark_size * 0.2f,
                               center + mark_size * 0.4f, Paint::solid(mark), 2.0f);
            commands.draw_line(center - mark_size * 0.2f,
                               center + mark_size * 0.4f,
                               center + mark_size * 0.5f,
                               center - mark_size * 0.4f, Paint::solid(mark), 2.0f);
        }
        if (!label_.empty()) {
            commands.draw_text(label_, size + spacing,
                               size * 0.5f - style->font_size * 0.5f,
                               style->font_family, style->font_size,
                               style->font_weight >= FontWeight::Bold,
                               style->text_color);
        }
    }
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

bool CheckboxWidget::needs_frame_update(const Element& elem) const {
    (void)elem;
    return std::abs(checkmark_scale_ - target_checkmark_scale_) > 0.01f;
}

void CheckboxWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--transition-duration", 0.0f);
    if (duration <= 0) {
        if (std::abs(checkmark_scale_ - target_checkmark_scale_) > 0.01f) {
            checkmark_scale_ = target_checkmark_scale_;
            elem.mark_paint_dirty();
        }
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

} // namespace flexUI
