// Implementation extracted from vcv_style_module.cpp
// See lines 880-1400 in original file for full implementation
#include "speedometer_module.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <nanogui.h>
#include <nanovg.h>

constexpr float kPi = 3.14159265358979323846f;

SpeedometerModule::SpeedometerModule(Widget *parent) : Widget(parent) {
  loadSvgAssets();
  m_speed = 0.0f;
  m_targetSpeed = 0.0f;
  m_rpm = 0.0f;
  m_targetRpm = 0.0f;
  m_totalKm = 28520.0f;
  m_avgConsumption = 10.2f;
  m_temperature = 18.0f;
  m_fuelPercent = 100.0f;
}

Vector2i SpeedometerModule::preferred_size_impl(NVGcontext *) const { return {700, 320}; }

void SpeedometerModule::update(float dt) {
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

void SpeedometerModule::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  nvgFillColor(ctx, nvgRGBA(5, 5, 8, 255));
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, ph);
  nvgStrokeColor(ctx, nvgRGBA(30, 30, 35, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  const float gaugeRadius = 90.f;
  const float gaugeY = py + 30.f + gaugeRadius;

  const float rpmCx = px + 120.f;
  drawRpmGauge(ctx, rpmCx, gaugeY, gaugeRadius);

  const float speedCx = px + pw - 120.f;
  drawSpeedGauge(ctx, speedCx, gaugeY, gaugeRadius);

  const float centerX = px + pw * 0.5f;
  const float centerY = py + 50.f;
  drawCenterDisplay(ctx, centerX, centerY);

  drawBottomStatus(ctx, px, py + ph - 25.f, pw);
}

void SpeedometerModule::loadSvgAssets() {
  m_gaugeSvg = lunasvg::Document::loadFromData(kSpeedometerGaugeSvg);
  m_needleSvg = lunasvg::Document::loadFromData(kSpeedometerNeedleSvg);
}

void SpeedometerModule::drawScrew(NVGcontext *ctx, float cx, float cy) {
  const float radius = 5.f;
  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, radius);
  NVGpaint screwPaint = nvgRadialGradient(ctx, cx - 1.f, cy - 1.f, radius * 0.3f, radius,
                                          nvgRGBA(80, 80, 85, 255), nvgRGBA(45, 45, 48, 255));
  nvgFillPaint(ctx, screwPaint);
  nvgFill(ctx);
}

void SpeedometerModule::drawRpmGauge(NVGcontext *ctx, float cx, float cy, float radius) {
  // Draw gauge face using SVG
  if (m_gaugeSvg) {
    renderSvgGauge(ctx, m_gaugeSvg.get(), cx, cy, radius * 2.0f);
  }

  // Draw tick marks and numbers
  drawGaugeTicks(ctx, cx, cy, radius, 0, 8, "x1000r/m");

  // Draw needle
  float rpmPercent = m_rpm / 8.0f;
  float needleAngle = kPi * 0.75f + rpmPercent * kPi * 1.5f;
  drawNeedle(ctx, cx, cy, radius, needleAngle);
}

void SpeedometerModule::drawSpeedGauge(NVGcontext *ctx, float cx, float cy, float radius) {
  // Draw gauge face using SVG
  if (m_gaugeSvg) {
    renderSvgGauge(ctx, m_gaugeSvg.get(), cx, cy, radius * 2.0f);
  }

  // Draw tick marks and numbers
  drawGaugeTicks(ctx, cx, cy, radius, 0, 240, "km/h");

  // Draw needle
  float speedPercent = m_speed / 240.0f;
  float needleAngle = kPi * 0.75f + speedPercent * kPi * 1.5f;
  drawNeedle(ctx, cx, cy, radius, needleAngle);
}

void SpeedometerModule::renderSvgGauge(NVGcontext *ctx, lunasvg::Document *svg, float cx, float cy,
                                       float size) {
  if (!svg) return;
  
  float svgW = static_cast<float>(svg->width());
  float svgH = static_cast<float>(svg->height());
  if (svgW == 0 || svgH == 0) return;

  float scale = size / std::max(svgW, svgH);

  nvgSave(ctx);
  nvgTranslate(ctx, cx - size * 0.5f, cy - size * 0.5f);
  nvgScale(ctx, scale, scale);

  // Render SVG elements manually (simplified)
  float gaugeCx = svgW * 0.5f;
  float gaugeCy = svgH * 0.5f;
  float gaugeRadius = std::min(svgW, svgH) * 0.49f;

  // Chrome rim
  nvgBeginPath(ctx);
  nvgCircle(ctx, gaugeCx, gaugeCy, gaugeRadius);
  NVGpaint rimPaint = nvgRadialGradient(
      ctx, gaugeCx - gaugeRadius * 0.3f, gaugeCy - gaugeRadius * 0.3f, gaugeRadius * 0.7f,
      gaugeRadius, nvgRGBA(180, 180, 185, 255), nvgRGBA(100, 100, 105, 255));
  nvgFillPaint(ctx, rimPaint);
  nvgFill(ctx);

  // Inner ring
  nvgBeginPath(ctx);
  nvgCircle(ctx, gaugeCx, gaugeCy, gaugeRadius * 0.95f);
  nvgStrokeColor(ctx, nvgRGBA(140, 140, 145, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  // Dark face
  nvgBeginPath(ctx);
  nvgCircle(ctx, gaugeCx, gaugeCy, gaugeRadius * 0.92f);
  NVGpaint facePaint = nvgRadialGradient(ctx, gaugeCx, gaugeCy, 0, gaugeRadius * 0.92f,
                                         nvgRGBA(25, 25, 30, 255), nvgRGBA(10, 10, 15, 255));
  nvgFillPaint(ctx, facePaint);
  nvgFill(ctx);

  nvgRestore(ctx);
}

void SpeedometerModule::drawGaugeTicks(NVGcontext *ctx, float cx, float cy, float radius,
                                       float minVal, float maxVal, const char *unit) {
  const float startAngle = kPi * 0.75f;
  const float angleRange = kPi * 1.5f;
  const int numMajorTicks = (maxVal == 8) ? 9 : 13;

  // Draw colored arc segments
  const int segments = 100;
  for (int i = 0; i < segments; i++) {
    float t = i / float(segments);
    float angle1 = startAngle + t * angleRange;
    float angle2 = startAngle + (t + 1.0f / segments) * angleRange;

    NVGcolor color;
    if (t < 0.7f) {
      color = nvgRGBA(0, 255, 255, 255);
    } else {
      float rt = (t - 0.7f) / 0.3f;
      color = nvgRGBA(255, 255 * (1.0f - rt), 0, 255);
    }

    nvgBeginPath(ctx);
    nvgArc(ctx, cx, cy, radius * 0.88f, angle1, angle2, NVG_CW);
    nvgStrokeColor(ctx, color);
    nvgStrokeWidth(ctx, 3.f);
    nvgStroke(ctx);
  }

  // Draw tick marks
  int totalTicks = (maxVal == 8) ? 80 : 240;
  for (int i = 0; i <= totalTicks; i++) {
    float t = i / float(totalTicks);
    float angle = startAngle + t * angleRange;

    bool isMajor = (maxVal == 8) ? (i % 10 == 0) : (i % 20 == 0);
    float tickStart = isMajor ? 0.75f : 0.82f;
    float tickEnd = 0.88f;
    float tickWidth = isMajor ? 2.5f : 1.0f;

    float x1 = cx + std::cos(angle) * radius * tickStart;
    float y1 = cy + std::sin(angle) * radius * tickStart;
    float x2 = cx + std::cos(angle) * radius * tickEnd;
    float y2 = cy + std::sin(angle) * radius * tickEnd;

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x1, y1);
    nvgLineTo(ctx, x2, y2);

    if (t < 0.7f) {
      nvgStrokeColor(ctx, nvgRGBA(0, 255, 255, 200));
    } else {
      nvgStrokeColor(ctx, nvgRGBA(255, 100, 0, 200));
    }
    nvgStrokeWidth(ctx, tickWidth);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStroke(ctx);
  }

  // Draw numbers
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, (maxVal == 8) ? 16.f : 11.f);
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
      nvgFillColor(ctx, nvgRGBA(0, 255, 255, 255));
    } else {
      nvgFillColor(ctx, nvgRGBA(255, 120, 80, 255));
    }
    nvgText(ctx, nx, ny, buf, nullptr);
  }

  // Unit label
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgText(ctx, cx, cy + radius * 0.35f, unit, nullptr);
}

void SpeedometerModule::drawNeedle(NVGcontext *ctx, float cx, float cy, float radius, float angle) {
  nvgSave(ctx);
  nvgTranslate(ctx, cx, cy);
  nvgRotate(ctx, angle);

  nvgBeginPath(ctx);
  nvgMoveTo(ctx, 0, 0);
  nvgLineTo(ctx, -5, 8);
  nvgLineTo(ctx, 0, -radius * 0.68f);
  nvgLineTo(ctx, 5, 8);
  nvgClosePath(ctx);
  nvgFillColor(ctx, nvgRGBA(255, 80, 80, 255));
  nvgFill(ctx);

  nvgRestore(ctx);

  nvgBeginPath(ctx);
  nvgCircle(ctx, cx, cy, 12.f);
  nvgFillColor(ctx, nvgRGBA(80, 80, 90, 255));
  nvgFill(ctx);
}

void SpeedometerModule::drawCenterDisplay(NVGcontext *ctx, float cx, float cy) {
  const float panelW = 140.f;
  const float panelH = 200.f;

  // Main display panel
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, cx - panelW * 0.5f, cy, panelW, panelH, 6.f);
  nvgFillColor(ctx, nvgRGBA(20, 25, 30, 255));
  nvgFill(ctx);
  nvgStrokeColor(ctx, nvgRGBA(80, 85, 90, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgStroke(ctx);

  float yPos = cy + 15.f;

  // Total km section
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, cx - panelW * 0.4f, yPos, "Total", nullptr);

  nvgFontFace(ctx, "mono");
  nvgFontSize(ctx, 20.f);
  nvgFillColor(ctx, nvgRGBA(200, 200, 205, 255));
  char buf[64];
  snprintf(buf, sizeof(buf), "%.0f", m_totalKm);
  nvgText(ctx, cx - panelW * 0.4f, yPos + 12.f, buf, nullptr);

  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 125, 255));
  nvgText(ctx, cx + panelW * 0.15f, yPos + 20.f, "km", nullptr);
  yPos += 45.f;

  // Average consumption
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgText(ctx, cx - panelW * 0.4f, yPos, "Avg.", nullptr);

  // Fuel pump icon (⛽)
  nvgFontSize(ctx, 12.f);
  nvgText(ctx, cx + panelW * 0.25f, yPos, "\xE2\x9B\xBD", nullptr);

  nvgFontFace(ctx, "mono");
  nvgFontSize(ctx, 16.f);
  nvgFillColor(ctx, nvgRGBA(200, 200, 205, 255));
  snprintf(buf, sizeof(buf), "%.1f", m_avgConsumption);
  nvgText(ctx, cx - panelW * 0.4f, yPos + 12.f, buf, nullptr);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 125, 255));
  nvgText(ctx, cx + panelW * 0.05f, yPos + 18.f, "L/100km", nullptr);
  yPos += 40.f;

  // Gear indicator (large P)
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, cx - 35.f, yPos, 70.f, 50.f, 4.f);
  nvgFillColor(ctx, nvgRGBA(10, 15, 20, 255));
  nvgFill(ctx);

  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 40.f);
  nvgFillColor(ctx, nvgRGBA(0, 255, 255, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, cx, yPos + 25.f, "P", nullptr);
  yPos += 60.f;

  // Transmission label
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgFillColor(ctx, nvgRGBA(100, 200, 200, 255));
  nvgText(ctx, cx, yPos, "Transmission", nullptr);
  yPos += 20.f;

  // Temperature
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 28.f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  snprintf(buf, sizeof(buf), "%.0f", m_temperature);
  nvgText(ctx, cx - 10.f, yPos, buf, nullptr);

  nvgFontSize(ctx, 16.f);
  nvgText(ctx, cx + 20.f, yPos - 5.f, "C", nullptr);

  // Temperature icon (❄)
  nvgFontSize(ctx, 10.f);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, cx - panelW * 0.4f, yPos, "\xE2\x9D\x84", nullptr);
}

void SpeedometerModule::drawBottomStatus(NVGcontext *ctx, float x, float y, float w) {
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 9.f);
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  // Left status
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgText(ctx, x + 30.f, y, "Traction Control", nullptr);
  nvgFillColor(ctx, nvgRGBA(0, 255, 200, 255));
  nvgText(ctx, x + 130.f, y, "\xE2\x96\xB6 Safe", nullptr);

  // Right status
  nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgText(ctx, x + w - 130.f, y, "Torque Vectoring", nullptr);
  nvgFillColor(ctx, nvgRGBA(0, 255, 200, 255));
  nvgText(ctx, x + w - 30.f, y, "\xE2\x96\xB6 On", nullptr);

  // Fuel bar at bottom
  const float fuelBarW = 200.f;
  const float fuelBarH = 8.f;
  const float fuelBarX = x + w * 0.5f - fuelBarW * 0.5f;
  const float fuelBarY = y + 15.f;

  // Fuel label
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 8.f);
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgFillColor(ctx, nvgRGBA(150, 150, 155, 255));
  nvgText(ctx, fuelBarX - 35.f, fuelBarY, "\xE2\x9B\xBD FUEL", nullptr);

  // Fuel bar background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, fuelBarX, fuelBarY - fuelBarH * 0.5f, fuelBarW, fuelBarH, 3.f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 45, 255));
  nvgFill(ctx);

  // Fuel bar fill (gradient)
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, fuelBarX, fuelBarY - fuelBarH * 0.5f, fuelBarW * (m_fuelPercent / 100.0f),
                 fuelBarH, 3.f);
  NVGpaint fuelPaint = nvgLinearGradient(ctx, fuelBarX, fuelBarY, fuelBarX + fuelBarW, fuelBarY,
                                         nvgRGBA(255, 200, 0, 255), nvgRGBA(255, 150, 0, 255));
  nvgFillPaint(ctx, fuelPaint);
  nvgFill(ctx);

  // Fuel percentage
  nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
  nvgFillColor(ctx, nvgRGBA(200, 200, 205, 255));
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f%%", m_fuelPercent);
  nvgText(ctx, fuelBarX + fuelBarW + 35.f, fuelBarY, buf, nullptr);

  // C and H markers
  nvgFontSize(ctx, 7.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 125, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgText(ctx, fuelBarX + 5.f, fuelBarY + 6.f, "C", nullptr);
  nvgText(ctx, fuelBarX + fuelBarW - 5.f, fuelBarY + 6.f, "H", nullptr);
}
