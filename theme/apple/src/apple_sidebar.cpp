/*
    src/apple_sidebar.cpp -- Apple HIG sidebar implementation
*/

#include <nanogui/apple_sidebar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleSidebar::AppleSidebar(Widget *parent)
    : Widget(parent), m_selected_section(-1), m_selected_index(-1),
      m_collapsed(false), m_item_height(28.0f) {
  set_fixed_width(280); // Standard macOS sidebar width
}

AppleTheme *AppleSidebar::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleSidebar::add_section(const std::string &title) {
  Section section(title);
  m_sections.push_back(section);
}

void AppleSidebar::add_item(const std::string &label, int icon) {
  if (m_sections.empty()) {
    add_section("");
  }
  m_sections.back().items.push_back(label);
  m_sections.back().icons.push_back(icon);
}

void AppleSidebar::set_selected_index(int index) {
  m_selected_index = index;
}

void AppleSidebar::set_collapsed(bool collapsed) {
  m_collapsed = collapsed;
  set_fixed_width(collapsed ? 64 : 280);
}

std::pair<int, int> AppleSidebar::item_at_position(const Vector2i &p) const {
  float y_offset = m_pos.y() + 8;
  
  for (size_t sec = 0; sec < m_sections.size(); ++sec) {
    // Section header
    if (!m_sections[sec].title.empty()) {
      y_offset += 24;
    }
    
    // Items
    for (size_t item = 0; item < m_sections[sec].items.size(); ++item) {
      if (p.y() >= y_offset && p.y() < y_offset + m_item_height) {
        return {static_cast<int>(sec), static_cast<int>(item)};
      }
      y_offset += m_item_height;
    }
    
    y_offset += 8; // Section spacing
  }
  
  return {-1, -1};
}

bool AppleSidebar::mouse_button_event(const Vector2i &p, int button, bool down,
                                       int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;

  if (down) {
    auto [section, index] = item_at_position(p);
    if (section >= 0 && index >= 0) {
      m_selected_section = section;
      m_selected_index = index;
      if (m_callback)
        m_callback(section, index);
    }
  }
  return true;
}

void AppleSidebar::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  // Draw background
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);

  // Draw right border
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, m_pos.x() + m_size.x(), m_pos.y());
  nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y() + m_size.y());
  nvgStrokeWidth(ctx, 1.0f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  float y_offset = m_pos.y() + 8;
  int global_index = 0;

  for (size_t sec = 0; sec < m_sections.size(); ++sec) {
    // Draw section header
    if (!m_sections[sec].title.empty() && !m_collapsed) {
      nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Caption1));
      nvgFontFace(ctx, "sans-bold");
      nvgFillColor(ctx, theme->tertiary_label());
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgText(ctx, m_pos.x() + 16, y_offset, m_sections[sec].title.c_str(), nullptr);
      y_offset += 24;
    }

    // Draw items
    for (size_t item = 0; item < m_sections[sec].items.size(); ++item) {
      bool is_selected = (static_cast<int>(sec) == m_selected_section && 
                         static_cast<int>(item) == m_selected_index);

      // Draw selection background
      if (is_selected) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, m_pos.x() + 8, y_offset, m_size.x() - 16,
                      m_item_height, 6);
        nvgFillColor(ctx, theme->accent());
        nvgFill(ctx);
      }

      float content_x = m_pos.x() + 16;

      // Draw icon
      if (m_sections[sec].icons[item]) {
        nvgFontSize(ctx, 16);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, is_selected ? nvgRGBA(255, 255, 255, 255) 
                                      : theme->secondary_label());
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, content_x, y_offset + m_item_height * 0.5f,
                utf8(m_sections[sec].icons[item]).data(), nullptr);
        content_x += 24;
      }

      // Draw label (if not collapsed)
      if (!m_collapsed) {
        nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Body));
        nvgFontFace(ctx, "sans");
        nvgFillColor(ctx, is_selected ? nvgRGBA(255, 255, 255, 255) 
                                      : theme->label());
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, content_x, y_offset + m_item_height * 0.5f,
                m_sections[sec].items[item].c_str(), nullptr);
      }

      y_offset += m_item_height;
      global_index++;
    }

    y_offset += 8; // Section spacing
  }
}

Vector2i AppleSidebar::preferred_size_impl(NVGcontext *ctx) const {
  int height = 16; // Top/bottom padding
  
  for (const auto &section : m_sections) {
    if (!section.title.empty()) {
      height += 24; // Section header
    }
    height += static_cast<int>(section.items.size() * m_item_height);
    height += 8; // Section spacing
  }
  
  return Vector2i(m_collapsed ? 64 : 280, height);
}

NAMESPACE_END(nanogui)
