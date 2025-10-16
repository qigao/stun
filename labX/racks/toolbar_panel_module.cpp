#include "toolbar_panel_module.h"
#include <cmath>

ToolbarPanelModule::ToolbarPanelModule(Widget *parent) : Widget(parent) {
  const float buttonSize = 48.f;
  const float spacing = 8.f;
  const float startX = 20.f;
  const float y = 20.f;

  // Create toolbar buttons
  std::vector<std::string> icons = {"lock",   "hand",   "cursor", "square", "diamond",
                                    "circle", "arrow",  "line",   "pen",    "text",
                                    "image",  "rotate", "tree"};

  for (size_t i = 0; i < icons.size(); i++) {
    float x = startX + i * (buttonSize + spacing);
    m_buttons.push_back({x, y, buttonSize, icons[i], i == 2, static_cast<int>(i)});
  }

  m_hoveredButton = -1;
  m_selectedButton = 2; // Cursor selected by default
  m_dragging = false;
}

Vector2i ToolbarPanelModule::preferred_size_impl(NVGcontext *) const {
  return {800, 88}; // Wide toolbar panel
}

bool ToolbarPanelModule::mouse_button_event(const Vector2i &p, int button, bool down,
                                            int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

    // Check if clicking on a button
    bool clickedButton = false;
    for (size_t i = 0; i < m_buttons.size(); i++) {
      float dx = localPos.x() - (m_buttons[i].x + m_buttons[i].size * 0.5f);
      float dy = localPos.y() - (m_buttons[i].y + m_buttons[i].size * 0.5f);
      float halfSize = m_buttons[i].size * 0.5f;

      if (std::abs(dx) <= halfSize && std::abs(dy) <= halfSize) {
        // Deselect all
        for (auto &btn : m_buttons) {
          btn.selected = false;
        }
        // Select clicked button
        m_buttons[i].selected = true;
        m_selectedButton = static_cast<int>(i);
        screen()->redraw();
        clickedButton = true;
        return true;
      }
    }

    // If not clicking a button, start dragging the panel
    if (!clickedButton) {
      m_dragging = true;
      m_dragStart = p;
      return true;
    }
  } else if (button == GLFW_MOUSE_BUTTON_1 && !down) {
    m_dragging = false;
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

bool ToolbarPanelModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                          int modifiers) {
  if (m_dragging) {
    // Move this widget directly (no parent window)
    Vector2i newPos = m_pos + rel;
    set_position(newPos);
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

void ToolbarPanelModule::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  // Draw rounded panel background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 16.f);
  nvgFillColor(ctx, nvgRGBA(245, 245, 250, 255));
  nvgFill(ctx);

  // Draw subtle shadow
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 16.f);
  nvgStrokeColor(ctx, nvgRGBA(220, 220, 230, 255));
  nvgStrokeWidth(ctx, 1.f);
  nvgStroke(ctx);

  // Draw all tool buttons
  for (const auto &btn : m_buttons) {
    bool hovered = (btn.id == m_hoveredButton);
    drawToolButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected, hovered);
  }
}

void ToolbarPanelModule::drawToolButton(NVGcontext *ctx, float x, float y, float size,
                                        const char *icon, bool selected, bool hovered) {
  const float cx = x + size * 0.5f;
  const float cy = y + size * 0.5f;

  // Draw button background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, size, size, 8.f);

  if (selected) {
    // Selected state - light purple/blue
    nvgFillColor(ctx, nvgRGBA(220, 220, 245, 255));
  } else if (hovered) {
    // Hovered state
    nvgFillColor(ctx, nvgRGBA(235, 235, 245, 255));
  } else {
    // Normal state - transparent
    nvgFillColor(ctx, nvgRGBA(245, 245, 250, 0));
  }
  nvgFill(ctx);

  // Draw button border for selected
  if (selected) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, size, size, 8.f);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 220, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);
  }

  // Draw icon based on type
  std::string iconStr(icon);
  if (iconStr == "lock")
    drawLockIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "hand")
    drawHandIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "cursor")
    drawCursorIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "square")
    drawSquareIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "diamond")
    drawDiamondIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "circle")
    drawCircleIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "arrow")
    drawArrowIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "line")
    drawLineIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "pen")
    drawPenIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "text")
    drawTextIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "image")
    drawImageIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "rotate")
    drawRotateIcon(ctx, cx, cy, size * 0.5f);
  else if (iconStr == "tree")
    drawTreeIcon(ctx, cx, cy, size * 0.5f);

  // Draw small number badge for some tools
  if (iconStr == "cursor" || iconStr == "square" || iconStr == "diamond" || iconStr == "circle" ||
      iconStr == "arrow" || iconStr == "line") {
    const char *num = "1";
    if (iconStr == "square")
      num = "2";
    else if (iconStr == "diamond")
      num = "3";
    else if (iconStr == "circle")
      num = "4";
    else if (iconStr == "arrow")
      num = "5";
    else if (iconStr == "line")
      num = "6";

    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, 10.f);
    nvgFillColor(ctx, nvgRGBA(140, 140, 160, 255));
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
    nvgText(ctx, x + size - 4.f, y + size - 2.f, num, nullptr);
  }
}

void ToolbarPanelModule::drawLockIcon(NVGcontext *ctx, float cx, float cy, float size) {
  const float w = size * 0.6f;
  const float h = size * 0.7f;
  const float lockY = cy - h * 0.2f;

  // Lock body
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, cx - w * 0.5f, lockY, w, h * 0.6f, 2.f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Lock shackle
  nvgBeginPath(ctx);
  nvgArc(ctx, cx, lockY, w * 0.35f, 3.14159f, 0.f, NVG_CW);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Keyhole
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, lockY + h * 0.2f, 2.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFill(ctx);
}

void ToolbarPanelModule::drawHandIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  // Palm
  nvgCircle(ctx, cx, cy + size * 0.1f, size * 0.3f);
  // Fingers
  nvgRect(ctx, cx - size * 0.15f, cy - size * 0.3f, size * 0.1f, size * 0.4f);
  nvgRect(ctx, cx + size * 0.05f, cy - size * 0.35f, size * 0.1f, size * 0.45f);
  // Thumb
  nvgCircle(ctx, cx - size * 0.3f, cy + size * 0.05f, size * 0.15f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);
}

void ToolbarPanelModule::drawCursorIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - size * 0.3f, cy - size * 0.3f);
  nvgLineTo(ctx, cx - size * 0.3f, cy + size * 0.4f);
  nvgLineTo(ctx, cx - size * 0.05f, cy + size * 0.1f);
  nvgLineTo(ctx, cx + size * 0.15f, cy + size * 0.35f);
  nvgLineTo(ctx, cx + size * 0.3f, cy + size * 0.25f);
  nvgLineTo(ctx, cx + size * 0.05f, cy);
  nvgClosePath(ctx);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFill(ctx);
}

void ToolbarPanelModule::drawSquareIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgRect(ctx, cx - size * 0.35f, cy - size * 0.35f, size * 0.7f, size * 0.7f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);
}

void ToolbarPanelModule::drawDiamondIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx, cy - size * 0.4f);
  nvgLineTo(ctx, cx + size * 0.4f, cy);
  nvgLineTo(ctx, cx, cy + size * 0.4f);
  nvgLineTo(ctx, cx - size * 0.4f, cy);
  nvgClosePath(ctx);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);
}

void ToolbarPanelModule::drawCircleIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, size * 0.35f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);
}

void ToolbarPanelModule::drawArrowIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - size * 0.3f, cy);
  nvgLineTo(ctx, cx + size * 0.3f, cy);
  nvgMoveTo(ctx, cx + size * 0.15f, cy - size * 0.15f);
  nvgLineTo(ctx, cx + size * 0.3f, cy);
  nvgLineTo(ctx, cx + size * 0.15f, cy + size * 0.15f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgLineCap(ctx, NVG_ROUND);
  nvgLineJoin(ctx, NVG_ROUND);
  nvgStroke(ctx);
}

void ToolbarPanelModule::drawLineIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - size * 0.3f, cy + size * 0.3f);
  nvgLineTo(ctx, cx + size * 0.3f, cy - size * 0.3f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgLineCap(ctx, NVG_ROUND);
  nvgStroke(ctx);
}

void ToolbarPanelModule::drawPenIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - size * 0.3f, cy + size * 0.3f);
  nvgLineTo(ctx, cx + size * 0.1f, cy - size * 0.1f);
  nvgLineTo(ctx, cx + size * 0.3f, cy - size * 0.3f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgLineCap(ctx, NVG_ROUND);
  nvgLineJoin(ctx, NVG_ROUND);
  nvgStroke(ctx);

  // Pen tip
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx - size * 0.35f, cy + size * 0.35f, size * 0.08f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFill(ctx);
}

void ToolbarPanelModule::drawTextIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, size * 0.8f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, cx, cy, "A", nullptr);
}

void ToolbarPanelModule::drawImageIcon(NVGcontext *ctx, float cx, float cy, float size) {
  // Frame
  nvgBeginPath(ctx);
  nvgRect(ctx, cx - size * 0.35f, cy - size * 0.35f, size * 0.7f, size * 0.7f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Mountain
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - size * 0.25f, cy + size * 0.25f);
  nvgLineTo(ctx, cx - size * 0.05f, cy - size * 0.05f);
  nvgLineTo(ctx, cx + size * 0.15f, cy + size * 0.25f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);

  // Sun
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx + size * 0.15f, cy - size * 0.15f, size * 0.1f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFill(ctx);
}

void ToolbarPanelModule::drawRotateIcon(NVGcontext *ctx, float cx, float cy, float size) {
  nvgBeginPath(ctx);
  nvgArc(ctx, cx, cy, size * 0.3f, 0.5f, 5.5f, NVG_CW);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Arrow head
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx + size * 0.25f, cy - size * 0.15f);
  nvgLineTo(ctx, cx + size * 0.35f, cy);
  nvgLineTo(ctx, cx + size * 0.15f, cy - size * 0.05f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFill(ctx);
}

void ToolbarPanelModule::drawTreeIcon(NVGcontext *ctx, float cx, float cy, float size) {
  // Root node
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy - size * 0.25f, size * 0.1f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFill(ctx);

  // Lines
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx, cy - size * 0.15f);
  nvgLineTo(ctx, cx, cy + size * 0.05f);
  nvgMoveTo(ctx, cx, cy);
  nvgLineTo(ctx, cx - size * 0.2f, cy + size * 0.2f);
  nvgMoveTo(ctx, cx, cy);
  nvgLineTo(ctx, cx + size * 0.2f, cy + size * 0.2f);
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Child nodes
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx - size * 0.2f, cy + size * 0.25f, size * 0.08f);
  nvgCircle(ctx, cx + size * 0.2f, cy + size * 0.25f, size * 0.08f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFill(ctx);
}
