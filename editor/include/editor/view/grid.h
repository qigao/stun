/*
 * Grid System
 *
 * Configurable grid for snapping and visual alignment.
 * Supports rectangular and isometric grids.
 */

#pragma once

#include "../core/types.h"
#include <flex/renderer.h>

namespace editor {

class EditorViewModel;

// Grid types
enum class GridType {
    Rectangular,
    Isometric
};

// Grid settings
struct GridSettings {
    GridType type = GridType::Rectangular;
    float size = 20.0f;          // Grid cell size
    int subdivisions = 4;         // Minor grid divisions
    bool visible = true;
    bool snap_enabled = true;

    Color major_color = {0.3f, 0.3f, 0.3f, 0.5f};
    Color minor_color = {0.25f, 0.25f, 0.25f, 0.3f};
};

class Grid {
public:
    Grid();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Settings
    GridSettings& settings() { return settings_; }
    const GridSettings& settings() const { return settings_; }

    void setSize(float size) { settings_.size = size; }
    float size() const { return settings_.size; }

    void setSubdivisions(int sub) { settings_.subdivisions = sub; }
    int subdivisions() const { return settings_.subdivisions; }

    void setVisible(bool v) { settings_.visible = v; }
    bool isVisible() const { return settings_.visible; }

    void setSnapEnabled(bool e) { settings_.snap_enabled = e; }
    bool isSnapEnabled() const { return settings_.snap_enabled; }

    void setGridType(GridType t) { settings_.type = t; }
    GridType gridType() const { return settings_.type; }

    // Snapping
    Point snapPoint(const Point& p) const;
    float snapValue(float v) const;

    // Rendering
    void render(flex::Renderer& renderer, const Rect& visibleArea, float zoom);

private:
    void renderRectangularGrid(flex::Renderer& renderer, const Rect& area, float zoom);
    void renderIsometricGrid(flex::Renderer& renderer, const Rect& area, float zoom);

    EditorViewModel* vm_ = nullptr;
    GridSettings settings_;
};

// Grid Panel UI
struct GridPanelStyle {
    float width = 200;
    float height = 160;
    float title_height = 24;
    float padding = 10;
    float row_height = 24;

    Color background = {0.18f, 0.18f, 0.18f, 0.95f};
    Color title_bg = {0.22f, 0.22f, 0.22f, 1.0f};
    Color title_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color label_text = {0.7f, 0.7f, 0.7f, 1.0f};
    Color value_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color toggle_on = {0.3f, 0.6f, 0.9f, 1.0f};
    Color toggle_off = {0.3f, 0.3f, 0.3f, 1.0f};
};

class GridPanel {
public:
    GridPanel();

    void setGrid(Grid* grid) { grid_ = grid; }

    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return style_.width; }
    float height() const { return style_.height; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isDragging() const { return panel_dragging_; }

    GridPanelStyle& style() { return style_; }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    void renderToggle(flex::Renderer& renderer, float x, float y, bool value, bool hovered);
    void renderSlider(flex::Renderer& renderer, float x, float y, float w, float value, float min, float max);
    int elementAt(float mx, float my) const;
    bool isInTitleBar(float mx, float my) const;

    Grid* grid_ = nullptr;
    float x_ = 0, y_ = 0;
    int hovered_element_ = -1;
    bool dragging_slider_ = false;
    int drag_slider_ = -1;

    // Panel drag state
    bool draggable_ = true;
    bool panel_dragging_ = false;
    float panel_drag_offset_x_ = 0;
    float panel_drag_offset_y_ = 0;

    GridPanelStyle style_;
};

} // namespace editor
