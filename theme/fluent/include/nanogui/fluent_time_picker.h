#pragma once

#include <nanogui/fluent_dialog.h>
#include <utility>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Time Picker
 * 
 * Clock-based time selection dialog.
 * Supports 12-hour and 24-hour formats.
 */
class NANOGUI_EXPORT FluentTimePicker : public FluentDialog {
public:
    enum class Format {
        Hour12,  ///< 12-hour format with AM/PM
        Hour24   ///< 24-hour format
    };
    
    FluentTimePicker(Widget *parent, Format format = Format::Hour12);
    
    /// Set selected time
    void set_time(int hour, int minute);
    
    /// Get selected time
    std::pair<int, int> get_time() const;
    
    /// Time format
    Format format() const { return m_format; }
    void set_format(Format format) { m_format = format; build_ui(); }
    
    /// Callback when time is selected
    std::function<void(int hour, int minute)> callback() const { return m_callback; }
    void set_callback(const std::function<void(int hour, int minute)> &callback) { 
        m_callback = callback; 
    }
    
    /// Callback when cancelled
    std::function<void()> cancel_callback() const { return m_cancel_callback; }
    void set_cancel_callback(const std::function<void()> &callback) { 
        m_cancel_callback = callback; 
    }
    
protected:
    void build_ui();
    void draw_clock(NVGcontext *ctx);
    
    enum class Mode { Hour, Minute };
    
    Format m_format;
    Mode m_mode;
    int m_hour;
    int m_minute;
    bool m_is_pm;
    
    std::function<void(int, int)> m_callback;
    std::function<void()> m_cancel_callback;
};

NAMESPACE_END(nanogui)
