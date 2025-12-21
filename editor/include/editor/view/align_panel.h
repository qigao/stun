/*
 * Align Panel
 *
 * Panel with alignment and distribution tools for selected objects.
 * Provides align left/center/right/top/middle/bottom and distribute operations.
 */

#pragma once

#include "../core/types.h"
#include <flex/renderer.h>
#include <functional>
#include <string>

namespace editor {

class EditorViewModel;

// Alignment types
enum class AlignType {
    Left, CenterH, Right,
    Top, CenterV, Bottom
};

// Distribution types
enum class DistributeType {
    Horizontal,
    Vertical
};

// Align panel style
struct AlignPanelStyle {
    float width = 200;
    float height = 100;
    float title_height = 24;
    float button_size = 28;
    float padding = 8;
    float spacing = 4;

    Color background = {0.18f, 0.18f, 0.18f, 0.95f};
    Color title_bg = {0.22f, 0.22f, 0.22f, 1.0f};
    Color title_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color button_normal = {0.25f, 0.25f, 0.25f, 1.0f};
    Color button_hover = {0.35f, 0.35f, 0.35f, 1.0f};
    Color button_disabled = {0.2f, 0.2f, 0.2f, 1.0f};
    Color icon_normal = {0.85f, 0.85f, 0.85f, 1.0f};
    Color icon_disabled = {0.4f, 0.4f, 0.4f, 1.0f};
    Color label_text = {0.7f, 0.7f, 0.7f, 1.0f};
};

class AlignPanel {
public:
    AlignPanel();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return style_.width; }
    float height() const { return style_.height; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isDragging() const { return dragging_; }

    AlignPanelStyle& style() { return style_; }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    void renderButton(flex::Renderer& renderer, float x, float y,
                     const char* icon, bool enabled, bool hovered);
    void alignSelection(AlignType type);
    void distributeSelection(DistributeType type);
    int buttonAt(float mx, float my) const;
    bool isInTitleBar(float mx, float my) const;

    EditorViewModel* vm_ = nullptr;
    float x_ = 0, y_ = 0;
    int hovered_button_ = -1;

    // Panel drag state
    bool draggable_ = true;
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;

    AlignPanelStyle style_;
};

} // namespace editor
