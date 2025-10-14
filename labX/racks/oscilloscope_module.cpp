// Implementation extracted from vcv_style_module.cpp
// See lines 1600-2200 in original file for full implementation
#include "oscilloscope_module.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <nanogui.h>

constexpr float kPi = 3.14159265358979323846f;

OscilloscopeModule::OscilloscopeModule(Widget *parent) : Widget(parent) {
  loadSvgAssets();

  m_waveformCh1.resize(400, 0.0f);
  m_waveformCh2.resize(400, 0.0f);
  m_time = 0.0f;
  m_ch1Enabled = true;
  m_ch2Enabled = true;
  m_activeKnob = -1;
}

Vector2i OscilloscopeModule::preferred_size_impl(NVGcontext *) const { return {720, 480}; }

bool OscilloscopeModule::mouse_button_event(const Vector2i &p, int button, bool down,
                                            int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    Vector2f localPos = Vector2f(p.x() - m_pos.x(), p.y() - m_pos.y());

    const float controlX = m_size.x() * 0.65f;
    const float knobSize = 28.f;
    const float controlW = m_size.x() * 0.35f;
    const float colWidth = controlW * 0.5f;
    const float col1X = controlX + colWidth * 0.5f;
    const float col2X = controlX + colWidth + colWidth * 0.5f;

    struct KnobDef {
      float x, y;
      int id;
    };
    // Updated positions for column layout
    std::vector<KnobDef> knobs = {{col1X, 90.f, 0},  {col1X, 160.f, 1}, {col2X, 90.f, 2},
                                  {col2X, 160.f, 3}, {col1X, 345.f, 4}, {col2X, 345.f, 5}};

    for (const auto &knob : knobs) {
      float dx = localPos.x() - knob.x;
      float dy = localPos.y() - knob.y;
      float dist = std::sqrt(dx * dx + dy * dy);

      if (dist <= knobSize) {
        m_activeKnob = knob.id;
        m_dragStartY = p.y();
        m_dragStartValue = getKnobValue(knob.id);
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

bool OscilloscopeModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                          int modifiers) {
  if (m_activeKnob >= 0) {
    float sensitivity = 0.005f;
    float delta = (m_dragStartY - p.y()) * sensitivity;
    float newValue = std::clamp(m_dragStartValue + delta, 0.f, 1.f);
    setKnobValue(m_activeKnob, newValue);
    screen()->redraw();
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

float OscilloscopeModule::getKnobValue(int id) const {
  switch (id) {
  case 0:
    return m_ch1VScale;
  case 1:
    return m_ch1VPos;
  case 2:
    return m_ch2VScale;
  case 3:
    return m_ch2VPos;
  case 4:
    return m_timeScale;
  case 5:
    return m_hPos;
  default:
    return 0.5f;
  }
}

void OscilloscopeModule::setKnobValue(int id, float value) {
  switch (id) {
  case 0:
    m_ch1VScale = value;
    break;
  case 1:
    m_ch1VPos = value;
    break;
  case 2:
    m_ch2VScale = value;
    break;
  case 3:
    m_ch2VPos = value;
    break;
  case 4:
    m_timeScale = value;
    break;
  case 5:
    m_hPos = value;
    break;
  }
}

void OscilloscopeModule::update(float dt) {
  m_time += dt * 2.0f;

  for (size_t i = 0; i < m_waveformCh1.size(); i++) {
    float t = (i / float(m_waveformCh1.size())) * kPi * 4.0f + m_time;
    m_waveformCh1[i] = std::sin(t) * 0.7f;
    m_waveformCh2[i] = std::cos(t * 1.5f) * 0.5f;
  }
}

void OscilloscopeModule::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  nvgFillColor(ctx, nvgRGBA(180, 180, 185, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  nvgStrokeColor(ctx, nvgRGBA(140, 140, 145, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, 25.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 65, 255));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 12.f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, px + 10.f, py + 12.f, "KEYSIGHT  DSO-X 3024A", nullptr);

  const float screenW = pw * 0.65f;
  const float screenH = ph - 80.f;
  drawScreen(ctx, px + 10.f, py + 35.f, screenW - 20.f, screenH);

  const float controlX = px + screenW;
  const float controlW = pw - screenW;
  drawControlPanel(ctx, controlX, py + 35.f, controlW, screenH);

  const float bottomY = py + ph - 40.f;
  drawBottomPanel(ctx, px, bottomY, pw, 40.f);
}

void OscilloscopeModule::loadSvgAssets() {
  m_bezelSvg = tvg::Picture::gen();
  if (m_bezelSvg) {
    m_bezelSvg->load(kOscilloscopeBezelSvg, std::strlen(kOscilloscopeBezelSvg), "svg", true);
  }
}

void OscilloscopeModule::drawScrew(NVGcontext *ctx, float cx, float cy) {
  const float radius = 5.f;
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  nvgFillColor(ctx, nvgRGBA(80, 80, 85, 255));
  nvgFill(ctx);
}

void OscilloscopeModule::drawScreen(NVGcontext *ctx, float x, float y, float w, float h) {
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, 4.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgFill(ctx);

  const float screenX = x + 8.f;
  const float screenY = y + 8.f;
  const float screenW = w - 16.f;
  const float screenH = h - 16.f;

  nvgBeginPath(ctx);
  nvgRect(ctx, screenX, screenY, screenW, screenH);
  nvgFillColor(ctx, nvgRGBA(8, 12, 8, 255));
  nvgFill(ctx);

  if (m_ch1Enabled && !m_waveformCh1.empty()) {
    nvgBeginPath(ctx);
    for (size_t i = 0; i < m_waveformCh1.size(); i++) {
      float t = i / float(m_waveformCh1.size() - 1);
      float wx = screenX + t * screenW;
      float wy = screenY + screenH * 0.5f - m_waveformCh1[i] * screenH * 0.35f;
      if (i == 0)
        nvgMoveTo(ctx, wx, wy);
      else
        nvgLineTo(ctx, wx, wy);
    }
    nvgStrokeColor(ctx, nvgRGBA(255, 255, 100, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);
  }

  if (m_ch2Enabled && !m_waveformCh2.empty()) {
    nvgBeginPath(ctx);
    for (size_t i = 0; i < m_waveformCh2.size(); i++) {
      float t = i / float(m_waveformCh2.size() - 1);
      float wx = screenX + t * screenW;
      float wy = screenY + screenH * 0.5f - m_waveformCh2[i] * screenH * 0.35f;
      if (i == 0)
        nvgMoveTo(ctx, wx, wy);
      else
        nvgLineTo(ctx, wx, wy);
    }
    nvgStrokeColor(ctx, nvgRGBA(100, 255, 100, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);
  }
}

void OscilloscopeModule::drawControlPanel(NVGcontext *ctx, float x, float y, float w, float h) {
  // Control panel background
  nvgBeginPath(ctx);
  nvgRect(ctx, x, y, w, h);
  nvgFillColor(ctx, nvgRGBA(160, 160, 165, 255));
  nvgFill(ctx);

  const float knobSize = 28.f;
  const float colWidth = w * 0.5f;
  
  // Column 1: Channel 1
  float col1X = x + colWidth * 0.5f;
  float yPos1 = y + 15.f;
  
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 10.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, col1X, yPos1, "CHANNEL 1", nullptr);
  
  drawChannelIndicator(ctx, col1X, yPos1 + 20.f, 1, m_ch1Enabled);
  yPos1 += 50.f;
  
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 65, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, col1X, yPos1, "V/DIV", nullptr);
  drawKnob(ctx, col1X, yPos1 + 25.f, knobSize, m_ch1VScale);
  yPos1 += 70.f;
  
  nvgText(ctx, col1X, yPos1, "POSITION", nullptr);
  drawKnob(ctx, col1X, yPos1 + 25.f, knobSize, m_ch1VPos);
  yPos1 += 70.f;
  
  nvgText(ctx, col1X, yPos1, "COUPLING", nullptr);
  drawButton(ctx, col1X - 25.f, yPos1 + 15.f, 50.f, 18.f, "DC", m_ch1Coupling == 0);
  
  // Column 2: Channel 2
  float col2X = x + colWidth + colWidth * 0.5f;
  float yPos2 = y + 15.f;
  
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 10.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, col2X, yPos2, "CHANNEL 2", nullptr);
  
  drawChannelIndicator(ctx, col2X, yPos2 + 20.f, 2, m_ch2Enabled);
  yPos2 += 50.f;
  
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 65, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, col2X, yPos2, "V/DIV", nullptr);
  drawKnob(ctx, col2X, yPos2 + 25.f, knobSize, m_ch2VScale);
  yPos2 += 70.f;
  
  nvgText(ctx, col2X, yPos2, "POSITION", nullptr);
  drawKnob(ctx, col2X, yPos2 + 25.f, knobSize, m_ch2VPos);
  yPos2 += 70.f;
  
  nvgText(ctx, col2X, yPos2, "COUPLING", nullptr);
  drawButton(ctx, col2X - 25.f, yPos2 + 15.f, 50.f, 18.f, "DC", m_ch2Coupling == 0);
  
  // Timebase Section (bottom, spanning both columns)
  float timebaseY = y + h - 120.f;
  
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 10.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, x + w * 0.5f, timebaseY, "TIMEBASE", nullptr);
  timebaseY += 25.f;
  
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 65, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, col1X, timebaseY, "TIME/DIV", nullptr);
  drawKnob(ctx, col1X, timebaseY + 25.f, knobSize, m_timeScale);
  
  nvgText(ctx, col2X, timebaseY, "H-POS", nullptr);
  drawKnob(ctx, col2X, timebaseY + 25.f, knobSize, m_hPos);
}

void OscilloscopeModule::drawBottomPanel(NVGcontext *ctx, float x, float y, float w, float h) {
  nvgBeginPath(ctx);
  nvgRect(ctx, x, y, w, h);
  nvgFillColor(ctx, nvgRGBA(140, 140, 145, 255));
  nvgFill(ctx);
}

void OscilloscopeModule::drawKnob(NVGcontext *ctx, float cx, float cy, float radius, float value) {
  // Knob body
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint knobPaint =
      nvgRadialGradient(ctx, cx - radius * 0.3f, cy - radius * 0.3f, radius * 0.2f, radius,
                        nvgRGBA(232, 232, 234, 255), nvgRGBA(160, 160, 165, 255));
  nvgFillPaint(ctx, knobPaint);
  nvgFill(ctx);

  // Knob border
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  nvgStrokeColor(ctx, nvgRGBA(112, 112, 117, 255));
  nvgStrokeWidth(ctx, 1.f);
  nvgStroke(ctx);

  // Indicator line
  float angle = -2.356f + value * 4.712f; // -135° to +135°
  float lineLen = radius * 0.7f;
  float x1 = cx + std::cos(angle) * lineLen * 0.3f;
  float y1 = cy + std::sin(angle) * lineLen * 0.3f;
  float x2 = cx + std::cos(angle) * lineLen;
  float y2 = cy + std::sin(angle) * lineLen;

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, x1, y1);
  nvgLineTo(ctx, x2, y2);
  nvgStrokeColor(ctx, nvgRGBA(80, 80, 85, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgLineCap(ctx, NVG_ROUND);
  nvgStroke(ctx);
}

void OscilloscopeModule::drawButton(NVGcontext *ctx, float x, float y, float w, float h,
                                    const char *label, bool pressed) {
  // Button body
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, 3.f);

  if (pressed) {
    nvgFillColor(ctx, nvgRGBA(100, 180, 100, 255));
  } else {
    NVGpaint btnPaint = nvgLinearGradient(ctx, x, y, x, y + h, nvgRGBA(208, 208, 213, 255),
                                          nvgRGBA(160, 160, 168, 255));
    nvgFillPaint(ctx, btnPaint);
  }
  nvgFill(ctx);

  // Button border
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, 3.f);
  nvgStrokeColor(ctx, nvgRGBA(128, 128, 136, 255));
  nvgStrokeWidth(ctx, 1.f);
  nvgStroke(ctx);

  // Label
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, pressed ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, x + w * 0.5f, y + h * 0.5f, label, nullptr);
}

void OscilloscopeModule::drawChannelIndicator(NVGcontext *ctx, float cx, float cy, int channel,
                                              bool enabled) {
  const float radius = 10.f;

  // Indicator circle
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);

  if (enabled) {
    if (channel == 1) {
      nvgFillColor(ctx, nvgRGBA(255, 215, 0, 255)); // Yellow for CH1
    } else {
      nvgFillColor(ctx, nvgRGBA(0, 255, 0, 255)); // Green for CH2
    }
  } else {
    nvgFillColor(ctx, nvgRGBA(80, 80, 85, 255)); // Gray when disabled
  }
  nvgFill(ctx);

  // Border
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  nvgStrokeColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Channel number
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 12.f);
  nvgFillColor(ctx, nvgRGBA(0, 0, 0, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", channel);
  nvgText(ctx, cx, cy, buf, nullptr);
}

void OscilloscopeModule::drawPort(NVGcontext *ctx, float cx, float cy, const char *label) {
  const float radius = 12.f;

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  nvgFillColor(ctx, nvgRGBA(20, 20, 22, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius * 0.75f);
  NVGpaint portPaint =
      nvgRadialGradient(ctx, cx, cy - radius * 0.3f, radius * 0.2f, radius * 0.75f,
                        nvgRGBA(60, 120, 180, 255), nvgRGBA(30, 30, 32, 255));
  nvgFillPaint(ctx, portPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx - radius * 0.25f, cy - radius * 0.25f, radius * 0.3f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 40));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
  nvgText(ctx, cx, cy - radius - 3.f, label, nullptr);
}
