/*
 * Zoom Panel
 *
 * Compact zoom controls with slider, percentage display, and preset buttons.
 */

#pragma once

#include "../core/types.h"
#include <flex/renderer.h>
#include <functional>

namespace editor {

class EditorViewModel;

// Zoom panel style
struct ZoomPanelStyle {
    float width = 180;
    float height = 32;
    float button_size = 24;
    float slider_width = 80;
    float padding = 6;

    Color background = {0.18f, 0.18f, 0.18f, 1.0f};
    Color button_normal = {0.25f, 0.25f, 0.25f, 1.0f};
    Color button_hover = {0.35f, 0.35f, 0.35f, 1.0f};
    Color slider_track = {0.3f, 0.3f, 0.3f, 1.0f};
    Color slider_fill = {0.4f, 0.6f, 0.9f, 1.0f};
    Color slider_thumb = {0.9f, 0.9f, 0.9f, 1.0f};
    Color text = {0.85f, 0.85f, 0.85f, 1.0f};
};

class ZoomPanel {
public:
    ZoomPanel();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return style_.width; }
    float height() const { return style_.height; }

    ZoomPanelStyle& style() { return style_; }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    float zoomToSlider(float zoom) const;
    float sliderToZoom(float slider) const;

    EditorViewModel* vm_ = nullptr;
    float x_ = 0, y_ = 0;

    bool dragging_slider_ = false;
    int hovered_element_ = -1;  // 0=minus, 1=slider, 2=plus, 3=fit, 4=100%

    ZoomPanelStyle style_;
};

} // namespace editor
