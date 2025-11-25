#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <vector>
#include <functional>

namespace flexui {

struct TabListStyle {
    float width = 200;
    float tabHeight = 48;
    NVGcolor bgColor = nvgRGB(255, 255, 255);
    NVGcolor activeBg = nvgRGB(227, 242, 253);
    NVGcolor hoverBg = nvgRGB(245, 245, 245);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    NVGcolor activeTextColor = nvgRGB(25, 118, 210);
    float fontSize = 14;
};

class TabList : public Widget {
public:
    using ChangeCallback = std::function<void(int)>;

    TabList(NVGCSSRenderer* renderer, const std::string& id,
            const std::vector<std::string>& tabs,
            const TabListStyle& style = TabListStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float mx, float my) override;
    bool handleMouseMove(float mx, float my) override;

    void setActiveTab(int index) { active_tab_ = index; }
    int getActiveTab() const { return active_tab_; }
    void setChangeCallback(ChangeCallback callback) { change_callback_ = callback; }

private:
    std::vector<std::string> tabs_;
    int active_tab_ = 0;
    int hover_tab_ = -1;
    TabListStyle style_;
    ChangeCallback change_callback_;
};

} // namespace flexui
