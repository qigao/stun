#pragma once

#include <nanogui/widget.h>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Carousel
 * 
 * Displays a collection of items in a slideshow format.
 * Supports manual navigation and auto-play.
 */
class NANOGUI_EXPORT FluentCarousel : public Widget {
public:
    struct Item {
        std::string title;
        std::string description;
        int image_id = -1; // NanoVG image ID
    };
    
    FluentCarousel(Widget *parent);
    
    /// Add carousel item
    void add_item(const std::string &title, const std::string &description = "");
    
    /// Current item index
    int current_index() const { return m_current_index; }
    void set_current_index(int index);
    
    /// Navigate
    void next();
    void previous();
    
    /// Auto-play
    bool auto_play() const { return m_auto_play; }
    void set_auto_play(bool enabled) { m_auto_play = enabled; }
    
    int auto_play_interval() const { return m_auto_play_interval; }
    void set_auto_play_interval(int ms) { m_auto_play_interval = ms; }
    
    /// Callback when item changes
    std::function<void(int)> callback() const { return m_callback; }
    void set_callback(const std::function<void(int)> &callback) { 
        m_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    std::vector<Item> m_items;
    int m_current_index;
    int m_previous_index;
    bool m_auto_play;
    int m_auto_play_interval;
    float m_animation_progress;
    std::chrono::steady_clock::time_point m_animation_start;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
