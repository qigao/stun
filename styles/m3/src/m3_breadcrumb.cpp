/*
    src/m3_breadcrumb.cpp -- M3 Breadcrumb implementation
*/

#include <nanogui/m3_breadcrumb.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Breadcrumb::M3Breadcrumb(Widget *parent) : Widget(parent) {
  set_fixed_height(48);
}

M3Theme *M3Breadcrumb::m3_theme() const {
  return dynamic_cast<M3Theme *>(const_cast<Theme *>(m_theme.get()));
}

void M3Breadcrumb::set_items(const std::vector<Item> &items) {
  m_items = items;
}

void M3Breadcrumb::add_item(const std::string &label, int icon,
                            std::function<void()> callback) {
  m_items.emplace_back(label, icon, callback);
}

int M3Breadcrumb::item_at_position(const Vector2i &p) const {
  Vector2i local = p - m_pos;

  for (size_t i = 0; i < m_item_positions.size(); ++i) {
    if (i + 1 < m_item_positions.size()) {
      if (local.x() >= m_item_positions[i] &&
          local.x() < m_item_positions[i + 1]) {
        return static_cast<int>(i);
      }
    } else {
      if (local.x() >= m_item_positions[i]) {
        return static_cast<int>(i);
      }
    }
  }
  return -1;
}

bool M3Breadcrumb::mouse_button_event(const Vector2i &p, int button, bool down,
                                      int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);

  if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT || !down)
    return false;

  int idx = item_at_position(p);
  if (idx >= 0 &&
      idx < static_cast<int>(m_items.size()) - 1) { // Don't click last item
    if (m_items[idx].callback) {
      m_items[idx].callback();
    }
    return true;
  }

  return false;
}

Vector2i M3Breadcrumb::preferred_size(NVGcontext *ctx) const {
  nvgFontSize(ctx, 14);
  nvgFontFace(ctx, "sans");

  float total_width = 16; // Left padding

  for (size_t i = 0; i < m_items.size(); ++i) {
    const auto &item = m_items[i];

    if (item.icon) {
      total_width += 24 + 8; // Icon + gap
    }

    float text_width =
        nvgTextBounds(ctx, 0, 0, item.label.c_str(), nullptr, nullptr);
    total_width += text_width;

    if (i < m_items.size() - 1) {
      total_width += 32; // Separator space
    }
  }

  total_width += 16; // Right padding

  return Vector2i(static_cast<int>(total_width), 48);
}

void M3Breadcrumb::draw(NVGcontext *ctx) {
  M3Theme *theme = m3_theme();
  if (!theme) {
    Widget::draw(ctx);
    return;
  }

  float x = m_pos.x(), y = m_pos.y(), h = m_size.y();

  nvgSave(ctx);

  m_item_positions.clear();
  float current_x = x + 16;

  for (size_t i = 0; i < m_items.size(); ++i) {
    const auto &item = m_items[i];
    bool is_last = (i == m_items.size() - 1);
    bool is_hovered = (static_cast<int>(i) == m_hover_index);

    m_item_positions.push_back(current_x);

    // Hover background
    if (is_hovered && !is_last && m_enabled) {
      nvgFontSize(ctx, 14);
      nvgFontFace(ctx, "sans");
      float text_width =
          nvgTextBounds(ctx, 0, 0, item.label.c_str(), nullptr, nullptr);
      float item_width = text_width + (item.icon ? 32 : 0) + 16;

      nvgBeginPath(ctx);
      nvgRoundedRect(ctx, current_x - 8, y + 8, item_width, 32, 16);
      nvgFillColor(ctx, theme->state_layer(theme->on_surface(), 0.08f));
      nvgFill(ctx);
    }

    // Icon
    if (item.icon) {
      nvgFontSize(ctx, 18);
      nvgFontFace(ctx, "icons");
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      Color icon_color =
          is_last ? theme->on_surface() : theme->on_surface_variant();
      nvgFillColor(ctx, icon_color);
      nvgText(ctx, current_x, y + h * 0.5f, utf8(item.icon).data(), nullptr);
      current_x += 24 + 8;
    }

    // Label
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, is_last ? "sans-bold" : "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    Color text_color =
        is_last ? theme->on_surface() : theme->on_surface_variant();
    nvgFillColor(ctx, text_color);

    float text_width =
        nvgTextBounds(ctx, 0, 0, item.label.c_str(), nullptr, nullptr);
    nvgText(ctx, current_x, y + h * 0.5f, item.label.c_str(), nullptr);
    current_x += text_width;

    // Separator
    if (!is_last) {
      current_x += 16;
      nvgFontSize(ctx, 12);
      nvgFontFace(ctx, "icons");
      nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(ctx, theme->on_surface_variant());
      nvgText(ctx, current_x, y + h * 0.5f, utf8(m_separator_icon).data(),
              nullptr);
      current_x += 16;
    }
  }

  nvgRestore(ctx);
  Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
