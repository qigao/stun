/*
    nanogui/m3_navigation_drawer.h -- M3 Navigation Drawer

    Based on: https://m3.material.io/components/navigation-drawer
*/

#pragma once

#include <nanogui/popup.h>
#include <nanogui/m3_theme.h>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3NavigationDrawer : public Popup {
public:
    enum class Type {
        Standard,  ///< Standard drawer (360dp wide)
        Modal      ///< Modal drawer with scrim
    };

    struct Item {
        int icon;
        std::string label;
        bool selected;
        std::function<void()> callback;
        
        Item(int i, const std::string &l, bool s = false, std::function<void()> cb = nullptr)
            : icon(i), label(l), selected(s), callback(cb) {}
    };

    M3NavigationDrawer(Widget *parent, Type type = Type::Modal);

    void add_item(int icon, const std::string &label, bool selected = false,
                  std::function<void()> callback = nullptr);
    void set_items(const std::vector<Item> &items) { m_items = items; }
    const std::vector<Item> &items() const { return m_items; }

    void set_selected(int index);
    int selected() const { return m_selected_index; }

    void set_header(const std::string &title, const std::string &subtitle = "");

    void show();
    void hide();

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;
    int item_at_position(const Vector2i &p) const;

    Type m_type;
    std::vector<Item> m_items;
    int m_selected_index = -1;
    int m_hover_index = -1;
    std::string m_header_title;
    std::string m_header_subtitle;
};

NAMESPACE_END(nanogui)
