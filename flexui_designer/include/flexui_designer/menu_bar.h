/*
 * flexUI Designer - Menu Bar
 *
 * Top menu bar with File, Edit, View, Arrange menus.
 */

#pragma once

#include <meta_editor/view/panel.h>
#include <functional>
#include <string>
#include <vector>

namespace flexui_designer {

struct MenuItem {
    std::string label;
    std::string shortcut;
    std::function<void()> action;
    bool separator = false;
    bool checked = false;
    bool checkable = false;
};

struct Menu {
    std::string label;
    std::vector<MenuItem> items;
};

class MenuBar : public meta_editor::Panel {
public:
    MenuBar();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;

    void add_menu(const std::string& label, std::vector<MenuItem> items);
    void close_menu() { open_menu_ = -1; }
    bool is_menu_open() const { return open_menu_ >= 0; }

    static constexpr float HEIGHT = 28.0f;

private:
    std::vector<Menu> menus_;
    int open_menu_ = -1;
    int hovered_item_ = -1;

    void render_dropdown(flex::Renderer& renderer, const Menu& menu, float x);
    float menu_x(int index) const;
    float menu_width(const std::string& label) const;
    
    static constexpr float MENU_PADDING = 16.0f;
    static constexpr float ITEM_HEIGHT = 26.0f;
    static constexpr float DROPDOWN_WIDTH = 200.0f;
};

} // namespace flexui_designer
