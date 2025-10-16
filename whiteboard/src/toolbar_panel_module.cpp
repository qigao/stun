#include "whiteboard/toolbar_panel_module.h"

// ToolButtonWidget implementation
ToolButtonWidget::ToolButtonWidget(Widget *parent, const std::string &icon, int id)
    : Widget(parent), m_icon(icon), m_id(id) {
  set_fixed_size(Vector2i(40, 40));
}

Vector2i ToolButtonWidget::preferred_size_impl(NVGcontext *) const { return {40, 40}; }

bool ToolButtonWidget::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    if (m_callback) {
      m_callback();
    }
    return true;
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

void ToolButtonWidget::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float size = 40.f;
  const float cx = px + size * 0.5f;
  const float cy = py + size * 0.5f;

  // Draw button background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, size, size, 8.f);

  if (m_selected) {
    nvgFillColor(ctx, nvgRGBA(220, 220, 245, 255));
  } else if (m_hovered) {
    nvgFillColor(ctx, nvgRGBA(235, 235, 245, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(245, 245, 250, 0));
  }
  nvgFill(ctx);

  if (m_selected) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, size, size, 8.f);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 220, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);
  }

  // Draw icon
  const float iconSize = size * 0.65f;
  nvgFontFace(ctx, "icons");
  nvgFontSize(ctx, iconSize);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  int faIcon = 0;
  if (m_icon == "lock")
    faIcon = m_locked_state ? 0xf023 : 0xf09c;
  else if (m_icon == "hand")
    faIcon = 0xf256;
  else if (m_icon == "cursor")
    faIcon = 0xf245;
  else if (m_icon == "square")
    faIcon = 0xf0c8;
  else if (m_icon == "diamond")
    faIcon = 0xf3a5;
  else if (m_icon == "circle")
    faIcon = 0xf111;
  else if (m_icon == "arrow")
    faIcon = 0xf061;
  else if (m_icon == "line")
    faIcon = 0xf068;
  else if (m_icon == "pen")
    faIcon = 0xf304;
  else if (m_icon == "text")
    faIcon = 0xf031;
  else if (m_icon == "image")
    faIcon = 0xf03e;
  else if (m_icon == "rotate")
    faIcon = 0xf021;
  else if (m_icon == "tree")
    faIcon = 0xf0e8;

  if (faIcon != 0) {
    std::string icon_text = utf8(faIcon);
    nvgText(ctx, cx, cy, icon_text.c_str(), nullptr);
  }

  // Draw number badge for some tools
  if (m_icon == "cursor" || m_icon == "square" || m_icon == "diamond" || m_icon == "circle" ||
      m_icon == "arrow" || m_icon == "line") {
    const char *num = "1";
    if (m_icon == "square")
      num = "2";
    else if (m_icon == "diamond")
      num = "3";
    else if (m_icon == "circle")
      num = "4";
    else if (m_icon == "arrow")
      num = "5";
    else if (m_icon == "line")
      num = "6";

    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, 10.f);
    nvgFillColor(ctx, nvgRGBA(140, 140, 160, 255));
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
    nvgText(ctx, px + size - 4.f, py + size - 2.f, num, nullptr);
  }
}

// ToolbarPanelModule implementation
ToolbarPanelModule::ToolbarPanelModule(Widget *parent, Orientation orientation)
    : Widget(parent), m_orientation(orientation), m_selectedButton(2) {

  // Use BoxLayout for automatic positioning
  set_layout(new BoxLayout(orientation, Alignment::Middle, 6, 12));

  create_buttons();
}

void ToolbarPanelModule::create_buttons() {
  std::vector<std::string> icons = {"lock",   "hand",   "cursor", "square", "diamond",
                                    "circle", "arrow",  "line",   "pen",    "text",
                                    "image",  "rotate", "tree"};

  for (size_t i = 0; i < icons.size(); i++) {
    auto *btn = new ToolButtonWidget(this, icons[i], static_cast<int>(i));

    if (i == 0) {
      // Lock button
      btn->set_is_lock_button(true);
      btn->set_callback([this, btn]() {
        m_is_locked = !m_is_locked;
        btn->set_locked_state(m_is_locked);
        screen()->redraw();
      });
      m_lock_button = btn;
    } else {
      // Tool buttons
      btn->set_selected(i == 2); // Cursor selected by default
      btn->set_callback([this, btn, i]() {
        // Deselect all tool buttons (skip lock button)
        for (size_t j = 1; j < m_buttons.size(); j++) {
          m_buttons[j]->set_selected(false);
        }
        btn->set_selected(true);
        m_selectedButton = static_cast<int>(i);

        if (m_tool_callback) {
          m_tool_callback(static_cast<int>(i));
        }
        screen()->redraw();
      });
    }

    m_buttons.push_back(btn);
  }
}

void ToolbarPanelModule::set_orientation(Orientation orientation) {
  if (m_orientation != orientation) {
    m_orientation = orientation;
    delete m_layout;
    set_layout(new BoxLayout(orientation, Alignment::Middle, 6, 12));
  }
}

void ToolbarPanelModule::set_locked(bool locked) {
  m_is_locked = locked;
  if (m_lock_button) {
    m_lock_button->set_locked_state(locked);
  }
}

bool ToolbarPanelModule::mouse_button_event(const Vector2i &p, int button, bool down,
                                            int modifiers) {
  // Let children handle the event first
  if (Widget::mouse_button_event(p, button, down, modifiers)) {
    return true;
  }

  // If no child handled it, check if we should start dragging
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f local_pos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

    // If click is within panel bounds, start dragging
    if (local_pos.x() >= 0 && local_pos.x() <= m_size.x() && local_pos.y() >= 0 &&
        local_pos.y() <= m_size.y()) {
      m_dragging = true;
      m_drag_start = p;
      return true;
    }
  } else if (button == GLFW_MOUSE_BUTTON_1 && !down) {
    m_dragging = false;
  }

  return false;
}

bool ToolbarPanelModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                          int modifiers) {
  if (m_dragging) {
    Vector2i new_pos = m_pos + rel;
    set_position(new_pos);
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

void ToolbarPanelModule::draw(NVGcontext *ctx) {
  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  // Draw rounded panel background
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

  // Draw children (buttons)
  Widget::draw(ctx);
}
