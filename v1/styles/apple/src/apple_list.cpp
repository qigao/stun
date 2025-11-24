/*
    src/apple_list.cpp -- Apple HIG list implementation
*/

#include <nanogui/apple_list.h>
#include <nanogui/opengl.h>
#include <nanogui/icons.h>

NAMESPACE_BEGIN(nanogui)

AppleList::AppleList(Widget *parent, Style style)
    : Widget(parent), m_style(style), m_selected_index(-1),
      m_hover_index(-1), m_item_height(44.0f) {}

AppleTheme *AppleList::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleList::add_item(const std::string &text, const std::string &detail,
                         int icon, bool disclosure) {
  Item item(text, detail, icon, disclosure);
  m_items.push_back(item);
}

void AppleList::add_item(const Item &item) {
  m_items.push_back(item);
}

void AppleList::set_selected_index(int index) {
  if (index >= -1 && index < static_cast<int>(m_items.size())) {
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_items.size())) {
      m_items[m_selected_index].selected = false;
    }
    m_selected_index = index;
    if (index >= 0) {
      m_items[index].selected = true;
    }
  }
}

int AppleList::item_at_position(const Vector2i &p) const {
  if (m_items.empty())
    return -1;

  float y_offset = m_pos.y();
  if (m_style == Style::Grouped) {
    y_offset += 8; // Top margin for grouped style
  }

  int index = static_cast<int>((p.y() - y_offset) / m_item_height);
  if (index >= 0 && index < static_cast<int>(m_items.size())) {
    return index;
  }
  return -1;
}

bool AppleList::mouse_button_event(const Vector2i &p, int button, bool down,
                                    int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;

  if (down) {
    int index = item_at_position(p);
    if (index >= 0) {
      set_selected_index(index);
      if (m_callback)
        m_callback(index);
      if (m_items[index].callback)
        m_items[index].callback();
    }
  }
  return true;
}

void AppleList::draw_item(NVGcontext *ctx, const Item &item, float x, float y,
                          float w, float h, bool hover) {
  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  // Draw background for hover/selected
  if (item.selected || hover) {
    nvgBeginPath(ctx);
    if (m_style == Style::Grouped) {
      nvgRoundedRect(ctx, x + 2, y, w - 4, h, 8);
    } else {
      nvgRect(ctx, x, y, w, h);
    }
    
    if (item.selected) {
      nvgFillColor(ctx, theme->accent());
    } else {
      nvgFillColor(ctx, theme->system_fill());
    }
    nvgFill(ctx);
  }

  float content_x = x + 16;
  float content_y = y + h * 0.5f;

  // Draw icon if present
  if (item.icon) {
    nvgFontSize(ctx, 20);
    nvgFontFace(ctx, "icons");
    nvgFillColor(ctx, item.selected ? nvgRGBA(255, 255, 255, 255) : theme->label());
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, content_x, content_y, utf8(item.icon).data(), nullptr);
    content_x += 32;
  }

  // Draw text
  nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Body));
  nvgFontFace(ctx, "sans");
  nvgFillColor(ctx, item.selected ? nvgRGBA(255, 255, 255, 255) : theme->label());
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, content_x, content_y, item.text.c_str(), nullptr);

  // Draw detail text if present
  if (!item.detail.empty()) {
    nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Subheadline));
    nvgFillColor(ctx, item.selected ? nvgRGBA(255, 255, 255, 200)
                                    : theme->secondary_label());
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + w - 32, content_y, item.detail.c_str(), nullptr);
  }

  // Draw disclosure indicator
  if (item.has_disclosure) {
    nvgFontSize(ctx, 16);
    nvgFontFace(ctx, "icons");
    nvgFillColor(ctx, item.selected ? nvgRGBA(255, 255, 255, 200)
                                    : theme->tertiary_label());
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + w - 12, content_y, utf8(FA_CHEVRON_RIGHT).data(), nullptr);
  }
}

void AppleList::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme || m_items.empty())
    return;

  float y_offset = m_pos.y();

  // Draw container for grouped style
  if (m_style == Style::Grouped) {
    float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Large);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x() + 8, m_pos.y() + 8, m_size.x() - 16,
                   m_items.size() * m_item_height, corner_radius);
    nvgFillColor(ctx, theme->secondary_system_background());
    nvgFill(ctx);
    
    nvgStrokeWidth(ctx, 1.0f);
    nvgStrokeColor(ctx, theme->separator());
    nvgStroke(ctx);
    
    y_offset += 8;
  }

  // Draw items
  for (size_t i = 0; i < m_items.size(); ++i) {
    float item_y = y_offset + i * m_item_height;
    bool hover = (static_cast<int>(i) == m_hover_index);
    
    draw_item(ctx, m_items[i], m_pos.x() + (m_style == Style::Grouped ? 8 : 0),
              item_y, m_size.x() - (m_style == Style::Grouped ? 16 : 0),
              m_item_height, hover);

    // Draw separator (except for last item)
    if (i < m_items.size() - 1) {
      nvgBeginPath(ctx);
      float sep_x = m_pos.x() + (m_style == Style::Grouped ? 24 : 16);
      nvgMoveTo(ctx, sep_x, item_y + m_item_height);
      nvgLineTo(ctx, m_pos.x() + m_size.x() - (m_style == Style::Grouped ? 8 : 0),
                item_y + m_item_height);
      nvgStrokeWidth(ctx, 0.5f);
      nvgStrokeColor(ctx, theme->separator());
      nvgStroke(ctx);
    }
  }
}

Vector2i AppleList::preferred_size_impl(NVGcontext *ctx) const {
  int height = static_cast<int>(m_items.size() * m_item_height);
  if (m_style == Style::Grouped) {
    height += 16; // Top and bottom margins
  }
  return Vector2i(300, height);
}

NAMESPACE_END(nanogui)
