#pragma once
#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <vector>
#include <functional>

namespace flexui {

struct MenuItem {
    std::string text;
    std::function<void()> callback;
    bool enabled = true;
};

struct MenuStyle {
    NVGcolor backgroundColor = nvgRGBA(50, 50, 50, 240);
    NVGcolor itemHoverColor = nvgRGBA(70, 70, 70, 240);
    NVGcolor textColor = nvgRGBA(255, 255, 255, 255);
    NVGcolor disabledTextColor = nvgRGBA(150, 150, 150, 255);
    float fontSize = 16.0f;
    float padding = 10.0f;
    float itemHeight = 30.0f;
    float borderRadius = 4.0f;
};

class Menu : public Widget {
public:
    Menu(cssboxRenderer* renderer, const std::string& id,
         const std::vector<MenuItem>& items = std::vector<MenuItem>(),
         const MenuStyle& style = MenuStyle());

    void addItem(const MenuItem& item);
    void show();
    void hide();
    bool isVisible() const { return visible_; }

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float mx, float my) override;
    bool handleMouseMove(float mx, float my) override;

private:
    std::vector<MenuItem> items_;
    MenuStyle style_;
    bool visible_ = false;
    int hovered_index_ = -1;
};

} // namespace flexui
