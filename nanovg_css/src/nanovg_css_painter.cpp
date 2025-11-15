/*
 * NanoVG CSS - Painter implementation
 *
 * Translates CSS properties to NanoVG drawing calls.
 */

#include "nanovg_css_internal.h"
#include <sstream>
#include <algorithm>
 // PHASE 4 SPRINT 4: Helper to create NVGCSSBox from computed layout
  static NVGCSSBox computed_to_box(const NVGCSSComputedLayout& computed) {
      NVGCSSBox box;
      box.x = computed.x;
      box.y = computed.y;
      box.width = computed.width;
      box.height = computed.height;
      for (int i = 0; i < 4; i++) {
          box.padding[i] = computed.padding[i];
          box.margin[i] = computed.margin[i];
          box.border_width[i] = computed.border[i];
          box.border_radius[i] = computed.border_radius[i];
      }
      return box;
  }

static float safe_stof(const std::string& value, float fallback = 0.f) {
    try {
        size_t processed = 0;
        float parsed = std::stof(value, &processed);
        return processed == 0 ? fallback : parsed;
    } catch (...) {
        return fallback;
    }
}

// Forward declaration for Sprint 31 background image rendering
static void render_background_image(const NVGCSSElement* element,
                                    const std::map<std::string, std::string>& style,
                                    const NVGCSSBox& box,
                                    NVGCSSRenderer* renderer);
NVGCSSPainter::NVGCSSPainter(NVGcontext* vg, NVGCSSRenderer* renderer) : vg_(vg), renderer_(renderer) {
}

// Sprint 33: Helper function to render a single border side with style
static void render_border_side(NVGcontext* vg,
                               float x1, float y1, float x2, float y2,
                               float width, NVGcolor color,
                               const std::string& style)
{
    if (style == "none" || style == "hidden" || width <= 0) {
        return;  // Don't render
    }

    if (style == "solid") {
        // Solid line (default)
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);
    }
    else if (style == "dashed") {
        // Dashed line
        nvgLineCap(vg, NVG_BUTT);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);

        float dash_length = width * 3.0f;  // Dash is 3x border width
        float gap_length = width * 3.0f;   // Gap is 3x border width

        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);

        if (length < 0.01f) return;  // Avoid division by zero

        float ux = dx / length;  // Unit vector
        float uy = dy / length;

        float pos = 0;
        bool draw = true;

        while (pos < length) {
            float segment_len = draw ? dash_length : gap_length;
            float end_pos = std::min(pos + segment_len, length);

            if (draw) {
                float sx = x1 + ux * pos;
                float sy = y1 + uy * pos;
                float ex = x1 + ux * end_pos;
                float ey = y1 + uy * end_pos;

                nvgBeginPath(vg);
                nvgMoveTo(vg, sx, sy);
                nvgLineTo(vg, ex, ey);
                nvgStroke(vg);
            }

            pos = end_pos;
            draw = !draw;
        }
    }
    else if (style == "dotted") {
        // Dotted line (circles)
        nvgStrokeColor(vg, color);
        nvgFillColor(vg, color);

        float dot_spacing = width * 2.5f;  // Space between dot centers

        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);

        if (length < 0.01f) return;  // Avoid division by zero

        float ux = dx / length;
        float uy = dy / length;

        float pos = width * 0.5f;  // Start half a dot width in

        while (pos < length - width * 0.5f) {
            float cx = x1 + ux * pos;
            float cy = y1 + uy * pos;

            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, width * 0.5f);
            nvgFill(vg);

            pos += dot_spacing;
        }
    }
    else if (style == "double") {
        // Double line (two parallel lines)
        float third_width = width / 3.0f;
        float offset = third_width;

        // Calculate perpendicular offset
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);

        if (length < 0.01f) return;  // Avoid division by zero

        float px = -dy / length * offset;  // Perpendicular
        float py = dx / length * offset;

        nvgStrokeWidth(vg, third_width);
        nvgStrokeColor(vg, color);

        // First line
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1 + px, y1 + py);
        nvgLineTo(vg, x2 + px, y2 + py);
        nvgStroke(vg);

        // Second line
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1 - px, y1 - py);
        nvgLineTo(vg, x2 - px, y2 - py);
        nvgStroke(vg);
    }
    else {
        // Unknown style - default to solid
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);
    }
}

// Sprint 34: Helper to parse per-side border color with fallback
static NVGcolor parse_border_color(const std::string& explicit_color,
                                    const std::map<std::string, std::string>& computed_style)
{
    // Use explicit per-side color if set
    if (!explicit_color.empty()) {
        return nvgcss_utils::parse_color(explicit_color);
    }

    // Fallback to general border-color from computed_style
    auto it = computed_style.find("border-color");
    if (it != computed_style.end()) {
        return nvgcss_utils::parse_color(it->second);
    }

    // Default: black
    return nvgRGBA(0, 0, 0, 255);
}

// Sprint 33: Render borders with per-side styles
// Sprint 34: Enhanced with per-side colors
static void render_styled_borders(NVGcontext* vg, const NVGCSSElement* element,
                                  const NVGCSSBox& box,
                                  const std::map<std::string, std::string>& computed_style)
{
    // Get border properties from element
    float top_width = box.border_width[0];
    float right_width = box.border_width[1];
    float bottom_width = box.border_width[2];
    float left_width = box.border_width[3];

    // Sprint 34: Parse per-side colors (fallback to general border-color)
    NVGcolor top_color = parse_border_color(element->explicit_style.border_top_color, computed_style);
    NVGcolor right_color = parse_border_color(element->explicit_style.border_right_color, computed_style);
    NVGcolor bottom_color = parse_border_color(element->explicit_style.border_bottom_color, computed_style);
    NVGcolor left_color = parse_border_color(element->explicit_style.border_left_color, computed_style);

    // Get per-side styles
    const std::string& top_style = element->explicit_style.border_top_style;
    const std::string& right_style = element->explicit_style.border_right_style;
    const std::string& bottom_style = element->explicit_style.border_bottom_style;
    const std::string& left_style = element->explicit_style.border_left_style;

    // Calculate border positions (centered on box edge)
    float half_top = top_width * 0.5f;
    float half_right = right_width * 0.5f;
    float half_bottom = bottom_width * 0.5f;
    float half_left = left_width * 0.5f;

    // Top border
    if (top_width > 0) {
        render_border_side(vg,
            box.x, box.y + half_top,
            box.x + box.width, box.y + half_top,
            top_width, top_color, top_style);
    }

    // Right border
    if (right_width > 0) {
        render_border_side(vg,
            box.x + box.width - half_right, box.y,
            box.x + box.width - half_right, box.y + box.height,
            right_width, right_color, right_style);
    }

    // Bottom border
    if (bottom_width > 0) {
        render_border_side(vg,
            box.x + box.width, box.y + box.height - half_bottom,
            box.x, box.y + box.height - half_bottom,
            bottom_width, bottom_color, bottom_style);
    }

    // Left border
    if (left_width > 0) {
        render_border_side(vg,
            box.x + half_left, box.y + box.height,
            box.x + half_left, box.y,
            left_width, left_color, left_style);
    }
}
void NVGCSSPainter::paint_element(const NVGCSSElement* element,
                                   const std::map<std::string, std::string>& computed_style) {
    if (!element->visible) return;

    // Save state
    nvgSave(vg_);

    // Apply transform
    nvgTransform(vg_,
                 element->transform[0], element->transform[1],
                 element->transform[2], element->transform[3],
                 element->transform[4], element->transform[5]);

    // Apply opacity
    nvgGlobalAlpha(vg_, element->opacity);

    // Draw based on element type
    // HTML elements (div, button, span with text) are treated as boxes
    bool is_box_element = (element->type == "rect" || element->type == "group" ||
                          element->type == "div" || element->type == "button" ||
                          element->type == "input" || element->type == "panel" ||
                          element->type == "screen" || element->type == "window" ||
                          element->type == "widget");

    if (is_box_element) {
        // Use computed layout for dimensions (PHASE 4 SPRINT 4)
        const auto& box = computed_to_box(element->computed);

        // Sprint 27: Render box-shadows first (so they're behind the element)
        paint_box_shadows(element, box);

        // Create path (rounded rectangle if border-radius specified)
        create_rounded_rect_path(box);

        // Fill background
        apply_background(computed_style, box);
        // Sprint 31: Render background image (on top of color/gradient)
        render_background_image(element, computed_style, box, renderer_);

        // Stroke border
        // apply_border(computed_style, box);  // Replaced by render_styled_borders (Sprint 33)
        // Sprint 33: Render borders with per-side styles
        render_styled_borders(vg_, element, box, computed_style);
        // Render text content if present (for buttons, labels, etc.)
        if (!element->text_content.empty()) {
            // Get text color
            auto color_it = computed_style.find("color");
            if (color_it == computed_style.end()) {
                color_it = computed_style.find("fill");  // SVG-style
            }

            NVGcolor text_color = (color_it != computed_style.end()) ?
                nvgcss_utils::parse_color(color_it->second) : nvgRGB(0, 0, 0);

            // Font size (with em, rem, %, keyword support)
            float parent_size = 16.0f;  // Default parent size
            float font_size = parse_font_size(computed_style, parent_size);

            // Font face (with font-weight and font-style)
            std::string font_face = compute_font_face(computed_style);

            // Text alignment (horizontal and vertical)
            int align = compute_text_align(computed_style);

            // Apply text transform
            std::string text_content = element->text_content;
            auto transform_it = computed_style.find("text-transform");
            if (transform_it != computed_style.end()) {
                text_content = apply_text_transform(text_content, transform_it->second);
            }

            // Apply typography settings to NanoVG
            nvgFontSize(vg_, font_size);
            nvgFontFace(vg_, font_face.c_str());
            nvgTextAlign(vg_, align);
            nvgFillColor(vg_, text_color);

            // Calculate text position based on alignment
            float text_x = box.x;
            float text_y = box.y;

            if (align & NVG_ALIGN_CENTER) {
                text_x += box.width / 2.0f;
            } else if (align & NVG_ALIGN_RIGHT) {
                text_x += box.width;
            }

            if (align & NVG_ALIGN_MIDDLE) {
                text_y += box.height / 2.0f;
            } else if (align & NVG_ALIGN_BOTTOM) {
                text_y += box.height;
            }

            // Sprint 28: Render text shadows FIRST (so they appear behind text)
            if (!element->text_shadows.empty()) {
                // Render each shadow in reverse order (last shadow first, so first shadow is on top)
                for (auto it = element->text_shadows.rbegin(); it != element->text_shadows.rend(); ++it) {
                    const TextShadow& shadow = *it;

                    // Calculate shadow position
                    float shadow_x = text_x + shadow.offset_x;
                    float shadow_y = text_y + shadow.offset_y;

                    // If blur is specified, render multiple times with decreasing alpha
                    if (shadow.blur_radius > 0) {
                        // Simple blur approximation: render text multiple times with decreasing alpha
                        int blur_samples = std::min(5, (int)shadow.blur_radius);
                        for (int i = 0; i < blur_samples; i++) {
                            float offset = (i - blur_samples / 2.0f) * (shadow.blur_radius / blur_samples);
                            float alpha_multiplier = 1.0f - (std::abs(i - blur_samples / 2.0f) / (blur_samples / 2.0f));

                            NVGcolor blurred_color = shadow.color;
                            blurred_color.a *= alpha_multiplier * 0.3f;  // Reduce alpha for blur

                            nvgFillColor(vg_, blurred_color);
                            nvgText(vg_, shadow_x + offset, shadow_y + offset, text_content.c_str(), nullptr);
                        }
                    } else {
                        // No blur - just render once
                        nvgFillColor(vg_, shadow.color);
                        nvgText(vg_, shadow_x, shadow_y, text_content.c_str(), nullptr);
                    }
                }
            }

            // Render actual text on top
            nvgFillColor(vg_, text_color);
            nvgText(vg_, text_x, text_y, text_content.c_str(), nullptr);

            // Apply text decoration (underline, line-through, overline)
            apply_text_decoration(element, computed_style, text_x, text_y, text_color);
        }
    }
    else if (element->type == "text" || element->type == "span") {
        // Phase 3: Text rendering with Sprint 11 typography support
        const auto& box = computed_to_box(element->computed);

        // Get text color
        auto color_it = computed_style.find("color");
        if (color_it == computed_style.end()) {
            color_it = computed_style.find("fill");  // SVG-style
        }

        NVGcolor text_color = (color_it != computed_style.end()) ?
            nvgcss_utils::parse_color(color_it->second) : nvgRGB(0, 0, 0);

        // Font size (with em, rem, %, keyword support)
        float parent_size = 16.0f;  // Default parent size
        float font_size = parse_font_size(computed_style, parent_size);

        // Font face (with font-weight and font-style)
        std::string font_face = compute_font_face(computed_style);

        // Text alignment (horizontal and vertical)
        int align = compute_text_align(computed_style);

        // Apply text transform
        std::string text_content = element->text_content;
        auto transform_it = computed_style.find("text-transform");
        if (transform_it != computed_style.end()) {
            text_content = apply_text_transform(text_content, transform_it->second);
        }

        // Apply typography settings to NanoVG
        nvgFontSize(vg_, font_size);
        nvgFontFace(vg_, font_face.c_str());
        nvgTextAlign(vg_, align);
        nvgFillColor(vg_, text_color);

        // Calculate text position based on alignment
        float text_x = box.x;
        float text_y = box.y;

        if (align & NVG_ALIGN_CENTER) {
            text_x += box.width / 2.0f;
        } else if (align & NVG_ALIGN_RIGHT) {
            text_x += box.width;
        }

        if (align & NVG_ALIGN_MIDDLE) {
            text_y += box.height / 2.0f;
        } else if (align & NVG_ALIGN_BOTTOM) {
            text_y += box.height;
        }

        // Sprint 28: Render text shadows FIRST (so they appear behind text)
        if (!element->text_shadows.empty()) {
            // Render each shadow in reverse order (last shadow first, so first shadow is on top)
            for (auto it = element->text_shadows.rbegin(); it != element->text_shadows.rend(); ++it) {
                const TextShadow& shadow = *it;

                // Calculate shadow position
                float shadow_x = text_x + shadow.offset_x;
                float shadow_y = text_y + shadow.offset_y;

                // If blur is specified, render multiple times with decreasing alpha
                if (shadow.blur_radius > 0) {
                    // Simple blur approximation: render text multiple times with decreasing alpha
                    int blur_samples = std::min(5, (int)shadow.blur_radius);
                    for (int i = 0; i < blur_samples; i++) {
                        float offset = (i - blur_samples / 2.0f) * (shadow.blur_radius / blur_samples);
                        float alpha_multiplier = 1.0f - (std::abs(i - blur_samples / 2.0f) / (blur_samples / 2.0f));

                        NVGcolor blurred_color = shadow.color;
                        blurred_color.a *= alpha_multiplier * 0.3f;  // Reduce alpha for blur

                        nvgFillColor(vg_, blurred_color);
                        nvgText(vg_, shadow_x + offset, shadow_y + offset, text_content.c_str(), nullptr);
                    }
                } else {
                    // No blur - just render once
                    nvgFillColor(vg_, shadow.color);
                    nvgText(vg_, shadow_x, shadow_y, text_content.c_str(), nullptr);
                }
            }
        }

        // Render actual text on top
        nvgFillColor(vg_, text_color);
        nvgText(vg_, text_x, text_y, text_content.c_str(), nullptr);

        // Apply text decoration (underline, line-through, overline)
        apply_text_decoration(element, computed_style, text_x, text_y, text_color);
    }

    // Call custom paint callback if provided
    if (element->custom_paint) {
        element->custom_paint(vg_, element, computed_style);
    }

    // Restore state
    nvgRestore(vg_);
}

void NVGCSSPainter::apply_background(const std::map<std::string, std::string>& style,
                                      const NVGCSSBox& box) {
    auto it = style.find("background");
    if (it == style.end()) {
        it = style.find("background-color");
    }

    if (it != style.end()) {
        const std::string& value = it->second;

        // Check if it's a gradient (linear or radial)
        if (value.find("linear-gradient") != std::string::npos ||
            value.find("radial-gradient") != std::string::npos) {
            NVGpaint paint = create_gradient(value, box);
            nvgFillPaint(vg_, paint);
        } else {
            // Solid color
            NVGcolor color = nvgcss_utils::parse_color(value);
            nvgFillColor(vg_, color);
        }
        nvgFill(vg_);
    }
}

void NVGCSSPainter::apply_border(const std::map<std::string, std::string>& style,
                                  const NVGCSSBox& box) {
    // Check for border properties
    auto color_it = style.find("border-color");
    auto width_it = style.find("border-width");
    auto style_it = style.find("border-style");

    // Also check shorthand
    if (color_it == style.end()) color_it = style.find("border");
    if (width_it == style.end() && style.count("border")) {
        // Parse width from shorthand (e.g., "2px solid red")
        // For simplicity, assume format: width style color
    }

    // Default values
    float border_width = 1.0f;
    NVGcolor border_color = nvgRGBA(0, 0, 0, 255);
    std::string border_style = "solid";

    if (width_it != style.end()) {
        border_width = nvgcss_utils::parse_length(width_it->second, box.width);
    }

    if (color_it != style.end()) {
        border_color = nvgcss_utils::parse_color(color_it->second);
    }

    if (style_it != style.end()) {
        border_style = style_it->second;
    }

    // Only draw if border width > 0
    if (border_width > 0 && border_style != "none") {
        // Recreate path for stroke
        create_rounded_rect_path(box);

        nvgStrokeWidth(vg_, border_width);
        nvgStrokeColor(vg_, border_color);

        // Handle dashed/dotted styles
        if (border_style == "dashed") {
            // NanoVG doesn't have native dashed lines, but we can approximate
            // For now, just use solid
        } else if (border_style == "dotted") {
            // Same - use solid for now
        }

        nvgStroke(vg_);
    }
}

// Sprint 27: Render box-shadows using parsed BoxShadow data structure
// Sprint 39: Added inset shadow support
void NVGCSSPainter::paint_box_shadows(const NVGCSSElement* element,
                                       const NVGCSSBox& box) {
    if (element->box_shadows.empty()) return;

    // Get border radius for rounded shadows
    float border_radius = (box.border_radius[0] + box.border_radius[1] +
                           box.border_radius[2] + box.border_radius[3]) / 4.0f;

    // Render each shadow in reverse order (last shadow first, so first shadow is on top)
    for (auto it = element->box_shadows.rbegin(); it != element->box_shadows.rend(); ++it) {
        const BoxShadow& shadow = *it;

        // Sprint 39: Handle inset shadows
        if (shadow.inset) {
            // Inset shadows appear inside the element bounds
            nvgSave(vg_);

            // Clip to element bounds
            nvgScissor(vg_, box.x, box.y, box.width, box.height);

            // Create inset shadow paint
            // For inset shadows, we invert the offsets and draw from inside
            NVGpaint shadow_paint = nvgBoxGradient(vg_,
                box.x + shadow.offset_x,
                box.y + shadow.offset_y,
                box.width - shadow.spread_radius * 2,
                box.height - shadow.spread_radius * 2,
                border_radius,
                shadow.blur_radius,
                nvgRGBA(0, 0, 0, 0),      // Inner color (transparent)
                shadow.color               // Outer color (shadow color)
            );

            // Draw shadow rectangle covering the entire element
            nvgBeginPath(vg_);
            float shadow_margin = shadow.blur_radius + shadow.spread_radius + 5.0f;

            // Outer rectangle (larger than element)
            nvgRect(vg_,
                box.x - shadow_margin,
                box.y - shadow_margin,
                box.width + shadow_margin * 2,
                box.height + shadow_margin * 2);

            // Inner rectangle (the actual shadow area, shrunk by spread)
            if (border_radius > 0) {
                nvgRoundedRect(vg_,
                    box.x + shadow.offset_x - shadow.spread_radius,
                    box.y + shadow.offset_y - shadow.spread_radius,
                    box.width + shadow.spread_radius * 2,
                    box.height + shadow.spread_radius * 2,
                    border_radius);
            } else {
                nvgRect(vg_,
                    box.x + shadow.offset_x - shadow.spread_radius,
                    box.y + shadow.offset_y - shadow.spread_radius,
                    box.width + shadow.spread_radius * 2,
                    box.height + shadow.spread_radius * 2);
            }
            nvgPathWinding(vg_, NVG_HOLE);

            nvgFillPaint(vg_, shadow_paint);
            nvgFill(vg_);

            nvgRestore(vg_);
        } else {
            // Outset shadows (existing implementation)
            nvgSave(vg_);

            // Create shadow paint with blur
            // nvgBoxGradient creates a soft shadow effect with feathered edges
            NVGpaint shadow_paint = nvgBoxGradient(vg_,
                box.x + shadow.offset_x,
                box.y + shadow.offset_y,
                box.width + shadow.spread_radius * 2,
                box.height + shadow.spread_radius * 2,
                border_radius + shadow.spread_radius,  // Corner radius
                shadow.blur_radius,                     // Blur feather
                shadow.color,                           // Inner color
                nvgRGBA(0, 0, 0, 0)                     // Outer color (transparent)
            );

            // Draw shadow rectangle (expanded by blur to accommodate feathering)
            nvgBeginPath(vg_);
            float shadow_margin = shadow.blur_radius + 5.0f;
            nvgRect(vg_,
                box.x + shadow.offset_x - shadow_margin,
                box.y + shadow.offset_y - shadow_margin,
                box.width + shadow.spread_radius * 2 + shadow_margin * 2,
                box.height + shadow.spread_radius * 2 + shadow_margin * 2);

            // Cut out the center (where the actual element will be)
            // This creates a "frame" effect so the shadow only shows outside the element
            if (border_radius > 0) {
                nvgRoundedRect(vg_, box.x, box.y, box.width, box.height, border_radius);
            } else {
                nvgRect(vg_, box.x, box.y, box.width, box.height);
            }
            nvgPathWinding(vg_, NVG_HOLE);

            nvgFillPaint(vg_, shadow_paint);
            nvgFill(vg_);

            nvgRestore(vg_);
        }
    }
}

// DEPRECATED: Old apply_shadow that parsed on every render
void NVGCSSPainter::apply_shadow(const std::map<std::string, std::string>& style,
                                  const NVGCSSBox& box) {
    // Sprint 27: This function is deprecated - use paint_box_shadows() instead
    // Kept for backward compatibility only
    // The new implementation uses element->box_shadows which is parsed during layout
}

void NVGCSSPainter::apply_opacity(const std::map<std::string, std::string>& style) {
    // Already handled in paint_element
}

NVGpaint NVGCSSPainter::create_gradient(const std::string& gradient_css,
                                        const NVGCSSBox& box) {
    // Phase 3: Parse gradient and dispatch to appropriate handler
    GradientData gradient = parse_gradient(gradient_css);

    if (gradient.type == GradientData::LINEAR) {
        return create_linear_gradient(gradient, box);
    } else {
        return create_radial_gradient(gradient, box);
    }
}

GradientData NVGCSSPainter::parse_gradient(const std::string& gradient_css) {
    GradientData result;

    // Determine gradient type
    if (gradient_css.find("radial-gradient") != std::string::npos) {
        result.type = GradientData::RADIAL;
    } else {
        result.type = GradientData::LINEAR;
    }

    std::string gradient = gradient_css;

    // Remove function name and parentheses
    size_t start_pos = gradient.find('(');
    size_t end_pos = gradient.rfind(')');
    if (start_pos != std::string::npos && end_pos != std::string::npos) {
        gradient = gradient.substr(start_pos + 1, end_pos - start_pos - 1);
    }

    // Split by commas (but be careful with rgba commas)
    std::vector<std::string> parts;
    std::string current;
    int paren_depth = 0;

    for (char c : gradient) {
        if (c == '(') paren_depth++;
        else if (c == ')') paren_depth--;
        else if (c == ',' && paren_depth == 0) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    if (!current.empty()) parts.push_back(current);

    // Trim all parts
    for (auto& part : parts) {
        part.erase(0, part.find_first_not_of(" \t"));
        part.erase(part.find_last_not_of(" \t") + 1);
    }

    if (parts.empty()) return result;

    size_t color_start_idx = 0;

    // Parse direction/position (first part might not be a color)
    if (!parts.empty()) {
        const std::string& first = parts[0];

        if (result.type == GradientData::LINEAR) {
            // Linear gradient direction
            if (first.find("to ") == 0) {
                // Named direction
                if (first == "to bottom") result.angle = 180.0f;
                else if (first == "to top") result.angle = 0.0f;
                else if (first == "to right") result.angle = 90.0f;
                else if (first == "to left") result.angle = 270.0f;
                else if (first == "to bottom right") result.angle = 135.0f;
                else if (first == "to bottom left") result.angle = 225.0f;
                else if (first == "to top right") result.angle = 45.0f;
                else if (first == "to top left") result.angle = 315.0f;
                color_start_idx = 1;
            } else if (first.find("deg") != std::string::npos) {
                // Angle in degrees
                result.angle = safe_stof(first);
                color_start_idx = 1;
            }
        } else {
            // Radial gradient shape/position
            if (first.find("circle") != std::string::npos) {
                result.shape = "circle";
                // Check for position
                if (first.find(" at ") != std::string::npos) {
                    size_t at_pos = first.find(" at ");
                    result.position = first.substr(at_pos + 4);
                }
                color_start_idx = 1;
            } else if (first.find("ellipse") != std::string::npos) {
                result.shape = "ellipse";
                if (first.find(" at ") != std::string::npos) {
                    size_t at_pos = first.find(" at ");
                    result.position = first.substr(at_pos + 4);
                }
                color_start_idx = 1;
            } else if (first.find("at ") != std::string::npos) {
                result.position = first.substr(3);  // Remove "at "
                color_start_idx = 1;
            }
        }
    }

    // Parse color stops
    for (size_t i = color_start_idx; i < parts.size(); ++i) {
        std::string stop_str = parts[i];

        GradientStop stop;
        stop.position = static_cast<float>(i - color_start_idx) /
                       static_cast<float>(parts.size() - color_start_idx - 1);

        // Check if stop has explicit position (e.g., "red 50%")
        size_t space_pos = stop_str.find_last_of(' ');
        if (space_pos != std::string::npos) {
            std::string pos_str = stop_str.substr(space_pos + 1);
            if (pos_str.find('%') != std::string::npos) {
                // Percentage position
                stop.position = safe_stof(pos_str) / 100.0f;
                stop_str = stop_str.substr(0, space_pos);
                stop_str.erase(stop_str.find_last_not_of(" \t") + 1);
            } else if (pos_str.find("px") != std::string::npos) {
                // Pixel position - convert to percentage (needs box size, skip for now)
                stop_str = stop_str.substr(0, space_pos);
                stop_str.erase(stop_str.find_last_not_of(" \t") + 1);
            }
        }

        stop.color = nvgcss_utils::parse_color(stop_str);
        result.stops.push_back(stop);
    }

    // Ensure we have at least 2 stops
    if (result.stops.size() < 2) {
        if (result.stops.empty()) {
            result.stops.push_back({0.0f, nvgRGB(0, 0, 0)});
        }
        result.stops.push_back({1.0f, nvgRGB(255, 255, 255)});
    }

    return result;
}

NVGpaint NVGCSSPainter::create_linear_gradient(const GradientData& gradient,
                                                const NVGCSSBox& box) {
    // Convert angle to start/end points
    float rad = (gradient.angle - 90.0f) * 3.14159f / 180.0f;
    float cx = box.x + box.width / 2.0f;
    float cy = box.y + box.height / 2.0f;
    float len = std::max(box.width, box.height);

    float sx = cx - std::cos(rad) * len / 2.0f;
    float sy = cy - std::sin(rad) * len / 2.0f;
    float ex = cx + std::cos(rad) * len / 2.0f;
    float ey = cy + std::sin(rad) * len / 2.0f;

    // NanoVG only supports 2-color gradients
    // Use first and last stop
    NVGcolor start_color = gradient.stops.front().color;
    NVGcolor end_color = gradient.stops.back().color;

    return nvgLinearGradient(vg_, sx, sy, ex, ey, start_color, end_color);
}

NVGpaint NVGCSSPainter::create_radial_gradient(const GradientData& gradient,
                                                const NVGCSSBox& box) {
    // Parse position
    float cx = box.x + box.width / 2.0f;   // Default: center
    float cy = box.y + box.height / 2.0f;

    // Parse position string (e.g., "center center", "top left", "50% 50%")
    if (!gradient.position.empty()) {
        std::istringstream pos_stream(gradient.position);
        std::string x_pos, y_pos;
        pos_stream >> x_pos >> y_pos;

        // Parse X position
        if (x_pos == "left") cx = box.x;
        else if (x_pos == "center") cx = box.x + box.width / 2.0f;
        else if (x_pos == "right") cx = box.x + box.width;
        else if (x_pos.find('%') != std::string::npos) {
            cx = box.x + (safe_stof(x_pos) / 100.0f) * box.width;
        } else if (x_pos.find("px") != std::string::npos) {
            cx = box.x + safe_stof(x_pos);
        }

        // Parse Y position
        if (y_pos == "top") cy = box.y;
        else if (y_pos == "center") cy = box.y + box.height / 2.0f;
        else if (y_pos == "bottom") cy = box.y + box.height;
        else if (y_pos.find('%') != std::string::npos) {
            cy = box.y + (safe_stof(y_pos) / 100.0f) * box.height;
        } else if (y_pos.find("px") != std::string::npos) {
            cy = box.y + safe_stof(y_pos);
        }
    }

    // Compute radius
    float radius;
    if (gradient.shape == "circle") {
        radius = std::min(box.width, box.height) / 2.0f;
    } else {
        // Ellipse - use average radius
        radius = (box.width + box.height) / 4.0f;
    }

    // NanoVG only supports 2-color radial gradients
    NVGcolor inner_color = gradient.stops.front().color;
    NVGcolor outer_color = gradient.stops.back().color;

    return nvgRadialGradient(vg_, cx, cy, 0, radius, inner_color, outer_color);
}

void NVGCSSPainter::create_rounded_rect_path(const NVGCSSBox& box) {
    nvgBeginPath(vg_);

    // Check if any border radius is specified
    bool has_radius = (box.border_radius[0] > 0 || box.border_radius[1] > 0 ||
                       box.border_radius[2] > 0 || box.border_radius[3] > 0);

    if (has_radius) {
        // Sprint 36: Per-corner border radius
        // Order: [0]=top-left, [1]=top-right, [2]=bottom-right, [3]=bottom-left
        float tl = box.border_radius[0];  // top-left
        float tr = box.border_radius[1];  // top-right
        float br = box.border_radius[2];  // bottom-right
        float bl = box.border_radius[3];  // bottom-left

        float x = box.x;
        float y = box.y;
        float w = box.width;
        float h = box.height;

        // Create path with per-corner radii using arc commands
        // Start from top-left corner, move clockwise

        // Top-left corner
        nvgMoveTo(vg_, x + tl, y);

        // Top edge
        nvgLineTo(vg_, x + w - tr, y);

        // Top-right corner
        if (tr > 0) {
            nvgArc(vg_, x + w - tr, y + tr, tr, -NVG_PI/2, 0, NVG_CW);
        }

        // Right edge
        nvgLineTo(vg_, x + w, y + h - br);

        // Bottom-right corner
        if (br > 0) {
            nvgArc(vg_, x + w - br, y + h - br, br, 0, NVG_PI/2, NVG_CW);
        }

        // Bottom edge
        nvgLineTo(vg_, x + bl, y + h);

        // Bottom-left corner
        if (bl > 0) {
            nvgArc(vg_, x + bl, y + h - bl, bl, NVG_PI/2, NVG_PI, NVG_CW);
        }

        // Left edge
        nvgLineTo(vg_, x, y + tl);

        // Top-left corner (closing)
        if (tl > 0) {
            nvgArc(vg_, x + tl, y + tl, tl, NVG_PI, 3*NVG_PI/2, NVG_CW);
        }

        nvgClosePath(vg_);
    } else {
        nvgRect(vg_, box.x, box.y, box.width, box.height);
    }
}


// ============================================================================
// Typography Support (Sprint 11)
// ============================================================================

std::string NVGCSSPainter::compute_font_face(const std::map<std::string, std::string>& style) {
    // Get base font family
    std::string family = "sans-serif";
    auto family_it = style.find("font-family");
    if (family_it != style.end()) {
        // Parse first font from comma-separated list
        std::string font_list = family_it->second;
        size_t comma_pos = font_list.find(',');
        if (comma_pos != std::string::npos) {
            family = font_list.substr(0, comma_pos);
        } else {
            family = font_list;
        }

        // Remove quotes if present
        if (!family.empty() && (family.front() == '"' || family.front() == '\'')) {
            family = family.substr(1, family.length() - 2);
        }

        // Trim whitespace
        size_t start = family.find_first_not_of(" \t");
        size_t end = family.find_last_not_of(" \t");
        if (start != std::string::npos) {
            family = family.substr(start, end - start + 1);
        }
    }

    // Check for font-weight (append suffix for bold)
    auto weight_it = style.find("font-weight");
    if (weight_it != style.end()) {
        const std::string& weight = weight_it->second;
        if (weight == "bold" || weight == "700" || weight == "800" || weight == "900") {
            // Check for font-style first (Bold-Italic ordering)
            auto style_it = style.find("font-style");
            if (style_it != style.end() && style_it->second == "italic") {
                family += "-BoldItalic";
                return family;  // Both weight and style applied
            }
            family += "-Bold";
        }
    }

    // Check for font-style (append suffix for italic)
    auto style_it = style.find("font-style");
    if (style_it != style.end()) {
        const std::string& font_style = style_it->second;
        if (font_style == "italic") {
            // Only add Italic if not already added as BoldItalic
            if (family.find("-BoldItalic") == std::string::npos &&
                family.find("-Bold") == std::string::npos) {
                family += "-Italic";
            }
        } else if (font_style == "oblique") {
            // Treat oblique same as italic
            if (family.find("-BoldItalic") == std::string::npos &&
                family.find("-Bold") == std::string::npos) {
                family += "-Italic";
            }
        }
    }

    return family;
}

float NVGCSSPainter::parse_font_size(const std::map<std::string, std::string>& style,
                                      float parent_size) {
    auto it = style.find("font-size");
    if (it == style.end()) return parent_size;

    const std::string& value = it->second;

    // Keywords
    static const std::map<std::string, float> keywords = {
        {"xx-small", 9.0f},
        {"x-small", 10.0f},
        {"small", 13.0f},
        {"medium", 16.0f},  // Default
        {"large", 18.0f},
        {"x-large", 24.0f},
        {"xx-large", 32.0f}
    };

    if (keywords.count(value)) {
        return keywords.at(value);
    }

    // Parse length with units
    if (value.find("em") != std::string::npos) {
        float multiplier = safe_stof(value, 1.f);
        return parent_size * multiplier;
    }

    if (value.find("rem") != std::string::npos) {
        float multiplier = safe_stof(value, 1.f);
        // Root font size is typically 16px
        return 16.0f * multiplier;
    }

    if (value.find("%") != std::string::npos) {
        float percent = safe_stof(value, 100.f);
        return parent_size * (percent / 100.0f);
    }

    // Default: px
    return nvgcss_utils::parse_length(value, parent_size);
}

int NVGCSSPainter::compute_text_align(const std::map<std::string, std::string>& style) {
    int h_align = NVG_ALIGN_LEFT;
    int v_align = NVG_ALIGN_TOP;

    // Horizontal alignment
    auto align_it = style.find("text-align");
    if (align_it != style.end()) {
        const std::string& align = align_it->second;
        if (align == "center") {
            h_align = NVG_ALIGN_CENTER;
        } else if (align == "right") {
            h_align = NVG_ALIGN_RIGHT;
        } else if (align == "left") {
            h_align = NVG_ALIGN_LEFT;
        }
    }

    // Vertical alignment
    auto v_align_it = style.find("vertical-align");
    if (v_align_it != style.end()) {
        const std::string& valign = v_align_it->second;
        if (valign == "middle") {
            v_align = NVG_ALIGN_MIDDLE;
        } else if (valign == "bottom") {
            v_align = NVG_ALIGN_BOTTOM;
        } else if (valign == "baseline") {
            v_align = NVG_ALIGN_BASELINE;
        } else if (valign == "top") {
            v_align = NVG_ALIGN_TOP;
        }
    }

    return h_align | v_align;
}

std::string NVGCSSPainter::apply_text_transform(const std::string& text,
                                                 const std::string& transform) {
    if (transform == "uppercase") {
        std::string result = text;
        for (char& c : result) {
            c = std::toupper(c);
        }
        return result;
    } else if (transform == "lowercase") {
        std::string result = text;
        for (char& c : result) {
            c = std::tolower(c);
        }
        return result;
    } else if (transform == "capitalize") {
        std::string result = text;
        bool capitalize_next = true;
        for (char& c : result) {
            if (std::isspace(c)) {
                capitalize_next = true;
            } else if (capitalize_next) {
                c = std::toupper(c);
                capitalize_next = false;
            } else {
                c = std::tolower(c);
            }
        }
        return result;
    }

    return text;  // "none" or unrecognized
}

void NVGCSSPainter::apply_text_decoration(const NVGCSSElement* element,
                                           const std::map<std::string, std::string>& style,
                                           float text_x, float text_y,
                                           NVGcolor text_color) {
    auto it = style.find("text-decoration");
    if (it == style.end() || it->second == "none") return;

    const std::string& decoration = it->second;

    // Get text bounds
    float bounds[4];
    nvgTextBounds(vg_, text_x, text_y, element->text_content.c_str(), nullptr, bounds);

    float line_y;
    if (decoration == "underline") {
        line_y = bounds[3] + 2;  // Below text
    } else if (decoration == "line-through") {
        line_y = (bounds[1] + bounds[3]) / 2.0f;  // Middle
    } else if (decoration == "overline") {
        line_y = bounds[1] - 2;  // Above text
    } else {
        return;  // Unrecognized decoration
    }

    // Draw decoration line
    nvgBeginPath(vg_);
    nvgMoveTo(vg_, bounds[0], line_y);
    nvgLineTo(vg_, bounds[2], line_y);
    nvgStrokeColor(vg_, text_color);
    nvgStrokeWidth(vg_, 1.0f);
    nvgStroke(vg_);
}

// ============================================================================
// Background Image Support (Sprint 31)
// ============================================================================

/**
 * @brief Load background image with caching
 *
 * Loads an image from file and caches it in the renderer's image cache.
 * Subsequent calls with the same path will return the cached image handle.
 *
 * @param path Image file path
 * @param renderer Renderer with image cache
 * @return NanoVG image handle, or -1 if loading failed
 */
static int load_background_image(const std::string& path, NVGCSSRenderer* renderer) {
    // Check cache first
    auto it = renderer->image_cache.find(path);
    if (it != renderer->image_cache.end()) {
        return it->second;  // Return cached image
    }

    // Load new image
    int image = nvgCreateImage(renderer->vg, path.c_str(), 0);

    if (image > 0) {
        // Cache for future use
        renderer->image_cache[path] = image;
    }

    return image;
}

/**
 * @brief Render background image
 *
 * Renders a background image with support for size, position, and repeat modes.
 * Called from apply_background() when background-image is present.
 *
 * @param element Element to render background for
 * @param style Computed style map
 * @param box Element's box dimensions
 * @param renderer Renderer (for image cache access)
 */
static void render_background_image(const NVGCSSElement* element,
                                    const std::map<std::string, std::string>& style,
                                    const NVGCSSBox& box,
                                    NVGCSSRenderer* renderer) {
    // Get background-image property
    auto image_it = style.find("background-image");
    if (image_it == style.end() || image_it->second.empty() || image_it->second == "none") {
        return;  // No background image
    }

    // Parse image URL
    std::string image_path = nvgcss_utils::parse_background_image(image_it->second);
    if (image_path.empty()) {
        return;  // Invalid URL
    }

    // Load image (with caching)
    int image = load_background_image(image_path, renderer);
    if (image < 0) {
        printf("Failed to load background image: %s\n", image_path.c_str());
        return;
    }

    // Get image dimensions
    int img_width, img_height;
    nvgImageSize(renderer->vg, image, &img_width, &img_height);

    // Parse background-size
    BackgroundSize size_mode;
    float size_width, size_height;
    auto size_it = style.find("background-size");
    std::string size_str = (size_it != style.end()) ? size_it->second : "auto";
    nvgcss_utils::parse_background_size(size_str, size_mode, size_width, size_height);

    // Calculate final image dimensions based on size mode
    float final_width = img_width;
    float final_height = img_height;

    switch (size_mode) {
        case BG_SIZE_AUTO:
            // Use intrinsic image size
            final_width = img_width;
            final_height = img_height;
            break;

        case BG_SIZE_COVER: {
            // Scale to cover entire box, maintaining aspect ratio
            float scale_x = box.width / img_width;
            float scale_y = box.height / img_height;
            float scale = std::max(scale_x, scale_y);
            final_width = img_width * scale;
            final_height = img_height * scale;
            break;
        }

        case BG_SIZE_CONTAIN: {
            // Scale to fit inside box, maintaining aspect ratio
            float scale_x = box.width / img_width;
            float scale_y = box.height / img_height;
            float scale = std::min(scale_x, scale_y);
            final_width = img_width * scale;
            final_height = img_height * scale;
            break;
        }

        case BG_SIZE_EXPLICIT:
            // Use explicit dimensions
            final_width = size_width;
            final_height = size_height;
            break;
    }

    // Parse background-position
    auto pos_it = style.find("background-position");
    std::string pos_str = (pos_it != style.end()) ? pos_it->second : "0% 0%";
    BackgroundPosition position = nvgcss_utils::parse_background_position(pos_str);

    // Calculate position offset
    float offset_x, offset_y;

    if (position.x_is_percent) {
        offset_x = box.x + (box.width - final_width) * position.x;
    } else {
        offset_x = box.x + position.x;
    }

    if (position.y_is_percent) {
        offset_y = box.y + (box.height - final_height) * position.y;
    } else {
        offset_y = box.y + position.y;
    }

    // Parse background-repeat
    auto repeat_it = style.find("background-repeat");
    std::string repeat_str = (repeat_it != style.end()) ? repeat_it->second : "repeat";
    BackgroundRepeat repeat = nvgcss_utils::parse_background_repeat(repeat_str);

    // Render based on repeat mode
    NVGcontext* vg = renderer->vg;

    // Save context state
    nvgSave(vg);

    // Create scissor to clip to box bounds
    nvgScissor(vg, box.x, box.y, box.width, box.height);

    switch (repeat) {
        case BG_NO_REPEAT: {
            // Single image at position
            NVGpaint img_paint = nvgImagePattern(vg,
                offset_x, offset_y,
                final_width, final_height,
                0.0f, image, 1.0f);

            nvgBeginPath(vg);
            nvgRect(vg, offset_x, offset_y, final_width, final_height);
            nvgFillPaint(vg, img_paint);
            nvgFill(vg);
            break;
        }

        case BG_REPEAT: {
            // Tile in both directions
            // Calculate start positions (align to pattern)
            float start_x = box.x + fmod(offset_x - box.x, final_width);
            float start_y = box.y + fmod(offset_y - box.y, final_height);

            if (start_x > box.x) start_x -= final_width;
            if (start_y > box.y) start_y -= final_height;

            for (float y = start_y; y < box.y + box.height; y += final_height) {
                for (float x = start_x; x < box.x + box.width; x += final_width) {
                    NVGpaint img_paint = nvgImagePattern(vg,
                        x, y, final_width, final_height,
                        0.0f, image, 1.0f);

                    nvgBeginPath(vg);
                    nvgRect(vg, x, y, final_width, final_height);
                    nvgFillPaint(vg, img_paint);
                    nvgFill(vg);
                }
            }
            break;
        }

        case BG_REPEAT_X: {
            // Tile horizontally only
            float start_x = box.x + fmod(offset_x - box.x, final_width);
            if (start_x > box.x) start_x -= final_width;

            for (float x = start_x; x < box.x + box.width; x += final_width) {
                NVGpaint img_paint = nvgImagePattern(vg,
                    x, offset_y, final_width, final_height,
                    0.0f, image, 1.0f);

                nvgBeginPath(vg);
                nvgRect(vg, x, offset_y, final_width, final_height);
                nvgFillPaint(vg, img_paint);
                nvgFill(vg);
            }
            break;
        }

        case BG_REPEAT_Y: {
            // Tile vertically only
            float start_y = box.y + fmod(offset_y - box.y, final_height);
            if (start_y > box.y) start_y -= final_height;

            for (float y = start_y; y < box.y + box.height; y += final_height) {
                NVGpaint img_paint = nvgImagePattern(vg,
                    offset_x, y, final_width, final_height,
                    0.0f, image, 1.0f);

                nvgBeginPath(vg);
                nvgRect(vg, offset_x, y, final_width, final_height);
                nvgFillPaint(vg, img_paint);
                nvgFill(vg);
            }
            break;
        }
    }

    // Restore context state (remove scissor)
    nvgRestore(vg);
}
