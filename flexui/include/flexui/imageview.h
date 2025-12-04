#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>

namespace flexui {

struct ImageViewStyle {
    float borderRadius = 0;
    float alpha = 1.0f;
    NVGcolor bgColor = nvgRGBA(200, 200, 200, 128);  // Placeholder color
};

class ImageView : public Widget {
public:
    ImageView(cssboxRenderer* renderer, const std::string& id,
              const std::string& imagePath = "",
              const ImageViewStyle& style = ImageViewStyle());
    ~ImageView();

    void draw(NVGcontext* vg) override;

    bool loadImage(NVGcontext* vg, const std::string& path);
    void setImagePath(const std::string& path) { imagePath_ = path; needsLoad_ = true; }

    void setImageViewStyle(const ImageViewStyle& style) { style_ = style; }

private:
    std::string imagePath_;
    ImageViewStyle style_;
    int imageHandle_ = -1;
    bool needsLoad_ = false;
    NVGcontext* lastVg_ = nullptr;  // For cleanup
};

} // namespace flexui
