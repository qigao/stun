/*
 * Flex Engine - Renderer Interface (Runtime)
 *
 * Abstract renderer interface for the runtime layer.
 * Backend implementations (gCanvas, NanoVG compatibility, etc.) are in the bridge layer.
 */

#pragma once

#include "flex/core/types.h"
#include <string>
#include <vector>

namespace flex {

// Opaque handles for backend-specific objects
using CanvasHandle = void*;
using PaintHandle = void*;

struct RendererCapabilities {
    bool retained_mode = false;
    bool surface_recreation = false;
    bool path_drawing = true;
    bool raster_images = true;
    bool svg_images = true;
    bool rotation = true;
    bool scaling = true;
    bool shadow = false;
    bool blur = false;
};

// ============================================================================
// Renderer Interface
// ============================================================================

class Renderer {
public:
    virtual ~Renderer() = default;

    // Frame control
    virtual void begin_frame(float width, float height, float pixel_ratio) = 0;
    virtual void end_frame() = 0;
    virtual bool requires_surface_recreation() const { return false; }
    virtual void acknowledge_surface_recreation() {}

    // Retained mode control - call before begin_frame() to enable
    virtual void set_retained_mode(bool enabled) = 0;
    bool retained_mode() const { return retained_mode_; }

    // Transform stack
    virtual void save() = 0;
    virtual void restore() = 0;
    virtual void reset() = 0;
    virtual void set_transform(const Transform& transform) = 0;
    virtual void translate(float x, float y) = 0;
    virtual void rotate(float degrees) = 0;
    virtual void scale(float sx, float sy) = 0;

    // Clipping
    virtual void clip_rect(float x, float y, float w, float h) = 0;
    virtual void reset_clip() = 0;

    // Global opacity
    virtual void set_global_alpha(float alpha) = 0;

    // Effects
    virtual void set_shadow(const Shadow& shadow) = 0;
    virtual void clear_shadow() = 0;
    virtual void set_blur(const BlurFilter& blur) = 0;
    virtual void clear_blur() = 0;

    // Core drawing (immediate mode)
    virtual void fill_path(const std::string& d, const Paint& paint) = 0;
    virtual void stroke_path(const std::string& d, const Paint& paint, float width) = 0;
    virtual void draw_line(float x1, float y1, float x2, float y2, const Paint& paint, float width) = 0;

    // Optimized primitives (immediate mode)
    virtual void draw_rect(float x, float y, float w, float h, float r,
                          const Paint& fill, const Paint& stroke, float stroke_width) = 0;
    virtual void draw_circle(float cx, float cy, float r,
                            const Paint& fill, const Paint& stroke, float stroke_width) = 0;
    virtual void draw_ellipse(float cx, float cy, float rx, float ry,
                             const Paint& fill, const Paint& stroke, float stroke_width) = 0;

    // Text rendering
    virtual void draw_text(const std::string& text, float x, float y,
                          const std::string& font_family, float font_size,
                          bool bold, const Color& color) = 0;

    // Optional asset facade. Backends that support runtime font registration
    // override these methods; custom renderers remain source-compatible.
    virtual bool register_font(const std::string& family,
                               const std::string& path) {
        (void)family;
        (void)path;
        return false;
    }
    virtual void unregister_font(const std::string& family) {
        (void)family;
    }

    // Image rendering
    virtual void draw_image(const std::string& src, float x, float y, float width, float height) = 0;

    // SVG rendering
    virtual void draw_svg(const std::string& src, float x, float y, float width, float height) = 0;
    virtual void draw_svg_data(const std::string& data, float x, float y, float width, float height) = 0;

    // Clear
    virtual void clear(const Color& color) = 0;

    // Viewport query (for culling)
    virtual Bounds viewport() const = 0;

    // Renderer feature matrix for backend-aware host/widget decisions.
    virtual RendererCapabilities capabilities() const {
        RendererCapabilities caps;
        caps.retained_mode = supports_retained_mode();
        return caps;
    }

    // -------------------------------------------
    // Retained Mode API (optional optimization)
    // -------------------------------------------
    // Usage:
    //   renderer->set_retained_mode(true);  // Enable once at startup
    //   // ... in render loop:
    //   renderer->begin_frame(w, h, 1.0f);  // Respects retained_mode setting
    //   instance->render(*renderer);
    //   renderer->end_frame();

    virtual bool supports_retained_mode() const = 0;
    virtual void set_retained_insertion_anchor(PaintHandle before) {
        (void)before;
    }
    virtual void remove_cached(PaintHandle paint) = 0;

    // Push cached objects (returns opaque handle)
    virtual PaintHandle push_rect(float x, float y, float w, float h, float r,
                                  const Paint& fill, const Paint& stroke, float stroke_width,
                                  const Transform& transform, float alpha) = 0;
    virtual PaintHandle push_circle(float cx, float cy, float r,
                                   const Paint& fill, const Paint& stroke, float stroke_width,
                                   const Transform& transform, float alpha) = 0;
    virtual PaintHandle push_ellipse(float cx, float cy, float rx, float ry,
                                    const Paint& fill, const Paint& stroke, float stroke_width,
                                    const Transform& transform, float alpha) = 0;
    virtual PaintHandle push_polygon(int sides, float radius,
                                    const Paint& fill, const Paint& stroke, float stroke_width,
                                    const Transform& transform, float alpha) = 0;
    virtual PaintHandle push_star(int points, float outer_radius, float inner_radius,
                                  const Paint& fill, const Paint& stroke, float stroke_width,
                                  const Transform& transform, float alpha) = 0;
    virtual PaintHandle push_path(const std::string& d,
                                 const Paint& fill, const Paint& stroke, float stroke_width,
                                 const Transform& transform, float alpha) = 0;
    virtual PaintHandle push_text(const std::string& text, const std::string& font_family,
                                  float font_size, bool bold, const Color& color,
                                  const Transform& transform, float alpha) {
        (void)text;
        (void)font_family;
        (void)font_size;
        (void)bold;
        (void)color;
        (void)transform;
        (void)alpha;
        return nullptr;
    }
    virtual PaintHandle push_image(const std::string& src, float width, float height,
                                   const Transform& transform, float alpha) {
        (void)src;
        (void)width;
        (void)height;
        (void)transform;
        (void)alpha;
        return nullptr;
    }
    virtual PaintHandle push_svg(const std::string& src, float width, float height,
                                 const Transform& transform, float alpha) {
        (void)src;
        (void)width;
        (void)height;
        (void)transform;
        (void)alpha;
        return nullptr;
    }
    virtual PaintHandle push_svg_data(const std::string& data, float width, float height,
                                      const Transform& transform, float alpha) {
        (void)data;
        (void)width;
        (void)height;
        (void)transform;
        (void)alpha;
        return nullptr;
    }

    // Update cached object transform
    virtual void update_transform(PaintHandle paint, const Transform& transform) = 0;

protected:
    bool retained_mode_ = false;
};

} // namespace flex
