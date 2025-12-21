/*
 * History Panel
 *
 * UI panel for visualizing undo/redo history.
 * Shows list of commands with ability to jump to any state.
 * Supports dragging to reposition.
 */

#pragma once

#include "../core/types.h"
#include "../command/command.h"
#include <flex/renderer.h>
#include <functional>

namespace editor {

// Forward declaration
class EditorViewModel;

// History panel style
struct HistoryPanelStyle {
    float width = 220;
    float title_height = 28;
    float row_height = 24;
    float padding = 8;
    float icon_size = 14;

    Color background = {0.18f, 0.18f, 0.18f, 0.95f};
    Color title_bg = {0.22f, 0.22f, 0.22f, 1.0f};
    Color title_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color row_current = {0.3f, 0.5f, 0.8f, 1.0f};
    Color row_hover = {0.28f, 0.28f, 0.28f, 1.0f};
    Color row_undo = {0.22f, 0.22f, 0.22f, 1.0f};
    Color row_redo = {0.18f, 0.18f, 0.18f, 1.0f};
    Color text_normal = {0.85f, 0.85f, 0.85f, 1.0f};
    Color text_dimmed = {0.5f, 0.5f, 0.5f, 1.0f};
    Color separator = {0.3f, 0.3f, 0.3f, 1.0f};
};

// History Panel
class HistoryPanel {
public:
    HistoryPanel();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Position and size
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return style_.width; }
    float height() const { return calculated_height_; }
    Rect bounds() const { return {x_, y_, style_.width, calculated_height_}; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isDragging() const { return dragging_; }

    // Style
    HistoryPanelStyle& style() { return style_; }

    // Rendering
    void render(flex::Renderer& renderer);

    // Input handling
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);
    bool onScroll(float mx, float my, float delta);

private:
    void renderHistoryItem(flex::Renderer& renderer, const std::string& desc,
                          float y, bool isCurrent, bool isUndo, bool isHovered);
    int itemAt(float mx, float my) const;
    bool isInTitleBar(float mx, float my) const;

    EditorViewModel* vm_ = nullptr;
    float x_ = 0, y_ = 0;
    float calculated_height_ = 200;
    float scroll_offset_ = 0;
    float max_scroll_ = 0;

    // Drag state
    bool draggable_ = true;
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;

    int hovered_item_ = -1;  // -1 = none, 0+ = undo items, negative = redo items

    HistoryPanelStyle style_;
};

} // namespace editor
