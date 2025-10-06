/*
    src/apple_segmented_control.cpp -- Apple HIG segmented control implementation
*/

#include <nanogui/apple_segmented_control.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleSegmentedControl::AppleSegmentedControl(Widget *parent,
                                             const std::vector<std::string> &items)
    : Widget(parent), m_items(items), m_selected_index(0), m_hover_index(-1) {
  set_fixed_height(28);
}

AppleTheme *AppleSegmentedControl::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleSegmentedControl::set_selected_index(int index) {
  if (index >= 0 && index < static_cast<int>(m_items.size())) {
    m_selected_index = index;
  }
}

void AppleSegmentedControl::set_items(const std::vector<std::string> &items) {
  m_items = items;
  if (m_selected_index >= static_cast<int>(items.size())) {
    m_selected_index = items.empty() ? -1 : 0;
  }
}

int AppleSegmentedControl::segment_at_position(const Vector2i &p) const {
  if (m_items.empty())
    return -1;

  float segment_width = static_cast<float>(m_size.x()) / m_items.size();
  int index = static_cast<int>((p.x() - m_pos.x()) / segment_width);
  
  if (index >= 0 && index < static_cast<int>(m_items.size())) {
    return index;
  }
  return -1;
}

bool AppleSegmentedControl::mouse_button_event(const Vector2i &p, int button,
                                                bool down, int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;

  if (down) {
    int index = segment_at_position(p);
    if (index >= 0) {
      m_selected_index = index;
      if (m_callback)
        m_callback(index);
    }
  }
  return true;
}

void AppleSegmentedControl::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme || m_items.empty())
    return;

  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Medium);
  float segment_width = static_cast<float>(m_size.x()) / m_items.size();

  // Draw container background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgFillColor(ctx, theme->system_fill());
  nvgFill(ctx);

  // Draw border
  nvgStrokeWidth(ctx, 1.0f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  // Draw segments
  for (size_t i = 0; i < m_items.size(); ++i) {
    float x = m_pos.x() + i * segment_width;
    bool is_selected = (static_cast<int>(i) == m_selected_index);

    // Draw selected segment background
    if (is_selected) {
      nvgBeginPath(ctx);
      if (i == 0) {
        // First segment - round left corners
        nvgRoundedRectVarying(ctx, x + 2, m_pos.y() + 2, segment_width - 4,
                              m_size.y() - 4, corner_radius - 2, 0, 0,
                              corner_radius - 2);
      } else if (i == m_items.size() - 1) {
        // Last segment - round right corners
        nvgRoundedRectVarying(ctx, x + 2, m_pos.y() + 2, segment_width - 4,
                              m_size.y() - 4, 0, corner_radius - 2,
                              corner_radius - 2, 0);
      } else {
        // Middle segment - no rounding
        nvgRect(ctx, x + 2, m_pos.y() + 2, segment_width - 4, m_size.y() - 4);
      }
      nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
      nvgFill(ctx);
    }

    // Draw separator (except after last segment)
    if (i < m_items.size() - 1) {
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x + segment_width, m_pos.y() + 4);
      nvgLineTo(ctx, x + segment_width, m_pos.y() + m_size.y() - 4);
      nvgStrokeWidth(ctx, 1.0f);
      nvgStrokeColor(ctx, theme->separator());
      nvgStroke(ctx);
    }

    // Draw text
    nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Subheadline));
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, m_enabled ? theme->label() : theme->tertiary_label());
    nvgText(ctx, x + segment_width * 0.5f, m_pos.y() + m_size.y() * 0.5f,
            m_items[i].c_str(), nullptr);
  }
}

Vector2i AppleSegmentedControl::preferred_size_impl(NVGcontext *ctx) const {
  AppleTheme *theme = apple_theme();
  float font_size = theme ? theme->font_size(AppleTheme::TextStyle::Subheadline) : 15.0f;

  nvgFontSize(ctx, font_size);
  nvgFontFace(ctx, "sans");

  float total_width = 0;
  for (const auto &item : m_items) {
    float tw = nvgTextBounds(ctx, 0, 0, item.c_str(), nullptr, nullptr);
    total_width += tw + 24; // Add padding
  }

  return Vector2i(static_cast<int>(total_width), 28);
}

NAMESPACE_END(nanogui)
