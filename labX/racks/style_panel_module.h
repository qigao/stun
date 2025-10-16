#pragma once

#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>

using namespace nanogui;

class StylePanelModule : public Widget {
public:
  struct ColorButton {
    float x, y, size;
    NVGcolor color;
    bool selected;
    bool hasPattern;
    int id;
  };

  struct IconButton {
    float x, y, size;
    std::string icon;
    bool selected;
    int id;
  };

  StylePanelModule(Widget *parent);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  void draw(NVGcontext *ctx) override;

private:
  void drawLabel(NVGcontext *ctx, float x, float y, const char *text);
  void drawColorButton(NVGcontext *ctx, float x, float y, float size, NVGcolor color, bool selected,
                       bool hasBorder, bool hasPattern);
  void drawIconButton(NVGcontext *ctx, float x, float y, float size, const char *icon,
                      bool selected);
  void drawSlider(NVGcontext *ctx, float x, float y, float w, float value, const char *minLabel,
                  const char *maxLabel);

  // Icon drawing helpers
  void drawStrokeWidthIcon(NVGcontext *ctx, float cx, float cy, float size, int type);
  void drawBorderStyleIcon(NVGcontext *ctx, float cx, float cy, float size, int type);
  void drawLineStyleIcon(NVGcontext *ctx, float cx, float cy, float size, int type);
  void drawCornerIcon(NVGcontext *ctx, float cx, float cy, float size, bool rounded);
  void drawArrowTypeIcon(NVGcontext *ctx, float cx, float cy, float size, int type);
  void drawEndpointIcon(NVGcontext *ctx, float cx, float cy, float size, int type);
  void drawLayerIcon(NVGcontext *ctx, float cx, float cy, float size, int direction);

  std::vector<ColorButton> m_strokeColors;
  std::vector<ColorButton> m_bgColors;
  std::vector<IconButton> m_strokeWidthButtons;
  std::vector<IconButton> m_borderStyleButtons;
  std::vector<IconButton> m_lineStyleButtons;
  std::vector<IconButton> m_cornerButtons;
  std::vector<IconButton> m_arrowTypeButtons;
  std::vector<IconButton> m_endpointButtons;
  std::vector<IconButton> m_layerButtons;

  float m_opacityValue;
  bool m_dragging;
  bool m_draggingSlider;
  Vector2i m_dragStart;
  float m_sliderY;
};
