/*
 * UI Panel System
 *
 * Base classes for UI panels and widgets in the editor.
 * Provides a simple immediate-mode style API for building panels.
 */

#pragma once

#include "../core/types.h"
#include <flex/flex.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace editor {

// Forward declarations
class Panel;
class Widget;

// Panel region (docking position)
enum class PanelRegion {
    Left,
    Right,
    Top,
    Bottom,
    Float
};

// =============================================================================
// DraggablePanel - Base class for moveable panels with title bar
// =============================================================================

struct DraggablePanelStyle {
    float title_height = 24;
    float corner_radius = 4;

    Color background = {0.15f, 0.15f, 0.15f, 0.95f};
    Color title_bg = {0.2f, 0.2f, 0.2f, 1.0f};
    Color title_text = {0.85f, 0.85f, 0.85f, 1.0f};
    Color border = {0.3f, 0.3f, 0.3f, 1.0f};
};

class DraggablePanel {
public:
    DraggablePanel(const std::string& title = "Panel");
    virtual ~DraggablePanel() = default;

    // Position and size
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    virtual float width() const { return width_; }
    virtual float height() const { return height_; }
    void setSize(float w, float h) { width_ = w; height_ = h; }

    // Title
    const std::string& title() const { return title_; }
    void setTitle(const std::string& t) { title_ = t; }

    // Visibility
    bool visible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    // Dragging
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isDragging() const { return dragging_; }

    // Style
    DraggablePanelStyle& panelStyle() { return panel_style_; }

    // Render title bar (call from subclass render)
    void renderTitleBar(flex::Renderer& renderer);

    // Input handling for drag (call from subclass input handlers)
    bool handleDragStart(float mx, float my, int button);
    bool handleDragMove(float mx, float my);
    bool handleDragEnd(float mx, float my, int button);

protected:
    bool isInTitleBar(float mx, float my) const;

    std::string title_;
    float x_ = 0, y_ = 0;
    float width_ = 200, height_ = 150;
    bool visible_ = true;
    bool draggable_ = true;

    // Drag state
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;

    DraggablePanelStyle panel_style_;
};

// Base Widget class
class Widget {
public:
    virtual ~Widget() = default;

    virtual float width() const { return width_; }
    virtual float height() const { return height_; }
    virtual void setSize(float w, float h) { width_ = w; height_ = h; }

    virtual void render(flex::Renderer& renderer, float x, float y) = 0;
    virtual bool onMouseDown(float x, float y, int button) { (void)x; (void)y; (void)button; return false; }
    virtual bool onMouseUp(float x, float y, int button) { (void)x; (void)y; (void)button; return false; }
    virtual bool onMouseMove(float x, float y) { (void)x; (void)y; return false; }
    virtual bool onKeyDown(int key, bool shift, bool ctrl) { (void)key; (void)shift; (void)ctrl; return false; }

protected:
    float width_ = 100;
    float height_ = 24;
};

// Label widget
class LabelWidget : public Widget {
public:
    explicit LabelWidget(const std::string& text) : text_(text) {}

    void setText(const std::string& text) { text_ = text; }
    const std::string& text() const { return text_; }

    void render(flex::Renderer& renderer, float x, float y) override;

private:
    std::string text_;
};

// Number input widget
class NumberWidget : public Widget {
public:
    NumberWidget(float value = 0, float min = -1e30f, float max = 1e30f)
        : value_(value), min_(min), max_(max) {
        height_ = 24;
    }

    float value() const { return value_; }
    void setValue(float v);

    void onChange(std::function<void(float)> cb) { onChange_ = std::move(cb); }

    void render(flex::Renderer& renderer, float x, float y) override;
    bool onMouseDown(float x, float y, int button) override;
    bool onMouseUp(float x, float y, int button) override;
    bool onMouseMove(float x, float y) override;

private:
    float value_;
    float min_, max_;
    std::function<void(float)> onChange_;
    bool dragging_ = false;
    float drag_start_x_ = 0;
    float drag_start_value_ = 0;
};

// Color swatch widget
class ColorSwatchWidget : public Widget {
public:
    explicit ColorSwatchWidget(const Color& color = {1, 1, 1, 1}) : color_(color) {
        width_ = 24;
        height_ = 24;
    }

    const Color& color() const { return color_; }
    void setColor(const Color& c);

    void onChange(std::function<void(const Color&)> cb) { onChange_ = std::move(cb); }
    void onClick(std::function<void()> cb) { onClick_ = std::move(cb); }

    void render(flex::Renderer& renderer, float x, float y) override;
    bool onMouseDown(float x, float y, int button) override;

private:
    Color color_;
    std::function<void(const Color&)> onChange_;
    std::function<void()> onClick_;
};

// Property row (label + widget)
struct PropertyRow {
    std::string label;
    std::unique_ptr<Widget> widget;
    float labelWidth = 60;
};

// Property Panel Style
struct PropertyPanelStyle {
    float width = 250;
    float title_height = 24;
    float section_height = 24;
    float row_height = 28;
    float padding = 8;

    Color background = {0.18f, 0.18f, 0.18f, 0.95f};
    Color title_bg = {0.22f, 0.22f, 0.22f, 1.0f};
    Color title_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color section_bg = {0.15f, 0.15f, 0.15f, 1.0f};
    Color section_text = {0.8f, 0.8f, 0.8f, 1.0f};
    Color label_text = {0.7f, 0.7f, 0.7f, 1.0f};
};

// Property Panel
class PropertyPanel {
public:
    PropertyPanel() : width_(250) {}

    float width() const { return width_; }
    float height() const { return height_; }
    void setWidth(float w) { width_ = w; }

    // Position
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isDragging() const { return dragging_; }

    PropertyPanelStyle& style() { return style_; }

    void clear();
    void addSection(const std::string& title);
    void addProperty(const std::string& label, std::unique_ptr<Widget> widget);
    void addNumberProperty(const std::string& label, float value, std::function<void(float)> onChange);
    void addColorProperty(const std::string& label, const Color& color,
                         std::function<void(const Color&)> onChange,
                         std::function<void()> onClick = nullptr);

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    bool isInTitleBar(float mx, float my) const;

    struct Section {
        std::string title;
        size_t startIndex;
    };

    float x_ = 0, y_ = 0;
    float width_ = 250;
    float height_ = 300;

    // Panel drag state
    bool draggable_ = true;
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;

    std::vector<PropertyRow> rows_;
    std::vector<Section> sections_;
    PropertyPanelStyle style_;
};

} // namespace editor
