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
    class Paint;
    class Shape;
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
    // State management
    virtual void save() = 0;
    virtual void restore() = 0;

    // Direct transform using Eigen matrix
    virtual void set_transform(const Transform& transform) = 0;
    virtual void reset() = 0;

    // Transform shortcuts
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

    // Optimized primitive drawing (avoids string parsing)
    virtual void draw_rect(float x, float y, float w, float h, float r, const Paint& fill, const Paint& stroke, float stroke_width) = 0;
    virtual void draw_circle(float cx, float cy, float r, const Paint& fill, const Paint& stroke, float stroke_width) = 0;
    virtual void draw_ellipse(float cx, float cy, float rx, float ry, const Paint& fill, const Paint& stroke, float stroke_width) = 0;

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

    // -------------------------------------------
    // Retained Mode API (for 120 FPS optimization)
    // -------------------------------------------

    // Check if renderer supports retained mode
    virtual bool supports_retained_mode() const { return false; }

    // Begin retained mode rendering (no canvas->remove())
    virtual void begin_retained_frame(float width, float height, float pixel_ratio) {
        begin_frame(width, height, pixel_ratio);
    }

    // Remove a specific cached paint from canvas
    virtual void remove_cached(tvg::Paint* paint) { (void)paint; }

    // Push a new shape to canvas and return the raw pointer
    virtual tvg::Shape* push_rect(float x, float y, float w, float h, float r,
                                  const Paint& fill, const Paint& stroke, float stroke_width,
                                  const Transform& transform, float alpha) { return nullptr; }

    virtual tvg::Shape* push_circle(float cx, float cy, float r,
                                    const Paint& fill, const Paint& stroke, float stroke_width,
                                    const Transform& transform, float alpha) { return nullptr; }

    virtual tvg::Shape* push_ellipse(float cx, float cy, float rx, float ry,
                                     const Paint& fill, const Paint& stroke, float stroke_width,
                                     const Transform& transform, float alpha) { return nullptr; }

    // Polygon: sides and radius
    virtual tvg::Shape* push_polygon(int sides, float radius,
                                     const Paint& fill, const Paint& stroke, float stroke_width,
                                     const Transform& transform, float alpha) { return nullptr; }

    // Star: points, outer/inner radius
    virtual tvg::Shape* push_star(int points, float outer_radius, float inner_radius,
                                  const Paint& fill, const Paint& stroke, float stroke_width,
                                  const Transform& transform, float alpha) { return nullptr; }

    // Path: pre-parsed SVG path string
    virtual tvg::Shape* push_path(const std::string& d,
                                  const Paint& fill, const Paint& stroke, float stroke_width,
                                  const Transform& transform, float alpha) { return nullptr; }

    // Update transform on existing cached shape
    virtual void update_transform(tvg::Paint* paint, const Transform& transform) { (void)paint; (void)transform; }
};

// ============================================================================
// Factory Functions
// ============================================================================

// Create a ThorVG-based renderer
std::unique_ptr<Renderer> create_thorvg_renderer(tvg::Canvas* canvas);

} // namespace flex
