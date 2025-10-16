#include "whiteboard/text_panel_module.h"
#include <algorithm>
TextPanelModule::TextPanelModule(Widget *parent) : Widget(parent) {
  const float padding = 20.f;
  const float buttonSize = 48.f;
  const float spacing = 8.f;

  // Stroke colors (插边)
  float y = 60.f;
  std::vector<NVGcolor> strokeCols = {nvgRGBA(40, 40, 45, 255),   nvgRGBA(220, 60, 60, 255),
                                      nvgRGBA(60, 180, 100, 255), nvgRGBA(60, 120, 220, 255),
                                      nvgRGBA(240, 160, 40, 255), nvgRGBA(30, 30, 35, 255)};
  for (size_t i = 0; i < strokeCols.size(); i++) {
    float x = padding + i * (buttonSize + spacing);
    m_strokeColors.push_back({x, y, buttonSize, strokeCols[i], i == 0, false, static_cast<int>(i)});
  }

  // Background colors (背景)
  y += 100.f;
  std::vector<NVGcolor> bgCols = {nvgRGBA(255, 255, 255, 255), nvgRGBA(255, 200, 200, 255),
                                  nvgRGBA(200, 255, 200, 255), nvgRGBA(200, 220, 255, 255),
                                  nvgRGBA(255, 255, 200, 255), nvgRGBA(240, 240, 245, 255)};
  for (size_t i = 0; i < bgCols.size(); i++) {
    float x = padding + i * (buttonSize + spacing);
    bool hasPattern = (i == 5);
    m_bgColors.push_back({x, y, buttonSize, bgCols[i], i == 0, hasPattern, static_cast<int>(i)});
  }

  // Stroke width (插边宽度)
  y += 100.f;
  for (int i = 0; i < 3; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_strokeWidthButtons.push_back({x, y, buttonSize, "width", i == 1, i});
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

  // Font style (字体) - 4 buttons
  y += 100.f;
  for (int i = 0; i < 4; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_fontButtons.push_back({x, y, buttonSize, "font", i == 0, i});
  }

  // Text size (字体大小) - 4 buttons: S, M, L, XL
  y += 100.f;
  for (int i = 0; i < 4; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_textSizeButtons.push_back({x, y, buttonSize, "size", i == 1, i});
  }

  // Text alignment (文本对齐) - 3 buttons
  y += 100.f;
  for (int i = 0; i < 3; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_alignButtons.push_back({x, y, buttonSize, "align", i == 0, i});
  }

  // Layer controls (图层)
  y += 180.f; // Extra space for opacity slider
  for (int i = 0; i < 4; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_layerButtons.push_back({x, y, buttonSize, "layer", false, i});
  }

  m_opacityValue = 0.8f;
  m_dragging = false;
  m_draggingSlider = false;
  m_sliderY = 0.f;
}

Vector2i TextPanelModule::preferred_size_impl(NVGcontext *) const {
  return {370, 1200}; // Vertical panel
}

bool TextPanelModule::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
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

        // Call color callback
        if (m_color_callback) {
          m_color_callback(btn.color);
        }

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

    // Check font buttons
    for (size_t i = 0; i < m_fontButtons.size(); i++) {
      auto &btn = m_fontButtons[i];
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_fontButtons)
          b.selected = false;
        btn.selected = true;

        // Call font face callback
        if (m_font_face_callback) {
          std::string fonts[] = {"sans", "sans-bold", "mono", "sans-bold"};
          m_font_face_callback(fonts[i]);
        }

        screen()->redraw();
        return true;
      }
    }

    // Check text size buttons
    for (size_t i = 0; i < m_textSizeButtons.size(); i++) {
      auto &btn = m_textSizeButtons[i];
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_textSizeButtons)
          b.selected = false;
        btn.selected = true;

        // Call font size callback
        if (m_font_size_callback) {
          float sizes[] = {12.0f, 16.0f, 20.0f, 24.0f}; // S, M, L, XL
          m_font_size_callback(sizes[i]);
        }

        screen()->redraw();
        return true;
      }
    }

    // Check alignment buttons
    for (size_t i = 0; i < m_alignButtons.size(); i++) {
      auto &btn = m_alignButtons[i];
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        for (auto &b : m_alignButtons)
          b.selected = false;
        btn.selected = true;

        // Call alignment callback
        if (m_align_callback) {
          int aligns[] = {NVG_ALIGN_LEFT | NVG_ALIGN_TOP, NVG_ALIGN_CENTER | NVG_ALIGN_TOP,
                          NVG_ALIGN_RIGHT | NVG_ALIGN_TOP};
          m_align_callback(aligns[i]);
        }

        screen()->redraw();
        return true;
      }
    }

    // Check layer buttons
    for (auto &btn : m_layerButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        screen()->redraw();
        return true;
      }
    }

    // Check slider
    float sliderX = 30.f;
    float sliderW = 310.f;
    float sliderY = m_sliderY;
    if (localPos.x() >= sliderX && localPos.x() <= sliderX + sliderW &&
        localPos.y() >= sliderY - 10.f && localPos.y() <= sliderY + 10.f) {
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

bool TextPanelModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
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

void TextPanelModule::draw(NVGcontext *ctx) {
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
    drawColorButton(ctx, px + btn.x, py + btn.y, btn.size, btn.color, btn.selected, true, false);
  }
  yOffset += 80.f;

  // Background section (背景)
  drawLabel(ctx, px + 20.f, py + yOffset, "背景");
  yOffset += 30.f;
  for (const auto &btn : m_bgColors) {
    drawColorButton(ctx, px + btn.x, py + btn.y, btn.size, btn.color, btn.selected, false,
                    btn.hasPattern);
  }
  yOffset += 80.f;

  // Stroke width section (插边宽度)
  drawLabel(ctx, px + 20.f, py + yOffset, "插边宽度");
  yOffset += 30.f;
  for (size_t i = 0; i < m_strokeWidthButtons.size(); i++) {
    const auto &btn = m_strokeWidthButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawStrokeWidthIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f,
                        btn.size * 0.4f, static_cast<int>(i));
  }
  yOffset += 80.f;

  // Border style section (边框样式)
  drawLabel(ctx, px + 20.f, py + yOffset, "边框样式");
  yOffset += 30.f;
  for (size_t i = 0; i < m_borderStyleButtons.size(); i++) {
    const auto &btn = m_borderStyleButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawBorderStyleIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f,
                        btn.size * 0.4f, static_cast<int>(i));
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
    drawCornerIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f, btn.size * 0.4f,
                   i == 1);
  }
  yOffset += 80.f;

  // Font section (字体)
  drawLabel(ctx, px + 20.f, py + yOffset, "字体");
  yOffset += 30.f;
  for (size_t i = 0; i < m_fontButtons.size(); i++) {
    const auto &btn = m_fontButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawFontIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f, btn.size * 0.4f,
                 static_cast<int>(i));
  }
  yOffset += 80.f;

  // Text size section (字体大小)
  drawLabel(ctx, px + 20.f, py + yOffset, "字体大小");
  yOffset += 30.f;
  const char *sizeLabels[] = {"S", "M", "L", "XL"};
  for (size_t i = 0; i < m_textSizeButtons.size(); i++) {
    const auto &btn = m_textSizeButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawTextSizeIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f,
                     btn.size * 0.4f, sizeLabels[i]);
  }
  yOffset += 80.f;

  // Text alignment section (文本对齐)
  drawLabel(ctx, px + 20.f, py + yOffset, "文本对齐");
  yOffset += 30.f;
  for (size_t i = 0; i < m_alignButtons.size(); i++) {
    const auto &btn = m_alignButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawAlignIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f, btn.size * 0.4f,
                  static_cast<int>(i));
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

void TextPanelModule::drawLabel(NVGcontext *ctx, float x, float y, const char *text) {
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(80, 80, 90, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, x, y, text, nullptr);
}

void TextPanelModule::drawColorButton(NVGcontext *ctx, float x, float y, float size, NVGcolor color,
                                      bool selected, bool hasBorder, bool hasPattern) {
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

  if (hasPattern) {
    nvgFillColor(ctx, nvgRGBA(220, 220, 225, 255));
    nvgFill(ctx);
    float cellSize = colorSize / 4.f;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if ((i + j) % 2 == 0) {
          nvgBeginPath(ctx);
          nvgRect(ctx, colorX + i * cellSize, colorY + j * cellSize, cellSize, cellSize);
          nvgFillColor(ctx, nvgRGBA(240, 240, 245, 255));
          nvgFill(ctx);
        }
      }
    }
  } else {
    nvgFillColor(ctx, color);
    nvgFill(ctx);
  }

  if (hasBorder) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, colorX, colorY, colorSize, colorSize, 6.f);
    nvgStrokeColor(ctx, nvgRGBA(200, 200, 210, 255));
    nvgStrokeWidth(ctx, 2.f);
    nvgStroke(ctx);
  }

  if (selected) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, size, size, 8.f);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 220, 255));
    nvgStrokeWidth(ctx, 2.f);
    nvgStroke(ctx);
  }
}

void TextPanelModule::drawIconButton(NVGcontext *ctx, float x, float y, float size,
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

void TextPanelModule::drawSlider(NVGcontext *ctx, float x, float y, float w, float value,
                                 const char *minLabel, const char *maxLabel) {
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y - 2.f, w, 4.f, 2.f);
  nvgFillColor(ctx, nvgRGBA(220, 220, 240, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y - 2.f, w * value, 4.f, 2.f);
  nvgFillColor(ctx, nvgRGBA(180, 180, 220, 255));
  nvgFill(ctx);

  float thumbX = x + w * value;
  nvgBeginPath(ctx);
  nvgCircle(ctx, thumbX, y, 12.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 50, 255));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 12.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 130, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, x, y + 20.f, minLabel, nullptr);
  nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
  nvgText(ctx, x + w, y + 20.f, maxLabel, nullptr);
}

void TextPanelModule::drawStrokeWidthIcon(NVGcontext *ctx, float cx, float cy, float size,
                                          int type) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgLineCap(ctx, NVG_ROUND);

  if (type == 0) {
    nvgStrokeWidth(ctx, 2.f);
  } else if (type == 1) {
    nvgStrokeWidth(ctx, 4.f);
  } else {
    nvgStrokeWidth(ctx, 6.f);
  }

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - size, cy);
  nvgLineTo(ctx, cx + size, cy);
  nvgStroke(ctx);
}

void TextPanelModule::drawBorderStyleIcon(NVGcontext *ctx, float cx, float cy, float size,
                                          int type) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 3.f);

  if (type == 0) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx + size, cy);
    nvgStroke(ctx);
  } else if (type == 1) {
    for (int i = -1; i <= 1; i++) {
      nvgBeginPath(ctx);
      nvgCircle(ctx, cx + i * size * 0.5f, cy, 2.5f);
      nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
      nvgFill(ctx);
    }
  } else {
    for (int i = -2; i <= 2; i++) {
      nvgBeginPath(ctx);
      nvgCircle(ctx, cx + i * size * 0.4f, cy, 1.5f);
      nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
      nvgFill(ctx);
    }
  }
}

void TextPanelModule::drawLineStyleIcon(NVGcontext *ctx, float cx, float cy, float size, int type) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.5f);
  nvgLineCap(ctx, NVG_ROUND);

  if (type == 0) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx + size, cy);
    nvgStroke(ctx);
  } else if (type == 1) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy + size * 0.3f);
    nvgBezierTo(ctx, cx - size * 0.3f, cy - size * 0.5f, cx + size * 0.3f, cy - size * 0.5f,
                cx + size, cy + size * 0.3f);
    nvgStroke(ctx);
  } else {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy + size * 0.3f);
    nvgLineTo(ctx, cx - size * 0.3f, cy - size * 0.3f);
    nvgLineTo(ctx, cx + size * 0.3f, cy + size * 0.3f);
    nvgLineTo(ctx, cx + size, cy - size * 0.3f);
    nvgStroke(ctx);
  }
}

void TextPanelModule::drawCornerIcon(NVGcontext *ctx, float cx, float cy, float size,
                                     bool rounded) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.5f);

  if (rounded) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx - size * 0.3f, cy);
    nvgArcTo(ctx, cx + size * 0.3f, cy, cx + size * 0.3f, cy + size, size * 0.6f);
    nvgLineTo(ctx, cx + size * 0.3f, cy + size);
    nvgStroke(ctx);
  } else {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size, cy);
    nvgLineTo(ctx, cx, cy);
    nvgLineTo(ctx, cx, cy + size);
    nvgStroke(ctx);
  }

  nvgStrokeColor(ctx, nvgRGBA(120, 120, 140, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgBeginPath(ctx);
  nvgRect(ctx, cx - size * 0.8f, cy - size * 0.8f, size * 1.6f, size * 1.6f);
  nvgStroke(ctx);
}

void TextPanelModule::drawFontIcon(NVGcontext *ctx, float cx, float cy, float size, int type) {
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);

  if (type == 0) {
    // Pen/Edit icon
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.3f, cy + size * 0.3f);
    nvgLineTo(ctx, cx + size * 0.1f, cy - size * 0.1f);
    nvgLineTo(ctx, cx + size * 0.3f, cy - size * 0.3f);
    nvgStroke(ctx);
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx - size * 0.35f, cy + size * 0.35f, size * 0.08f);
    nvgFill(ctx);
  } else if (type == 1) {
    // A icon (text)
    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, size * 1.2f);
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, cx, cy, "A", nullptr);
  } else if (type == 2) {
    // Code icon </>
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, size * 0.9f);
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, cx, cy, "</>", nullptr);
  } else {
    // Large A icon
    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, size * 1.4f);
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, cx, cy, "A", nullptr);
  }
}

void TextPanelModule::drawTextSizeIcon(NVGcontext *ctx, float cx, float cy, float size,
                                       const char *label) {
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, size * 0.9f);
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, cx, cy, label, nullptr);
}

void TextPanelModule::drawAlignIcon(NVGcontext *ctx, float cx, float cy, float size, int type) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgLineCap(ctx, NVG_ROUND);

  float lineSpacing = size * 0.4f;

  if (type == 0) {
    // Left align
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.6f, cy - lineSpacing);
    nvgLineTo(ctx, cx + size * 0.3f, cy - lineSpacing);
    nvgMoveTo(ctx, cx - size * 0.6f, cy);
    nvgLineTo(ctx, cx + size * 0.6f, cy);
    nvgMoveTo(ctx, cx - size * 0.6f, cy + lineSpacing);
    nvgLineTo(ctx, cx + size * 0.2f, cy + lineSpacing);
    nvgStroke(ctx);
  } else if (type == 1) {
    // Center align
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.3f, cy - lineSpacing);
    nvgLineTo(ctx, cx + size * 0.3f, cy - lineSpacing);
    nvgMoveTo(ctx, cx - size * 0.6f, cy);
    nvgLineTo(ctx, cx + size * 0.6f, cy);
    nvgMoveTo(ctx, cx - size * 0.2f, cy + lineSpacing);
    nvgLineTo(ctx, cx + size * 0.2f, cy + lineSpacing);
    nvgStroke(ctx);
  } else {
    // Right align
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.3f, cy - lineSpacing);
    nvgLineTo(ctx, cx + size * 0.6f, cy - lineSpacing);
    nvgMoveTo(ctx, cx - size * 0.6f, cy);
    nvgLineTo(ctx, cx + size * 0.6f, cy);
    nvgMoveTo(ctx, cx - size * 0.2f, cy + lineSpacing);
    nvgLineTo(ctx, cx + size * 0.6f, cy + lineSpacing);
    nvgStroke(ctx);
  }
}

void TextPanelModule::drawLayerIcon(NVGcontext *ctx, float cx, float cy, float size,
                                    int direction) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.5f);

  if (direction == 0) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy - size * 0.5f);
    nvgLineTo(ctx, cx, cy + size * 0.5f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy + size * 0.2f);
    nvgLineTo(ctx, cx, cy + size * 0.5f);
    nvgLineTo(ctx, cx + size * 0.3f, cy + size * 0.2f);
    nvgStroke(ctx);
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.5f, cy + size * 0.6f);
    nvgLineTo(ctx, cx + size * 0.5f, cy + size * 0.6f);
    nvgStroke(ctx);
  } else if (direction == 1) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy - size * 0.3f);
    nvgLineTo(ctx, cx, cy + size * 0.3f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy);
    nvgLineTo(ctx, cx, cy + size * 0.3f);
    nvgLineTo(ctx, cx + size * 0.3f, cy);
    nvgStroke(ctx);
  } else if (direction == 2) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy + size * 0.3f);
    nvgLineTo(ctx, cx, cy - size * 0.3f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy);
    nvgLineTo(ctx, cx, cy - size * 0.3f);
    nvgLineTo(ctx, cx + size * 0.3f, cy);
    nvgStroke(ctx);
  } else {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy + size * 0.5f);
    nvgLineTo(ctx, cx, cy - size * 0.5f);
    nvgMoveTo(ctx, cx - size * 0.3f, cy - size * 0.2f);
    nvgLineTo(ctx, cx, cy - size * 0.5f);
    nvgLineTo(ctx, cx + size * 0.3f, cy - size * 0.2f);
    nvgStroke(ctx);
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.5f, cy - size * 0.6f);
    nvgLineTo(ctx, cx + size * 0.5f, cy - size * 0.6f);
    nvgStroke(ctx);
  }
}
