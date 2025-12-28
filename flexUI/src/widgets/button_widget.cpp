/*
 * flexUI - ButtonWidget Implementation
 */

#include <flexUI/widgets/button_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>
#include <stb_sprintf.h>

namespace flexUI {

ButtonWidget::ButtonWidget(const std::string& text)
    : text_(text) {}

void ButtonWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Check if rebuild needed
    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        background_ != nullptr) {
        return;
    }

    root_.clear();
    cached_width_ = elem.width();
    cached_height_ = elem.height();

    float radius = style->border_radius[0];

    // Background
    background_ = root_.add<RectShape>(0, 0, elem.width(), elem.height(), radius);

    // Text (centered)
    std::string display_text = !elem.text().empty() ? elem.text() : text_;
    if (!display_text.empty()) {
        float char_width = style->font_size * 0.6f;
        float text_width = display_text.size() * char_width;
        float text_x = (elem.width() - text_width) / 2;
        float text_y = (elem.height() - style->font_size) / 2;

        text_shape_ = root_.add<TextShape>(text_x, text_y, display_text);
        text_shape_->set_font_family(style->font_family);
        text_shape_->set_font_size(style->font_size);
    }
}

void ButtonWidget::update_shapes(const Element& elem) {
    if (!background_) return;

    auto* style = elem.computed_style;
    if (!style) return;

    bool is_loading_state = loading_ || elem.has_state("loading");

    // Background color
    Color bg_color = style->get_variable_color("--bg", style->background_color);
    background_->set_fill(bg_color);

    // Text color and visibility
    if (text_shape_) {
        Color text_color = style->get_variable_color("--text", style->text_color);
        text_shape_->set_color(text_color);
        text_shape_->set_visible(!is_loading_state);
    }
}

void ButtonWidget::render_ripples(flex::Renderer& renderer, const Transform& world_transform, float opacity, const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    Color ripple_color = style->get_variable_color("--ripple-color", color_from_u8(255, 255, 255, 128));

    for (const auto& ripple : ripples_) {
        if (ripple.finished) continue;

        Color c = ripple_color;
        c.a *= ripple.alpha * opacity;

        renderer.save();
        renderer.set_transform(world_transform);
        renderer.draw_circle(ripple.x, ripple.y, ripple.radius, Paint::solid(c), Paint::none(), 0);
        renderer.restore();
    }
}

void ButtonWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Rebuild shapes if needed
    rebuild_shapes(elem);

    // Update shape properties
    update_shapes(elem);

    // Apply scale transform
    Transform scale_transform = Transform::Identity();
    if (current_scale_ != 1.0f) {
        float cx = elem.width() / 2;
        float cy = elem.height() / 2;
        scale_transform = flex::make_translation(cx, cy) *
                          flex::make_scale(current_scale_, current_scale_) *
                          flex::make_translation(-cx, -cy);
    }

    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y()) * scale_transform;

    float opacity = style->opacity;

    // Draw main group
    root_.draw(renderer.flex(), world_transform, opacity);

    // Draw ripples (separate from group, as they're dynamic)
    if (style->get_variable("--ripple", "false") == "true") {
        render_ripples(renderer.flex(), world_transform, opacity, elem);
    }

    // Draw loading spinner if needed
    bool is_loading_state = loading_ || elem.has_state("loading");
    if (is_loading_state) {
        Color spinner_color = style->get_variable_color("--loading-spinner-color", style->text_color);
        float cx = elem.width() / 2;
        float cy = elem.height() / 2;
        float r = style->font_size * 0.6f;

        // Draw arc using path
        char path[256];
        float start_rad = spinner_rotation_ * 3.14159f / 180.0f;
        float end_rad = (spinner_rotation_ + 270) * 3.14159f / 180.0f;
        float x1 = cx + r * std::cos(start_rad);
        float y1 = cy + r * std::sin(start_rad);
        float x2 = cx + r * std::cos(end_rad);
        float y2 = cy + r * std::sin(end_rad);

        stbsp_snprintf(path, sizeof(path), "M %.4g %.4g A %.4g %.4g 0 1 1 %.4g %.4g",
                 x1, y1, r, r, x2, y2);

        renderer.flex().save();
        renderer.flex().set_transform(world_transform);
        renderer.flex().stroke_path(path, Paint::solid(spinner_color), 2.0f);
        renderer.flex().restore();
    }

    dirty_ = false;
}

bool ButtonWidget::handle_event(const Event& event, Element& elem) {
    if (disabled_ || loading_ || elem.has_state("disabled") || elem.has_state("loading")) {
        return false;
    }

    auto* style = elem.computed_style;
    if (!style) return false;

    switch (event.type) {
        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                // Add ripple effect
                if (style->get_variable("--ripple", "false") == "true") {
                    float local_x = event.x - elem.absolute_x();
                    float local_y = event.y - elem.absolute_y();
                    add_ripple(local_x, local_y, elem);
                }

                target_scale_ = style->get_variable_float("--active-scale", 0.95f);
                elem.mark_paint_dirty();
            }
            return false;

        case EventType::MouseUp:
            if (elem.has_state("hover")) {
                target_scale_ = style->get_variable_float("--hover-scale", 1.0f);
            } else {
                target_scale_ = 1.0f;
            }
            elem.mark_paint_dirty();
            return false;

        default:
            break;
    }

    return false;
}

void ButtonWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Update scale transition
    float duration = style->get_variable_float("--transition-duration", 0);
    if (duration > 0) {
        float speed = 1000.0f / duration;
        float delta = speed * (delta_ms / 1000.0f);

        if (std::abs(current_scale_ - target_scale_) > 0.001f) {
            if (current_scale_ < target_scale_) {
                current_scale_ = std::min(current_scale_ + delta, target_scale_);
            } else {
                current_scale_ = std::max(current_scale_ - delta, target_scale_);
            }
            elem.mark_paint_dirty();
        }
    } else {
        current_scale_ = target_scale_;
    }

    // Update hover state target
    if (elem.has_state("hover") && !elem.has_state("active")) {
        float hover_scale = style->get_variable_float("--hover-scale", 1.0f);
        if (target_scale_ != hover_scale) {
            target_scale_ = hover_scale;
        }
    } else if (!elem.has_state("hover") && !elem.has_state("active")) {
        if (target_scale_ != 1.0f) {
            target_scale_ = 1.0f;
        }
    }

    // Update ripples
    update_ripples(delta_ms, elem);

    // Update spinner
    if (loading_ || elem.has_state("loading")) {
        spinner_rotation_ += (360.0f / 1000.0f) * delta_ms;
        if (spinner_rotation_ >= 360.0f) {
            spinner_rotation_ -= 360.0f;
        }
        elem.mark_paint_dirty();
    }
}

void ButtonWidget::add_ripple(float x, float y, const Element& elem) {
    Ripple ripple;
    ripple.x = x;
    ripple.y = y;
    ripple.radius = 0;
    ripple.alpha = 1.0f;

    // Calculate max radius: distance to farthest corner
    float dx1 = x, dy1 = y;
    float dx2 = elem.width() - x, dy2 = elem.height() - y;
    float dist1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    float dist2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    float dist3 = std::sqrt(dx1 * dx1 + dy2 * dy2);
    float dist4 = std::sqrt(dx2 * dx2 + dy1 * dy1);

    ripple.max_radius = std::max({dist1, dist2, dist3, dist4});

    ripples_.push_back(ripple);
    dirty_ = true;
}

void ButtonWidget::update_ripples(float delta_ms, Element& elem) {
    bool any_active = false;

    for (auto& ripple : ripples_) {
        if (ripple.finished) continue;

        // Ripple expansion speed: 300ms to reach max radius
        float speed = ripple.max_radius / 300.0f;
        ripple.radius += speed * delta_ms;

        // Alpha decay
        ripple.alpha = 1.0f - (ripple.radius / ripple.max_radius);

        if (ripple.radius >= ripple.max_radius) {
            ripple.finished = true;
        } else {
            any_active = true;
        }
    }

    // Clean up finished ripples
    ripples_.erase(
        std::remove_if(ripples_.begin(), ripples_.end(),
                       [](const Ripple& r) { return r.finished; }),
        ripples_.end()
    );

    if (any_active) {
        elem.mark_paint_dirty();
    }
}

} // namespace flexUI
