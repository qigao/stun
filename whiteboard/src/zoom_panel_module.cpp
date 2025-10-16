#include "whiteboard/zoom_panel_module.h"

// ZoomButtonWidget implementation
ZoomButtonWidget::ZoomButtonWidget(Widget *parent, int icon, const std::string &label)
    : Widget(parent), m_icon(icon), m_label(label) {
  set_fixed_size(Vector2i(60, 60));
}

Vector2i ZoomButtonWidget::preferred_size_impl(NVGcontext *) const {
  return {60, 60};
}

bool ZoomButtonWidget::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1) {
    if (down) {
      m_pressed = true;
      screen()->redraw();
      return true;
    } else {
      if (m_pressed && m_callback) {
        m_callback();
      }
      m_pressed = false;
      screen()->redraw();
      return true;
    }
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

void ZoomButtonWidget::draw(NVGcontext *ctx) {
  Widget::draw(ctx);
  
  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float size = 60.f;

  // Draw button background (same style as toolbar)
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, size, size, 8.f);

  if (m_pressed) {
    nvgFillColor(ctx, nvgRGBA(200, 200, 220, 255));
  } else if (m_hovered) {
    nvgFillColor(ctx, nvgRGBA(235, 235, 245, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(245, 245, 250, 0));
  }
  nvgFill(ctx);

  // Draw border for pressed state
  if (m_pressed) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, size, size, 8.f);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 220, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);
  }

  // Draw icon (dark color like toolbar)
  nvgFontFace(ctx, "icons");
  nvgFontSize(ctx, 20.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  if (m_icon > 0) {
    std::string icon_str = utf8(m_icon);
    nvgText(ctx, px + size * 0.5f, py + size * 0.5f, icon_str.c_str(), nullptr);
  }
}

// ZoomPanelModule implementation
ZoomPanelModule::ZoomPanelModule(Widget *parent) : Widget(parent) {
  // Use horizontal BoxLayout with all elements in a row
  set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 12));
  
  create_buttons();
  
  // Zoom level label
  m_zoom_label = new Label(this, "100%", "sans", 12);
  m_zoom_label->set_color(Color(60, 60, 80, 255));
}

void ZoomPanelModule::create_buttons() {
  // Zoom Out button
  auto *zoom_out_btn = new ZoomButtonWidget(this, FA_SEARCH_MINUS, "Zoom Out");
  zoom_out_btn->set_callback([this]() {
    m_zoom_level = std::max(0.1f, m_zoom_level - 0.1f);
    if (m_zoom_callback)
      m_zoom_callback(-1); // -1 = zoom out
    set_zoom_level(m_zoom_level);
  });
  m_buttons.push_back(zoom_out_btn);

  // Reset Zoom button
  auto *reset_btn = new ZoomButtonWidget(this, FA_EXPAND, "Reset");
  reset_btn->set_callback([this]() {
    m_zoom_level = 1.0f;
    if (m_zoom_callback)
      m_zoom_callback(0); // 0 = reset
    set_zoom_level(m_zoom_level);
  });
  m_buttons.push_back(reset_btn);

  // Zoom In button
  auto *zoom_in_btn = new ZoomButtonWidget(this, FA_SEARCH_PLUS, "Zoom In");
  zoom_in_btn->set_callback([this]() {
    m_zoom_level = std::min(5.0f, m_zoom_level + 0.1f);
    if (m_zoom_callback)
      m_zoom_callback(1); // 1 = zoom in
    set_zoom_level(m_zoom_level);
  });
  m_buttons.push_back(zoom_in_btn);
}

void ZoomPanelModule::set_zoom_level(float zoom) {
  m_zoom_level = std::clamp(zoom, 0.1f, 5.0f);
  
  // Update label
  if (m_zoom_label) {
    char zoom_text[32];
    snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", m_zoom_level * 100.0f);
    m_zoom_label->set_caption(zoom_text);
  }
}

bool ZoomPanelModule::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  // Let children handle the event first
  if (Widget::mouse_button_event(p, button, down, modifiers)) {
    return true;
  }
  
  // If no child handled it, check if we should start dragging
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f local_pos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());
    
    // If click is within panel bounds, start dragging
    if (local_pos.x() >= 0 && local_pos.x() <= m_size.x() &&
        local_pos.y() >= 0 && local_pos.y() <= m_size.y()) {
      m_dragging = true;
      m_drag_start = p;
      return true;
    }
  } else if (button == GLFW_MOUSE_BUTTON_1 && !down) {
    m_dragging = false;
  }
  
  return false;
}

bool ZoomPanelModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
  if (m_dragging) {
    Vector2i new_pos = m_pos + rel;
    set_position(new_pos);
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

void ZoomPanelModule::draw(NVGcontext *ctx) {
  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  // Draw rounded panel background (same style as toolbar)
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
  nvgFillColor(ctx, nvgRGBA(245, 245, 250, 255));
  nvgFill(ctx);

  // Draw subtle border
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
  nvgStrokeColor(ctx, nvgRGBA(220, 220, 230, 255));
  nvgStrokeWidth(ctx, 1.f);
  nvgStroke(ctx);

  // Draw children (buttons, zoom label)
  Widget::draw(ctx);
}
