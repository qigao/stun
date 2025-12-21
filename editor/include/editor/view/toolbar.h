/*
 * Toolbar
 *
 * Horizontal or vertical toolbar with tool buttons.
 * Displays icons and handles tool switching.
 * Supports dragging to reposition.
 */

#pragma once

#include "../core/types.h"
#include <flex/flex.h>
#include <string>
#include <vector>
#include <functional>

namespace editor {

// Forward declaration
class EditorViewModel;

// Toolbar orientation
enum class ToolbarOrientation {
    Horizontal,
    Vertical
};

// Toolbar button definition
struct ToolbarButton {
    std::string id;
    std::string icon;       // Icon character or SVG path
    std::string svgPath;    // SVG path data for vector icon
    std::string tooltip;
    std::string shortcut;   // Display shortcut hint
    bool separator = false; // Is this a separator?

    ToolbarButton() = default;
    ToolbarButton(const std::string& id_, const std::string& icon_,
                  const std::string& tip, const std::string& key = "")
        : id(id_), icon(icon_), tooltip(tip), shortcut(key) {}

    // Constructor with SVG path
    ToolbarButton(const std::string& id_, const std::string& icon_,
                  const std::string& svg, const std::string& tip, const std::string& key)
        : id(id_), icon(icon_), svgPath(svg), tooltip(tip), shortcut(key) {}

    static ToolbarButton Separator() {
        ToolbarButton btn;
        btn.separator = true;
        return btn;
    }
};

// Toolbar style settings
struct ToolbarStyle {
    float button_size = 32;
    float padding = 4;
    float separator_size = 8;
    float icon_size = 18;

    Color background = {0.18f, 0.18f, 0.18f, 1.0f};
    Color button_normal = {0.25f, 0.25f, 0.25f, 1.0f};
    Color button_hover = {0.35f, 0.35f, 0.35f, 1.0f};
    Color button_active = {0.3f, 0.5f, 0.8f, 1.0f};
    Color icon_color = {0.85f, 0.85f, 0.85f, 1.0f};
    Color tooltip_bg = {0.1f, 0.1f, 0.1f, 0.95f};
    Color tooltip_text = {0.9f, 0.9f, 0.9f, 1.0f};
};

// Toolbar component
class Toolbar {
public:
    Toolbar(ToolbarOrientation orient = ToolbarOrientation::Vertical);

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Position and size
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const;
    float height() const;
    Rect bounds() const { return {x_, y_, width(), height()}; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isDragging() const { return dragging_; }

    // Orientation
    ToolbarOrientation orientation() const { return orientation_; }
    void setOrientation(ToolbarOrientation o) { orientation_ = o; }

    // Button management
    void addButton(const ToolbarButton& btn);
    void addSeparator();
    void clearButtons();

    // Active tool
    const std::string& activeToolId() const { return active_tool_id_; }
    void setActiveToolId(const std::string& id) { active_tool_id_ = id; }

    // Callbacks
    void onToolSelected(std::function<void(const std::string&)> cb) { onToolSelected_ = std::move(cb); }

    // Style
    ToolbarStyle& style() { return style_; }

    // Rendering
    void render(flex::Renderer& renderer);

    // Input handling
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

    // Setup default tools with SVG icons
    void setupDefaultTools();

private:
    void renderButton(flex::Renderer& renderer, const ToolbarButton& btn,
                     float bx, float by, bool hovered, bool active);
    void renderSvgIcon(flex::Renderer& renderer, const std::string& svgPath,
                      float x, float y, float size, const Color& color);
    void renderTooltip(flex::Renderer& renderer);
    int buttonAt(float mx, float my) const;
    bool isInDragArea(float mx, float my) const;

    EditorViewModel* vm_ = nullptr;
    ToolbarOrientation orientation_;
    float x_ = 0, y_ = 0;
    bool draggable_ = true;

    // Drag state
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;

    std::vector<ToolbarButton> buttons_;
    std::string active_tool_id_ = "select";

    int hovered_button_ = -1;
    float hover_time_ = 0;
    bool show_tooltip_ = false;

    ToolbarStyle style_;
    std::function<void(const std::string&)> onToolSelected_;
};

} // namespace editor
