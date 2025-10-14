#include "wasp_filter_module.h"
#include <algorithm>
#include <cmath>
#include <nanogui.h>

WaspFilterModule::WaspFilterModule(Widget *parent) : Widget(parent) {
  loadSvgAssets();

  const float pw = 140.f;
  const float portX = 35.f;
  const float knobX = 95.f;

  m_ports.push_back({portX, 80.f, "Audio In", true, 0});
  m_ports.push_back({portX, 140.f, "CV1", true, 1});
  m_ports.push_back({portX, 200.f, "CV2", true, 2});
  m_ports.push_back({portX, 260.f, "BP Out", false, 3});
  m_ports.push_back({portX, 320.f, "LP/HP Out", false, 4});

  m_knobs.push_back({knobX, 80.f, 32.f, 0.5f, "Lev.", 0});
  m_knobs.push_back({knobX, 140.f, 32.f, 0.7f, "Frq.", 1});
  m_knobs.push_back({knobX, 200.f, 32.f, 0.3f, "CV2", 2});
  m_knobs.push_back({knobX, 260.f, 32.f, 0.8f, "Res.", 3});
  m_knobs.push_back({knobX, 320.f, 32.f, 0.5f, "Mix", 4});

  m_activeKnob = -1;
}

Vector2i WaspFilterModule::preferred_size_impl(NVGcontext *) const { return {140, 380}; }

bool WaspFilterModule::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

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
    if (m_activeKnob >= 0) {
      m_activeKnob = -1;
      return true;
    }
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

bool WaspFilterModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                        int modifiers) {
  if (m_activeKnob >= 0) {
    float sensitivity = 0.005f;
    float delta = (m_dragStartY - p.y()) * sensitivity;
    m_knobs[m_activeKnob].value = std::clamp(m_dragStartValue + delta, 0.f, 1.f);
    screen()->redraw();
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

void WaspFilterModule::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  NVGpaint panelPaint = nvgLinearGradient(ctx, px, py, px + pw, py, nvgRGBA(215, 215, 220, 255),
                                          nvgRGBA(205, 205, 210, 255));
  nvgFillPaint(ctx, panelPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  nvgStrokeColor(ctx, nvgRGBA(180, 180, 185, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  drawScrew(ctx, px + 10.f, py + 10.f);
  drawScrew(ctx, px + pw - 10.f, py + 10.f);
  drawScrew(ctx, px + 10.f, py + ph - 10.f);
  drawScrew(ctx, px + pw - 10.f, py + ph - 10.f);

  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 13.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, px + pw * 0.5f, py + 15.f, "A-124 VCF5", nullptr);

  nvgFontSize(ctx, 11.f);
  nvgText(ctx, px + pw * 0.5f, py + 32.f, "Wasp Filter", nullptr);

  for (const auto &port : m_ports) {
    drawPort(ctx, px + port.x, py + port.y, port.label.c_str(), port.isInput);
  }

  for (const auto &knob : m_knobs) {
    drawKnobWithScale(ctx, px + knob.x, py + knob.y, knob.radius, knob.value, knob.label.c_str());
  }
}

void WaspFilterModule::loadSvgAssets() {}

void WaspFilterModule::drawScrew(NVGcontext *ctx, float cx, float cy) {
  const float radius = 4.f;
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint screwPaint = nvgRadialGradient(ctx, cx - 1.f, cy - 1.f, radius * 0.3f, radius,
                                          nvgRGBA(100, 100, 105, 255), nvgRGBA(60, 60, 65, 255));
  nvgFillPaint(ctx, screwPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - radius * 0.6f, cy);
  nvgLineTo(ctx, cx + radius * 0.6f, cy);
  nvgStrokeColor(ctx, nvgRGBA(35, 35, 40, 255));
  nvgStrokeWidth(ctx, 1.2f);
  nvgStroke(ctx);
}

void WaspFilterModule::drawPort(NVGcontext *ctx, float cx, float cy, const char *label,
                                bool input) {
  const float hexRadius = 18.f;
  constexpr float kPi = 3.14159265358979323846f;

  nvgBeginPath(ctx);
  for (int i = 0; i < 6; i++) {
    float angle = (i / 6.0f) * kPi * 2.0f - kPi * 0.5f;
    float x = cx + std::cos(angle) * hexRadius;
    float y = cy + std::sin(angle) * hexRadius;
    if (i == 0)
      nvgMoveTo(ctx, x, y);
    else
      nvgLineTo(ctx, x, y);
  }
  nvgClosePath(ctx);
  nvgFillColor(ctx, nvgRGBA(140, 140, 145, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  for (int i = 0; i < 6; i++) {
    float angle = (i / 6.0f) * kPi * 2.0f - kPi * 0.5f;
    float x = cx + std::cos(angle) * (hexRadius - 3.f);
    float y = cy + std::sin(angle) * (hexRadius - 3.f);
    if (i == 0)
      nvgMoveTo(ctx, x, y);
    else
      nvgLineTo(ctx, x, y);
  }
  nvgClosePath(ctx);
  nvgFillColor(ctx, nvgRGBA(180, 180, 185, 255));
  nvgFill(ctx);

  const float portRadius = 10.f;
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, portRadius);
  nvgFillColor(ctx, nvgRGBA(30, 30, 35, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, portRadius * 0.8f);
  NVGpaint portPaint =
      nvgRadialGradient(ctx, cx - portRadius * 0.3f, cy - portRadius * 0.3f, portRadius * 0.2f,
                        portRadius * 0.8f, nvgRGBA(80, 80, 85, 255), nvgRGBA(40, 40, 45, 255));
  nvgFillPaint(ctx, portPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx - portRadius * 0.3f, cy - portRadius * 0.3f, portRadius * 0.4f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 30));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, cx - hexRadius - 5.f, cy, label, nullptr);
}

void WaspFilterModule::drawKnobWithScale(NVGcontext *ctx, float cx, float cy, float radius,
                                         float value, const char *label) {
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 7.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 65, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  const float scaleRadius = radius + 12.f;
  for (int i = 0; i <= 10; i++) {
    float angle = -2.356f + (i / 10.0f) * 4.712f;
    float x = cx + std::cos(angle) * scaleRadius;
    float y = cy + std::sin(angle) * scaleRadius;

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", i);
    nvgText(ctx, x, y, buf, nullptr);
  }

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint knobPaint =
      nvgRadialGradient(ctx, cx - radius * 0.3f, cy - radius * 0.3f, radius * 0.2f, radius,
                        nvgRGBA(180, 180, 185, 255), nvgRGBA(120, 120, 125, 255));
  nvgFillPaint(ctx, knobPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  nvgStrokeColor(ctx, nvgRGBA(100, 100, 105, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius * 0.7f);
  NVGpaint innerPaint =
      nvgRadialGradient(ctx, cx - radius * 0.2f, cy - radius * 0.2f, radius * 0.1f, radius * 0.7f,
                        nvgRGBA(160, 160, 165, 255), nvgRGBA(100, 100, 105, 255));
  nvgFillPaint(ctx, innerPaint);
  nvgFill(ctx);

  float angle = -2.356f + value * 4.712f;
  float lineLen = radius * 0.6f;
  float x2 = cx + std::cos(angle) * lineLen;
  float y2 = cy + std::sin(angle) * lineLen;

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx, cy);
  nvgLineTo(ctx, x2, y2);
  nvgStrokeColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgStrokeWidth(ctx, 3.f);
  nvgLineCap(ctx, NVG_ROUND);
  nvgStroke(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx - radius * 0.3f, cy - radius * 0.3f, radius * 0.4f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 60));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 10.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, cx + radius + 15.f, cy, label, nullptr);
}
