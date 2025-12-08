#include <flexui/tabbar.h>
#include <cssbox_internal.h>
#include <fmtlog.h>
#include <algorithm>

namespace flexui {

TabBar::TabBar(cssboxRenderer* renderer, const std::string& id,
               const std::vector<std::string>& tabs, const TabBarStyle& style)
    : Widget(renderer, id, "tabbar"), tabs_(tabs), style_(style) {
}

void TabBar::draw(NVGcontext* vg) {
    if (tabs_.empty()) return;

    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    float tabWidth = w / tabs_.size();

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    // Background
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, bgColor);
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
        nvgFontSize(vg, fontSize);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, isActive ? style_.activeTextColor : textColor);
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
    float x, y;
    getVisualPosition(x, y);
    float w = element()->layout.width;
    float h = element()->layout.height;

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
        updatePageVisibility();
        if (callback_) {
            callback_(index, tabs_[index]);
        }
    }
}

void TabBar::registerPage(int tabIndex, Widget* page) {
    if (tabIndex < 0 || !page) return;

    // Expand pages_ vector if needed
    if (tabIndex >= (int)pages_.size()) {
        pages_.resize(tabIndex + 1, nullptr);
    }
    pages_[tabIndex] = page;
    updatePageVisibility();
}

void TabBar::registerPages(const std::vector<Widget*>& pages) {
    pages_ = pages;
    updatePageVisibility();
}

void TabBar::updatePageVisibility() {
    logi("[TABBAR] updatePageVisibility called, activeTab_={}", activeTab_);
    for (size_t i = 0; i < pages_.size(); ++i) {
        if (pages_[i]) {
            if ((int)i == activeTab_) {
                // Active page: show with flex display
                logi("[TABBAR] Page {} '{}': SHOW (display: flex)", i, pages_[i]->id());
                pages_[i]->removeClass("page-hidden");
                pages_[i]->setInlineStyle("display", "flex");
            } else {
                // Inactive pages: hide with display none
                logi("[TABBAR] Page {} '{}': HIDE (display: none)", i, pages_[i]->id());
                pages_[i]->addClass("page-hidden");
                pages_[i]->setInlineStyle("display", "none");
            }
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
