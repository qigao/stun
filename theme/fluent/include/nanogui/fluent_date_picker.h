#pragma once

#include <nanogui/fluent_dialog.h>
#include <tuple>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Date Picker
 * 
 * Calendar-based date selection dialog.
 * Follows Fluent Design 3 specifications.
 */
class NANOGUI_EXPORT FluentDatePicker : public FluentDialog {
public:
    FluentDatePicker(Widget *parent);
    
    /// Set selected date
    void set_date(int year, int month, int day);
    
    /// Get selected date
    std::tuple<int, int, int> get_date() const;
    
    /// Callback when date is selected
    std::function<void(int year, int month, int day)> callback() const { return m_callback; }
    void set_callback(const std::function<void(int year, int month, int day)> &callback) { 
        m_callback = callback; 
    }
    
    /// Callback when cancelled
    std::function<void()> cancel_callback() const { return m_cancel_callback; }
    void set_cancel_callback(const std::function<void()> &callback) { 
        m_cancel_callback = callback; 
    }
    
protected:
    void build_ui();
    std::string get_month_name(int month) const;
    int get_days_in_month(int year, int month) const;
    int get_first_day_of_month(int year, int month) const;
    
    int m_year;
    int m_month;
    int m_day;
    int m_selected_year;
    int m_selected_month;
    int m_selected_day;
    
    std::function<void(int, int, int)> m_callback;
    std::function<void()> m_cancel_callback;
};

NAMESPACE_END(nanogui)
