#pragma once

#include "svg_assets.h"
#include <memory>
#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <thorvg.h>
#include <vector>

using namespace nanogui;

constexpr float kPi = 3.14159265358979323846f;

class VCVModulePanel : public Widget {
public:
  struct Knob {
    float x, y;
    float radius;
    float value;
    std::string label;
    int id;
  };

  struct Port {
    float x, y;
    std::string label;
    bool isInput;
    int id;
  };

  struct Cable {
    int fromPortId;
    int toPortId;
    NVGcolor color;
  };

  VCVModulePanel(Widget *parent);
  ~VCVModulePanel();

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                        int modifiers) override;
  bool mouse_enter_event(const Vector2i &p, bool enter) override;
  void draw(NVGcontext *ctx) override;

  std::vector<Cable> m_cables;

private:
  void loadPortSvgs();
  void drawScrew(NVGcontext *ctx, float cx, float cy);
  void drawKnob(NVGcontext *ctx, float cx, float cy, float radius, float value, const char *label);
  void renderSvgKnob(NVGcontext *ctx, tvg::Picture *svg, float cx, float cy, float size,
                     float value);
  void drawCable(NVGcontext *ctx, const Cable &cable);
  void drawPort(NVGcontext *ctx, float cx, float cy, const char *label, bool input);
  void renderSvgPort(NVGcontext *ctx, tvg::Picture *svg, float x, float y, float size);

  std::unique_ptr<tvg::Picture> m_inputPortSvg;
  std::unique_ptr<tvg::Picture> m_outputPortSvg;
  std::unique_ptr<tvg::Picture> m_largeKnobSvg;
  std::unique_ptr<tvg::Picture> m_smallKnobSvg;

  std::vector<Knob> m_knobs;
  int m_activeKnob = -1;
  int m_dragStartY = 0;
  float m_dragStartValue = 0.f;

  std::vector<Port> m_ports;
  bool m_cableDragging = false;
  int m_cableStartPort = -1;
  Vector2i m_cableDragPos;
};
