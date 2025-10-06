#pragma once

#include <nanogui/widget.h>
#include <string>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Bottom Navigation
 * 
 * Bottom navigation bars allow movement between primary destinations in an app.
 * Optimized for mobile and small screens.
 */
class NANOGUI_EXPORT FluentBottomNavigation : public Widget {
public:
    struct Item {
        int icon;
        std::string label;
        bool selected;
        
        Item(int icon_id, const std::string &text) 
            : icon(icon_id), label(text), selected(false) {}
    };
    
    FluentBottomNavigation(Widget *parent);
    
    /// Add navigation item (3-5 items recommended)
    void add_item(int icon, const std::string &label);
    
    /// Get/set selected item index
    int selected_index() const { return m_selected_index; }
    void set_selected_index(int index);
    
    /// Callback when item is selected
    std::function<void(int)> callback() const { return m_callback; }
    void set_callback(const std::function<void(int)> &callback) { m_callback = callback; }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    std::vector<Item> m_items;
    int m_selected_index;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
