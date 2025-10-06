#include <nanogui/fluent_time_picker.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/fluent_button.h>
#include <nanogui/opengl.h>
#include <cmath>
#include <ctime>

NAMESPACE_BEGIN(nanogui)

FluentTimePicker::FluentTimePicker(Widget *parent, Format format)
    : FluentDialog(parent, "Select Time"), m_format(format), 
      m_mode(Mode::Hour), m_is_pm(false) {
    
    // Get current time
    std::time_t now = std::time(nullptr);
    std::tm *local = std::localtime(&now);
    m_hour = local->tm_hour;
    m_minute = local->tm_min;
    
    if (m_format == Format::Hour12) {
        m_is_pm = m_hour >= 12;
        if (m_hour > 12) m_hour -= 12;
        if (m_hour == 0) m_hour = 12;
    }
    
    build_ui();
}

void FluentTimePicker::set_time(int hour, int minute) {
    m_hour = hour;
    m_minute = minute;
    
    if (m_format == Format::Hour12) {
        m_is_pm = hour >= 12;
        if (hour > 12) m_hour = hour - 12;
        if (hour == 0) m_hour = 12;
    }
    
    build_ui();
}

std::pair<int, int> FluentTimePicker::get_time() const {
    int hour = m_hour;
    
    if (m_format == Format::Hour12) {
        if (m_is_pm && hour != 12) hour += 12;
        if (!m_is_pm && hour == 12) hour = 0;
    }
    
    return std::make_pair(hour, m_minute);
}

void FluentTimePicker::build_ui() {
    // Clear existing content
    while (child_count() > 0) {
        remove_child(0);
    }
    
    auto container = new Widget(this);
    container->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 16, 16));
    
    // Time display
    auto time_display = new Widget(container);
    time_display->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 0));
    
    char time_str[16];
    if (m_format == Format::Hour12) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d %s", 
                 m_hour, m_minute, m_is_pm ? "PM" : "AM");
    } else {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", m_hour, m_minute);
    }
    
    auto time_label = new Label(time_display, time_str);
    time_label->set_font_size(36);
    
    // AM/PM toggle for 12-hour format
    if (m_format == Format::Hour12) {
        auto am_pm = new Widget(container);
        am_pm->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 8, 0));
        
        auto am_btn = new FluentButton(am_pm, "AM", 0,
                                         m_is_pm ? FluentButton::Style::Outlined : FluentButton::Style::Filled);
        am_btn->set_callback([this]() {
            m_is_pm = false;
            build_ui();
        });
        
        auto pm_btn = new FluentButton(am_pm, "PM", 0,
                                         m_is_pm ? FluentButton::Style::Filled : FluentButton::Style::Outlined);
        pm_btn->set_callback([this]() {
            m_is_pm = true;
            build_ui();
        });
    }
    
    // Clock face (simplified - would draw actual clock in production)
    auto clock_container = new Widget(container);
    clock_container->set_fixed_size(Vector2i(280, 280));
    
    // Mode toggle
    auto mode_toggle = new Widget(container);
    mode_toggle->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 8, 0));
    
    auto hour_btn = new FluentButton(mode_toggle, "Hour", 0,
                                       m_mode == Mode::Hour ? FluentButton::Style::Filled : FluentButton::Style::Text);
    hour_btn->set_callback([this]() {
        m_mode = Mode::Hour;
        build_ui();
    });
    
    auto minute_btn = new FluentButton(mode_toggle, "Minute", 0,
                                         m_mode == Mode::Minute ? FluentButton::Style::Filled : FluentButton::Style::Text);
    minute_btn->set_callback([this]() {
        m_mode = Mode::Minute;
        build_ui();
    });
    
    // Number buttons
    auto numbers = new Widget(container);
    numbers->set_layout(new GridLayout(Orientation::Horizontal, 4, Alignment::Fill, 4, 4));
    
    if (m_mode == Mode::Hour) {
        int max_hour = (m_format == Format::Hour12) ? 12 : 23;
        int start_hour = (m_format == Format::Hour12) ? 1 : 0;
        
        for (int h = start_hour; h <= max_hour; ++h) {
            auto btn = new FluentButton(numbers, std::to_string(h), 0, FluentButton::Style::Text);
            btn->set_callback([this, h]() {
                m_hour = h;
                m_mode = Mode::Minute;
                build_ui();
            });
        }
    } else {
        for (int m = 0; m < 60; m += 5) {
            auto btn = new FluentButton(numbers, std::to_string(m), 0, FluentButton::Style::Text);
            btn->set_callback([this, m]() {
                m_minute = m;
                build_ui();
            });
        }
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
        if (m_callback) {
            auto [hour, minute] = get_time();
            m_callback(hour, minute);
        }
        dispose();
    });
}

void FluentTimePicker::draw_clock(NVGcontext *ctx) {
    // This would draw the actual clock face with numbers
    // For now, we use button-based selection
}

NAMESPACE_END(nanogui)
