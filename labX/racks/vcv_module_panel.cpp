#include "vcv_module_panel.h"
#include "bezier.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <nanogui.h>

VCVModulePanel::VCVModulePanel(Widget *parent) : Widget(parent) {
  loadPortSvgs();

  const float pw = 180.f;
  m_knobs.push_back({pw * 0.5f, 80.f, 28.f, 0.3f, "FREQ", 0});
  m_knobs.push_back({pw * 0.5f, 150.f, 28.f, 0.7f, "FINE", 1});
  m_knobs.push_back({pw * 0.5f, 220.f, 22.f, 0.5f, "PWM", 2});

  m_ports.push_back({pw * 0.3f, 290.f, "V/OCT", true, 0});
  m_ports.push_back({pw * 0.7f, 290.f, "FM", true, 1});
  m_ports.push_back({pw * 0.5f, 340.f, "OUT", false, 2});
}

VCVModulePanel::~VCVModulePanel() {}

Vector2i VCVModulePanel::preferred_size_impl(NVGcontext *) const { return {180, 380}; }

bool VCVModulePanel::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

    for (size_t i = 0; i < m_ports.size(); i++) {
      float dx = localPos.x() - m_ports[i].x;
      float dy = localPos.y() - m_ports[i].y;
      float dist = std::sqrt(dx * dx + dy * dy);
      float portRadius = 12.f;

      if (dist <= portRadius) {
        if (!m_ports[i].isInput) {
          m_cableDragging = true;
          m_cableStartPort = static_cast<int>(i);
          m_cableDragPos = p;
          return true;
        }
      }
    }

    for (size_t i = 0; i < m_knobs.size(); i++) {
      float dx = localPos.x() - m_knobs[i].x;
      float dy = localPos.y() - m_knobs[i].y;
      float dist = std::sqrt(dx * dx + dy * dy);

      if (dist <= m_knobs[i].radius) {
        m_activeKnob = static_cast<int>(i);
        m_dragStartY = p.y();
        m_dragStartValue = m_knobs[i].value;
        return true;
      }
    }
  } else if (button == GLFW_MOUSE_BUTTON_1 && !down) {
    if (m_cableDragging) {
      Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

      for (size_t i = 0; i < m_ports.size(); i++) {
        if (m_ports[i].isInput) {
          float dx = localPos.x() - m_ports[i].x;
          float dy = localPos.y() - m_ports[i].y;
          float dist = std::sqrt(dx * dx + dy * dy);
          float portRadius = 12.f;

          if (dist <= portRadius) {
            NVGcolor cableColor = nvgRGBA(255, 200, 80 + (m_cables.size() * 40) % 100, 255);
            m_cables.push_back({m_cableStartPort, static_cast<int>(i), cableColor});
            m_cableDragging = false;
            return true;
          }
        }
      }

      m_cableDragging = false;
      return true;
    }

    if (m_activeKnob >= 0) {
      m_activeKnob = -1;
      return true;
    }
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

bool VCVModulePanel::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                      int modifiers) {
  if (m_cableDragging) {
    m_cableDragPos = p;
    screen()->redraw();
    return true;
  }

  if (m_activeKnob >= 0) {
    float sensitivity = 0.005f;
    float delta = (m_dragStartY - p.y()) * sensitivity;
    m_knobs[m_activeKnob].value = std::clamp(m_dragStartValue + delta, 0.f, 1.f);
    screen()->redraw();
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

bool VCVModulePanel::mouse_enter_event(const Vector2i &p, bool enter) {
  if (!enter && m_activeKnob >= 0) {
    m_activeKnob = -1;
  }
  return Widget::mouse_enter_event(p, enter);
}

void VCVModulePanel::loadPortSvgs() {
  m_inputPortSvg = lunasvg::Document::loadFromData(kInputPortSvg);
  m_outputPortSvg = lunasvg::Document::loadFromData(kOutputPortSvg);
  m_largeKnobSvg = lunasvg::Document::loadFromData(kLargeKnobSvg);
  m_smallKnobSvg = lunasvg::Document::loadFromData(kSmallKnobSvg);
}

void VCVModulePanel::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  NVGpaint panelPaint =
      nvgLinearGradient(ctx, px, py, px + pw, py, nvgRGBA(35, 35, 38, 255), nvgRGBA(28, 28, 30, 255));
  nvgFillPaint(ctx, panelPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  nvgStrokeColor(ctx, nvgRGBA(20, 20, 22, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  drawScrew(ctx, px + 10.f, py + 10.f);
  drawScrew(ctx, px + pw - 10.f, py + 10.f);
  drawScrew(ctx, px + 10.f, py + ph - 10.f);
  drawScrew(ctx, px + pw - 10.f, py + ph - 10.f);

  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 16.f);
  nvgFillColor(ctx, nvgRGBA(200, 200, 205, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, px + pw * 0.5f, py + 30.f, "VCO", nullptr);

  for (const auto &knob : m_knobs) {
    drawKnob(ctx, px + knob.x, py + knob.y, knob.radius, knob.value, knob.label.c_str());
  }

  for (const auto &cable : m_cables) {
    drawCable(ctx, cable);
  }

  if (m_cableDragging && m_cableStartPort >= 0) {
    const Port &startPort = m_ports[m_cableStartPort];
    float x0 = px + startPort.x;
    float y0 = py + startPort.y;
    float x3 = static_cast<float>(m_cableDragPos.x());
    float y3 = static_cast<float>(m_cableDragPos.y());

    float dx = x3 - x0;
    float dy = y3 - y0;
    float x1 = x0 + dx * 0.4f;
    float y1 = y0 + std::abs(dy) * 0.5f;
    float x2 = x3 - dx * 0.4f;
    float y2 = y3 - std::abs(dy) * 0.5f;

    bezier::Bezier<3> curve(
        {bezier::Point(x0, y0), bezier::Point(x1, y1), bezier::Point(x2, y2), bezier::Point(x3, y3)});

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x0, y0);
    for (int i = 1; i <= 50; i++) {
      auto p = curve.valueAt(i / 50.f);
      nvgLineTo(ctx, static_cast<float>(p.x), static_cast<float>(p.y));
    }
    nvgStrokeColor(ctx, nvgRGBA(255, 255, 255, 150));
    nvgStrokeWidth(ctx, 4.f);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStroke(ctx);
  }

  for (const auto &port : m_ports) {
    drawPort(ctx, px + port.x, py + port.y, port.label.c_str(), port.isInput);
  }
}

void VCVModulePanel::drawScrew(NVGcontext *ctx, float cx, float cy) {
  const float radius = 5.f;
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint screwPaint = nvgRadialGradient(ctx, cx - 1.f, cy - 1.f, radius * 0.3f, radius,
                                          nvgRGBA(80, 80, 85, 255), nvgRGBA(45, 45, 48, 255));
  nvgFillPaint(ctx, screwPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - radius * 0.6f, cy);
  nvgLineTo(ctx, cx + radius * 0.6f, cy);
  nvgStrokeColor(ctx, nvgRGBA(25, 25, 28, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);
}

void VCVModulePanel::drawKnob(NVGcontext *ctx, float cx, float cy, float radius, float value,
                               const char *label) {
  lunasvg::Document *knobSvg = (radius > 25.f) ? m_largeKnobSvg.get() : m_smallKnobSvg.get();
  renderSvgKnob(ctx, knobSvg, cx, cy, radius * 2.f, value);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 11.f);
  nvgFillColor(ctx, nvgRGBA(160, 160, 165, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, cx, cy + radius + 8.f, label, nullptr);
}

void VCVModulePanel::renderSvgKnob(NVGcontext *ctx, lunasvg::Document *svg, float cx, float cy, float size,
                                   float value) {
  if (!svg) return;
  
  float svgW = static_cast<float>(svg->width());
  float svgH = static_cast<float>(svg->height());
  if (svgW == 0 || svgH == 0) return;

  const float angle = -2.356f + value * 4.712f;

  nvgSave(ctx);
  nvgTranslate(ctx, cx, cy);
  nvgRotate(ctx, angle);

  float scale = size / std::max(svgW, svgH);
  nvgScale(ctx, scale, scale);
  nvgTranslate(ctx, -svgW * 0.5f, -svgH * 0.5f);

  float knobCx = svgW * 0.5f;
  float knobCy = svgH * 0.5f;
  float knobRadius = std::min(svgW, svgH) * 0.475f;

  nvgBeginPath(ctx);
  nvgCircle(ctx, knobCx, knobCy, knobRadius);
  NVGpaint rimPaint = nvgRadialGradient(ctx, knobCx - knobRadius * 0.2f, knobCy - knobRadius * 0.2f,
                                        knobRadius * 0.1f, knobRadius, nvgRGBA(90, 90, 96, 255),
                                        nvgRGBA(58, 58, 64, 255));
  nvgFillPaint(ctx, rimPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, knobCx, knobCy, knobRadius * 0.84f);
  NVGpaint bodyPaint =
      nvgRadialGradient(ctx, knobCx - knobRadius * 0.25f, knobCy - knobRadius * 0.25f, knobRadius * 0.1f,
                        knobRadius * 0.84f, nvgRGBA(106, 106, 112, 255), nvgRGBA(42, 42, 48, 255));
  nvgFillPaint(ctx, bodyPaint);
  nvgFill(ctx);

  if (size > 50.f) {
    for (int i = 0; i < 8; i++) {
      float notchAngle = i * kPi * 0.25f;
      float notchX = knobCx + std::cos(notchAngle) * knobRadius * 0.7f;
      float notchY = knobCy + std::sin(notchAngle) * knobRadius * 0.7f;
      nvgBeginPath(ctx);
      nvgCircle(ctx, notchX, notchY, knobRadius * 0.075f);
      nvgFillColor(ctx, nvgRGBA(26, 26, 32, 153));
      nvgFill(ctx);
    }
  }

  nvgBeginPath(ctx);
  nvgCircle(ctx, knobCx, knobCy, knobRadius * 0.3f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 46, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRect(ctx, knobCx - knobRadius * 0.05f, knobCy - knobRadius * 0.7f, knobRadius * 0.1f,
          knobRadius * 0.45f);
  NVGpaint indicatorPaint =
      nvgLinearGradient(ctx, knobCx, knobCy - knobRadius * 0.7f, knobCx, knobCy - knobRadius * 0.25f,
                        nvgRGBA(232, 232, 234, 255), nvgRGBA(168, 168, 170, 255));
  nvgFillPaint(ctx, indicatorPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, knobCx, knobCy, knobRadius * 0.15f);
  nvgFillColor(ctx, nvgRGBA(58, 58, 64, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, knobCx - knobRadius * 0.12f, knobCy);
  nvgLineTo(ctx, knobCx + knobRadius * 0.12f, knobCy);
  nvgStrokeColor(ctx, nvgRGBA(26, 26, 32, 255));
  nvgStrokeWidth(ctx, knobRadius * 0.04f);
  nvgStroke(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, knobCx - knobRadius * 0.2f, knobCy - knobRadius * 0.2f, knobRadius * 0.3f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 38));
  nvgFill(ctx);

  nvgRestore(ctx);
}

void VCVModulePanel::drawCable(NVGcontext *ctx, const Cable &cable) {
  const Port *fromPort = nullptr;
  const Port *toPort = nullptr;

  for (const auto &port : m_ports) {
    if (port.id == cable.fromPortId)
      fromPort = &port;
    if (port.id == cable.toPortId)
      toPort = &port;
  }

  if (!fromPort || !toPort)
    return;

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());

  float x0 = px + fromPort->x;
  float y0 = py + fromPort->y;
  float x3 = px + toPort->x;
  float y3 = py + toPort->y;

  float dx = x3 - x0;
  float dy = y3 - y0;
  float tension = 0.4f;

  float x1 = x0 + dx * tension;
  float y1 = y0 + std::abs(dy) * 0.5f;
  float x2 = x3 - dx * tension;
  float y2 = y3 - std::abs(dy) * 0.5f;

  bezier::Bezier<3> curve(
      {bezier::Point(x0, y0), bezier::Point(x1, y1), bezier::Point(x2, y2), bezier::Point(x3, y3)});

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, x0, y0 + 2.f);
  for (int i = 1; i <= 50; i++) {
    auto p = curve.valueAt(i / 50.f);
    nvgLineTo(ctx, static_cast<float>(p.x), static_cast<float>(p.y + 2.0));
  }
  nvgStrokeColor(ctx, nvgRGBA(0, 0, 0, 60));
  nvgStrokeWidth(ctx, 5.f);
  nvgStroke(ctx);

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, x0, y0);
  for (int i = 1; i <= 50; i++) {
    auto p = curve.valueAt(i / 50.f);
    nvgLineTo(ctx, static_cast<float>(p.x), static_cast<float>(p.y));
  }
  nvgStrokeColor(ctx, cable.color);
  nvgStrokeWidth(ctx, 4.f);
  nvgLineCap(ctx, NVG_ROUND);
  nvgStroke(ctx);

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, x0, y0);
  for (int i = 1; i <= 50; i++) {
    auto p = curve.valueAt(i / 50.f);
    nvgLineTo(ctx, static_cast<float>(p.x), static_cast<float>(p.y));
  }
  NVGcolor highlightColor = cable.color;
  highlightColor.r = std::min(1.0f, highlightColor.r * 1.3f);
  highlightColor.g = std::min(1.0f, highlightColor.g * 1.3f);
  highlightColor.b = std::min(1.0f, highlightColor.b * 1.3f);
  nvgStrokeColor(ctx, highlightColor);
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);
}

void VCVModulePanel::drawPort(NVGcontext *ctx, float cx, float cy, const char *label, bool input) {
  const float portSize = 24.f;

  lunasvg::Document *portSvg = input ? m_inputPortSvg.get() : m_outputPortSvg.get();
  renderSvgPort(ctx, portSvg, cx - portSize * 0.5f, cy - portSize * 0.5f, portSize);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 10.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
  nvgText(ctx, cx, cy - 12.f - 5.f, label, nullptr);
}

void VCVModulePanel::renderSvgPort(NVGcontext *ctx, lunasvg::Document *svg, float x, float y, float size) {
  if (!svg) return;
  
  float svgW = static_cast<float>(svg->width());
  float svgH = static_cast<float>(svg->height());
  if (svgW == 0 || svgH == 0) return;

  float scale = size / std::max(svgW, svgH);

  nvgSave(ctx);
  nvgTranslate(ctx, x, y);
  nvgScale(ctx, scale, scale);

  float cx = 24.f;
  float cy = 24.f;

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, 22.f);
  nvgFillColor(ctx, nvgRGBA(20, 22, 26, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, 18.f);

  bool isInput = (svg == m_inputPortSvg.get());
  NVGpaint portGrad =
      isInput ? nvgRadialGradient(ctx, cx - 5.f, cy - 5.f, 2.f, 20.f, nvgRGBA(74, 159, 216, 255),
                                  nvgRGBA(30, 90, 125, 255))
              : nvgRadialGradient(ctx, cx - 5.f, cy - 5.f, 2.f, 20.f, nvgRGBA(232, 160, 64, 255),
                                  nvgRGBA(141, 90, 32, 255));
  nvgFillPaint(ctx, portGrad);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, 8.f);
  NVGpaint holeGrad =
      nvgRadialGradient(ctx, cx, cy, 1.f, 8.f, nvgRGBA(10, 12, 16, 255), nvgRGBA(26, 30, 40, 255));
  nvgFillPaint(ctx, holeGrad);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx - 4.f, cy - 4.f, 6.f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 76));
  nvgFill(ctx);

  nvgRestore(ctx);
}
