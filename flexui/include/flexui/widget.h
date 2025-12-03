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

    // Mouse event handling (final - do NOT override)
    virtual bool handleClick(float x, float y) final;
    void handleHover(float x, float y);

    // Override these for widget-specific behavior
    virtual bool onClicked() { return click_callback_ ? click_callback_(this) : false; }

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
    virtual bool handleScroll(float x, float y, float deltaX, float deltaY) { return false; }

    virtual void draw(NVGcontext* vg) {} // Override for custom drawing

    NVGCSSElement* element() { return element_; }
    const NVGCSSElement* element() const { return element_; }
    const std::string& id() const { return id_; }

    // Check if widget should be visible based on inline style
    bool isVisible() const;

    // CSS style helpers - reduce duplication in widget draw() methods
    NVGcolor cssBackground(const NVGcolor& fallback) const;
    float cssBorderRadius(float fallback) const;
    float cssBorderWidth(float fallback) const;
    float cssFontSize(float fallback) const;
    NVGcolor cssColor(const NVGcolor& fallback) const;
    NVGcolor cssBorderColor(const NVGcolor& fallback) const;
    float cssPaddingLeft(float fallback) const;

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
