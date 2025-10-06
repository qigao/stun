/*
    nanogui/m3_breadcrumb.h -- M3 Breadcrumb Navigation

    Based on: https://m3.material.io/components/breadcrumbs
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3Breadcrumb : public Widget {
public:
    struct Item {
        std::string label;
        int icon;
        std::function<void()> callback;
        
        Item(const std::string &l, int i = 0, std::function<void()> cb = nullptr)
            : label(l), icon(i), callback(cb) {}
    };

    M3Breadcrumb(Widget *parent);

    void set_items(const std::vector<Item> &items);
    void add_item(const std::string &label, int icon = 0, std::function<void()> callback = nullptr);
    void clear_items() { m_items.clear(); }

    void set_separator(int icon) { m_separator_icon = icon; }

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;
    int item_at_position(const Vector2i &p) const;

    std::vector<Item> m_items;
    std::vector<float> m_item_positions;
    int m_separator_icon = 0xf054; // Right chevron
    int m_hover_index = -1;
};

NAMESPACE_END(nanogui)
