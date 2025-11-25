#include <flexui/widget.h>
#include <nanovg_css_internal.h>
#include <nanovg_css_types.h>
#include <fmtlog.h>

namespace flexui {

Widget::Widget(NVGCSSRenderer* renderer, const std::string& id, const std::string& tag)
    : renderer_(renderer), id_(id) {
    element_ = nvgcssCreateElement(renderer_, id.c_str(), tag.c_str());
}

void Widget::setClass(const std::string& className) {
    nvgcssAddClass(element_, className.c_str());
}

void Widget::addClass(const std::string& className) {
    nvgcssAddClass(element_, className.c_str());
}

void Widget::removeClass(const std::string& className) {
    nvgcssRemoveClass(element_, className.c_str());
}

void Widget::setPosition(float x, float y) {
    x_ = x;
    y_ = y;
    // Set inline style for positioning
    char style[128];
    snprintf(style, sizeof(style), "left: %.0fpx; top: %.0fpx;", x, y);
    element_->inline_style["left"] = std::to_string((int)x) + "px";
    element_->inline_style["top"] = std::to_string((int)y) + "px";
}

void Widget::setSize(float w, float h) {
    w_ = w;
    h_ = h;
    element_->inline_style["width"] = std::to_string((int)w) + "px";
    element_->inline_style["height"] = std::to_string((int)h) + "px";
}

void Widget::setText(const std::string& text) {
    nvgcssSetText(element_, text.c_str());
}

void Widget::addChild(Widget* child) {
    nvgcssAppendChild(renderer_, element_, child->element());
}

void Widget::setInlineStyle(const std::string& property, const std::string& value) {
    element_->inline_style[property] = value;
}

bool Widget::handleClick(float x, float y) {
    float ex = element_->computed.x;
    float ey = element_->computed.y;
    float ew = element_->computed.width;
    float eh = element_->computed.height;
    
    if (x >= ex && x <= ex + ew && y >= ey && y <= ey + eh) {
        nvgcssSetPseudoState(element_, "active", 1);
        if (click_callback_) {
            return click_callback_(this);
        }
        return true;
    }
    return false;
}

void Widget::handleHover(float x, float y) {
    float ex = element_->computed.x;
    float ey = element_->computed.y;
    float ew = element_->computed.width;
    float eh = element_->computed.height;

    bool inside = (x >= ex && x <= ex + ew && y >= ey && y <= ey + eh);

    if (inside != hovered_) {
        hovered_ = inside;
        nvgcssSetPseudoState(element_, "hover", inside ? 1 : 0);
        if (hover_callback_) {
            hover_callback_(this, inside);
        }
    }
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

void Widget::setHandDrawn(bool enabled, float seed) {
    element_->has_stroke_salt = enabled;
    if (enabled) {
        element_->stroke_salt = seed;
    }
}

bool Widget::isVisible() const {
    // Check inline style first (for dynamic visibility control)
    auto it = element_->inline_style.find("display");
    if (it != element_->inline_style.end() && it->second == "none") {
        return false;
    }

    // Check computed style
    if (element_->style.display == nvgcss::Display::NONE) {
        return false;
    }

    // Check visible flag
    return element_->visible;
}

} // namespace flexui
