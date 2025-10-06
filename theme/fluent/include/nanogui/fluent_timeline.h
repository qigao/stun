#pragma once

#include <nanogui/widget.h>
#include <vector>
#include <string>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Timeline
 * 
 * Displays events in chronological order.
 * Supports vertical and horizontal orientations.
 */
class NANOGUI_EXPORT FluentTimeline : public Widget {
public:
    enum class Orientation {
        Vertical,
        Horizontal
    };
    
    struct Event {
        std::string title;
        std::string description;
        std::string time;
        int icon;
        
        Event(const std::string &t, const std::string &d, 
              const std::string &tm, int i = 0)
            : title(t), description(d), time(tm), icon(i) {}
    };
    
    FluentTimeline(Widget *parent, Orientation orientation = Orientation::Vertical);
    
    /// Add event
    void add_event(const std::string &title, const std::string &description = "",
                   const std::string &time = "", int icon = 0);
    
    /// Clear all events
    void clear_events();
    
    /// Orientation
    Orientation orientation() const { return m_orientation; }
    void set_orientation(Orientation orientation) { m_orientation = orientation; }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    
protected:
    std::vector<Event> m_events;
    Orientation m_orientation;
};

NAMESPACE_END(nanogui)
