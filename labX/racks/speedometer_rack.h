#pragma once
#include <nanogui.h>
#include <nanovg.h>
#include <vector>

using namespace nanogui;

class SpeedometerRack : public Widget {
public:
  SpeedometerRack(Widget *parent);
  ~SpeedometerRack() override = default;

  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;
  void update(float dt);

private:
  void drawRackPanel(NVGcontext *ctx, float x, float y, float w, float h);
  void drawScrew(NVGcontext *ctx, float cx, float cy);
  void drawRpmGauge(NVGcontext *ctx, float cx, float cy, float radius);
  void drawSpeedGauge(NVGcontext *ctx, float cx, float cy, float radius);
  void drawGaugeTicks(NVGcontext *ctx, float cx, float cy, float radius, float minVal,
                      float maxVal, const char *unit);
  void drawNeedle(NVGcontext *ctx, float cx, float cy, float radius, float angle);
  void drawCenterDisplay(NVGcontext *ctx, float cx, float cy);

  float m_speed = 0.0f;
  float m_targetSpeed = 0.0f;
  float m_rpm = 0.0f;
  float m_targetRpm = 0.0f;
  float m_totalKm = 28520.0f;
  float m_avgConsumption = 10.2f;
  float m_temperature = 18.0f;
  float m_fuelPercent = 100.0f;
};
