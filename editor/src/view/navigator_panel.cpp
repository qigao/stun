/*
 * Navigator Panel Implementation
 */

#include <editor/view/navigator_panel.h>
#include <editor/viewmodel/editor_vm.h>
#include <algorithm>

namespace editor {

NavigatorPanel::NavigatorPanel() {}

bool NavigatorPanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + style_.width &&
           my >= y_ && my < y_ + style_.title_height;
}

Rect NavigatorPanel::getCanvasArea() const {
    return {
        x_ + style_.padding,
        y_ + style_.title_height + style_.padding,
        style_.width - style_.padding * 2,
        style_.height - style_.title_height - style_.padding * 2 - 30  // Leave room for zoom slider
    };
}

Rect NavigatorPanel::getViewportRect() const {
    if (!vm_) return {};

    auto canvasArea = getCanvasArea();
    auto& camera = vm_->camera();
    auto* doc = vm_->document();

    if (!doc) return {};

    float docW = doc->width();
    float docH = doc->height();
    if (docW <= 0) docW = 800;
    if (docH <= 0) docH = 600;

    float scaleX = canvasArea.width / docW;
    float scaleY = canvasArea.height / docH;
    float scale = std::min(scaleX, scaleY) * 0.9f;

    float docDisplayW = docW * scale;
    float docDisplayH = docH * scale;
    float docDisplayX = canvasArea.x + (canvasArea.width - docDisplayW) / 2;
    float docDisplayY = canvasArea.y + (canvasArea.height - docDisplayH) / 2;

    float viewW = 800 / camera.zoom();
    float viewH = 600 / camera.zoom();
    float viewX = camera.panX();
    float viewY = camera.panY();

    float vpX = docDisplayX + viewX * scale;
    float vpY = docDisplayY + viewY * scale;
    float vpW = viewW * scale;
    float vpH = viewH * scale;

    return {vpX, vpY, vpW, vpH};
}

void NavigatorPanel::navigateTo(float mx, float my) {
    if (!vm_) return;

    auto canvasArea = getCanvasArea();
    auto& camera = vm_->camera();
    auto* doc = vm_->document();

    if (!doc) return;

    float docW = doc->width();
    float docH = doc->height();
    if (docW <= 0) docW = 800;
    if (docH <= 0) docH = 600;

    float scaleX = canvasArea.width / docW;
    float scaleY = canvasArea.height / docH;
    float scale = std::min(scaleX, scaleY) * 0.9f;

    float docDisplayW = docW * scale;
    float docDisplayH = docH * scale;
    float docDisplayX = canvasArea.x + (canvasArea.width - docDisplayW) / 2;
    float docDisplayY = canvasArea.y + (canvasArea.height - docDisplayH) / 2;

    float docX = (mx - docDisplayX) / scale;
    float docY = (my - docDisplayY) / scale;

    float viewW = 800 / camera.zoom();
    float viewH = 600 / camera.zoom();
    camera.setPan(docX - viewW / 2, docY - viewH / 2);
}

void NavigatorPanel::render(flex::Renderer& renderer) {
    // Background with rounded corners
    std::string bg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.height - 8) +
        " a 4 4 0 0 1 -4 4" +
        " h " + std::to_string(-(style_.width - 8)) +
        " a 4 4 0 0 1 -4 -4" +
        " v " + std::to_string(-(style_.height - 8)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));

    // Title bar
    std::string titleBg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.title_height - 4) +
        " h " + std::to_string(-style_.width) +
        " v " + std::to_string(-(style_.title_height - 4)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(titleBg, flex::Paint::solid({style_.title_bg.r, style_.title_bg.g,
        style_.title_bg.b, style_.title_bg.a}));

    // Drag grip
    if (draggable_) {
        float gripX = x_ + 8;
        float gripY = y_ + style_.title_height / 2 - 4;
        for (int i = 0; i < 3; ++i) {
            std::string line = "M " + std::to_string(gripX) + " " + std::to_string(gripY + i * 3) +
                " h 8";
            renderer.stroke_path(line, flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.5f}), 1.0f);
        }
    }

    renderer.draw_text("Navigator", x_ + 22, y_ + 16, "Arial", 11, true,
        {style_.title_text.r, style_.title_text.g, style_.title_text.b, 1});

    // Canvas area background
    auto canvasArea = getCanvasArea();
    std::string canvasBg = "M " + std::to_string(canvasArea.x) + " " + std::to_string(canvasArea.y) +
        " h " + std::to_string(canvasArea.width) +
        " v " + std::to_string(canvasArea.height) +
        " h " + std::to_string(-canvasArea.width) + " Z";
    renderer.fill_path(canvasBg, flex::Paint::solid({style_.canvas_bg.r, style_.canvas_bg.g,
        style_.canvas_bg.b, style_.canvas_bg.a}));

    if (!vm_ || !vm_->document()) return;

    auto* doc = vm_->document();
    float docW = doc->width();
    float docH = doc->height();
    if (docW <= 0) docW = 800;
    if (docH <= 0) docH = 600;

    float scaleX = canvasArea.width / docW;
    float scaleY = canvasArea.height / docH;
    float scale = std::min(scaleX, scaleY) * 0.9f;

    float docDisplayW = docW * scale;
    float docDisplayH = docH * scale;
    float docDisplayX = canvasArea.x + (canvasArea.width - docDisplayW) / 2;
    float docDisplayY = canvasArea.y + (canvasArea.height - docDisplayH) / 2;

    // Draw mini representations of shapes
    for (auto& layer : doc->layers()) {
        if (!layer->visible()) continue;

        for (auto& node : layer->nodes()) {
            auto bounds = node->bounds();

            float rx = docDisplayX + bounds.x * scale;
            float ry = docDisplayY + bounds.y * scale;
            float rw = std::max(2.0f, bounds.width * scale);
            float rh = std::max(2.0f, bounds.height * scale);

            std::string shape = "M " + std::to_string(rx) + " " + std::to_string(ry) +
                " h " + std::to_string(rw) + " v " + std::to_string(rh) +
                " h " + std::to_string(-rw) + " Z";
            renderer.fill_path(shape, flex::Paint::solid({style_.shape_fill.r,
                style_.shape_fill.g, style_.shape_fill.b, style_.shape_fill.a}));
        }
    }

    // Draw viewport rectangle
    auto vp = getViewportRect();
    if (vp.width > 0 && vp.height > 0) {
        float vpX1 = std::max(canvasArea.x, vp.x);
        float vpY1 = std::max(canvasArea.y, vp.y);
        float vpX2 = std::min(canvasArea.x + canvasArea.width, vp.x + vp.width);
        float vpY2 = std::min(canvasArea.y + canvasArea.height, vp.y + vp.height);

        if (vpX2 > vpX1 && vpY2 > vpY1) {
            std::string vpRect = "M " + std::to_string(vpX1) + " " + std::to_string(vpY1) +
                " h " + std::to_string(vpX2 - vpX1) +
                " v " + std::to_string(vpY2 - vpY1) +
                " h " + std::to_string(-(vpX2 - vpX1)) + " Z";
            renderer.fill_path(vpRect, flex::Paint::solid({style_.viewport_fill.r,
                style_.viewport_fill.g, style_.viewport_fill.b, style_.viewport_fill.a}));
            renderer.stroke_path(vpRect, flex::Paint::solid({style_.viewport_border.r,
                style_.viewport_border.g, style_.viewport_border.b, 1}), 1.5f);
        }
    }

    // Draw zoom percentage at bottom
    if (vm_) {
        char zoomText[16];
        snprintf(zoomText, sizeof(zoomText), "%.0f%%", vm_->camera().zoom() * 100);
        float zoomY = y_ + style_.height - 12;
        renderer.draw_text(zoomText, x_ + style_.width - 40, zoomY, "Arial", 10, false,
            {0.6f, 0.6f, 0.6f, 1});
    }
}

bool NavigatorPanel::onMouseDown(float mx, float my, int button) {
    if (button != 0) return false;

    // Check for panel drag start on title bar
    if (draggable_ && isInTitleBar(mx, my)) {
        panel_dragging_ = true;
        panel_drag_offset_x_ = mx - x_;
        panel_drag_offset_y_ = my - y_;
        return true;
    }

    auto canvasArea = getCanvasArea();
    if (!canvasArea.contains({mx, my})) return false;

    auto vp = getViewportRect();
    if (vp.contains({mx, my})) {
        nav_dragging_ = true;
        nav_offset_x_ = mx - vp.x;
        nav_offset_y_ = my - vp.y;
    } else {
        navigateTo(mx, my);
        nav_dragging_ = true;
        nav_offset_x_ = vp.width / 2;
        nav_offset_y_ = vp.height / 2;
    }

    return true;
}

bool NavigatorPanel::onMouseMove(float mx, float my) {
    // Handle panel dragging
    if (panel_dragging_) {
        x_ = mx - panel_drag_offset_x_;
        y_ = my - panel_drag_offset_y_;
        return true;
    }

    // Handle viewport navigation dragging
    if (nav_dragging_) {
        navigateTo(mx - nav_offset_x_ + getViewportRect().width / 2,
                   my - nav_offset_y_ + getViewportRect().height / 2);
        return true;
    }

    auto canvasArea = getCanvasArea();
    return canvasArea.contains({mx, my}) || isInTitleBar(mx, my);
}

bool NavigatorPanel::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    if (panel_dragging_) {
        panel_dragging_ = false;
        return true;
    }
    nav_dragging_ = false;
    return false;
}

} // namespace editor
