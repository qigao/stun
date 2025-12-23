/*
 * Flex Engine - Renderer Interface
 *
 * Simplified renderer interface.
 * Uses unified Paint type for all fill/stroke operations.
 * All geometry is rendered via SVG path strings.
 */

#pragma once

#include "flex/types.h"
#include <string>
#include <memory>

// Forward declare ThorVG types
namespace tvg {
    class Canvas;
}

namespace flex {

// ============================================================================
// Renderer - Abstract rendering interface
// ============================================================================

class Renderer {
public:
    virtual ~Renderer() = default;

    // -------------------------------------------
    // Frame Control
    // -------------------------------------------

    virtual void begin_frame(float width, float height, float pixel_ratio) = 0;
    virtual void end_frame() = 0;

    // -------------------------------------------
    // Viewport Query (Phase 3.2: For culling optimization)
    // -------------------------------------------

    virtual Bounds viewport() const = 0;

    // -------------------------------------------
    // Transform Stack
    // -------------------------------------------

    virtual void save() = 0;
    virtual void restore() = 0;
    virtual void reset() = 0;

    virtual void translate(float x, float y) = 0;
    virtual void rotate(float degrees) = 0;
    virtual void scale(float sx, float sy) = 0;

    // -------------------------------------------
    // Clipping
    // -------------------------------------------

    virtual void clip_rect(float x, float y, float w, float h) = 0;
    virtual void reset_clip() = 0;

    // -------------------------------------------
    // Opacity
    // -------------------------------------------

    virtual void set_global_alpha(float alpha) = 0;

    // -------------------------------------------
    // Effects (Shadow and Blur)
    // -------------------------------------------

    // Set drop shadow for subsequent drawing operations
    virtual void set_shadow(const Shadow& shadow) = 0;
    virtual void clear_shadow() = 0;

    // Set blur filter for subsequent drawing operations
    virtual void set_blur(const BlurFilter& blur) = 0;
    virtual void clear_blur() = 0;

    // -------------------------------------------
    // Core Drawing (unified API)
    // -------------------------------------------

    // Fill path with Paint (solid color, linear gradient, or radial gradient)
    virtual void fill_path(const std::string& d, const Paint& paint) = 0;

    // Stroke path with Paint
    virtual void stroke_path(const std::string& d, const Paint& paint, float width) = 0;

    // -------------------------------------------
    // Text
    // -------------------------------------------

    virtual void draw_text(const std::string& text, float x, float y,
                          const std::string& font_family, float font_size,
                          bool bold, const Color& color) = 0;

    // -------------------------------------------
    // Image
    // -------------------------------------------

    virtual void draw_image(const std::string& src, float x, float y,
                           float width, float height) = 0;

    // -------------------------------------------
    // SVG
    // -------------------------------------------

    virtual void draw_svg(const std::string& src, float x, float y,
                         float width, float height) = 0;
    virtual void draw_svg_data(const std::string& data, float x, float y,
                              float width, float height) = 0;

    // -------------------------------------------
    // Background
    // -------------------------------------------

    virtual void clear(const Color& color) = 0;
};

// ============================================================================
// Factory Functions
// ============================================================================

// Create a ThorVG-based renderer
std::unique_ptr<Renderer> create_thorvg_renderer(tvg::Canvas* canvas);

} // namespace flex
