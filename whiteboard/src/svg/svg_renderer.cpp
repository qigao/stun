/**
 * \file svg_renderer.cpp
 * \brief Implementation of SVG rendering service using LunaSVG.
 */

#include "whiteboard/svg/svg_renderer.h"
#include "whiteboard/svg/svg_generator.h"
#include <fmtlog.h>
#include <fstream>
#include <iostream>
#include <nanogui/vector.h>

namespace whiteboard {

SVGRenderer::SVGRenderer() { logd("SVGRenderer: Initialized"); }

SVGRenderer::~SVGRenderer() { logd("SVGRenderer: Destroyed"); }

std::unique_ptr<lunasvg::Document> SVGRenderer::load_svg(const std::string &svg_data) {
  if (svg_data.empty()) {
    loge("SVGRenderer: Empty SVG data");
    return nullptr;
  }

  try {
    auto document = lunasvg::Document::loadFromData(svg_data);

    if (!document) {
      loge("SVGRenderer: Failed to load SVG document");
      return nullptr;
    }

    double w = document->width();
    double h = document->height();

    // Only log if dimensions are invalid
    if (w <= 0 || h <= 0) {
      logw("SVGRenderer: Invalid dimensions {}x{}, using defaults", w, h);
    }

    return document;

  } catch (const std::exception &e) {
    loge("SVGRenderer: Exception loading SVG: {}", e.what());
    return nullptr;
  }
}

void SVGRenderer::render(NVGcontext *ctx, lunasvg::Document *document, const nanogui::Vector2f &pos,
                         float scale_x, float scale_y, float rotation, const NVGcolor *tint_color) {
  if (!ctx || !document) {
    loge("SVGRenderer: ctx or document is null!");
    return;
  }

  try {
    // Get document dimensions
    double doc_width = document->width();
    double doc_height = document->height();

    // If document has no dimensions, use a default size
    if (doc_width <= 0 || doc_height <= 0) {
      logw("SVGRenderer: Invalid dimensions, using default 100x100");
      doc_width = 100.0;
      doc_height = 100.0;
    }

    // Calculate render dimensions
    int render_width = static_cast<int>(doc_width * scale_x);
    int render_height = static_cast<int>(doc_height * scale_y);

    if (render_width <= 0 || render_height <= 0) {
      logw("SVGRenderer: Invalid render size: {}x{}", render_width, render_height);
      return;
    }

    // Render SVG to bitmap
    auto bitmap = document->renderToBitmap(render_width, render_height);

    if (bitmap.isNull()) {
      loge("SVGRenderer: Failed to render bitmap");
      return;
    }

    // LunaSVG outputs ARGB32, but NanoVG expects RGBA
    // Manually convert byte order and apply color tint if requested
    unsigned char *data = const_cast<unsigned char *>(bitmap.data());
    int pixel_count = bitmap.width() * bitmap.height();

    for (int i = 0; i < pixel_count; i++) {
      int offset = i * 4;
      unsigned char a = data[offset + 0];
      unsigned char r = data[offset + 1];
      unsigned char g = data[offset + 2];
      unsigned char b = data[offset + 3];

      // Apply color tint if provided (blend with tint color)
      if (tint_color && a > 0) {
        float tint_strength = 0.4f; // 40% tint
        float inv_strength = 1.0f - tint_strength;
        r = static_cast<unsigned char>(r * inv_strength + tint_color->r * 255.0f * tint_strength);
        g = static_cast<unsigned char>(g * inv_strength + tint_color->g * 255.0f * tint_strength);
        b = static_cast<unsigned char>(b * inv_strength + tint_color->b * 255.0f * tint_strength);
      }

      // Convert to RGBA
      data[offset + 0] = r;
      data[offset + 1] = g;
      data[offset + 2] = b;
      data[offset + 3] = a;
    }

    // Get bitmap dimensions and data
    int bmp_width = static_cast<int>(bitmap.width());
    int bmp_height = static_cast<int>(bitmap.height());
    const unsigned char *bmp_data = bitmap.data();

    // Create NanoVG image from bitmap data (RGBA format with premultiplied alpha)
    int nvg_image =
        nvgCreateImageRGBA(ctx, bmp_width, bmp_height, NVG_IMAGE_PREMULTIPLIED, bmp_data);

    if (nvg_image == 0) {
      loge("SVGRenderer: Failed to create NanoVG image");
      return;
    }

    // Draw the SVG image with proper rotation transform
    nvgSave(ctx);

    // Calculate center of the shape for rotation
    float center_x = pos.x() + static_cast<float>(render_width) / 2.0f;
    float center_y = pos.y() + static_cast<float>(render_height) / 2.0f;

    // Apply rotation transform around center
    nvgTranslate(ctx, center_x, center_y);
    nvgRotate(ctx, rotation);
    nvgTranslate(ctx, -center_x, -center_y);

    // Draw the image (with tint already applied to bitmap if requested)
    NVGpaint img_paint = nvgImagePattern(ctx, pos.x(), pos.y(), static_cast<float>(render_width),
                                         static_cast<float>(render_height), 0.0f, nvg_image, 1.0f);
    nvgBeginPath(ctx);
    nvgRect(ctx, pos.x(), pos.y(), static_cast<float>(render_width),
            static_cast<float>(render_height));
    nvgFillPaint(ctx, img_paint);
    nvgFill(ctx);

    nvgRestore(ctx);

    // Note: We must NOT delete the image immediately after drawing
    // NanoVG needs the image data to persist until the frame is rendered
    // The image will be deleted at the end of the frame by the caller
    // For now, we'll accept this small memory leak per frame
    // TODO: Implement proper image caching
    // nvgDeleteImage(ctx, nvg_image);

  } catch (const std::exception &e) {
    loge("SVGRenderer: Exception during render: {}", e.what());
  } catch (...) {
    loge("SVGRenderer: Unknown exception during render!");
  }
}

void SVGRenderer::render_shape(NVGcontext* ctx, const SVGShapeDesc& shape,
                               const nanogui::Vector2f& pos, float scale_x, float scale_y, 
                               float rotation, const NVGcolor* tint_color) {
  if (!ctx) {
    loge("SVGRenderer: ctx is null!");
    return;
  }

  logi("SVGRenderer: render_shape() called for type '{}' at ({}, {})", 
       shape.type, pos.x(), pos.y());

  try {
    // Generate SVG from shape description
    SVGGenerator generator;
    std::string svg_data = generator.generate(shape);
    
    if (svg_data.empty()) {
      loge("SVGRenderer: Failed to generate SVG from shape description");
      return;
    }
    
    logi("SVGRenderer: Generated SVG from DDF-like description ({} bytes)", svg_data.size());
    
    // Load and render the generated SVG
    auto document = load_svg(svg_data);
    if (document) {
      render(ctx, document.get(), pos, scale_x, scale_y, rotation, tint_color);
      logi("SVGRenderer: ✓ Successfully rendered shape '{}' from DDF-like description", shape.type);
    } else {
      loge("SVGRenderer: Failed to load generated SVG");
    }
  } catch (const std::exception& e) {
    loge("SVGRenderer: Exception in render_shape: {}", e.what());
  } catch (...) {
    loge("SVGRenderer: Unknown exception in render_shape!");
  }
}

void SVGRenderer::render_shapes(NVGcontext* ctx, const std::vector<SVGShapeDesc>& shapes,
                                const nanogui::Vector2f& pos, float scale_x, float scale_y,
                                float rotation, const NVGcolor* tint_color) {
  if (!ctx) {
    loge("SVGRenderer: ctx is null!");
    return;
  }

  if (shapes.empty()) {
    logw("SVGRenderer: No shapes to render");
    return;
  }

  logi("SVGRenderer: render_shapes() called for {} shapes at ({}, {})", 
       shapes.size(), pos.x(), pos.y());

  try {
    // Generate SVG from multiple shape descriptions
    SVGGenerator generator;
    std::string svg_data = generator.generate_multi(shapes);
    
    if (svg_data.empty()) {
      loge("SVGRenderer: Failed to generate SVG from shape descriptions");
      return;
    }
    
    logi("SVGRenderer: Generated multi-shape SVG from DDF-like descriptions ({} bytes)", 
         svg_data.size());
    
    // Load and render the generated SVG
    auto document = load_svg(svg_data);
    if (document) {
      render(ctx, document.get(), pos, scale_x, scale_y, rotation, tint_color);
      logi("SVGRenderer: ✓ Successfully rendered {} shapes from DDF-like descriptions", shapes.size());
    } else {
      loge("SVGRenderer: Failed to load generated SVG");
    }
  } catch (const std::exception& e) {
    loge("SVGRenderer: Exception in render_shapes: {}", e.what());
  } catch (...) {
    loge("SVGRenderer: Unknown exception in render_shapes!");
  }
}

} // namespace whiteboard
