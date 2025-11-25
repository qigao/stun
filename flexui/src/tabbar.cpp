#include <flexui/tabbar.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

TabBar::TabBar(NVGCSSRenderer* renderer, const std::string& id,
               const std::vector<std::string>& tabs, const TabBarStyle& style)
    : Widget(renderer, id, "tabbar"), tabs_(tabs), style_(style) {
}

void TabBar::draw(NVGcontext* vg) {
    if (tabs_.empty()) return;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    float tabWidth = w / tabs_.size();

    // Background
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, style_.bgColor);
    nvgFill(vg);

    // Draw each tab
    for (size_t i = 0; i < tabs_.size(); ++i) {
        float tx = x + i * tabWidth;
        bool isActive = (int)i == activeTab_;
        bool isHovered = (int)i == hoveredTab_;

        // Tab background
        if (isActive) {
            nvgBeginPath(vg);
            nvgRect(vg, tx, y, tabWidth, h);
            nvgFillColor(vg, style_.activeTabColor);
            nvgFill(vg);
        } else if (isHovered) {
            nvgBeginPath(vg);
            nvgRect(vg, tx, y, tabWidth, h);
            nvgFillColor(vg, style_.hoverTabColor);
            nvgFill(vg);
        }

        // Tab text
        nvgFontSize(vg, style_.fontSize);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, isActive ? style_.activeTextColor : style_.textColor);
        nvgText(vg, tx + tabWidth / 2, y + h / 2, tabs_[i].c_str(), nullptr);

        // Active indicator (bottom line)
        if (isActive) {
            nvgBeginPath(vg);
            nvgRect(vg, tx, y + h - style_.indicatorHeight, tabWidth, style_.indicatorHeight);
            nvgFillColor(vg, style_.indicatorColor);
            nvgFill(vg);
        }
    }
}

bool TabBar::handleMouseDown(float mx, float my) {
    int tab = getTabAtPosition(mx, my);
    if (tab >= 0) {
        setActiveTab(tab);
        return true;
    }
    return false;
}

int TabBar::getTabAtPosition(float mx, float my) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (mx < x || mx > x + w || my < y || my > y + h) {
        return -1;
    }

    float tabWidth = w / tabs_.size();
    int index = (int)((mx - x) / tabWidth);

    if (index >= 0 && index < (int)tabs_.size()) {
        return index;
    }
    return -1;
}

void TabBar::setActiveTab(int index) {
    if (index >= 0 && index < (int)tabs_.size() && index != activeTab_) {
        activeTab_ = index;
        if (callback_) {
            callback_(index, tabs_[index]);
        }
    }
}

void TabBar::addTab(const std::string& name) {
    tabs_.push_back(name);
}

void TabBar::removeTab(int index) {
    if (index >= 0 && index < (int)tabs_.size()) {
        tabs_.erase(tabs_.begin() + index);
        if (activeTab_ >= (int)tabs_.size()) {
            activeTab_ = std::max(0, (int)tabs_.size() - 1);
        }
    }
}

} // namespace flexui
