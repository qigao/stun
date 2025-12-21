/*
 * Zoom Panel Implementation
 */

#include <editor/view/zoom_panel.h>
#include <editor/viewmodel/editor_vm.h>
#include <cmath>
#include <cstdio>

namespace editor {

ZoomPanel::ZoomPanel() {}

float ZoomPanel::zoomToSlider(float zoom) const {
    // Logarithmic scale: 0.1x to 10x maps to 0-1
    return (std::log(zoom) - std::log(0.1f)) / (std::log(10.0f) - std::log(0.1f));
}

float ZoomPanel::sliderToZoom(float slider) const {
    // Inverse logarithmic: 0-1 maps to 0.1x to 10x
    return std::exp(std::log(0.1f) + slider * (std::log(10.0f) - std::log(0.1f)));
}

void ZoomPanel::render(flex::Renderer& renderer) {
    float zoom = vm_ ? vm_->camera().zoom() : 1.0f;

    // Background
    std::string bg = "M " + std::to_string(x_) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width) +
        " v " + std::to_string(style_.height) +
        " h " + std::to_string(-style_.width) + " Z";
    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));

    float bx = x_ + style_.padding;
    float by = y_ + (style_.height - style_.button_size) / 2;

    // Minus button [-]
    std::string minusBtn = "M " + std::to_string(bx) + " " + std::to_string(by) +
        " h " + std::to_string(style_.button_size) +
        " v " + std::to_string(style_.button_size) +
        " h " + std::to_string(-style_.button_size) + " Z";
    Color minusBg = hovered_element_ == 0 ? style_.button_hover : style_.button_normal;
    renderer.fill_path(minusBtn, flex::Paint::solid({minusBg.r, minusBg.g, minusBg.b, 1}));
    renderer.draw_text("-", bx + 8, by + 17, "Arial", 14, true, {style_.text.r, style_.text.g, style_.text.b, 1});

    bx += style_.button_size + style_.padding;

    // Slider track
    float sliderY = y_ + style_.height / 2;
    std::string track = "M " + std::to_string(bx) + " " + std::to_string(sliderY - 2) +
        " h " + std::to_string(style_.slider_width) + " v 4 h " +
        std::to_string(-style_.slider_width) + " Z";
    renderer.fill_path(track, flex::Paint::solid({style_.slider_track.r, style_.slider_track.g,
        style_.slider_track.b, 1}));

    // Slider fill
    float sliderPos = zoomToSlider(zoom);
    float fillWidth = sliderPos * style_.slider_width;
    std::string fill = "M " + std::to_string(bx) + " " + std::to_string(sliderY - 2) +
        " h " + std::to_string(fillWidth) + " v 4 h " + std::to_string(-fillWidth) + " Z";
    renderer.fill_path(fill, flex::Paint::solid({style_.slider_fill.r, style_.slider_fill.g,
        style_.slider_fill.b, 1}));

    // Slider thumb
    float thumbX = bx + fillWidth;
    std::string thumb = "M " + std::to_string(thumbX - 4) + " " + std::to_string(sliderY - 6) +
        " h 8 v 12 h -8 Z";
    renderer.fill_path(thumb, flex::Paint::solid({style_.slider_thumb.r, style_.slider_thumb.g,
        style_.slider_thumb.b, 1}));

    bx += style_.slider_width + style_.padding;

    // Plus button [+]
    std::string plusBtn = "M " + std::to_string(bx) + " " + std::to_string(by) +
        " h " + std::to_string(style_.button_size) +
        " v " + std::to_string(style_.button_size) +
        " h " + std::to_string(-style_.button_size) + " Z";
    Color plusBg = hovered_element_ == 2 ? style_.button_hover : style_.button_normal;
    renderer.fill_path(plusBtn, flex::Paint::solid({plusBg.r, plusBg.g, plusBg.b, 1}));
    renderer.draw_text("+", bx + 6, by + 17, "Arial", 14, true, {style_.text.r, style_.text.g, style_.text.b, 1});

    bx += style_.button_size + style_.padding;

    // Zoom percentage
    char zoomText[16];
    snprintf(zoomText, sizeof(zoomText), "%d%%", static_cast<int>(zoom * 100 + 0.5f));
    renderer.draw_text(zoomText, bx, y_ + style_.height / 2 + 4, "Arial", 11, false,
        {style_.text.r, style_.text.g, style_.text.b, 1});
}

bool ZoomPanel::onMouseDown(float mx, float my, int button) {
    if (!vm_ || button != 0) return false;

    float bx = x_ + style_.padding;
    float by = y_ + (style_.height - style_.button_size) / 2;

    // Check minus button
    if (mx >= bx && mx < bx + style_.button_size &&
        my >= by && my < by + style_.button_size) {
        vm_->camera().setZoom(vm_->camera().zoom() * 0.8f);
        return true;
    }

    bx += style_.button_size + style_.padding;

    // Check slider
    float sliderY = y_ + style_.height / 2;
    if (mx >= bx && mx < bx + style_.slider_width &&
        my >= sliderY - 10 && my < sliderY + 10) {
        dragging_slider_ = true;
        float sliderPos = (mx - bx) / style_.slider_width;
        sliderPos = std::max(0.0f, std::min(1.0f, sliderPos));
        vm_->camera().setZoom(sliderToZoom(sliderPos));
        return true;
    }

    bx += style_.slider_width + style_.padding;

    // Check plus button
    if (mx >= bx && mx < bx + style_.button_size &&
        my >= by && my < by + style_.button_size) {
        vm_->camera().setZoom(vm_->camera().zoom() * 1.25f);
        return true;
    }

    return false;
}

bool ZoomPanel::onMouseMove(float mx, float my) {
    if (dragging_slider_ && vm_) {
        float bx = x_ + style_.padding + style_.button_size + style_.padding;
        float sliderPos = (mx - bx) / style_.slider_width;
        sliderPos = std::max(0.0f, std::min(1.0f, sliderPos));
        vm_->camera().setZoom(sliderToZoom(sliderPos));
        return true;
    }

    // Update hover state
    hovered_element_ = -1;
    float bx = x_ + style_.padding;
    float by = y_ + (style_.height - style_.button_size) / 2;

    if (mx >= bx && mx < bx + style_.button_size && my >= by && my < by + style_.button_size) {
        hovered_element_ = 0;  // Minus
    }
    bx += style_.button_size + style_.padding;
    if (mx >= bx && mx < bx + style_.slider_width) {
        hovered_element_ = 1;  // Slider
    }
    bx += style_.slider_width + style_.padding;
    if (mx >= bx && mx < bx + style_.button_size && my >= by && my < by + style_.button_size) {
        hovered_element_ = 2;  // Plus
    }

    return hovered_element_ >= 0;
}

bool ZoomPanel::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    dragging_slider_ = false;
    return false;
}

} // namespace editor
