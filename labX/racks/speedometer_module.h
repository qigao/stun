#pragma once

#include "svg_assets.h"
#include <memory>
#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <lunasvg.h>

using namespace nanogui;

class SpeedometerModule : public Widget {
public:
  SpeedometerModule(Widget *parent);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  void update(float dt);
  void draw(NVGcontext *ctx) override;

private:
  void loadSvgAssets();
  void drawScrew(NVGcontext *ctx, float cx, float cy);
  void drawRpmGauge(NVGcontext *ctx, float cx, float cy, float radius);
  void drawSpeedGauge(NVGcontext *ctx, float cx, float cy, float radius);
  void drawNeedle(NVGcontext *ctx, float cx, float cy, float radius, float angle);
  void drawCenterDisplay(NVGcontext *ctx, float cx, float cy);
  void drawBottomStatus(NVGcontext *ctx, float x, float y, float w);
  void renderSvgGauge(NVGcontext *ctx, lunasvg::Document *svg, float cx, float cy, float size);
  void drawGaugeTicks(NVGcontext *ctx, float cx, float cy, float radius, 
                      float minVal, float maxVal, const char *unit);

  std::unique_ptr<lunasvg::Document> m_gaugeSvg;
  std::unique_ptr<lunasvg::Document> m_needleSvg;
  float m_speed;
  float m_targetSpeed;
  float m_rpm;
  float m_targetRpm;
  float m_totalKm;
  float m_avgConsumption;
  float m_temperature;
  float m_fuelPercent;
};
