#pragma once

#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>

using namespace nanogui;

class ToolbarPanelModule : public Widget {
public:
  struct ToolButton {
    float x, y, size;
    std::string icon;
    bool selected;
    int id;
  };

  ToolbarPanelModule(Widget *parent);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  void draw(NVGcontext *ctx) override;

private:
  void drawToolButton(NVGcontext *ctx, float x, float y, float size, const char *icon,
                      bool selected, bool hovered);
  void drawLockIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawHandIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawCursorIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawSquareIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawDiamondIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawCircleIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawArrowIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawLineIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawPenIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawTextIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawImageIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawRotateIcon(NVGcontext *ctx, float cx, float cy, float size);
  void drawTreeIcon(NVGcontext *ctx, float cx, float cy, float size);

  std::vector<ToolButton> m_buttons;
  int m_hoveredButton;
  int m_selectedButton;
  bool m_dragging;
  Vector2i m_dragStart;
};
