#include <flexui/imageview.h>
#include <cssbox_internal.h>

namespace flexui {

ImageView::ImageView(cssboxRenderer* renderer, const std::string& id,
                     const std::string& imagePath, const ImageViewStyle& style)
    : Widget(renderer, id, "img"),
      imagePath_(imagePath),
      style_(style),
      needsLoad_(!imagePath.empty()) {
}

ImageView::~ImageView() {
    if (imageHandle_ >= 0 && lastVg_) {
        nvgDeleteImage(lastVg_, imageHandle_);
    }
}

void ImageView::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    lastVg_ = vg;

    // Lazy load image on first draw
    if (needsLoad_ && !imagePath_.empty()) {
        loadImage(vg, imagePath_);
        needsLoad_ = false;
    }

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float borderRadius = cssBorderRadius(style_.borderRadius);

    // Draw placeholder or image
    nvgBeginPath(vg);
    if (borderRadius > 0) {
        nvgRoundedRect(vg, x, y, w, h, borderRadius);
    } else {
        nvgRect(vg, x, y, w, h);
    }

    if (imageHandle_ >= 0) {
        // Draw image
        NVGpaint imgPaint = nvgImagePattern(vg, x, y, w, h, 0, imageHandle_, style_.alpha);
        nvgFillPaint(vg, imgPaint);
    } else {
        // Draw placeholder
        nvgFillColor(vg, bgColor);
    }
    nvgFill(vg);
}

bool ImageView::loadImage(NVGcontext* vg, const std::string& path) {
    if (imageHandle_ >= 0) {
        nvgDeleteImage(vg, imageHandle_);
    }

    imageHandle_ = nvgCreateImage(vg, path.c_str(), 0);
    imagePath_ = path;
    lastVg_ = vg;

    return imageHandle_ >= 0;
}

} // namespace flexui
