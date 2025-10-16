#include "properties_panel_module.h"
#include <cmath>
#include <algorithm>
PropertiesPanelModule::PropertiesPanelModule(Widget *parent) : Widget(parent) {
  const float padding = 20.f;
  const float buttonSize = 48.f;
  const float spacing = 8.f;

  // Stroke colors (插边)
  float y = 60.f;
  std::vector<NVGcolor> strokeCols = {
      nvgRGBA(40, 40, 45, 255),   nvgRGBA(220, 60, 60, 255),  nvgRGBA(60, 180, 100, 255),
      nvgRGBA(60, 120, 220, 255), nvgRGBA(240, 160, 40, 255), nvgRGBA(30, 30, 35, 255)};
  for (size_t i = 0; i < strokeCols.size(); i++) {
    float x = padding + i * (buttonSize + spacing);
    m_strokeColors.push_back({x, y, buttonSize, strokeCols[i], i == 0, static_cast<int>(i)});
  }

  // Background colors (背景)
  y += 100.f;
  std::vector<NVGcolor> bgCols = {nvgRGBA(255, 255, 255, 255), nvgRGBA(255, 200, 200, 255),
                                  nvgRGBA(200, 255, 200, 255), nvgRGBA(200, 220, 255, 255),
                                  nvgRGBA(255, 255, 200, 255), nvgRGBA(240, 240, 245, 255)};
  for (size_t i = 0; i < bgCols.size(); i++) {
    float x = padding + i * (buttonSize + spacing);
    m_bgColors.push_back({x, y, buttonSize, bgCols[i], i == 0, static_cast<int>(i)});
  }

  // Stroke width (插边宽度)
  y += 100.f;
  for (int i = 0; i < 3; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_strokeWidthButtons.push_back({x, y, buttonSize, "dash", i == 1, i});
  }

  // Border style (边框样式)
  y += 100.f;
  for (int i = 0; i < 3; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_borderStyleButtons.push_back({x, y, buttonSize, "border", i == 0, i});
  }

  // Line style (线条风格)
  y += 100.f;
  for (int i = 0; i < 3; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_lineStyleButtons.push_back({x, y, buttonSize, "line", i == 1, i});
  }

  // Corner style (边角)
  y += 100.f;
  for (int i = 0; i < 2; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_cornerButtons.push_back({x, y, buttonSize, "corner", i == 1, i});
  }

  // Layer controls (图层)
  y += 100.f;
  for (int i = 0; i < 4; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_layerButtons.push_back({x, y, buttonSize, "layer", false, i});
  }

  m_opacityValue = 0.8f;
  m_dragging = false;
  m_draggingSlider = false;
  m_sliderY = y - 100.f;
}

Vector2i PropertiesPanelModule::preferred_size_impl(NVGcontext *) const {
  return {370, 1000}; // Vertical panel
}

bool PropertiesPanelModule::mouse_button_event(const Vector2i &p, int button, bool down,
                                               int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

    // Check stroke colors
    for (auto &btn : m_strokeColors) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_strokeColors)
          b.selected = false;
        btn.selected = true;
        screen()->redraw();
        return true;
      }
    }

    // Check background colors
    for (auto &btn : m_bgColors) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_bgColors)
          b.selected = false;
        btn.selected = true;
        screen()->redraw();
        return true;
      }
    }

    // Check stroke width buttons
    for (auto &btn : m_strokeWidthButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_strokeWidthButtons)
          b.selected = false;
        btn.selected = true;
        screen()->redraw();
        return true;
      }
    }

    // Check border style buttons
    for (auto &btn : m_borderStyleButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_borderStyleButtons)
          b.selected = false;
        btn.selected = true;
        screen()->redraw();
        return true;
      }
    }

    // Check line style buttons
    for (auto &btn : m_lineStyleButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_lineStyleButtons)
          b.selected = false;
        btn.selected = true;
        screen()->redraw();
        return true;
      }
    }

    // Check corner buttons
    for (auto &btn : m_cornerButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_cornerButtons)
          b.selected = false;
        btn.selected = true;
        screen()->redraw();
        return true;
      }
    }

    // Check layer buttons (these don't deselect others - can click multiple times)
    for (auto &btn : m_layerButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        // Layer buttons are action buttons, just trigger an action
        screen()->redraw();
        return true;
      }
    }

    // Check slider
    float sliderX = 30.f;
    float sliderW = 310.f;
    float sliderY = m_sliderY;
    if (localPos.x() >= sliderX && localPos.x() <= sliderX + sliderW && localPos.y() >= sliderY - 10.f &&
        localPos.y() <= sliderY + 10.f) {
      m_draggingSlider = true;
      float t = (localPos.x() - sliderX) / sliderW;
      m_opacityValue = std::clamp(t, 0.f, 1.f);
      screen()->redraw();
      return true;
    }

    // Start dragging panel
    m_dragging = true;
    m_dragStart = p;
    return true;
  } else if (button == GLFW_MOUSE_BUTTON_1 && !down) {
    m_dragging = false;
    m_draggingSlider = false;
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

bool PropertiesPanelModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                             int modifiers) {
  if (m_draggingSlider) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());
    float sliderX = 30.f;
    float sliderW = 310.f;
    float t = (localPos.x() - sliderX) / sliderW;
    m_opacityValue = std::clamp(t, 0.f, 1.f);
    screen()->redraw();
    return true;
  }

  if (m_dragging) {
    Vector2i newPos = m_pos + rel;
    set_position(newPos);
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

void PropertiesPanelModule::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  // Draw rounded panel background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 16.f);
  nvgFillColor(ctx, nvgRGBA(248, 248, 252, 255));
  nvgFill(ctx);

  float yOffset = 20.f;

  // Stroke section (插边)
  drawLabel(ctx, px + 20.f, py + yOffset, "插边");
  yOffset += 30.f;
  for (const auto &btn : m_strokeColors) {
    drawColorButton(ctx, px + btn.x, py + btn.y, btn.size, btn.color, btn.selected, true);
  }
  yOffset += 80.f;

  // Background section (背景)
  drawLabel(ctx, px + 20.f, py + yOffset, "背景");
  yOffset += 30.f;
  for (const auto &btn : m_bgColors) {
    drawColorButton(ctx, px + btn.x, py + btn.y, btn.size, btn.color, btn.selected, false);
  }
  yOffset += 80.f;

  // Stroke width section (插边宽度)
  drawLabel(ctx, px + 20.f, py + yOffset, "插边宽度");
  yOffset += 30.f;
  for (size_t i = 0; i < m_strokeWidthButtons.size(); i++) {
    const auto &btn = m_strokeWidthButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawDashIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f, btn.size * 0.4f,
                 static_cast<int>(i));
  }
  yOffset += 80.f;

  // Border style section (边框样式)
  drawLabel(ctx, px + 20.f, py + yOffset, "边框样式");
  yOffset += 30.f;
  for (size_t i = 0; i < m_borderStyleButtons.size(); i++) {
    const auto &btn = m_borderStyleButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawDashIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f, btn.size * 0.4f,
                 static_cast<int>(i));
  }
  yOffset += 80.f;

  // Line style section (线条风格)
  drawLabel(ctx, px + 20.f, py + yOffset, "线条风格");
  yOffset += 30.f;
  for (size_t i = 0; i < m_lineStyleButtons.size(); i++) {
    const auto &btn = m_lineStyleButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawLineStyleIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f,
                      btn.size * 0.4f, static_cast<int>(i));
  }
  yOffset += 80.f;

  // Corner section (边角)
  drawLabel(ctx, px + 20.f, py + yOffset, "边角");
  yOffset += 30.f;
  for (size_t i = 0; i < m_cornerButtons.size(); i++) {
    const auto &btn = m_cornerButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawCornerIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f,
                   btn.size * 0.4f, i == 1);
  }
  yOffset += 80.f;

  // Opacity slider (透明度)
  drawLabel(ctx, px + 20.f, py + yOffset, "透明度");
  yOffset += 30.f;
  m_sliderY = yOffset;
  drawSlider(ctx, px + 30.f, py + yOffset, 310.f, m_opacityValue, "0", "100");
  yOffset += 80.f;

  // Layer section (图层)
  drawLabel(ctx, px + 20.f, py + yOffset, "图层");
  yOffset += 30.f;
  for (size_t i = 0; i < m_layerButtons.size(); i++) {
    const auto &btn = m_layerButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawLayerIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f, btn.size * 0.4f,
                  static_cast<int>(i));
  }
}

void PropertiesPanelModule::drawLabel(NVGcontext *ctx, float x, float y, const char *text) {
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(80, 80, 90, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, x, y, text, nullptr);
}

void PropertiesPanelModule::drawColorButton(NVGcontext *ctx, float x, float y, float size,
                                            NVGcolor color, bool selected, bool hasBorder) {
  // Button background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, size, size, 8.f);

  if (selected) {
    nvgFillColor(ctx, nvgRGBA(220, 220, 245, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(248, 248, 252, 0));
  }
  nvgFill(ctx);

  // Color square
  float colorSize = size * 0.7f;
  float colorX = x + (size - colorSize) * 0.5f;
  float colorY = y + (size - colorSize) * 0.5f;

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, colorX, colorY, colorSize, colorSize, 6.f);
  nvgFillColor(ctx, color);
  nvgFill(ctx);

  // Border for stroke colors
  if (hasBorder) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, colorX, colorY, colorSize, colorSize, 6.f);
    nvgStrokeColor(ctx, nvgRGBA(200, 200, 210, 255));
    nvgStrokeWidth(ctx, 2.f);
    nvgStroke(ctx);
  }

  // Selection border
  if (selected) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, size, size, 8.f);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 220, 255));
    nvgStrokeWidth(ctx, 2.f);
    nvgStroke(ctx);
  }
}

void PropertiesPanelModule::drawIconButton(NVGcontext *ctx, float x, float y, float size,
                                           const char *icon, bool selected) {
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, size, size, 8.f);

  if (selected) {
    nvgFillColor(ctx, nvgRGBA(220, 220, 245, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(240, 240, 245, 255));
  }
  nvgFill(ctx);

  if (selected) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, size, size, 8.f);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 220, 255));
    nvgStrokeWidth(ctx, 2.f);
    nvgStroke(ctx);
  }
}

void PropertiesPanelModule::drawSlider(NVGcontext *ctx, float x, float y, float w, float value,
                                       const char *minLabel, const char *maxLabel) {
  // Track
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y - 2.f, w, 4.f, 2.f);
  nvgFillColor(ctx, nvgRGBA(220, 220, 240, 255));
  nvgFill(ctx);

  // Filled track
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y - 2.f, w * value, 4.f, 2.f);
  nvgFillColor(ctx, nvgRGBA(180, 180, 220, 255));
  nvgFill(ctx);

  // Thumb
  float thumbX = x + w * value;
  nvgBeginPath(ctx);
  nvgCircle(ctx, thumbX, y, 12.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 50, 255));
  nvgFill(ctx);

  // Labels
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 12.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 130, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, x, y + 20.f, minLabel, nullptr);
  nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
  nvgText(ctx, x + w, y + 20.f, maxLabel, nullptr);
}

void PropertiesPanelModule::drawDashIcon(NVGcontext *ctx, float cx, float cy, float size,
                                         int dashType) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 3.f);

  if (dashType == 0) {
    // Solid line
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx + size, cy);
    nvgStroke(ctx);
  } else if (dashType == 1) {
    // Medium dash
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx - size * 0.3f, cy);
    nvgMoveTo(ctx, cx - size * 0.1f, cy);
    nvgLineTo(ctx, cx + size * 0.1f, cy);
    nvgMoveTo(ctx, cx + size * 0.3f, cy);
    nvgLineTo(ctx, cx + size, cy);
    nvgStroke(ctx);
  } else {
    // Dotted
    for (int i = -2; i <= 2; i++) {
      nvgBeginPath(ctx);
      nvgCircle(ctx, cx + i * size * 0.4f, cy, 2.f);
      nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
      nvgFill(ctx);
    }
  }
}

void PropertiesPanelModule::drawLineStyleIcon(NVGcontext *ctx, float cx, float cy, float size,
                                              int styleType) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.5f);
  nvgLineCap(ctx, NVG_ROUND);

  if (styleType == 0) {
    // Straight
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx + size, cy);
    nvgStroke(ctx);
  } else if (styleType == 1) {
    // Curved
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy + size * 0.3f);
    nvgBezierTo(ctx, cx - size * 0.3f, cy - size * 0.5f, cx + size * 0.3f, cy - size * 0.5f,
                cx + size, cy + size * 0.3f);
    nvgStroke(ctx);
  } else {
    // Zigzag
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy + size * 0.3f);
    nvgLineTo(ctx, cx - size * 0.3f, cy - size * 0.3f);
    nvgLineTo(ctx, cx + size * 0.3f, cy + size * 0.3f);
    nvgLineTo(ctx, cx + size, cy - size * 0.3f);
    nvgStroke(ctx);
  }
}

void PropertiesPanelModule::drawCornerIcon(NVGcontext *ctx, float cx, float cy, float size,
                                           bool rounded) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.5f);

  if (rounded) {
    // Rounded corner
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx - size * 0.3f, cy);
    nvgArcTo(ctx, cx + size * 0.3f, cy, cx + size * 0.3f, cy + size, size * 0.6f);
    nvgLineTo(ctx, cx + size * 0.3f, cy + size);
    nvgStroke(ctx);
  } else {
    // Sharp corner
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx, cy);
    nvgLineTo(ctx, cx, cy + size);
    nvgStroke(ctx);
  }

  // Dashed border
  nvgStrokeColor(ctx, nvgRGBA(120, 120, 140, 255));
  nvgStrokeWidth(ctx, 1.5f);
  float dashes[2] = {3.f, 3.f};
  nvgBeginPath(ctx);
  nvgRect(ctx, cx - size * 0.8f, cy - size * 0.8f, size * 1.6f, size * 1.6f);
  nvgStroke(ctx);
}

void PropertiesPanelModule::drawLayerIcon(NVGcontext *ctx, float cx, float cy, float size,
                                          int direction) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.5f);

  if (direction == 0) {
    // Move to bottom
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy - size * 0.5f);
    nvgLineTo(ctx, cx, cy + size * 0.5f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy + size * 0.2f);
    nvgLineTo(ctx, cx, cy + size * 0.5f);
    nvgLineTo(ctx, cx + size * 0.3f, cy + size * 0.2f);
    nvgStroke(ctx);
    // Bottom line
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.5f, cy + size * 0.6f);
    nvgLineTo(ctx, cx + size * 0.5f, cy + size * 0.6f);
    nvgStroke(ctx);
  } else if (direction == 1) {
    // Move down
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy - size * 0.3f);
    nvgLineTo(ctx, cx, cy + size * 0.3f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy);
    nvgLineTo(ctx, cx, cy + size * 0.3f);
    nvgLineTo(ctx, cx + size * 0.3f, cy);
    nvgStroke(ctx);
  } else if (direction == 2) {
    // Move up
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy + size * 0.3f);
    nvgLineTo(ctx, cx, cy - size * 0.3f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy);
    nvgLineTo(ctx, cx, cy - size * 0.3f);
    nvgLineTo(ctx, cx + size * 0.3f, cy);
    nvgStroke(ctx);
  } else {
    // Move to top
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy + size * 0.5f);
    nvgLineTo(ctx, cx, cy - size * 0.5f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy - size * 0.2f);
    nvgLineTo(ctx, cx, cy - size * 0.5f);
    nvgLineTo(ctx, cx + size * 0.3f, cy - size * 0.2f);
    nvgStroke(ctx);
    // Top line
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.5f, cy - size * 0.6f);
    nvgLineTo(ctx, cx + size * 0.5f, cy - size * 0.6f);
    nvgStroke(ctx);
  }
}
