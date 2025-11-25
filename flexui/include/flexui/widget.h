#pragma once

#include <nanovg_css.h>
#include <functional>
#include <string>

namespace flexui {

class Widget {
public:
    using ClickCallback = std::function<bool(Widget*)>;
    using HoverCallback = std::function<void(Widget*, bool)>;

    Widget(NVGCSSRenderer* renderer, const std::string& id, const std::string& tag);
    virtual ~Widget() = default; // Added virtual destructor to make the class polymorphic

    void setClass(const std::string& className);
    void addClass(const std::string& className);
    void removeClass(const std::string& className);
    void setPosition(float x, float y);
    void setSize(float w, float h);
    void setText(const std::string& text);
    void addChild(Widget* child);
    void setInlineStyle(const std::string& property, const std::string& value);
    
    void setClickCallback(ClickCallback cb) { click_callback_ = cb; }
    void setHoverCallback(HoverCallback cb) { hover_callback_ = cb; }

    bool handleClick(float x, float y);
    void handleHover(float x, float y);

    // SVG-specific styling
    void setStroke(const std::string& color, float width);
    void setStrokeDash(const std::string& pattern);
    void setStrokeLineCap(const std::string& cap);
    void setStrokeLineJoin(const std::string& join);
    void setFill(const std::string& color);
    
    // Path manipulation
    void addPathPoint(float x, float y);
    void clearPath();
    void setHandDrawn(bool enabled, float seed = 42.0f);

    // Virtual mouse event handlers for drag-based widgets (like Slider)
    virtual bool handleMouseDown(float x, float y) { return false; }
    virtual bool handleMouseMove(float x, float y) { return false; }
    virtual bool handleMouseUp(float x, float y) { return false; }

    virtual void draw(NVGcontext* vg) {} // Override for custom drawing

    NVGCSSElement* element() { return element_; }
    const std::string& id() const { return id_; }

    // Check if widget should be visible based on inline style
    bool isVisible() const;

private:
    NVGCSSRenderer* renderer_;
    NVGCSSElement* element_;
    std::string id_;
    float x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    ClickCallback click_callback_;
    HoverCallback hover_callback_;
    bool hovered_ = false;
};

} // namespace flexui
