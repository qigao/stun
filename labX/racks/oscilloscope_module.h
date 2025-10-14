#pragma once

#include "svg_assets.h"
#include <memory>
#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <thorvg.h>
#include <vector>

using namespace nanogui;

class OscilloscopeModule : public Widget {
public:
  OscilloscopeModule(Widget *parent);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                        int modifiers) override;
  void update(float dt);
  void draw(NVGcontext *ctx) override;

private:
  float getKnobValue(int id) const;
  void setKnobValue(int id, float value);
  void loadSvgAssets();
  void drawScrew(NVGcontext *ctx, float cx, float cy);
  void drawScreen(NVGcontext *ctx, float x, float y, float w, float h);
  void drawControlPanel(NVGcontext *ctx, float x, float y, float w, float h);
  void drawBottomPanel(NVGcontext *ctx, float x, float y, float w, float h);
  void drawKnob(NVGcontext *ctx, float cx, float cy, float radius, float value);
  void drawButton(NVGcontext *ctx, float x, float y, float w, float h, const char *label,
                  bool pressed);
  void drawChannelIndicator(NVGcontext *ctx, float cx, float cy, int channel, bool enabled);
  void drawPort(NVGcontext *ctx, float cx, float cy, const char *label);

  std::unique_ptr<tvg::Picture> m_bezelSvg;
  std::vector<float> m_waveformCh1;
  std::vector<float> m_waveformCh2;
  float m_time;

  bool m_ch1Enabled;
  float m_ch1VScale = 0.5f;
  float m_ch1VPos = 0.5f;
  int m_ch1Coupling = 0;

  bool m_ch2Enabled;
  float m_ch2VScale = 0.5f;
  float m_ch2VPos = 0.5f;
  int m_ch2Coupling = 0;

  float m_timeScale = 0.5f;
  float m_hPos = 0.5f;
  float m_trigLevel = 0.5f;
  int m_trigMode = 0;

  float m_ch1Vpp = 2.45f;
  float m_frequency = 1.23f;
  float m_period = 0.813f;

  int m_activeKnob;
  int m_dragStartY;
  float m_dragStartValue;
};
