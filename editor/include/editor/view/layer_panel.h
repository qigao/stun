/*
 * Layer Panel
 *
 * UI panel for managing document layers.
 * Displays layer list with visibility, lock, and reordering controls.
 * Supports drag-and-drop reordering.
 */

#pragma once

#include "../model/document.h"
#include <flex/renderer.h>

namespace editor {

class LayerPanel {
public:
    LayerPanel() = default;

    float width() const { return width_; }
    float height() const { return height_; }
    void setWidth(float w) { width_ = w; }

    void setDocument(Document* doc) { document_ = doc; }

    // Position for draggable panel
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isPanelDragging() const { return panel_dragging_; }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseUp(float mx, float my, int button);
    bool onMouseMove(float mx, float my);

    // Check if dragging layer (for cursor feedback)
    bool isDraggingLayer() const { return layer_dragging_; }

private:
    // Icon drawing helpers
    void drawEyeIcon(flex::Renderer& renderer, float x, float y, bool visible);
    void drawLockIcon(flex::Renderer& renderer, float x, float y, bool locked);
    void drawButton(flex::Renderer& renderer, float x, float y, const char* label, bool hovered);
    void drawArrow(flex::Renderer& renderer, float x, float y, bool up);
    void drawDropIndicator(flex::Renderer& renderer, float x, float y, float w);
    bool isInTitleBar(float mx, float my) const;

    // Actions
    void addNewLayer();
    void deleteActiveLayer();
    void moveLayerUp(size_t index);
    void moveLayerDown(size_t index);

    Document* document_ = nullptr;
    float x_ = 0, y_ = 0;
    float width_ = 200;
    float height_ = 300;

    // Layout constants
    static constexpr float TITLE_HEIGHT = 36;
    static constexpr float ROW_HEIGHT = 28;
    static constexpr float ICON_SIZE = 16;
    static constexpr float PADDING = 8;
    static constexpr float BUTTON_SIZE = 20;

    // Hover state
    int hovered_row_ = -1;
    int hovered_button_ = -1;  // 0=add, 1=delete, 2=up, 3=down

    // Panel drag state
    bool draggable_ = true;
    bool panel_dragging_ = false;
    float panel_drag_offset_x_ = 0;
    float panel_drag_offset_y_ = 0;

    // Layer drag state
    bool layer_dragging_ = false;
    int drag_source_row_ = -1;      // Row being dragged (visual index)
    int drag_source_layer_ = -1;    // Layer index being dragged
    float drag_start_y_ = 0;
    float drag_current_y_ = 0;
    int drop_target_row_ = -1;      // Where to drop (visual index)
};

} // namespace editor
