#include "speedometer_rack.h"
#include <algorithm>
#include <cmath>
#include <nanogui.h>

constexpr float kPi = 3.14159265358979323846f;

SpeedometerRack::SpeedometerRack(Widget *parent) : Widget(parent) {}

Vector2i SpeedometerRack::preferred_size_impl(NVGcontext *) const { return {720, 380}; }

void SpeedometerRack::update(float dt) {
  m_speed += (m_targetSpeed - m_speed) * dt * 3.0f;
  m_rpm += (m_targetRpm - m_rpm) * dt * 4.0f;

  static float time = 0.0f;
  time += dt;
  m_targetSpeed = 80.0f + std::sin(time * 0.5f) * 60.0f + std::cos(time * 0.3f) * 30.0f;
  m_targetSpeed = std::clamp(m_targetSpeed, 0.0f, 240.0f);

  m_targetRpm = 2.0f + std::sin(time * 0.6f) * 2.0f + std::cos(time * 0.4f) * 1.5f;
  m_targetRpm = std::clamp(m_targetRpm, 0.0f, 8.0f);

  m_totalKm += m_speed * dt / 3600.0f;
}

void SpeedometerRack::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  drawRackPanel(ctx, px, py, pw, ph);

  const float gaugeRadius = 80.f;
  const float gaugeY = py + 40.f + gaugeRadius;

  const float rpmCx = px + 130.f;
  drawRpmGauge(ctx, rpmCx, gaugeY, gaugeRadius);

  const float speedCx = px + pw - 130.f;
  drawSpeedGauge(ctx, speedCx, gaugeY, gaugeRadius);

  const float centerX = px + pw * 0.5f;
  const float centerY = py + 60.f;
  drawCenterDisplay(ctx, centerX, centerY);
}

void SpeedometerRack::drawRackPanel(NVGcontext *ctx, float x, float y, float w, float h) {
  // Rack panel background
  nvgBeginPath(ctx);
  nvgRect(ctx, x, y, w, h);
  NVGpaint bgPaint = nvgLinearGradient(ctx, x, y, x, y + h, nvgRGBA(70, 70, 75, 255),
                                       nvgRGBA(50, 50, 55, 255));
  nvgFillPaint(ctx, bgPaint);
  nvgFill(ctx);

  // Border
  nvgBeginPath(ctx);
  nvgRect(ctx, x, y, w, h);
  nvgStrokeColor(ctx, nvgRGBA(30, 30, 35, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Screws
  drawScrew(ctx, x + 10.f, y + 10.f);
  drawScrew(ctx, x + w - 10.f, y + 10.f);
  drawScrew(ctx, x + 10.f, y + h - 10.f);
  drawScrew(ctx, x + w - 10.f, y + h - 10.f);

  // Title
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(200, 200, 205, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, x + w * 0.5f, y + 8.f, "SPEEDOMETER", nullptr);
}

void SpeedometerRack::drawScrew(NVGcontext *ctx, float cx, float cy) {
  const float radius = 5.f;
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint screwPaint = nvgRadialGradient(ctx, cx - 1.f, cy - 1.f, radius * 0.3f, radius,
                                          nvgRGBA(100, 100, 105, 255), nvgRGBA(60, 60, 65, 255));
  nvgFillPaint(ctx, screwPaint);
  nvgFill(ctx);

  // Screw slot
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, cx - radius * 0.6f, cy);
  nvgLineTo(ctx, cx + radius * 0.6f, cy);
  nvgStrokeColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);
}

void SpeedometerRack::drawRpmGauge(NVGcontext *ctx, float cx, float cy, float radius) {
  // Gauge face
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint rimPaint = nvgRadialGradient(ctx, cx - radius * 0.3f, cy - radius * 0.3f,
                                        radius * 0.7f, radius, nvgRGBA(120, 120, 125, 255),
                                        nvgRGBA(80, 80, 85, 255));
  nvgFillPaint(ctx, rimPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius * 0.92f);
  NVGpaint facePaint = nvgRadialGradient(ctx, cx, cy, 0, radius * 0.92f, nvgRGBA(25, 25, 30, 255),
                                         nvgRGBA(10, 10, 15, 255));
  nvgFillPaint(ctx, facePaint);
  nvgFill(ctx);

  drawGaugeTicks(ctx, cx, cy, radius, 0, 8, "x1000r/m");

  float rpmPercent = m_rpm / 8.0f;
  float needleAngle = kPi * 0.75f + rpmPercent * kPi * 1.5f;
  drawNeedle(ctx, cx, cy, radius, needleAngle);
}

void SpeedometerRack::drawSpeedGauge(NVGcontext *ctx, float cx, float cy, float radius) {
  // Gauge face
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint rimPaint = nvgRadialGradient(ctx, cx - radius * 0.3f, cy - radius * 0.3f,
                                        radius * 0.7f, radius, nvgRGBA(120, 120, 125, 255),
                                        nvgRGBA(80, 80, 85, 255));
  nvgFillPaint(ctx, rimPaint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius * 0.92f);
  NVGpaint facePaint = nvgRadialGradient(ctx, cx, cy, 0, radius * 0.92f, nvgRGBA(25, 25, 30, 255),
                                         nvgRGBA(10, 10, 15, 255));
  nvgFillPaint(ctx, facePaint);
  nvgFill(ctx);

  drawGaugeTicks(ctx, cx, cy, radius, 0, 240, "km/h");

  float speedPercent = m_speed / 240.0f;
  float needleAngle = kPi * 0.75f + speedPercent * kPi * 1.5f;
  drawNeedle(ctx, cx, cy, radius, needleAngle);
}

void SpeedometerRack::drawGaugeTicks(NVGcontext *ctx, float cx, float cy, float radius,
                                     float minVal, float maxVal, const char *unit) {
  const float startAngle = kPi * 0.75f;
  const float angleRange = kPi * 1.5f;
  const int numMajorTicks = (maxVal == 8) ? 9 : 13;

  // Colored arc
  const int segments = 100;
  for (int i = 0; i < segments; i++) {
    float t = i / float(segments);
    float angle1 = startAngle + t * angleRange;
    float angle2 = startAngle + (t + 1.0f / segments) * angleRange;

    NVGcolor color;
    if (t < 0.7f) {
      color = nvgRGBA(0, 200, 200, 255);
    } else {
      float rt = (t - 0.7f) / 0.3f;
      color = nvgRGBA(255, 200 * (1.0f - rt), 0, 255);
    }

    nvgBeginPath(ctx);
    nvgArc(ctx, cx, cy, radius * 0.88f, angle1, angle2, NVG_CW);
    nvgStrokeColor(ctx, color);
    nvgStrokeWidth(ctx, 2.5f);
    nvgStroke(ctx);
  }

  // Tick marks
  int totalTicks = (maxVal == 8) ? 80 : 240;
  for (int i = 0; i <= totalTicks; i++) {
    float t = i / float(totalTicks);
    float angle = startAngle + t * angleRange;

    bool isMajor = (maxVal == 8) ? (i % 10 == 0) : (i % 20 == 0);
    float tickStart = isMajor ? 0.75f : 0.82f;
    float tickEnd = 0.88f;
    float tickWidth = isMajor ? 2.0f : 1.0f;

    float x1 = cx + std::cos(angle) * radius * tickStart;
    float y1 = cy + std::sin(angle) * radius * tickStart;
    float x2 = cx + std::cos(angle) * radius * tickEnd;
    float y2 = cy + std::sin(angle) * radius * tickEnd;

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x1, y1);
    nvgLineTo(ctx, x2, y2);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 185, 200));
    nvgStrokeWidth(ctx, tickWidth);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStroke(ctx);
  }

  // Numbers
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, (maxVal == 8) ? 14.f : 10.f);
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  for (int i = 0; i < numMajorTicks; i++) {
    float t = i / float(numMajorTicks - 1);
    float angle = startAngle + t * angleRange;
    float value = minVal + (maxVal - minVal) * t;

    float nx = cx + std::cos(angle) * radius * 0.62f;
    float ny = cy + std::sin(angle) * radius * 0.62f;

    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f", value);

    if (t < 0.7f) {
      nvgFillColor(ctx, nvgRGBA(0, 200, 200, 255));
    } else {
      nvgFillColor(ctx, nvgRGBA(255, 150, 80, 255));
    }
    nvgText(ctx, nx, ny, buf, nullptr);
  }

  // Unit label
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgText(ctx, cx, cy + radius * 0.35f, unit, nullptr);
}

void SpeedometerRack::drawNeedle(NVGcontext *ctx, float cx, float cy, float radius, float angle) {
  nvgSave(ctx);
  nvgTranslate(ctx, cx, cy);
  nvgRotate(ctx, angle);

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, 0, 0);
  nvgLineTo(ctx, -4, 6);
  nvgLineTo(ctx, 0, -radius * 0.68f);
  nvgLineTo(ctx, 4, 6);
  nvgClosePath(ctx);
  nvgFillColor(ctx, nvgRGBA(255, 80, 80, 255));
  nvgFill(ctx);

  nvgRestore(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, 10.f);
  nvgFillColor(ctx, nvgRGBA(80, 80, 90, 255));
  nvgFill(ctx);
}

void SpeedometerRack::drawCenterDisplay(NVGcontext *ctx, float cx, float cy) {
  const float panelW = 120.f;
  const float panelH = 180.f;

  // Display panel
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, cx - panelW * 0.5f, cy, panelW, panelH, 4.f);
  nvgFillColor(ctx, nvgRGBA(15, 20, 25, 255));
  nvgFill(ctx);
  nvgStrokeColor(ctx, nvgRGBA(60, 65, 70, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);

  float yPos = cy + 12.f;

  // Total km
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, cx - panelW * 0.4f, yPos, "Total", nullptr);

  nvgFontFace(ctx, "mono");
  nvgFontSize(ctx, 18.f);
  nvgFillColor(ctx, nvgRGBA(200, 200, 205, 255));
  char buf[64];
  snprintf(buf, sizeof(buf), "%.0f", m_totalKm);
  nvgText(ctx, cx - panelW * 0.4f, yPos + 10.f, buf, nullptr);

  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 125, 255));
  nvgText(ctx, cx + panelW * 0.15f, yPos + 18.f, "km", nullptr);
  yPos += 40.f;

  // Avg consumption
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgText(ctx, cx - panelW * 0.4f, yPos, "Avg. Fuel", nullptr);

  nvgFontFace(ctx, "mono");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(200, 200, 205, 255));
  snprintf(buf, sizeof(buf), "%.1f", m_avgConsumption);
  nvgText(ctx, cx - panelW * 0.4f, yPos + 10.f, buf, nullptr);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 7.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 125, 255));
  nvgText(ctx, cx + panelW * 0.05f, yPos + 16.f, "L/100km", nullptr);
  yPos += 35.f;

  // Gear indicator
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, cx - 30.f, yPos, 60.f, 45.f, 3.f);
  nvgFillColor(ctx, nvgRGBA(5, 10, 15, 255));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 36.f);
  nvgFillColor(ctx, nvgRGBA(0, 200, 200, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, cx, yPos + 22.f, "P", nullptr);
  yPos += 55.f;

  // Temperature
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 24.f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  snprintf(buf, sizeof(buf), "%.0f", m_temperature);
  nvgText(ctx, cx - 8.f, yPos, buf, nullptr);

  nvgFontSize(ctx, 14.f);
  nvgText(ctx, cx + 18.f, yPos - 4.f, "C", nullptr);

  // Fuel bar
  const float fuelBarW = panelW * 0.8f;
  const float fuelBarH = 6.f;
  const float fuelBarX = cx - fuelBarW * 0.5f;
  const float fuelBarY = cy + panelH - 15.f;

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, fuelBarX, fuelBarY - fuelBarH * 0.5f, fuelBarW, fuelBarH, 2.f);
  nvgFillColor(ctx, nvgRGBA(30, 30, 35, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, fuelBarX, fuelBarY - fuelBarH * 0.5f, fuelBarW * (m_fuelPercent / 100.0f),
                 fuelBarH, 2.f);
  nvgFillColor(ctx, nvgRGBA(255, 180, 0, 255));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 7.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  snprintf(buf, sizeof(buf), "FUEL %.0f%%", m_fuelPercent);
  nvgText(ctx, cx, fuelBarY + 4.f, buf, nullptr);
}
