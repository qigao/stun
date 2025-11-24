/*
    src/apple_date_picker.cpp -- Apple HIG date picker implementation
*/

#include <nanogui/apple_date_picker.h>
#include <nanogui/opengl.h>
#include <ctime>

NAMESPACE_BEGIN(nanogui)

AppleDatePicker::AppleDatePicker(Widget *parent, Mode mode)
    : Widget(parent), m_mode(mode), m_selected_component(0) {
  
  // Initialize with current date/time
  std::time_t now = std::time(nullptr);
  std::tm *local = std::localtime(&now);
  
  m_month = local->tm_mon + 1;
  m_day = local->tm_mday;
  m_year = local->tm_year + 1900;
  m_hour = local->tm_hour;
  m_minute = local->tm_min;
  
  set_fixed_height(216); // Standard iOS picker height
}

AppleTheme *AppleDatePicker::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleDatePicker::set_date(int year, int month, int day) {
  m_year = year;
  m_month = month;
  m_day = day;
  if (m_callback)
    m_callback(m_year, m_month, m_day, m_hour, m_minute);
}

void AppleDatePicker::set_time(int hour, int minute) {
  m_hour = hour;
  m_minute = minute;
  if (m_callback)
    m_callback(m_year, m_month, m_day, m_hour, m_minute);
}

bool AppleDatePicker::mouse_button_event(const Vector2i &p, int button, bool down,
                                          int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;

  // Simple increment/decrement based on click position
  if (down) {
    float center_y = m_pos.y() + m_size.y() * 0.5f;
    bool increment = p.y() < center_y;
    
    switch (m_mode) {
      case Mode::Date:
        if (increment) {
          m_day = (m_day % 31) + 1;
        } else {
          m_day = m_day > 1 ? m_day - 1 : 31;
        }
        break;
      case Mode::Time:
        if (increment) {
          m_hour = (m_hour + 1) % 24;
        } else {
          m_hour = m_hour > 0 ? m_hour - 1 : 23;
        }
        break;
      case Mode::DateTime:
        // Handle both
        break;
    }
    
    if (m_callback)
      m_callback(m_year, m_month, m_day, m_hour, m_minute);
  }
  
  return true;
}

void AppleDatePicker::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  // Draw background
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);

  // Draw selection highlight
  float highlight_y = m_pos.y() + m_size.y() * 0.5f - 22;
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x() + 8, highlight_y, m_size.x() - 16, 44, 8);
  nvgFillColor(ctx, theme->system_fill());
  nvgFill(ctx);

  // Draw date/time components
  float center_y = m_pos.y() + m_size.y() * 0.5f;
  float font_size = theme->font_size(AppleTheme::TextStyle::Title2);
  
  nvgFontSize(ctx, font_size);
  nvgFontFace(ctx, "sans");
  nvgFillColor(ctx, theme->label());
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  char buffer[64];
  
  switch (m_mode) {
    case Mode::Date:
      snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", m_month, m_day, m_year);
      break;
    case Mode::Time:
      snprintf(buffer, sizeof(buffer), "%02d:%02d", m_hour, m_minute);
      break;
    case Mode::DateTime:
      snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d",
               m_month, m_day, m_year, m_hour, m_minute);
      break;
  }
  
  nvgText(ctx, m_pos.x() + m_size.x() * 0.5f, center_y, buffer, nullptr);

  // Draw top/bottom borders
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, m_pos.x(), m_pos.y());
  nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y());
  nvgMoveTo(ctx, m_pos.x(), m_pos.y() + m_size.y());
  nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y() + m_size.y());
  nvgStrokeWidth(ctx, 0.5f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);
}

Vector2i AppleDatePicker::preferred_size_impl(NVGcontext *ctx) const {
  return Vector2i(320, 216);
}

NAMESPACE_END(nanogui)
