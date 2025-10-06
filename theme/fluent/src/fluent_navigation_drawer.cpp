#include <nanogui/fluent_navigation_drawer.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/fluent_easing.h>
#include <nanogui/opengl.h>
#include <chrono>
NAMESPACE_BEGIN(nanogui)

FluentNavigationDrawer::FluentNavigationDrawer(Widget *parent, Type type)
    : Widget(parent), m_type(type), m_selected_index(-1), 
      m_open(type == Type::Permanent), m_drawer_width(360),
      m_animation_progress(type == Type::Permanent ? 1.0f : 0.0f) {
}

void FluentNavigationDrawer::add_item(int icon, const std::string &label, 
                                        const std::function<void()> &callback) {
    Item item(icon, label);
    item.callback = callback;
    m_items.emplace_back(item);
}

void FluentNavigationDrawer::add_divider() {
    m_items.emplace_back(DrawerItem::ItemType::Divider);
}

void FluentNavigationDrawer::add_header(const std::string &text) {
    m_items.emplace_back(DrawerItem::ItemType::Header, text);
}

void FluentNavigationDrawer::set_selected_index(int index) {
    if (index < 0 || index >= (int)m_items.size())
        return;
    
    // Deselect previous
    if (m_selected_index >= 0 && m_selected_index < (int)m_items.size()) {
        if (m_items[m_selected_index].type == DrawerItem::ItemType::Item)
            m_items[m_selected_index].item.selected = false;
    }
    
    // Select new
    m_selected_index = index;
    if (m_items[index].type == DrawerItem::ItemType::Item) {
        m_items[index].item.selected = true;
        
        // Call callback
        if (m_items[index].item.callback)
            m_items[index].item.callback();
    }
}

void FluentNavigationDrawer::open() {
    if (m_type == Type::Permanent || m_open)
        return;
    
    m_open = true;
    m_animation_start = std::chrono::steady_clock::now();
}

void FluentNavigationDrawer::close() {
    if (m_type == Type::Permanent || !m_open)
        return;
    
    m_open = false;
    m_animation_start = std::chrono::steady_clock::now();
}

Vector2i FluentNavigationDrawer::preferred_size_impl(NVGcontext *ctx) const {
    return Vector2i(m_drawer_width, m_parent ? m_parent->height() : 600);
}

bool FluentNavigationDrawer::mouse_button_event(const Vector2i &p, int button, 
                                                   bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    // Check if click is on scrim (outside drawer)
    if (m_type == Type::Modal && m_open) {
        if (p.x() > m_drawer_width) {
            close();
            return true;
        }
    }
    
    // Check item clicks
    int y_offset = 16;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto &drawer_item = m_items[i];
        
        if (drawer_item.type == DrawerItem::ItemType::Item) {
            if (p.y() >= y_offset && p.y() < y_offset + 56) {
                set_selected_index(i);
                
                // Close modal drawer after selection
                if (m_type == Type::Modal)
                    close();
                
                return true;
            }
            y_offset += 56;
        } else if (drawer_item.type == DrawerItem::ItemType::Divider) {
            y_offset += 17;
        } else if (drawer_item.type == DrawerItem::ItemType::Header) {
            y_offset += 40;
        }
    }
    
    return false;
}

void FluentNavigationDrawer::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Update animation
    if (m_type != Type::Permanent) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_animation_start).count();
        
        float target = m_open ? 1.0f : 0.0f;
        float progress = std::min(1.0f, elapsed / 300.0f);
        float eased = FluentEasing::ease(FluentEasing::Curve::Emphasized, progress);
        
        m_animation_progress = m_open ? eased : (1.0f - eased);
        
        if (m_animation_progress <= 0.0f && !m_open)
            return; // Don't draw if closed
    }
    
    // Draw scrim for modal drawer
    if (m_type == Type::Modal && m_animation_progress > 0.0f) {
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_parent->width(), m_parent->height());
        Color scrim = Color(0.0f, 0.0f, 0.0f, 0.4f * m_animation_progress);
        nvgFillColor(ctx, scrim);
        nvgFill(ctx);
    }
    
    // Calculate drawer position
    float drawer_x = m_pos.x() - m_drawer_width * (1.0f - m_animation_progress);
    
    // Drawer background
    nvgBeginPath(ctx);
    nvgRect(ctx, drawer_x, m_pos.y(), m_drawer_width, m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    // Draw items
    float y_offset = 16;
    
    for (const auto &drawer_item : m_items) {
        if (drawer_item.type == DrawerItem::ItemType::Item) {
            const auto &item = drawer_item.item;
            
            // Item background (if selected)
            if (item.selected) {
                nvgBeginPath(ctx);
                nvgRoundedRect(ctx, drawer_x + 12, y_offset, m_drawer_width - 24, 56, 28);
                nvgFillColor(ctx, Color(0.9f, 0.9f, 1.0f, 1.0f));
                nvgFill(ctx);
            }
            
            // Icon
            nvgFontSize(ctx, 24.0f);
            nvgFontFace(ctx, "icons");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, item.selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
            
            char icon_str[8];
            snprintf(icon_str, sizeof(icon_str), "%c", (char)item.icon);
            nvgText(ctx, drawer_x + 28, y_offset + 28, icon_str, nullptr);
            
            // Label
            nvgFontSize(ctx, 14.0f);
            nvgFontFace(ctx, "sans");
            nvgFillColor(ctx, item.selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
            nvgText(ctx, drawer_x + 68, y_offset + 28, item.label.c_str(), nullptr);
            
            y_offset += 56;
            
        } else if (drawer_item.type == DrawerItem::ItemType::Divider) {
            // Divider
            nvgBeginPath(ctx);
            nvgRect(ctx, drawer_x + 28, y_offset + 8, m_drawer_width - 56, 1);
            nvgFillColor(ctx, Color(0.7f, 0.7f, 0.7f, 1.0f));
            nvgFill(ctx);
            
            y_offset += 17;
            
        } else if (drawer_item.type == DrawerItem::ItemType::Header) {
            // Section header
            nvgFontSize(ctx, 11.0f);
            nvgFontFace(ctx, "sans-bold");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, theme->on_surface_color());
            nvgText(ctx, drawer_x + 28, y_offset + 20, drawer_item.header_text.c_str(), nullptr);
            
            y_offset += 40;
        }
    }
}

NAMESPACE_END(nanogui)
