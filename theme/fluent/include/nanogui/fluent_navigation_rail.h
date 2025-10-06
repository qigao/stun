#pragma once

#include <nanogui/widget.h>
#include <string>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Navigation Rail
 * 
 * Vertical navigation for medium to large screens.
 * Provides access to primary destinations in an app.
 */
class NANOGUI_EXPORT FluentNavigationRail : public Widget {
public:
    struct Destination {
        int icon;
        std::string label;
        bool selected;
        
        Destination(int icon_id, const std::string &text) 
            : icon(icon_id), label(text), selected(false) {}
    };
    
    FluentNavigationRail(Widget *parent);
    
    /// Add a navigation destination
    void add_destination(int icon, const std::string &label);
    
    /// Get/set selected destination index
    int selected_index() const { return m_selected_index; }
    void set_selected_index(int index);
    
    /// Callback when destination is selected
    std::function<void(int)> callback() const { return m_callback; }
    void set_callback(const std::function<void(int)> &callback) { m_callback = callback; }
    
    /// Show/hide labels
    bool show_labels() const { return m_show_labels; }
    void set_show_labels(bool show) { m_show_labels = show; }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    std::vector<Destination> m_destinations;
    int m_selected_index;
    bool m_show_labels;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
