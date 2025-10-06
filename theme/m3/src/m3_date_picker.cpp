/*
    src/m3_date_picker.cpp -- M3 Date Picker implementation
*/

#include <nanogui/m3_date_picker.h>
#include <nanogui/opengl.h>
#include <ctime>

NAMESPACE_BEGIN(nanogui)

M3DatePicker::M3DatePicker(Widget *parent)
    : Popup(parent) {
    set_modal(true);
    
    // Initialize to current date
    time_t now = time(nullptr);
    tm *local = localtime(&now);
    m_year = local->tm_year + 1900;
    m_month = local->tm_mon + 1;
    m_day = local->tm_mday;
}

M3Theme *M3DatePicker::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3DatePicker::set_date(int year, int month, int day) {
    m_year = year;
    m_month = month;
    m_day = day;
}

void M3DatePicker::get_date(int &year, int &month, int &day) const {
    year = m_year;
    month = m_month;
    day = m_day;
}

void M3DatePicker::show() {
    if (m_parent) {
        int w = 328, h = 400;
        set_position(Vector2i((m_parent->width() - w) / 2, (m_parent->height() - h) / 2));
        set_size(Vector2i(w, h));
    }
    set_visible(true);
}

void M3DatePicker::hide() {
    set_visible(false);
}

Vector2i M3DatePicker::preferred_size_impl(NVGcontext *) const {
    return Vector2i(328, 400);
}

int M3DatePicker::days_in_month(int year, int month) const {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        return 29; // Leap year
    }
    return days[month - 1];
}

int M3DatePicker::first_day_of_month(int year, int month) const {
    tm time_in = {0, 0, 0, 1, month - 1, year - 1900};
    time_t time_temp = mktime(&time_in);
    const tm *time_out = localtime(&time_temp);
    return time_out->tm_wday; // 0 = Sunday
}

int M3DatePicker::day_at_position(const Vector2i &p) const {
    Vector2i local = p - m_pos;
    
    float calendar_y = 120;
    float cell_size = 40;
    
    if (local.y() < calendar_y) return -1;
    
    int col = static_cast<int>((local.x() - 16) / cell_size);
    int row = static_cast<int>((local.y() - calendar_y) / cell_size);
    
    if (col < 0 || col >= 7 || row < 0) return -1;
    
    int first_day = first_day_of_month(m_year, m_month);
    int day = row * 7 + col - first_day + 1;
    int days = days_in_month(m_year, m_month);
    
    return (day >= 1 && day <= days) ? day : -1;
}

bool M3DatePicker::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Popup::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT || !down) return false;
    
    int day = day_at_position(p);
    if (day > 0) {
        m_day = day;
        if (m_callback) {
            m_callback(m_year, m_month, m_day);
        }
        hide();
        return true;
    }
    
    return false;
}

void M3DatePicker::draw(NVGcontext *ctx) {
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

    // Header
    nvgFontSize(ctx, 32);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(ctx, theme->on_surface());
    
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%d/%d/%d", m_month, m_day, m_year);
    nvgText(ctx, x + 24, y + 24, date_str, nullptr);

    // Month/Year
    nvgFontSize(ctx, 16);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char month_year[32];
    snprintf(month_year, sizeof(month_year), "%s %d", months[m_month - 1], m_year);
    nvgText(ctx, x + w * 0.5f, y + 80, month_year, nullptr);

    // Day headers
    const char *days[] = {"S", "M", "T", "W", "T", "F", "S"};
    float cell_size = 40;
    float calendar_y = 120;
    
    nvgFontSize(ctx, 12);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface_variant());
    
    for (int i = 0; i < 7; ++i) {
        nvgText(ctx, x + 16 + i * cell_size + cell_size * 0.5f, 
                y + calendar_y - 20, days[i], nullptr);
    }

    // Calendar grid
    int first_day = first_day_of_month(m_year, m_month);
    int days_count = days_in_month(m_year, m_month);
    
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    
    for (int day = 1; day <= days_count; ++day) {
        int pos = first_day + day - 1;
        int row = pos / 7;
        int col = pos % 7;
        
        float cx = x + 16 + col * cell_size + cell_size * 0.5f;
        float cy = y + calendar_y + row * cell_size + cell_size * 0.5f;
        
        bool is_selected = (day == m_day);
        bool is_hovered = (day == m_hover_day);
        
        // Background
        if (is_selected) {
            nvgBeginPath(ctx);
            nvgCircle(ctx, cx, cy, cell_size * 0.4f);
            nvgFillColor(ctx, theme->primary());
            nvgFill(ctx);
        } else if (is_hovered) {
            nvgBeginPath(ctx);
            nvgCircle(ctx, cx, cy, cell_size * 0.4f);
            nvgFillColor(ctx, theme->state_layer(theme->on_surface(), 0.08f));
            nvgFill(ctx);
        }
        
        // Day number
        Color text_color = is_selected ? theme->on_primary() : theme->on_surface();
        nvgFillColor(ctx, text_color);
        
        char day_str[4];
        snprintf(day_str, sizeof(day_str), "%d", day);
        nvgText(ctx, cx, cy, day_str, nullptr);
    }

    nvgRestore(ctx);
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
