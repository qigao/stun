/*
    src/m3_time_picker.cpp -- M3 Time Picker implementation
*/

#include <nanogui/m3_time_picker.h>
#include <nanogui/opengl.h>
#include <cmath>
#include <ctime>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

NAMESPACE_BEGIN(nanogui)

M3TimePicker::M3TimePicker(Widget *parent, bool use_24_hour)
    : Popup(parent), m_use_24_hour(use_24_hour), m_mode(Mode::Hour), m_dragging(false) {
    set_modal(true);

    // Initialize to current time
    time_t now = time(nullptr);
    tm *local = localtime(&now);
    m_hour = local->tm_hour;
    m_minute = local->tm_min;
}

M3Theme *M3TimePicker::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3TimePicker::set_time(int hour, int minute) {
    m_hour = hour;
    m_minute = minute;
}

void M3TimePicker::get_time(int &hour, int &minute) const {
    hour = m_hour;
    minute = m_minute;
}

void M3TimePicker::show() {
    if (m_parent) {
        int w = 328, h = 450;
        set_position(Vector2i((m_parent->width() - w) / 2, (m_parent->height() - h) / 2));
        set_size(Vector2i(w, h));
    }
    m_mode = Mode::Hour;
    set_visible(true);
}

void M3TimePicker::hide() {
    set_visible(false);
}

Vector2i M3TimePicker::preferred_size_impl(NVGcontext *) const {
    return Vector2i(328, 450);
}

int M3TimePicker::value_at_position(const Vector2i &p) const {
    Vector2i local = p - m_pos;
    
    float cx = m_size.x() * 0.5f;
    float cy = 250;
    float radius = 100;
    
    float dx = local.x() - cx;
    float dy = local.y() - cy;
    float dist = std::sqrt(dx * dx + dy * dy);
    
    if (dist < 30 || dist > radius + 20) return -1;
    
    float angle = std::atan2(dy, dx);
    angle = angle * 180.0f / M_PI + 90;
    if (angle < 0) angle += 360;
    
    if (m_mode == Mode::Hour) {
        int max_hour = m_use_24_hour ? 24 : 12;
        int value = static_cast<int>((angle / 360.0f) * max_hour);
        if (value == 0) value = max_hour;
        return value;
    } else {
        int value = static_cast<int>((angle / 360.0f) * 60);
        return value;
    }
}

bool M3TimePicker::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Popup::mouse_button_event(p, button, down, modifiers);

    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT) return false;

    if (down) {
        int value = value_at_position(p);
        if (value >= 0) {
            m_dragging = true;
            if (m_mode == Mode::Hour) {
                m_hour = value;
            } else {
                m_minute = value;
            }
            return true;
        }
    } else {
        // Mouse up
        if (m_dragging) {
            m_dragging = false;
            if (m_mode == Mode::Hour) {
                m_mode = Mode::Minute;
            } else {
                if (m_callback) {
                    m_callback(m_hour, m_minute);
                }
                hide();
            }
            return true;
        }
    }

    return false;
}

bool M3TimePicker::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    if (m_dragging && button == NANOGUI_MOUSE_BUTTON_LEFT) {
        int value = value_at_position(p);
        if (value >= 0) {
            if (m_mode == Mode::Hour) {
                m_hour = value;
            } else {
                m_minute = value;
            }
        }
        return true;
    }
    return false;
}

void M3TimePicker::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Popup::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();
    float corner = theme->corner_radius(M3Theme::ShapeFamily::Large);

    nvgSave(ctx);

    // Scrim
    if (m_modal) {
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_parent->width(), m_parent->height());
        nvgFillColor(ctx, Color(theme->scrim().r(), theme->scrim().g(), theme->scrim().b(), 0.32f));
        nvgFill(ctx);
    }

    // Background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner);
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    // Elevation tint
    Color tint = theme->elevation_tint(M3Theme::Elevation::Level3);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner);
    nvgFillColor(ctx, tint);
    nvgFill(ctx);

    // Header - Display time
    nvgFontSize(ctx, 48);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(ctx, theme->on_surface());
    
    char time_str[16];
    if (m_use_24_hour) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", m_hour, m_minute);
    } else {
        int display_hour = m_hour % 12;
        if (display_hour == 0) display_hour = 12;
        snprintf(time_str, sizeof(time_str), "%d:%02d %s", 
                display_hour, m_minute, m_hour >= 12 ? "PM" : "AM");
    }
    nvgText(ctx, x + w * 0.5f, y + 40, time_str, nullptr);

    // Mode indicator
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(ctx, theme->on_surface_variant());
    nvgText(ctx, x + w * 0.5f, y + 100, 
            m_mode == Mode::Hour ? "Select hour" : "Select minute", nullptr);

    // Clock face
    float cx = x + w * 0.5f;
    float cy = y + 250;
    float radius = 100;

    // Clock circle
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius);
    nvgFillColor(ctx, theme->surface_variant());
    nvgFill(ctx);

    // Clock numbers
    int max_value = (m_mode == Mode::Hour) ? (m_use_24_hour ? 24 : 12) : 60;
    int step = (m_mode == Mode::Hour) ? 1 : 5;
    
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    
    for (int i = 0; i < max_value; i += step) {
        float angle = (i / static_cast<float>(max_value)) * 2.0f * M_PI - M_PI * 0.5f;
        float nx = cx + std::cos(angle) * (radius - 20);
        float ny = cy + std::sin(angle) * (radius - 20);
        
        int display_value = (m_mode == Mode::Hour && i == 0) ? max_value : i;
        
        bool is_selected = (m_mode == Mode::Hour && display_value == m_hour) ||
                          (m_mode == Mode::Minute && i == m_minute);
        
        if (is_selected) {
            nvgBeginPath(ctx);
            nvgCircle(ctx, nx, ny, 18);
            nvgFillColor(ctx, theme->primary());
            nvgFill(ctx);
            nvgFillColor(ctx, theme->on_primary());
        } else {
            nvgFillColor(ctx, theme->on_surface());
        }
        
        char num_str[4];
        snprintf(num_str, sizeof(num_str), "%d", display_value);
        nvgText(ctx, nx, ny, num_str, nullptr);
    }

    // Center dot
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, 4);
    nvgFillColor(ctx, theme->primary());
    nvgFill(ctx);

    // Hand
    int current_value = (m_mode == Mode::Hour) ? m_hour : m_minute;
    float hand_angle = (current_value / static_cast<float>(max_value)) * 2.0f * M_PI - M_PI * 0.5f;
    float hand_x = cx + std::cos(hand_angle) * (radius - 20);
    float hand_y = cy + std::sin(hand_angle) * (radius - 20);
    
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy);
    nvgLineTo(ctx, hand_x, hand_y);
    nvgStrokeWidth(ctx, 2.0f);
    nvgStrokeColor(ctx, theme->primary());
    nvgStroke(ctx);

    nvgRestore(ctx);
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
