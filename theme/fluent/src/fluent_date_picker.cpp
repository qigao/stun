#include <nanogui/fluent_date_picker.h>
#include <nanogui/icons.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/fluent_button.h>
#include <nanogui/fluent_icon_button.h>
#include <nanogui/opengl.h>
#include <ctime>

NAMESPACE_BEGIN(nanogui)

FluentDatePicker::FluentDatePicker(Widget *parent)
    : FluentDialog(parent, "Select Date") {
    
    // Get current date
    std::time_t now = std::time(nullptr);
    std::tm *local = std::localtime(&now);
    m_year = local->tm_year + 1900;
    m_month = local->tm_mon + 1;
    m_day = local->tm_mday;
    m_selected_year = m_year;
    m_selected_month = m_month;
    m_selected_day = m_day;
    
    build_ui();
}

void FluentDatePicker::set_date(int year, int month, int day) {
    m_selected_year = year;
    m_selected_month = month;
    m_selected_day = day;
    build_ui();
}

std::tuple<int, int, int> FluentDatePicker::get_date() const {
    return std::make_tuple(m_selected_year, m_selected_month, m_selected_day);
}

void FluentDatePicker::build_ui() {
    // Clear existing content
    while (child_count() > 0) {
        remove_child(0);
    }
    
    auto container = new Widget(this);
    container->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 16, 16));
    
    // Month/Year header
    auto header = new Widget(container);
    header->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 0));
    
    auto prev_month = new FluentIconButton(header, FA_CHEVRON_LEFT);
    prev_month->set_callback([this]() {
        if (m_month == 1) {
            m_month = 12;
            m_year--;
        } else {
            m_month--;
        }
        build_ui();
    });
    
    auto month_year = new Label(header, get_month_name(m_month) + " " + std::to_string(m_year));
    month_year->set_font_size(18);
    
    auto next_month = new FluentIconButton(header, FA_CHEVRON_RIGHT);
    next_month->set_callback([this]() {
        if (m_month == 12) {
            m_month = 1;
            m_year++;
        } else {
            m_month++;
        }
        build_ui();
    });
    
    // Day names
    auto day_names = new Widget(container);
    day_names->set_layout(new GridLayout(Orientation::Horizontal, 7, Alignment::Fill, 4, 4));
    
    const char* days[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    for (int i = 0; i < 7; ++i) {
        auto label = new Label(day_names, days[i]);
        label->set_font_size(12);
    }
    
    // Calendar grid
    auto calendar = new Widget(container);
    calendar->set_layout(new GridLayout(Orientation::Horizontal, 7, Alignment::Fill, 4, 4));
    
    int first_day = get_first_day_of_month(m_year, m_month);
    int days_in_month = get_days_in_month(m_year, m_month);
    
    // Empty cells before first day
    for (int i = 0; i < first_day; ++i) {
        new Label(calendar, "");
    }
    
    // Day buttons
    for (int day = 1; day <= days_in_month; ++day) {
        auto day_btn = new FluentButton(calendar, std::to_string(day), 0,
                                          FluentButton::Style::Text);
        
        // Highlight selected day
        if (day == m_selected_day && m_month == m_selected_month && m_year == m_selected_year) {
            day_btn->set_style(FluentButton::Style::Filled);
        }
        
        day_btn->set_callback([this, day]() {
            m_selected_day = day;
            m_selected_month = m_month;
            m_selected_year = m_year;
            build_ui();
        });
    }
    
    // Action buttons
    auto actions = new Widget(container);
    actions->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 8, 0));
    
    new Widget(actions); // Spacer
    
    auto cancel = new FluentButton(actions, "Cancel", 0, FluentButton::Style::Text);
    cancel->set_callback([this]() {
        if (m_cancel_callback)
            m_cancel_callback();
        dispose();
    });
    
    auto ok = new FluentButton(actions, "OK", 0, FluentButton::Style::Text);
    ok->set_callback([this]() {
        if (m_callback)
            m_callback(m_selected_year, m_selected_month, m_selected_day);
        dispose();
    });
}

std::string FluentDatePicker::get_month_name(int month) const {
    const char* months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    return months[month - 1];
}

int FluentDatePicker::get_days_in_month(int year, int month) const {
    if (month == 2) {
        // Leap year check
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            return 29;
        return 28;
    }
    
    if (month == 4 || month == 6 || month == 9 || month == 11)
        return 30;
    
    return 31;
}

int FluentDatePicker::get_first_day_of_month(int year, int month) const {
    // Zeller's congruence algorithm
    int q = 1; // First day of month
    int m = month;
    int y = year;
    
    if (m < 3) {
        m += 12;
        y--;
    }
    
    int k = y % 100;
    int j = y / 100;
    
    int h = (q + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)) % 7;
    
    // Convert to Sunday = 0
    return (h + 6) % 7;
}

NAMESPACE_END(nanogui)
