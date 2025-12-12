#include <flexui/widget.h>
#include <cssbox_internal.h>
#include <cssbox_types.h>
#include <fmtlog.h>
#include <sstream>

namespace flexui {

Widget::Widget(cssboxRenderer* renderer, const std::string& id, const std::string& tag)
    : renderer_(renderer), id_(id) {
    element_ = cssboxCreateElement(renderer_, id.c_str(), tag.c_str());
}

void Widget::setClass(const std::string& className) {
    // Split className on whitespace to support multiple classes
    // e.g., "btn num" becomes ["btn", "num"]
    std::istringstream iss(className);
    std::string singleClass;
    while (iss >> singleClass) {
        if (!singleClass.empty()) {
            cssboxAddClass(element_, singleClass.c_str());
        }
    }
    renderer_->style_dirty = true;
    renderer_->layout_dirty = true;
}

void Widget::addClass(const std::string& className) {
    cssboxAddClass(element_, className.c_str());
    renderer_->style_dirty = true;
    renderer_->layout_dirty = true;
}

void Widget::removeClass(const std::string& className) {
    cssboxRemoveClass(element_, className.c_str());
    renderer_->style_dirty = true;
    renderer_->layout_dirty = true;
}

void Widget::setPosition(float x, float y) {
    x_ = x;
    y_ = y;
    cssboxSetInlineStyle(renderer_, element_, "left", (std::to_string((int)x) + "px").c_str());
    cssboxSetInlineStyle(renderer_, element_, "top", (std::to_string((int)y) + "px").c_str());
}

void Widget::setSize(float w, float h) {
    w_ = w;
    h_ = h;
    cssboxSetInlineStyle(renderer_, element_, "width", (std::to_string((int)w) + "px").c_str());
    cssboxSetInlineStyle(renderer_, element_, "height", (std::to_string((int)h) + "px").c_str());
}

void Widget::setText(const std::string& text) {
    cssboxSetText(element_, text.c_str());
}

void Widget::addChild(Widget* child) {
    cssboxAppendChild(renderer_, element_, child->element());
}

void Widget::setInlineStyle(const std::string& property, const std::string& value) {
    cssboxSetInlineStyle(renderer_, element_, property.c_str(), value.c_str());
}

bool Widget::handleClick(float x, float y) {
    // Spatial index already guarantees mouse is within visual bounds
    // Set active pseudo-state
    cssboxSetPseudoStateEx(renderer_, element_, "active", 1);

    // Call virtual onClicked() for widget-specific logic
    return onClicked();
}

bool Widget::handleHover(float x, float y) {
    // Calculate visual position accounting for scroll offset
    float scroll_x = 0.0f, scroll_y = 0.0f;
    cssboxElement* parent = cssboxGetParent(renderer_, element_);
    while (parent) {
        scroll_x += parent->scroll_x;
        scroll_y += parent->scroll_y;
        parent = cssboxGetParent(renderer_, parent);
    }

    float ex = element_->layout.x - scroll_x;
    float ey = element_->layout.y - scroll_y;
    float ew = element_->layout.width;
    float eh = element_->layout.height;

    bool inside = (x >= ex && x <= ex + ew && y >= ey && y <= ey + eh);

    if (inside != hovered_) {
        hovered_ = inside;
        cssboxSetPseudoStateEx(renderer_, element_, "hover", inside ? 1 : 0);
        if (hover_callback_) {
            hover_callback_(this, inside);
        }
        return true;  // Hover state changed
    }
    return false;  // No change
}

void Widget::setStroke(const std::string& color, float width) {
    setInlineStyle("stroke", color);
    setInlineStyle("stroke-width", std::to_string((int)width) + "px");
}

void Widget::setStrokeDash(const std::string& pattern) {
    setInlineStyle("stroke-dasharray", pattern);
}

void Widget::setStrokeLineCap(const std::string& cap) {
    setInlineStyle("stroke-linecap", cap);
}

void Widget::setStrokeLineJoin(const std::string& join) {
    setInlineStyle("stroke-linejoin", join);
}

void Widget::setFill(const std::string& color) {
    setInlineStyle("fill", color);
}

void Widget::addPathPoint(float x, float y) {
    element_->stroke_points.push_back({x, y});
}

void Widget::clearPath() {
    element_->stroke_points.clear();
}

bool Widget::isVisible() const {
    // Single source of truth: computed style (inline_style is merged during style computation)
    if (element_->style.display == cssbox::Display::NONE) {
        return false;
    }
    return element_->visible;
}

NVGcolor Widget::cssBackground(const NVGcolor& fallback) const {
    if (element_->style.background.type == cssbox::BackgroundType::COLOR) {
        return element_->style.background.color;
    }
    return fallback;
}

float Widget::cssBorderRadius(float fallback) const {
    return element_->style.border.radius[0] > 0 ? element_->style.border.radius[0] : fallback;
}

float Widget::cssBorderWidth(float fallback) const {
    return element_->style.border.width[0] > 0 ? element_->style.border.width[0] : fallback;
}

float Widget::cssFontSize(float fallback) const {
    return element_->style.font_size > 0 ? element_->style.font_size : fallback;
}

NVGcolor Widget::cssColor(const NVGcolor& fallback) const {
    return element_->style.color.a > 0 ? element_->style.color : fallback;
}

NVGcolor Widget::cssBorderColor(const NVGcolor& fallback) const {
    return element_->style.border.color[0].a > 0 ? element_->style.border.color[0] : fallback;
}

float Widget::cssPaddingLeft(float fallback) const {
    return element_->style.padding[3].value > 0 ? element_->style.padding[3].value : fallback;
}

const char* Widget::cssFontFamily(const char* fallback) const {
    return !element_->style.font_family.empty() ? element_->style.font_family.c_str() : fallback;
}

void Widget::getVisualPosition(float& vx, float& vy) const {
    float scroll_x = 0.0f, scroll_y = 0.0f;
    cssboxElement* parent = cssboxGetParent(renderer_, element_);
    while (parent) {
        scroll_x += parent->scroll_x;
        scroll_y += parent->scroll_y;
        parent = cssboxGetParent(renderer_, parent);
    }
    vx = element_->layout.x - scroll_x;
    vy = element_->layout.y - scroll_y;
}

} // namespace flexui
