/*
 * flexUI - ButtonWidget Implementation
 */

#include <flexUI/widgets/button_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <cmath>
#include <stb_sprintf.h>

namespace flexUI {

namespace {

struct ButtonPalette {
    Color background;
    Color background_hover;
    Color background_active;
    Color text;
    Color border;
    Color focus_border;
    float border_width = 0.0f;
};

struct ButtonMetrics {
    float radius = 8.0f;
    float font_size = 14.0f;
};

bool approx_equal(float lhs, float rhs) {
    return std::fabs(lhs - rhs) <= 0.001f;
}

bool is_default_background(const Color& color) {
    return approx_equal(color.r, 1.0f) &&
           approx_equal(color.g, 1.0f) &&
           approx_equal(color.b, 1.0f) &&
           approx_equal(color.a, 0.0f);
}

bool is_default_text(const Color& color) {
    return approx_equal(color.r, 0.0f) &&
           approx_equal(color.g, 0.0f) &&
           approx_equal(color.b, 0.0f) &&
           approx_equal(color.a, 1.0f);
}

bool is_default_border(const Color& color) {
    return is_default_text(color);
}

Color rgba_u8(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

Color mix(const Color& lhs, const Color& rhs, float t) {
    return {
        lhs.r + (rhs.r - lhs.r) * t,
        lhs.g + (rhs.g - lhs.g) * t,
        lhs.b + (rhs.b - lhs.b) * t,
        lhs.a + (rhs.a - lhs.a) * t,
    };
}

ButtonPalette button_palette(ButtonWidget::Variant variant) {
    switch (variant) {
    case ButtonWidget::Variant::Secondary:
        return {
            rgba_u8(241, 245, 249),
            rgba_u8(226, 232, 240),
            rgba_u8(203, 213, 225),
            rgba_u8(15, 23, 42),
            rgba_u8(226, 232, 240),
            rgba_u8(59, 130, 246, 255),
            1.0f,
        };
    case ButtonWidget::Variant::Outline:
        return {
            rgba_u8(255, 255, 255, 0),
            rgba_u8(248, 250, 252),
            rgba_u8(226, 232, 240),
            rgba_u8(15, 23, 42),
            rgba_u8(203, 213, 225),
            rgba_u8(59, 130, 246, 255),
            1.0f,
        };
    case ButtonWidget::Variant::Ghost:
        return {
            rgba_u8(255, 255, 255, 0),
            rgba_u8(241, 245, 249),
            rgba_u8(226, 232, 240),
            rgba_u8(15, 23, 42),
            rgba_u8(255, 255, 255, 0),
            rgba_u8(59, 130, 246, 255),
            0.0f,
        };
    case ButtonWidget::Variant::Destructive:
        return {
            rgba_u8(220, 38, 38),
            rgba_u8(185, 28, 28),
            rgba_u8(153, 27, 27),
            rgba_u8(248, 250, 252),
            rgba_u8(220, 38, 38, 0),
            rgba_u8(248, 113, 113, 255),
            0.0f,
        };
    case ButtonWidget::Variant::Default:
    default:
        return {
            rgba_u8(15, 23, 42),
            rgba_u8(30, 41, 59),
            rgba_u8(2, 6, 23),
            rgba_u8(248, 250, 252),
            rgba_u8(15, 23, 42, 0),
            rgba_u8(96, 165, 250, 255),
            0.0f,
        };
    }
}

ButtonMetrics button_metrics(ButtonWidget::Size size) {
    switch (size) {
    case ButtonWidget::Size::Small:
        return {6.0f, 13.0f};
    case ButtonWidget::Size::Large:
        return {10.0f, 16.0f};
    case ButtonWidget::Size::Icon:
        return {8.0f, 14.0f};
    case ButtonWidget::Size::Default:
    default:
        return {8.0f, 14.0f};
    }
}

} // namespace

ButtonWidget::ButtonWidget(const std::string& text)
    : text_(text) {}

void ButtonWidget::build_semantic_tree() {
    ripple_layer_ = create_part("ripple-layer", "ripple-layer");
    label_element_ = create_part("label", "label");
    spinner_element_ = create_part("spinner", "spinner");
}

bool ButtonWidget::paints_part_box(std::string_view part_name) const {
    return part_name == "label" || part_name == "ripple-layer" ||
           part_name == "spinner";
}

Color ButtonWidget::resolved_text_color(const Element& elem) const {
    const auto* style = elem.computed_style;
    if (!style) return Color{};
    const auto palette = button_palette(variant_);
    const bool has_css_text =
        !style->get_variable("--button-text", "").empty() ||
        !style->get_variable("--text", "").empty() ||
        !is_default_text(style->text_color);
    Color color = style->get_variable_color(
        "--button-text", style->get_variable_color(
                             "--text", has_css_text ? style->text_color
                                                    : palette.text));
    if (disabled_ || loading_ || elem.has_state("disabled") ||
        elem.has_state("loading")) {
        color.a *= 0.65f;
    }
    return color;
}

bool ButtonWidget::emit_part_render_commands(
    const Element& host, const Element& part, std::string_view part_name,
    RenderCommandList& commands) {
    const auto* style = host.computed_style;
    if (!style) return true;
    const auto& caps = commands.capabilities();
    const float render_scale = caps.scaling ? current_scale_ : 1.0f;
    Transform transform;
    if (!approx_equal(render_scale, 1.0f)) {
        const float cx = host.width() * 0.5f;
        const float cy = host.height() * 0.5f;
        transform = flex::make_translation(cx, cy) *
                    flex::make_scale(render_scale, render_scale) *
                    flex::make_translation(-cx, -cy);
    }

    if (part_name == "label") {
        const std::string display = !host.text().empty() ? host.text() : text_;
        if (!display.empty() && !loading_ && !host.has_state("loading")) {
            ComputedStyle text_style = *style;
            const auto metrics = button_metrics(size_);
            text_style.font_size = style->get_variable_float(
                "--button-font-size",
                !approx_equal(style->font_size, 16.0f) ? style->font_size
                                                       : metrics.font_size);
            const Color color = part.computed_style
                                    ? part.computed_style->text_color
                                    : resolved_text_color(host);
            auto block = layout_text_block(&text_style, display, 0.0f, 0.0f,
                                           host.width(), host.height(), color,
                                           TextVerticalAlign::Middle);
            commands.save();
            commands.set_transform(transform);
            emit_text_block(commands, block);
            commands.restore();
        }
        return true;
    }
    if (part_name == "ripple-layer") {
        if (style->get_variable("--ripple", "false") == "true") {
            render_ripples(commands, transform, style->opacity, host);
        }
        return true;
    }
    if (part_name == "spinner") {
        if (loading_ || host.has_state("loading")) {
            const auto metrics = button_metrics(size_);
            const float font_size = style->get_variable_float(
                "--button-font-size",
                !approx_equal(style->font_size, 16.0f) ? style->font_size
                                                       : metrics.font_size);
            const float radius = font_size * 0.6f;
            const float cx = host.width() * 0.5f;
            const float cy = host.height() * 0.5f;
            const float start = spinner_rotation_ * 3.14159f / 180.0f;
            const float end = (spinner_rotation_ + 270.0f) * 3.14159f / 180.0f;
            char path[256];
            stbsp_snprintf(path, sizeof(path),
                           "M %.4g %.4g A %.4g %.4g 0 1 1 %.4g %.4g",
                           cx + radius * std::cos(start),
                           cy + radius * std::sin(start), radius, radius,
                           cx + radius * std::cos(end),
                           cy + radius * std::sin(end));
            const Color color = part.computed_style
                                    ? part.computed_style->text_color
                                    : resolved_text_color(host);
            commands.save();
            commands.set_transform(transform);
            commands.stroke_path(path, Paint::solid(color), 2.0f);
            commands.restore();
        }
        return true;
    }
    return false;
}

void ButtonWidget::update_part_geometry(const Element& elem) {
    for (Element* part : {ripple_layer_, label_element_, spinner_element_}) {
        if (part) part->set_layout_bounds(0.0f, 0.0f, elem.width(), elem.height());
    }
    if (label_element_) {
        label_element_->set_visible(!loading_ && !elem.has_state("loading"));
    }
    if (spinner_element_) {
        spinner_element_->set_visible(loading_ || elem.has_state("loading"));
    }
    if (ripple_layer_) ripple_layer_->set_visible(!ripples_.empty());
}

void ButtonWidget::sync_host_semantics_for_layout(Element& elem) {
    sync_host_semantics();
    update_part_geometry(elem);
}

bool ButtonWidget::measure_intrinsic_size(const Element& elem, float available_width,
                                          float available_height, float& out_width,
                                          float& out_height) const {
    (void)available_width;
    (void)available_height;
    const auto metrics = button_metrics(size_);
    const auto* style = elem.computed_style;
    float font_size = metrics.font_size;
    if (style) {
        font_size = style->get_variable_float(
            "--button-font-size",
            !approx_equal(style->font_size, 16.0f) ? style->font_size : metrics.font_size);
    }

    if (size_ == Size::Icon) {
        const float side = std::max(font_size + 20.0f, 36.0f);
        out_width = side;
        out_height = side;
        return true;
    }

    float padding_x = 16.0f;
    float min_height = 36.0f;
    switch (size_) {
    case Size::Small:
        padding_x = 12.0f;
        min_height = 32.0f;
        break;
    case Size::Large:
        padding_x = 24.0f;
        min_height = 40.0f;
        break;
    case Size::Default:
    default:
        break;
    }

    ComputedStyle measure_style;
    if (style) {
        measure_style = *style;
    }
    measure_style.font_size = font_size;
    const std::string display_text = !elem.text().empty() ? elem.text() : text_;
    const float text_width = display_text.empty()
                                 ? font_size
                                 : approximate_segmented_text_width(&measure_style, display_text);
    out_width = std::max(text_width + padding_x * 2.0f, min_height);
    out_height = std::max(font_size + 18.0f, min_height);
    return true;
}

void ButtonWidget::sync_host_semantics() {
    set_host_attribute("role", "button");
    set_host_boolean_attribute("aria-disabled", disabled_ || loading_);
    set_host_boolean_attribute("aria-busy", loading_);

    if (loading_) {
        set_host_attribute("data-state", "loading");
    } else if (disabled_) {
        set_host_attribute("data-state", "disabled");
    } else {
        set_host_attribute("data-state", "idle");
    }

    switch (variant_) {
    case Variant::Secondary:
        set_host_attribute("data-variant", "secondary");
        break;
    case Variant::Outline:
        set_host_attribute("data-variant", "outline");
        break;
    case Variant::Ghost:
        set_host_attribute("data-variant", "ghost");
        break;
    case Variant::Destructive:
        set_host_attribute("data-variant", "destructive");
        break;
    case Variant::Default:
    default:
        set_host_attribute("data-variant", "default");
        break;
    }

    switch (size_) {
    case Size::Small:
        set_host_attribute("data-size", "sm");
        break;
    case Size::Large:
        set_host_attribute("data-size", "lg");
        break;
    case Size::Icon:
        set_host_attribute("data-size", "icon");
        break;
    case Size::Default:
    default:
        set_host_attribute("data-size", "default");
        break;
    }
}

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
    text_shapes_.clear();
    text_decoration_shapes_.clear();
    cached_width_ = elem.width();
    cached_height_ = elem.height();

    const auto metrics = button_metrics(size_);
    float radius = style->border_radius[0] > 0.0f
        ? style->border_radius[0]
        : style->get_variable_float("--button-radius", metrics.radius);

    // Background
    background_ =
        root_.add<RectShape>(0.0f, 0.0f, elem.width(), elem.height(), radius);

    // Text (centered)
    std::string display_text = !elem.text().empty() ? elem.text() : text_;
    if (!display_text.empty()) {
        const float font_size = style->get_variable_float(
            "--button-font-size",
            !approx_equal(style->font_size, 16.0f) ? style->font_size : metrics.font_size);
        ComputedStyle text_measure = *style;
        text_measure.font_size = font_size;
        const auto text_block = layout_text_block(
            &text_measure, display_text, 0.0f, 0.0f, elem.width(), elem.height(),
            style->text_color, TextVerticalAlign::Middle);

        for (const auto& line : text_block.lines) {
            auto* text_shape = root_.add<TextShape>(line.x, line.baseline_y, line.text);
            text_shape->set_font_family(text_block.font_family);
            text_shape->set_font_size(text_block.font_size);
            text_shape->set_bold(text_block.bold);
            text_shapes_.push_back(text_shape);

            if (text_block.overline && line.width > 0.0f) {
                auto* overline = root_.add<LineShape>(
                    line.x, line.baseline_y,
                    line.x + line.width, line.baseline_y,
                    text_block.decoration_thickness);
                text_decoration_shapes_.push_back(overline);
            }
            if (text_block.underline && line.width > 0.0f) {
                const float underline_y =
                    line.baseline_y + text_block.font_size + text_block.underline_offset;
                auto* underline = root_.add<LineShape>(
                    line.x, underline_y,
                    line.x + line.width, underline_y,
                    text_block.decoration_thickness);
                text_decoration_shapes_.push_back(underline);
            }
            if (text_block.line_through && line.width > 0.0f) {
                const float strike_y =
                    line.baseline_y + text_block.font_size * 0.5f +
                    text_block.line_through_offset;
                auto* strike = root_.add<LineShape>(
                    line.x, strike_y,
                    line.x + line.width, strike_y,
                    text_block.decoration_thickness);
                text_decoration_shapes_.push_back(strike);
            }
        }
    }
}

void ButtonWidget::update_shapes(const Element& elem) {
    if (!background_) return;

    auto* style = elem.computed_style;
    if (!style) return;

    const auto palette = button_palette(variant_);
    const bool is_loading_state = loading_ || elem.has_state("loading");
    const bool is_disabled_state = disabled_ || elem.has_state("disabled");
    const bool has_css_background =
        !style->get_variable("--button-bg", "").empty() ||
        !style->get_variable("--bg", "").empty() ||
        !is_default_background(style->background_color);
    const bool has_css_text =
        !style->get_variable("--button-text", "").empty() ||
        !style->get_variable("--text", "").empty() ||
        !is_default_text(style->text_color);
    const bool has_css_border =
        !style->get_variable("--button-border", "").empty() ||
        !style->get_variable("--border-color", "").empty() ||
        !style->get_variable("--border", "").empty() ||
        !is_default_border(style->border_color);

    Color bg_color = style->get_variable_color(
        "--button-bg", style->get_variable_color("--bg",
        has_css_background ? style->background_color : palette.background));
    Color bg_hover = style->get_variable_color(
        "--button-bg-hover",
        has_css_background ? mix(bg_color, Color{1.0f, 1.0f, 1.0f, bg_color.a}, 0.08f)
                           : palette.background_hover);
    Color bg_active = style->get_variable_color(
        "--button-bg-active",
        has_css_background ? mix(bg_color, Color{0.0f, 0.0f, 0.0f, bg_color.a}, 0.12f)
                           : palette.background_active);
    Color text_color = style->get_variable_color(
        "--button-text", style->get_variable_color("--text",
        has_css_text ? style->text_color : palette.text));
    Color border_color = style->get_variable_color(
        "--button-border",
        style->get_variable_color("--border-color",
        style->get_variable_color("--border",
        has_css_border ? style->border_color : palette.border)));
    Color focus_border = style->get_variable_color("--button-border-focus",
                                                   palette.focus_border);
    float border_width = style->get_variable_float(
        "--button-border-width",
        has_css_border ? 1.0f : palette.border_width);

    if (elem.has_state("active")) {
        bg_color = bg_active;
    } else if (elem.has_state("hover")) {
        bg_color = bg_hover;
    }

    if (elem.has_state("focus") && border_width <= 0.0f) {
        border_width = 1.5f;
        border_color = focus_border;
    } else if (elem.has_state("focus")) {
        border_color = focus_border;
    }

    if (is_disabled_state || is_loading_state) {
        bg_color = mix(bg_color, Color{1.0f, 1.0f, 1.0f, bg_color.a}, 0.35f);
        text_color.a *= 0.65f;
        border_color.a *= 0.75f;
    }

    background_->set_fill(bg_color);
    if (border_width > 0.0f && border_color.a > 0.0f) {
        background_->set_stroke(border_color, border_width);
    } else {
        background_->set_stroke(Paint::none(), 0.0f);
    }

    // Text color and visibility
    for (auto* text_shape : text_shapes_) {
        if (!text_shape) {
            continue;
        }
        text_shape->set_color(text_color);
        text_shape->set_visible(!is_loading_state);
    }
    for (auto* line_shape : text_decoration_shapes_) {
        if (!line_shape) {
            continue;
        }
        const Color decoration_color = style->get_variable_color(
            Symbol("--text-decoration-color"), text_color);
        line_shape->set_stroke(decoration_color, line_shape->stroke_width());
        line_shape->set_visible(!is_loading_state);
    }
}

void ButtonWidget::render_ripples(RenderCommandList& commands, const Transform& local_transform, float opacity, const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    Color ripple_color = style->get_variable_color("--ripple-color", color_from_u8(255, 255, 255, 128));

    for (const auto& ripple : ripples_) {
        if (ripple.finished) continue;

        Color c = ripple_color;
        c.a *= ripple.alpha * opacity;

        commands.save();
        commands.set_transform(local_transform);
        commands.draw_circle(ripple.x, ripple.y, ripple.radius, Paint::solid(c), Paint::none(), 0);
        commands.restore();
    }
}

bool ButtonWidget::render_cache_matches(const Element& elem,
                                        const flex::RendererCapabilities& caps) const {
    const auto* style = elem.computed_style;
    if (!render_cache_valid_ || !style) {
        return false;
    }
    if (loading_ || elem.has_state("loading") || !ripples_.empty()) {
        return false;
    }
    if (!approx_equal(current_scale_, target_scale_)) {
        return false;
    }

    const std::string& elem_text = elem.text();
    const std::string& display_text = !elem_text.empty() ? elem_text : text_;
    const float render_scale = caps.scaling ? current_scale_ : 1.0f;
    return cached_text_ == display_text &&
           cached_scaling_capability_ == caps.scaling &&
           cached_hover_state_ == elem.has_state("hover") &&
           cached_active_state_ == elem.has_state("active") &&
           cached_focus_state_ == elem.has_state("focus") &&
           cached_disabled_state_ == (disabled_ || elem.has_state("disabled")) &&
           cached_loading_state_ == false &&
           cached_style_signature_ == style->variables_signature() &&
           cached_variant_ == static_cast<int>(variant_) &&
           cached_size_ == static_cast<int>(size_) &&
           cached_render_width_ == elem.width() &&
           cached_render_height_ == elem.height() &&
           cached_render_scale_ == render_scale &&
           cached_opacity_ == style->opacity &&
           cached_font_size_ == style->font_size &&
           cached_letter_spacing_ == style->letter_spacing &&
           cached_word_spacing_ == style->word_spacing &&
           cached_text_indent_ == style->text_indent &&
           cached_tab_size_ == style->tab_size &&
           cached_border_radius_ == style->border_radius[0] &&
           cached_background_color_ == style->background_color &&
           cached_text_color_ == style->text_color &&
           cached_border_color_ == style->border_color &&
           cached_font_weight_ == static_cast<int>(style->font_weight) &&
           cached_font_style_ == static_cast<int>(style->font_style) &&
           cached_text_align_ == static_cast<int>(style->text_align) &&
           cached_text_transform_ == static_cast<int>(style->text_transform) &&
           cached_direction_ == static_cast<int>(style->direction) &&
           cached_font_family_ == style->font_family;
}

void ButtonWidget::update_render_cache_key(const Element& elem,
                                           const flex::RendererCapabilities& caps) {
    const auto* style = elem.computed_style;
    if (!style) {
        render_cache_valid_ = false;
        return;
    }

    const std::string& elem_text = elem.text();
    cached_text_ = !elem_text.empty() ? elem_text : text_;
    cached_scaling_capability_ = caps.scaling;
    cached_hover_state_ = elem.has_state("hover");
    cached_active_state_ = elem.has_state("active");
    cached_focus_state_ = elem.has_state("focus");
    cached_disabled_state_ = disabled_ || elem.has_state("disabled");
    cached_loading_state_ = false;
    cached_style_signature_ = style->variables_signature();
    cached_variant_ = static_cast<int>(variant_);
    cached_size_ = static_cast<int>(size_);
    cached_render_width_ = elem.width();
    cached_render_height_ = elem.height();
    cached_render_scale_ = caps.scaling ? current_scale_ : 1.0f;
    cached_opacity_ = style->opacity;
    cached_font_size_ = style->font_size;
    cached_letter_spacing_ = style->letter_spacing;
    cached_word_spacing_ = style->word_spacing;
    cached_text_indent_ = style->text_indent;
    cached_tab_size_ = style->tab_size;
    cached_border_radius_ = style->border_radius[0];
    cached_background_color_ = style->background_color;
    cached_text_color_ = style->text_color;
    cached_border_color_ = style->border_color;
    cached_font_weight_ = static_cast<int>(style->font_weight);
    cached_font_style_ = static_cast<int>(style->font_style);
    cached_text_align_ = static_cast<int>(style->text_align);
    cached_text_transform_ = static_cast<int>(style->text_transform);
    cached_direction_ = static_cast<int>(style->direction);
    cached_font_family_ = style->font_family;
    render_cache_valid_ = true;
}

void ButtonWidget::invalidate_render_cache() {
    render_cache_valid_ = false;
    dirty_ = true;
    if (auto* host = host_element()) {
        host->mark_paint_dirty();
    }
}

void ButtonWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    auto* style = elem.computed_style;
    if (!style) return;
    sync_host_semantics();

    const auto& caps = commands.capabilities();
    if (render_cache_matches(elem, caps)) {
        commands.append_with_transform_prefix(render_cache_);
        dirty_ = false;
        return;
    }

    // Rebuild the host surface. Stable content layers are real child elements.
    rebuild_shapes(elem);

    // Update shape properties
    update_shapes(elem);
    if (host_element() && is_semantic_tree_rendering()) {
        for (auto* text : text_shapes_) if (text) text->set_visible(false);
        for (auto* line : text_decoration_shapes_) if (line) line->set_visible(false);
    }

    // Apply scale transform
    const float render_scale = caps.scaling ? current_scale_ : 1.0f;
    Transform scale_transform = Transform(); // identity
    if (render_scale != 1.0f) {
        float cx = elem.width() / 2;
        float cy = elem.height() / 2;
        scale_transform = flex::make_translation(cx, cy) *
                          flex::make_scale(render_scale, render_scale) *
                          flex::make_translation(-cx, -cy);
    }

    Transform local_transform = scale_transform;

    float opacity = style->opacity;

    const bool is_loading_state = loading_ || elem.has_state("loading");
    const bool cacheable_static_frame =
        !is_loading_state && ripples_.empty() && approx_equal(current_scale_, target_scale_);

    if (cacheable_static_frame) {
        RenderCommandList rebuilt(caps);
        root_.draw(rebuilt, local_transform, opacity);
        render_cache_ = rebuilt.commands();
        update_render_cache_key(elem, caps);
        commands.append_with_transform_prefix(render_cache_);
        dirty_ = false;
        return;
    }

    root_.draw(commands, local_transform, opacity);

    // Draw ripples (separate from group, as they're dynamic)
    if ((!host_element() || !is_semantic_tree_rendering()) &&
        style->get_variable("--ripple", "false") == "true") {
        render_ripples(commands, local_transform, opacity, elem);
    }

    // Draw loading spinner if needed
    if ((!host_element() || !is_semantic_tree_rendering()) && is_loading_state) {
        const auto palette = button_palette(variant_);
        const bool has_css_text =
            !style->get_variable("--button-text", "").empty() ||
            !style->get_variable("--text", "").empty() ||
            !is_default_text(style->text_color);
        Color spinner_color = style->get_variable_color(
            "--loading-spinner-color",
            style->get_variable_color("--button-text",
            style->get_variable_color("--text",
            has_css_text ? style->text_color : palette.text)));
        float cx = elem.width() / 2;
        float cy = elem.height() / 2;
        const auto metrics = button_metrics(size_);
        const float font_size = style->get_variable_float(
            "--button-font-size",
            !approx_equal(style->font_size, 16.0f) ? style->font_size : metrics.font_size);
        float r = font_size * 0.6f;

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

        commands.save();
        commands.set_transform(local_transform);
        commands.stroke_path(path, Paint::solid(spinner_color), 2.0f);
        commands.restore();
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
        case EventType::MouseEnter: {
            if (!elem.has_state("active")) {
                const float next_scale =
                    style->get_variable_float("--hover-scale", 1.0f);
                if (!approx_equal(target_scale_, next_scale)) {
                    target_scale_ = next_scale;
                    if (style->get_variable_float("--transition-duration", 0) <= 0.0f) {
                        current_scale_ = target_scale_;
                    }
                    elem.mark_paint_dirty();
                }
            }
            return false;
        }

        case EventType::MouseLeave: {
            if (!elem.has_state("active") && !approx_equal(target_scale_, 1.0f)) {
                target_scale_ = 1.0f;
                if (style->get_variable_float("--transition-duration", 0) <= 0.0f) {
                    current_scale_ = target_scale_;
                }
                elem.mark_paint_dirty();
            }
            return false;
        }

        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                // Add ripple effect
                if (style->get_variable("--ripple", "false") == "true") {
                    const flex::Vec2 local_pos =
                        detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
                    float local_x = local_pos.x;
                    float local_y = local_pos.y;
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

bool ButtonWidget::needs_frame_update(const Element& elem) const {
    return loading_ || elem.has_state("loading") || !ripples_.empty() ||
           std::abs(current_scale_ - target_scale_) > 0.001f;
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
    invalidate_render_cache();
}

void ButtonWidget::update_ripples(float delta_ms, Element& elem) {
    bool any_active = false;
    const size_t previous_count = ripples_.size();

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

    if (any_active || ripples_.size() != previous_count) {
        elem.mark_paint_dirty();
    }
}

} // namespace flexUI
