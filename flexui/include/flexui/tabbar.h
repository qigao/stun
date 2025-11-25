#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>
#include <string>
#include <vector>

namespace flexui {

struct TabBarStyle {
    NVGcolor bgColor = nvgRGB(245, 245, 245);
    NVGcolor activeTabColor = nvgRGB(255, 255, 255);
    NVGcolor hoverTabColor = nvgRGB(230, 230, 230);
    NVGcolor textColor = nvgRGB(100, 100, 100);
    NVGcolor activeTextColor = nvgRGB(33, 150, 243);
    NVGcolor indicatorColor = nvgRGB(33, 150, 243);
    float fontSize = 14;
    float indicatorHeight = 3;
};

class TabBar : public Widget {
public:
    using TabChangeCallback = std::function<void(int, const std::string&)>;

    TabBar(NVGCSSRenderer* renderer, const std::string& id,
           const std::vector<std::string>& tabs,
           const TabBarStyle& style = TabBarStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float x, float y) override;

    void setActiveTab(int index);
    int getActiveTab() const { return activeTab_; }
    const std::string& getActiveTabName() const { return tabs_[activeTab_]; }

    void setTabChangeCallback(TabChangeCallback cb) { callback_ = cb; }
    void setTabBarStyle(const TabBarStyle& style) { style_ = style; }

    void addTab(const std::string& name);
    void removeTab(int index);

private:
    std::vector<std::string> tabs_;
    TabBarStyle style_;
    int activeTab_ = 0;
    int hoveredTab_ = -1;
    TabChangeCallback callback_;

    int getTabAtPosition(float mx, float my);
};

} // namespace flexui
