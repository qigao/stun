#pragma once

#include <nanogui/widget.h>
#include <chrono>
#include <string>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Navigation Drawer
 * 
 * Side navigation panel for accessing destinations and app functionality.
 * Supports modal, standard, and permanent variants.
 */
class NANOGUI_EXPORT FluentNavigationDrawer : public Widget {
public:
    enum class Type {
        Modal,      ///< Overlays content, dismissible
        Standard,   ///< Side-by-side with content, dismissible
        Permanent   ///< Always visible, not dismissible
    };
    
    struct Item {
        int icon;
        std::string label;
        bool selected;
        std::function<void()> callback;
        
        Item(int icon_id, const std::string &text) 
            : icon(icon_id), label(text), selected(false) {}
    };
    
    FluentNavigationDrawer(Widget *parent, Type type = Type::Modal);
    
    /// Add navigation item
    void add_item(int icon, const std::string &label, const std::function<void()> &callback = nullptr);
    
    /// Add divider
    void add_divider();
    
    /// Add section header
    void add_header(const std::string &text);
    
    /// Get/set selected item index
    int selected_index() const { return m_selected_index; }
    void set_selected_index(int index);
    
    /// Open/close drawer
    void open();
    void close();
    bool is_open() const { return m_open; }
    
    /// Drawer type
    Type type() const { return m_type; }
    void set_type(Type type) { m_type = type; }
    
    /// Drawer width
    int drawer_width() const { return m_drawer_width; }
    void set_drawer_width(int width) { m_drawer_width = width; }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    struct DrawerItem {
        enum class ItemType { Item, Divider, Header };
        ItemType type;
        Item item;
        std::string header_text;
        
        DrawerItem(const Item &i) : type(ItemType::Item), item(i) {}
        DrawerItem(ItemType t, const std::string &text = "") 
            : type(t), item(0, ""), header_text(text) {}
    };
    
    std::vector<DrawerItem> m_items;
    Type m_type;
    int m_selected_index;
    bool m_open;
    int m_drawer_width;
    float m_animation_progress;
    std::chrono::steady_clock::time_point m_animation_start;
};

NAMESPACE_END(nanogui)
