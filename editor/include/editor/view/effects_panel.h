/*
 * Effects Panel
 *
 * UI panel for editing node effects (Shadow, Blur).
 */

#pragma once

#include "../core/types.h"
#include "../model/node.h"
#include "color_picker.h"
#include <flex/flex.h>
#include <functional>

namespace editor {

class EffectsPanel {
public:
    EffectsPanel() : width_(260), height_(320) {}

    float width() const { return width_; }
    float height() const { return height_; }

    bool visible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    void setPosition(float x, float y) { x_ = x; y_ = y; }

    // Set node to edit
    void setNode(EditorNode* node);
    EditorNode* node() const { return node_; }

    void onChange(std::function<void()> cb) { onChange_ = std::move(cb); }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    void renderShadowSection(flex::Renderer& renderer, float x, float& y);
    void renderBlurSection(flex::Renderer& renderer, float x, float& y);
    void renderSlider(flex::Renderer& renderer, float x, float y, float w,
                      float value, float minVal, float maxVal, const char* label, int id);
    void renderCheckbox(flex::Renderer& renderer, float x, float y, bool checked, const char* label, int id);

    void notifyChange();

    float width_, height_;
    float x_ = 0, y_ = 0;
    bool visible_ = false;

    EditorNode* node_ = nullptr;

    // Shadow state
    bool shadow_enabled_ = false;
    float shadow_ox_ = 4;
    float shadow_oy_ = 4;
    float shadow_blur_ = 8;
    Color shadow_color_ = {0, 0, 0, 0.5f};

    // Blur state
    bool blur_enabled_ = false;
    float blur_radius_ = 0;

    // UI state
    int dragging_slider_ = -1;
    Rect slider_rects_[5];
    Rect checkbox_rects_[2];
    bool editing_shadow_color_ = false;

    ColorPicker color_picker_;

    std::function<void()> onChange_;
};

} // namespace editor
