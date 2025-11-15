#include "whiteboard/properties_panel_module.h"
#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/panels/properties_controller.h"
#include "whiteboard/types.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PropertiesPanelModule::PropertiesPanelModule(Widget *parent) : Widget(parent) {
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
  y += 100.f;
  for (int i = 0; i < 4; i++) {
    float x = padding + i * (buttonSize + spacing);
    m_layerButtons.push_back({x, y, buttonSize, "layer", false, i});
  }

  m_opacityValue = 0.8f;
  m_rotationValue = 0.0f;
  m_scaleXValue = 0.5f;  // 1.0 scale = 0.5 slider position (range 0.1-3.0)
  m_scaleYValue = 0.5f;
  m_dragging = false;
  m_draggingSlider = false;
  m_draggingScaleXSlider = false;
  m_draggingScaleYSlider = false;
  m_draggingRotationSlider = false;
  m_sliderY = y - 100.f;
  m_rotationSliderY = y;

  // SVG support
  m_current_tab = Tab::Properties;
  m_is_svg_shape = false;
}

void PropertiesPanelModule::set_svg_shape(bool is_svg, const std::string &shape_id,
                                          const std::map<std::string, std::string> &params) {
  m_is_svg_shape = is_svg;
  m_svg_shape_id = shape_id;
  m_svg_parameters = params;

  // Reset to properties tab when selection changes
  if (!is_svg) {
    m_current_tab = Tab::Properties;
  }
}

Vector2i PropertiesPanelModule::preferred_size_impl(NVGcontext *) const {
  return {370, 1000}; // Vertical panel
}

bool PropertiesPanelModule::mouse_button_event(const Vector2i &p, int button, bool down,
                                               int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

    // Check tab clicks (if SVG shape)
    if (m_is_svg_shape && localPos.y() >= 10.f && localPos.y() <= 45.f) {
      float tab_width = 160.f;
      if (localPos.x() >= 20.f && localPos.x() <= 20.f + tab_width) {
        m_current_tab = Tab::Properties;
        screen()->redraw();
        return true;
      } else if (localPos.x() >= 25.f + tab_width && localPos.x() <= 25.f + tab_width * 2) {
        m_current_tab = Tab::SVGParameters;
        screen()->redraw();
        return true;
      }
    }

    // Check stroke colors
    for (auto &btn : m_strokeColors) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_stroke_color_click(btn.id);
        return true;
      }
    }

    // Check background colors
    for (auto &btn : m_bgColors) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_fill_color_click(btn.id);
        return true;
      }
    }

    // Check stroke width buttons
    for (auto &btn : m_strokeWidthButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_stroke_width_click(btn.id);
        return true;
      }
    }

    // Check border style buttons
    for (auto &btn : m_borderStyleButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_stroke_style_click(btn.id);
        return true;
      }
    }

    // Check line style buttons (currently not used - same as border style)
    for (auto &btn : m_lineStyleButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        // Line style - for now just update selection visually
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
        handle_corner_click(btn.id);
        return true;
      }
    }

    // Check font buttons
    for (auto &btn : m_fontButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_font_click(btn.id);
        return true;
      }
    }

    // Check text size buttons
    for (auto &btn : m_textSizeButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_text_size_click(btn.id);
        return true;
      }
    }

    // Check alignment buttons
    for (auto &btn : m_alignButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_align_click(btn.id);
        return true;
      }
    }

    // Check layer buttons
    for (auto &btn : m_layerButtons) {
      float dx = localPos.x() - (btn.x + btn.size * 0.5f);
      float dy = localPos.y() - (btn.y + btn.size * 0.5f);
      if (std::abs(dx) <= btn.size * 0.5f && std::abs(dy) <= btn.size * 0.5f) {
        handle_layer_click(btn.id);
        return true;
      }
    }

    // Check opacity slider
    float sliderX = 30.f;
    float sliderW = 310.f;
    float sliderY = m_sliderY;
    if (localPos.x() >= sliderX && localPos.x() <= sliderX + sliderW &&
        localPos.y() >= sliderY - 10.f && localPos.y() <= sliderY + 10.f) {
      m_draggingSlider = true;
      float t = (localPos.x() - sliderX) / sliderW;
      float value = std::clamp(t, 0.f, 1.f);
      handle_opacity_change(value);
      return true;
    }

    // Check rotation slider
    float rotationSliderY = m_rotationSliderY;
    if (localPos.x() >= sliderX && localPos.x() <= sliderX + sliderW &&
        localPos.y() >= rotationSliderY - 10.f && localPos.y() <= rotationSliderY + 10.f) {
      m_draggingRotationSlider = true;
      float t = (localPos.x() - sliderX) / sliderW;
      float value = std::clamp(t, 0.f, 1.f);
      handle_rotation_change(value);
      return true;
    }

    // Check scale X slider (only for SVG shapes)
    if (m_is_svg_shape) {
      float scaleXSliderY = m_scaleXSliderY;
      if (localPos.x() >= sliderX && localPos.x() <= sliderX + sliderW &&
          localPos.y() >= scaleXSliderY - 10.f && localPos.y() <= scaleXSliderY + 10.f) {
        m_draggingScaleXSlider = true;
        float t = (localPos.x() - sliderX) / sliderW;
        float value = std::clamp(t, 0.f, 1.f);
        handle_scale_x_change(value);
        return true;
      }

      // Check scale Y slider
      float scaleYSliderY = m_scaleYSliderY;
      if (localPos.x() >= sliderX && localPos.x() <= sliderX + sliderW &&
          localPos.y() >= scaleYSliderY - 10.f && localPos.y() <= scaleYSliderY + 10.f) {
        m_draggingScaleYSlider = true;
        float t = (localPos.x() - sliderX) / sliderW;
        float value = std::clamp(t, 0.f, 1.f);
        handle_scale_y_change(value);
        return true;
      }
    }

    // Start dragging panel
    m_dragging = true;
    m_dragStart = p;
    return true;
  } else if (button == GLFW_MOUSE_BUTTON_1 && !down) {
    m_dragging = false;
    m_draggingSlider = false;
    m_draggingRotationSlider = false;
    m_draggingScaleXSlider = false;
    m_draggingScaleYSlider = false;
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
    float value = std::clamp(t, 0.f, 1.f);
    handle_opacity_change(value);
    return true;
  }

  if (m_draggingRotationSlider) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());
    float sliderX = 30.f;
    float sliderW = 310.f;
    float t = (localPos.x() - sliderX) / sliderW;
    float value = std::clamp(t, 0.f, 1.f);
    handle_rotation_change(value);
    return true;
  }

  if (m_draggingScaleXSlider) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());
    float sliderX = 30.f;
    float sliderW = 310.f;
    float t = (localPos.x() - sliderX) / sliderW;
    float value = std::clamp(t, 0.f, 1.f);
    handle_scale_x_change(value);
    return true;
  }

  if (m_draggingScaleYSlider) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());
    float sliderX = 30.f;
    float sliderW = 310.f;
    float t = (localPos.x() - sliderX) / sliderW;
    float value = std::clamp(t, 0.f, 1.f);
    handle_scale_y_change(value);
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
  nvgRoundedRect(ctx, px, py, pw, ph, 8.f);
  nvgFillColor(ctx, nvgRGBA(248, 248, 252, 255));
  nvgFill(ctx);
  
  // Draw border
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 8.f);
  nvgStrokeColor(ctx, nvgRGBA(200, 200, 210, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);

  float yOffset = 20.f;

  // Draw tabs if SVG shape
  if (m_is_svg_shape) {
    drawTabs(ctx, px + 20.f, py + 10.f, pw - 40.f);
    yOffset += 50.f;
  }

  // If SVG Parameters tab is selected, show SVG parameters instead of regular properties
  if (m_is_svg_shape && m_current_tab == Tab::SVGParameters) {
    drawSVGParameters(ctx, px + 20.f, py + yOffset, pw - 40.f);
    return;
  }

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
  for (size_t i = 0; i < m_textSizeButtons.size(); i++) {
    const auto &btn = m_textSizeButtons[i];
    drawIconButton(ctx, px + btn.x, py + btn.y, btn.size, btn.icon.c_str(), btn.selected);
    drawTextSizeIcon(ctx, px + btn.x + btn.size * 0.5f, py + btn.y + btn.size * 0.5f,
                     btn.size * 0.4f, static_cast<int>(i));
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

  // Rotation slider (旋转)
  drawLabel(ctx, px + 20.f, py + yOffset, "旋转");
  yOffset += 30.f;
  m_rotationSliderY = yOffset;
  drawSlider(ctx, px + 30.f, py + yOffset, 310.f, m_rotationValue, "0°", "360°");
  yOffset += 80.f;

  // Scale X slider (缩放 X) - only for SVG shapes
  if (m_is_svg_shape) {
    drawLabel(ctx, px + 20.f, py + yOffset, "缩放 X");
    yOffset += 30.f;
    m_scaleXSliderY = yOffset;
    drawSlider(ctx, px + 30.f, py + yOffset, 310.f, m_scaleXValue, "0.1x", "3.0x");
    yOffset += 80.f;

    // Scale Y slider (缩放 Y)
    drawLabel(ctx, px + 20.f, py + yOffset, "缩放 Y");
    yOffset += 30.f;
    m_scaleYSliderY = yOffset;
    drawSlider(ctx, px + 30.f, py + yOffset, 310.f, m_scaleYValue, "0.1x", "3.0x");
    yOffset += 80.f;
  }

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

void PropertiesPanelModule::drawFontIcon(NVGcontext *ctx, float cx, float cy, float size,
                                         int fontType) {
  nvgFontFace(ctx, "sans");
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  const char *labels[] = {"Aa", "Aa", "Aa", "Aa"};
  float sizes[] = {14.f, 14.f, 14.f, 14.f};

  if (fontType >= 0 && fontType < 4) {
    nvgFontSize(ctx, sizes[fontType]);
    nvgText(ctx, cx, cy, labels[fontType], nullptr);
  }
}

void PropertiesPanelModule::drawTextSizeIcon(NVGcontext *ctx, float cx, float cy, float size,
                                             int sizeType) {
  nvgFontFace(ctx, "sans");
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  const char *labels[] = {"S", "M", "L", "XL"};
  float sizes[] = {10.f, 12.f, 14.f, 16.f};

  if (sizeType >= 0 && sizeType < 4) {
    nvgFontSize(ctx, sizes[sizeType]);
    nvgText(ctx, cx, cy, labels[sizeType], nullptr);
  }
}

void PropertiesPanelModule::drawAlignIcon(NVGcontext *ctx, float cx, float cy, float size,
                                          int alignType) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);

  float lineSpacing = size * 0.35f;

  if (alignType == 0) {
    // Left align
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.6f, cy - lineSpacing);
    nvgLineTo(ctx, cx + size * 0.4f, cy - lineSpacing);
    nvgMoveTo(ctx, cx - size * 0.6f, cy);
    nvgLineTo(ctx, cx + size * 0.6f, cy);
    nvgMoveTo(ctx, cx - size * 0.6f, cy + lineSpacing);
    nvgLineTo(ctx, cx + size * 0.2f, cy + lineSpacing);
    nvgStroke(ctx);
  } else if (alignType == 1) {
    // Center align
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.4f, cy - lineSpacing);
    nvgLineTo(ctx, cx + size * 0.4f, cy - lineSpacing);
    nvgMoveTo(ctx, cx - size * 0.6f, cy);
    nvgLineTo(ctx, cx + size * 0.6f, cy);
    nvgMoveTo(ctx, cx - size * 0.2f, cy + lineSpacing);
    nvgLineTo(ctx, cx + size * 0.2f, cy + lineSpacing);
    nvgStroke(ctx);
  } else {
    // Right align
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx - size * 0.4f, cy - lineSpacing);
    nvgLineTo(ctx, cx + size * 0.6f, cy - lineSpacing);
    nvgMoveTo(ctx, cx - size * 0.6f, cy);
    nvgLineTo(ctx, cx + size * 0.6f, cy);
    nvgMoveTo(ctx, cx - size * 0.2f, cy + lineSpacing);
    nvgLineTo(ctx, cx + size * 0.6f, cy + lineSpacing);
    nvgStroke(ctx);
  }
}

void PropertiesPanelModule::drawTabs(NVGcontext *ctx, float x, float y, float w) {
  if (!m_is_svg_shape) {
    return; // No tabs for non-SVG shapes
  }

  float tab_width = 160.f;
  float tab_height = 35.f;

  // Draw Properties tab
  nvgBeginPath(ctx);
  nvgRoundedRectVarying(ctx, x, y, tab_width, tab_height, 6, 6, 0, 0);
  if (m_current_tab == Tab::Properties) {
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(230, 230, 240, 255));
  }
  nvgFill(ctx);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 12.0f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, x + tab_width / 2, y + tab_height / 2, "属性", nullptr);

  // Draw SVG Parameters tab
  nvgBeginPath(ctx);
  nvgRoundedRectVarying(ctx, x + tab_width + 5, y, tab_width, tab_height, 6, 6, 0, 0);
  if (m_current_tab == Tab::SVGParameters) {
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(230, 230, 240, 255));
  }
  nvgFill(ctx);

  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgText(ctx, x + tab_width + 5 + tab_width / 2, y + tab_height / 2, "SVG参数", nullptr);
}

void PropertiesPanelModule::drawSVGParameters(NVGcontext *ctx, float x, float y, float w) {
  float yOffset = y;

  // Title
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, x, yOffset, "SVG形状参数", nullptr);
  yOffset += 30.f;

  // Shape ID
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 11.f);
  nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
  nvgText(ctx, x, yOffset, "形状ID:", nullptr);
  yOffset += 18.f;

  nvgFontSize(ctx, 10.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 40, 255));
  nvgText(ctx, x + 10, yOffset, m_svg_shape_id.c_str(), nullptr);
  yOffset += 30.f;

  // Parameters
  if (!m_svg_parameters.empty()) {
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, 11.f);
    nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
    nvgText(ctx, x, yOffset, "参数:", nullptr);
    yOffset += 20.f;

    for (const auto &param : m_svg_parameters) {
      nvgFontSize(ctx, 10.f);
      nvgFillColor(ctx, nvgRGBA(100, 100, 100, 255));
      nvgText(ctx, x + 10, yOffset, param.first.c_str(), nullptr);
      yOffset += 15.f;

      nvgFillColor(ctx, nvgRGBA(40, 40, 40, 255));
      nvgText(ctx, x + 20, yOffset, param.second.c_str(), nullptr);
      yOffset += 25.f;
    }
  }
}

// === MVC Integration ===

void PropertiesPanelModule::set_controller(whiteboard::PropertiesController* controller) {
  m_controller = controller;
}

void PropertiesPanelModule::set_document(whiteboard::WhiteboardDocument* document) {
  m_document = document;
}

// === IDocumentObserver Implementation ===

void PropertiesPanelModule::on_strokes_changed() {
  // Strokes changed - may need to update if selection properties changed
  // For now, we'll rely on on_selection_changed() for updates
}

void PropertiesPanelModule::on_selection_changed() {
  update_from_selection();
}

void PropertiesPanelModule::on_tool_changed() {
  // Tool changed - not relevant for properties panel
}

void PropertiesPanelModule::on_properties_changed() {
  // Default properties changed - update UI if no selection
  if (!m_document) return;
  
  const auto& selected = m_document->get_selected_indices();
  if (selected.empty()) {
    update_from_defaults();
  }
}

// === UI Update Methods ===

void PropertiesPanelModule::update_from_selection() {
  if (!m_document) return;
  
  const auto& selected = m_document->get_selected_indices();
  
  if (selected.empty()) {
    // No selection - show default tool properties
    update_from_defaults();
    return;
  }
  
  if (selected.size() == 1) {
    // Single selection - show that shape's properties
    const auto& strokes = m_document->get_strokes();
    int index = selected[0];
    
    if (index >= 0 && index < static_cast<int>(strokes.size())) {
      const whiteboard::Stroke& stroke = strokes[index];
      update_from_stroke(stroke);
    }
  } else {
    // Multiple selection - show neutral state
    update_for_multi_selection();
  }
}

void PropertiesPanelModule::update_from_stroke(const whiteboard::Stroke& stroke) {
  // Update stroke color buttons
  for (auto& btn : m_strokeColors) {
    NVGcolor btn_color = btn.color;
    bool matches = (std::abs(stroke.color.r() - btn_color.r * 255) < 5 &&
                    std::abs(stroke.color.g() - btn_color.g * 255) < 5 &&
                    std::abs(stroke.color.b() - btn_color.b * 255) < 5);
    btn.selected = matches;
  }
  
  // Update fill color buttons
  for (auto& btn : m_bgColors) {
    NVGcolor btn_color = btn.color;
    bool matches = (std::abs(stroke.fill_color.r() - btn_color.r * 255) < 5 &&
                    std::abs(stroke.fill_color.g() - btn_color.g * 255) < 5 &&
                    std::abs(stroke.fill_color.b() - btn_color.b * 255) < 5);
    btn.selected = matches;
  }
  
  // Update stroke width buttons (map to 1.0, 3.0, 6.0)
  for (auto& btn : m_strokeWidthButtons) {
    float width = (btn.id == 0) ? 1.0f : (btn.id == 1) ? 3.0f : 6.0f;
    btn.selected = (std::abs(stroke.width - width) < 0.5f);
  }
  
  // Update stroke style buttons
  for (auto& btn : m_borderStyleButtons) {
    btn.selected = (static_cast<int>(stroke.stroke_style) == btn.id);
  }
  
  // Update corner buttons (0 = sharp, 1 = rounded)
  for (auto& btn : m_cornerButtons) {
    float radius = (btn.id == 0) ? 0.0f : 8.0f;
    btn.selected = (std::abs(stroke.corner_radius - radius) < 1.0f);
  }
  
  // Update sliders
  m_opacityValue = stroke.opacity;
  m_rotationValue = stroke.rotation / (2.0f * M_PI); // Normalize to 0-1
  
  // Update scale sliders for SVG shapes (map 0.1-3.0 to 0-1)
  if (stroke.tool == whiteboard::Tool::SVGShape) {
    m_scaleXValue = (stroke.svg_scale_x - 0.1f) / 2.9f;  // 0.1-3.0 -> 0-1
    m_scaleYValue = (stroke.svg_scale_y - 0.1f) / 2.9f;
  }
  
  // Update text properties (if text shape)
  if (stroke.tool == whiteboard::Tool::Text) {
    // Update font buttons
    for (auto& btn : m_fontButtons) {
      // Map button id to font face
      std::string font = (btn.id == 0) ? "sans" : 
                        (btn.id == 1) ? "sans-bold" :
                        (btn.id == 2) ? "serif" : "mono";
      btn.selected = (stroke.font_face == font);
    }
    
    // Update text size buttons (12, 16, 24, 32)
    for (auto& btn : m_textSizeButtons) {
      float size = (btn.id == 0) ? 12.0f :
                   (btn.id == 1) ? 16.0f :
                   (btn.id == 2) ? 24.0f : 32.0f;
      btn.selected = (std::abs(stroke.font_size - size) < 1.0f);
    }
    
    // Update alignment buttons
    for (auto& btn : m_alignButtons) {
      // Map button id to alignment
      int align = (btn.id == 0) ? (NVG_ALIGN_LEFT | NVG_ALIGN_TOP) :
                  (btn.id == 1) ? (NVG_ALIGN_CENTER | NVG_ALIGN_TOP) :
                                  (NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
      btn.selected = (stroke.text_align == align);
    }
  }
  
  screen()->redraw();
}

void PropertiesPanelModule::update_from_defaults() {
  if (!m_document) return;
  
  // Read default properties from document
  auto stroke_color = m_document->get_stroke_color();
  auto fill_color = m_document->get_fill_color();
  float stroke_width = m_document->get_stroke_width();
  std::string font_face = m_document->get_font_face();
  float font_size = m_document->get_font_size();
  int text_align = m_document->get_text_align();
  
  // Update stroke color buttons
  for (auto& btn : m_strokeColors) {
    NVGcolor btn_color = btn.color;
    bool matches = (std::abs(stroke_color.r() - btn_color.r * 255) < 5 &&
                    std::abs(stroke_color.g() - btn_color.g * 255) < 5 &&
                    std::abs(stroke_color.b() - btn_color.b * 255) < 5);
    btn.selected = matches;
  }
  
  // Update fill color buttons
  for (auto& btn : m_bgColors) {
    NVGcolor btn_color = btn.color;
    bool matches = (std::abs(fill_color.r() - btn_color.r * 255) < 5 &&
                    std::abs(fill_color.g() - btn_color.g * 255) < 5 &&
                    std::abs(fill_color.b() - btn_color.b * 255) < 5);
    btn.selected = matches;
  }
  
  // Update stroke width buttons
  for (auto& btn : m_strokeWidthButtons) {
    float width = (btn.id == 0) ? 1.0f : (btn.id == 1) ? 3.0f : 6.0f;
    btn.selected = (std::abs(stroke_width - width) < 0.5f);
  }
  
  // Update font buttons
  for (auto& btn : m_fontButtons) {
    std::string font = (btn.id == 0) ? "sans" : 
                      (btn.id == 1) ? "sans-bold" :
                      (btn.id == 2) ? "serif" : "mono";
    btn.selected = (font_face == font);
  }
  
  // Update text size buttons
  for (auto& btn : m_textSizeButtons) {
    float size = (btn.id == 0) ? 12.0f :
                 (btn.id == 1) ? 16.0f :
                 (btn.id == 2) ? 24.0f : 32.0f;
    btn.selected = (std::abs(font_size - size) < 1.0f);
  }
  
  // Update alignment buttons
  for (auto& btn : m_alignButtons) {
    int align = (btn.id == 0) ? (NVG_ALIGN_LEFT | NVG_ALIGN_TOP) :
                (btn.id == 1) ? (NVG_ALIGN_CENTER | NVG_ALIGN_TOP) :
                                (NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    btn.selected = (text_align == align);
  }
  
  // Reset sliders to defaults
  m_opacityValue = 1.0f;
  m_rotationValue = 0.0f;
  
  screen()->redraw();
}

void PropertiesPanelModule::update_for_multi_selection() {
  // Clear all button selections (neutral state)
  for (auto& btn : m_strokeColors) btn.selected = false;
  for (auto& btn : m_bgColors) btn.selected = false;
  for (auto& btn : m_strokeWidthButtons) btn.selected = false;
  for (auto& btn : m_borderStyleButtons) btn.selected = false;
  for (auto& btn : m_lineStyleButtons) btn.selected = false;
  for (auto& btn : m_cornerButtons) btn.selected = false;
  for (auto& btn : m_fontButtons) btn.selected = false;
  for (auto& btn : m_textSizeButtons) btn.selected = false;
  for (auto& btn : m_alignButtons) btn.selected = false;
  
  // Set sliders to middle positions
  m_opacityValue = 0.5f;
  m_rotationValue = 0.5f;
  
  screen()->redraw();
}

// === Button Click Handlers ===

void PropertiesPanelModule::handle_stroke_color_click(int color_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_strokeColors) {
    btn.selected = (btn.id == color_index);
  }
  
  // Get color from palette
  if (color_index >= 0 && color_index < static_cast<int>(m_strokeColors.size())) {
    NVGcolor nvg_color = m_strokeColors[color_index].color;
    // NVGcolor is already in 0-1 range, nanogui::Color expects 0-1 range too
    nanogui::Color color(nvg_color.r, nvg_color.g, nvg_color.b, nvg_color.a);
    
    // Apply via controller
    m_controller->set_stroke_color(color);
  }
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_fill_color_click(int color_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_bgColors) {
    btn.selected = (btn.id == color_index);
  }
  
  // Get color from palette
  if (color_index >= 0 && color_index < static_cast<int>(m_bgColors.size())) {
    NVGcolor nvg_color = m_bgColors[color_index].color;
    // NVGcolor is already in 0-1 range, nanogui::Color expects 0-1 range too
    nanogui::Color color(nvg_color.r, nvg_color.g, nvg_color.b, nvg_color.a);
    
    // Apply via controller
    m_controller->set_fill_color(color);
  }
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_stroke_width_click(int width_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_strokeWidthButtons) {
    btn.selected = (btn.id == width_index);
  }
  
  // Map index to width value
  float width = (width_index == 0) ? 1.0f : (width_index == 1) ? 3.0f : 6.0f;
  
  // Apply via controller
  m_controller->set_stroke_width(width);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_stroke_style_click(int style_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_borderStyleButtons) {
    btn.selected = (btn.id == style_index);
  }
  
  // Map index to StrokeStyle enum
  whiteboard::StrokeStyle style = (style_index == 0) ? whiteboard::StrokeStyle::Solid :
                                   (style_index == 1) ? whiteboard::StrokeStyle::Dashed :
                                                        whiteboard::StrokeStyle::Dotted;
  
  // Apply via controller
  m_controller->set_stroke_style(style);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_corner_click(int corner_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_cornerButtons) {
    btn.selected = (btn.id == corner_index);
  }
  
  // Map index to radius value (0 = sharp, 1 = rounded)
  float radius = (corner_index == 0) ? 0.0f : 8.0f;
  
  // Apply via controller
  m_controller->set_corner_radius(radius);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_font_click(int font_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_fontButtons) {
    btn.selected = (btn.id == font_index);
  }
  
  // Map index to font face
  std::string font_face = (font_index == 0) ? "sans" :
                          (font_index == 1) ? "sans-bold" :
                          (font_index == 2) ? "serif" : "mono";
  
  // Apply via controller
  m_controller->set_font_face(font_face);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_text_size_click(int size_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_textSizeButtons) {
    btn.selected = (btn.id == size_index);
  }
  
  // Map index to font size (S, M, L, XL)
  float font_size = (size_index == 0) ? 12.0f :
                    (size_index == 1) ? 16.0f :
                    (size_index == 2) ? 24.0f : 32.0f;
  
  // Apply via controller
  m_controller->set_font_size(font_size);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_align_click(int align_index) {
  if (!m_controller) return;
  
  // Update button selection state
  for (auto& btn : m_alignButtons) {
    btn.selected = (btn.id == align_index);
  }
  
  // Map index to NVG alignment flags
  int text_align = (align_index == 0) ? (NVG_ALIGN_LEFT | NVG_ALIGN_TOP) :
                   (align_index == 1) ? (NVG_ALIGN_CENTER | NVG_ALIGN_TOP) :
                                        (NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
  
  // Apply via controller
  m_controller->set_text_align(text_align);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_layer_click(int layer_action) {
  if (!m_controller) return;
  
  // Map action to controller method
  // 0 = to bottom, 1 = backward, 2 = forward, 3 = to top
  switch (layer_action) {
    case 0:
      m_controller->send_to_back();
      break;
    case 1:
      m_controller->send_backward();
      break;
    case 2:
      m_controller->bring_forward();
      break;
    case 3:
      m_controller->bring_to_front();
      break;
  }
  
  screen()->redraw();
}

// === Slider Handlers ===

void PropertiesPanelModule::handle_opacity_change(float value) {
  if (!m_controller) return;
  
  // Clamp to valid range
  value = std::clamp(value, 0.0f, 1.0f);
  
  // Update slider visual position
  m_opacityValue = value;
  
  // Apply via controller
  m_controller->set_opacity(value);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_rotation_change(float value) {
  if (!m_controller) return;
  
  // Clamp to valid range (0-1 normalized)
  value = std::clamp(value, 0.0f, 1.0f);
  
  // Update slider visual position
  m_rotationValue = value;
  
  // Convert normalized value (0-1) to radians (0-2π)
  float rotation = value * 2.0f * M_PI;
  
  // Apply via controller
  m_controller->set_rotation(rotation);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_scale_x_change(float value) {
  if (!m_controller) return;
  
  // Clamp to valid range (0-1 normalized)
  value = std::clamp(value, 0.0f, 1.0f);
  
  // Update slider visual position
  m_scaleXValue = value;
  
  // Convert normalized value (0-1) to scale (0.1-3.0)
  float scale = 0.1f + value * 2.9f;
  
  // Apply via controller
  m_controller->set_svg_scale_x(scale);
  
  screen()->redraw();
}

void PropertiesPanelModule::handle_scale_y_change(float value) {
  if (!m_controller) return;
  
  // Clamp to valid range (0-1 normalized)
  value = std::clamp(value, 0.0f, 1.0f);
  
  // Update slider visual position
  m_scaleYValue = value;
  
  // Convert normalized value (0-1) to scale (0.1-3.0)
  float scale = 0.1f + value * 2.9f;
  
  // Apply via controller
  m_controller->set_svg_scale_y(scale);
  
  screen()->redraw();
}
