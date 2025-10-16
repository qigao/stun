/**
 * \file svg_renderer.cpp
 * \brief Implementation of SVG rendering service using LunaSVG.
 */

#include "whiteboard/svg/svg_renderer.h"
#include <fmtlog.h>
#include <iostream>
#include <nanogui/vector.h>

namespace whiteboard {

SVGRenderer::SVGRenderer() {
  logi("SVGRenderer: Initialized with LunaSVG");
}

SVGRenderer::~SVGRenderer() {
  logi("SVGRenderer: Destroyed");
}

std::unique_ptr<lunasvg::Document> SVGRenderer::load_svg(const std::string &svg_data) {
  logd("SVGRenderer: load_svg called with {} bytes", svg_data.size());

  if (svg_data.empty()) {
    loge("SVGRenderer: Empty SVG data");
    return nullptr;
  }

  try {
    // Parse SVG with LunaSVG (simple and clean!)
    auto document = lunasvg::Document::loadFromData(svg_data);
    
    if (!document) {
      loge("SVGRenderer: Failed to parse SVG data");
      return nullptr;
    }

    logd("SVGRenderer: SVG loaded successfully");
    return document;
    
  } catch (const std::exception &e) {
    loge("SVGRenderer: Exception loading SVG: {}", e.what());
    return nullptr;
  }
}

void SVGRenderer::render(NVGcontext *ctx, lunasvg::Document *document, const nanogui::Vector2f &pos,
                         float scale_x, float scale_y, float rotation) {
  logd("SVGRenderer: render called - pos=({}, {}), scale=({}, {}), rotation={}", 
       pos.x(), pos.y(), scale_x, scale_y, rotation);

  if (!ctx || !document) {
    loge("SVGRenderer: ctx or document is null!");
    return;
  }

  try {
    // Get document dimensions
    double doc_width = document->width();
    double doc_height = document->height();
    
    logd("SVGRenderer: Document size: {}x{}", doc_width, doc_height);

    // Calculate render dimensions
    int render_width = static_cast<int>(doc_width * scale_x);
    int render_height = static_cast<int>(doc_height * scale_y);

    if (render_width <= 0 || render_height <= 0) {
      logw("SVGRenderer: Invalid render dimensions: {}x{}", render_width, render_height);
      return;
    }

    // Render SVG to bitmap (LunaSVG handles everything!)
    auto bitmap = document->renderToBitmap(render_width, render_height);
    
    if (!bitmap.valid()) {
      loge("SVGRenderer: Failed to render bitmap");
      return;
    }

    logd("SVGRenderer: Bitmap rendered: {}x{}", bitmap.width(), bitmap.height());

    // Convert RGBA to format NanoVG expects
    bitmap.convertToRGBA();

    // Create NanoVG image from bitmap data
    int nvg_image = nvgCreateImageRGBA(ctx, bitmap.width(), bitmap.height(), 
                                       0, bitmap.data());
    
    if (nvg_image == 0) {
      loge("SVGRenderer: Failed to create NanoVG image");
      return;
    }

    logd("SVGRenderer: NanoVG image created (id={})", nvg_image);

    // Draw the image with transforms
    nvgSave(ctx);
    nvgTranslate(ctx, pos.x(), pos.y());

    if (rotation != 0.0f) {
      // Rotate around center of image
      nvgTranslate(ctx, render_width * 0.5f, render_height * 0.5f);
      nvgRotate(ctx, rotation);
      nvgTranslate(ctx, -render_width * 0.5f, -render_height * 0.5f);
    }

    // Draw the image
    NVGpaint img_paint = nvgImagePattern(ctx, 0, 0, 
                                         static_cast<float>(render_width),
                                         static_cast<float>(render_height), 
                                         0, nvg_image, 1.0f);

    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, static_cast<float>(render_width), static_cast<float>(render_height));
    nvgFillPaint(ctx, img_paint);
    nvgFill(ctx);

    nvgRestore(ctx);

    // Delete the NanoVG image (bitmap destructor handles its own cleanup)
    nvgDeleteImage(ctx, nvg_image);

    logd("SVGRenderer: Render complete!");

  } catch (const std::exception &e) {
    loge("SVGRenderer: Exception during render: {}", e.what());
  } catch (...) {
    loge("SVGRenderer: Unknown exception during render!");
  }
}

} // namespace whiteboard
