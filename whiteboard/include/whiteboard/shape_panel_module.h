#pragma once

#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>
#include <functional>

using namespace nanogui;

namespace whiteboard {
class SVGShapeLibrary;
class SVGRenderer;
}

class ShapePanelModule : public Widget {
public:
  struct CategorySection {
    float y;
    float height;
    std::string name;
    bool expanded;
    std::vector<int> shape_indices; // Indices into m_all_shapes
  };

  struct ShapeButton {
    float x, y, size;
    std::string id;
    std::string name;
    std::string category;
    bool hovered;
  };

  ShapePanelModule(Widget *parent, whiteboard::SVGShapeLibrary *library);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  bool scroll_event(const Vector2i &p, const Vector2f &rel) override;
  void draw(NVGcontext *ctx) override;

  // Callback type
  using ShapeCallback = std::function<void(const std::string &)>;

  // Setter method
  void set_shape_callback(ShapeCallback cb) { m_shape_callback = cb; }

private:
  void rebuild_accordion();
  void drawLabel(NVGcontext *ctx, float x, float y, const char *text);
  void drawCategoryHeader(NVGcontext *ctx, const CategorySection &section, float px, float py);
  void drawShapeButton(NVGcontext *ctx, const ShapeButton &btn, float px, float py);
  void drawExpandIcon(NVGcontext *ctx, float x, float y, float size, bool expanded);

  whiteboard::SVGShapeLibrary *m_library;
  std::vector<CategorySection> m_categories;
  std::vector<ShapeButton> m_all_shapes;

  bool m_dragging;
  Vector2i m_dragStart;
  float m_scroll_offset;
  float m_max_scroll;

  ShapeCallback m_shape_callback;
  
  // SVG renderer for drawing shape icons
  std::unique_ptr<whiteboard::SVGRenderer> m_svg_renderer;
};
