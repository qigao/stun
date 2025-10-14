#pragma once

#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>

using namespace nanogui;

class WaspFilterModule : public Widget {
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

  WaspFilterModule(Widget *parent);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                        int modifiers) override;
  void draw(NVGcontext *ctx) override;

private:
  void loadSvgAssets();
  void drawScrew(NVGcontext *ctx, float cx, float cy);
  void drawPort(NVGcontext *ctx, float cx, float cy, const char *label, bool input);
  void drawKnobWithScale(NVGcontext *ctx, float cx, float cy, float radius, float value,
                         const char *label);

  std::vector<Knob> m_knobs;
  std::vector<Port> m_ports;
  int m_activeKnob;
  int m_dragStartY;
  float m_dragStartValue;
};
