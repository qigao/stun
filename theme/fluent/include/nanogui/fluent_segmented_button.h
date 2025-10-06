#pragma once

#include <nanogui/widget.h>
#include <string>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Segmented Button
 * 
 * Segmented buttons help people select options, switch views, or sort elements.
 * Supports single or multi-select modes.
 */
class NANOGUI_EXPORT FluentSegmentedButton : public Widget {
public:
    struct Segment {
        std::string label;
        int icon;
        bool selected;
        
        Segment(const std::string &text, int icon_id = 0) 
            : label(text), icon(icon_id), selected(false) {}
    };
    
    enum class SelectMode {
        Single,    // Only one segment can be selected
        Multi      // Multiple segments can be selected
    };
    
    FluentSegmentedButton(Widget *parent, SelectMode mode = SelectMode::Single);
    
    /// Add a segment
    void add_segment(const std::string &label, int icon = 0);
    
    /// Get/set selected segments
    std::vector<int> selected_indices() const;
    void set_selected(int index, bool selected);
    
    /// Select mode
    SelectMode mode() const { return m_mode; }
    void set_mode(SelectMode mode) { m_mode = mode; }
    
    /// Callback when selection changes
    std::function<void(const std::vector<int>&)> callback() const { return m_callback; }
    void set_callback(const std::function<void(const std::vector<int>&)> &callback) { 
        m_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    std::vector<Segment> m_segments;
    SelectMode m_mode;
    std::function<void(const std::vector<int>&)> m_callback;
};

NAMESPACE_END(nanogui)
