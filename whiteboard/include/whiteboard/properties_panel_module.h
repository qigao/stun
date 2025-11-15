#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/types.h"
#include <map>
#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>

using namespace nanogui;

// Forward declarations
namespace whiteboard {
class WhiteboardDocument;
class PropertiesController;
} // namespace whiteboard

class PropertiesPanelModule : public Widget, public whiteboard::IDocumentObserver {
public:
  enum class Tab { Properties, SVGParameters };

  struct ColorButton {
    float x, y, size;
    NVGcolor color;
    bool selected;
    int id;
  };

  struct IconButton {
    float x, y, size;
    std::string icon;
    bool selected;
    int id;
  };

  PropertiesPanelModule(Widget *parent);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  void draw(NVGcontext *ctx) override;

  // Set controller
  void set_controller(whiteboard::PropertiesController *controller);

  // Set document reference
  void set_document(whiteboard::WhiteboardDocument *document);

  // IDocumentObserver interface
  void on_strokes_changed() override;
  void on_selection_changed() override;
  void on_tool_changed() override;
  void on_properties_changed() override;

  // Update UI to reflect current selection
  void update_from_selection();

  // Set whether current selection is SVG shape
  void set_svg_shape(bool is_svg, const std::string &shape_id = "",
                     const std::map<std::string, std::string> &params = {});

private:
  void drawSection(NVGcontext *ctx, float x, float y, float w, const char *title, float &yOffset);
  void drawColorButton(NVGcontext *ctx, float x, float y, float size, NVGcolor color, bool selected,
                       bool hasBorder);
  void drawIconButton(NVGcontext *ctx, float x, float y, float size, const char *icon,
                      bool selected);
  void drawSlider(NVGcontext *ctx, float x, float y, float w, float value, const char *minLabel,
                  const char *maxLabel);
  void drawLabel(NVGcontext *ctx, float x, float y, const char *text);

  // Icon drawing helpers
  void drawDashIcon(NVGcontext *ctx, float cx, float cy, float size, int dashType);
  void drawLineStyleIcon(NVGcontext *ctx, float cx, float cy, float size, int styleType);
  void drawCornerIcon(NVGcontext *ctx, float cx, float cy, float size, bool rounded);
  void drawLayerIcon(NVGcontext *ctx, float cx, float cy, float size, int direction);
  void drawFontIcon(NVGcontext *ctx, float cx, float cy, float size, int fontType);
  void drawTextSizeIcon(NVGcontext *ctx, float cx, float cy, float size, int sizeType);
  void drawAlignIcon(NVGcontext *ctx, float cx, float cy, float size, int alignType);

  void drawTabs(NVGcontext *ctx, float x, float y, float w);
  void drawSVGParameters(NVGcontext *ctx, float x, float y, float w);

  // UI update helpers
  void update_from_stroke(const whiteboard::Stroke &stroke);
  void update_from_defaults();
  void update_for_multi_selection();

  // Button click handlers
  void handle_stroke_color_click(int color_index);
  void handle_fill_color_click(int color_index);
  void handle_stroke_width_click(int width_index);
  void handle_stroke_style_click(int style_index);
  void handle_corner_click(int corner_index);
  void handle_font_click(int font_index);
  void handle_text_size_click(int size_index);
  void handle_align_click(int align_index);
  void handle_layer_click(int layer_action);

  // Slider handlers
  void handle_opacity_change(float value);
  void handle_rotation_change(float value);
  void handle_scale_x_change(float value);
  void handle_scale_y_change(float value);

  std::vector<ColorButton> m_strokeColors;
  std::vector<ColorButton> m_bgColors;
  std::vector<IconButton> m_strokeWidthButtons;
  std::vector<IconButton> m_borderStyleButtons;
  std::vector<IconButton> m_lineStyleButtons;
  std::vector<IconButton> m_cornerButtons;
  std::vector<IconButton> m_fontButtons;
  std::vector<IconButton> m_textSizeButtons;
  std::vector<IconButton> m_alignButtons;
  std::vector<IconButton> m_layerButtons;

  float m_opacityValue;
  float m_rotationValue;
  float m_scaleXValue;
  float m_scaleYValue;
  bool m_dragging;
  bool m_draggingSlider;
  bool m_draggingRotationSlider;
  bool m_draggingScaleXSlider;
  bool m_draggingScaleYSlider;
  Vector2i m_dragStart;
  float m_sliderY;
  float m_rotationSliderY;
  float m_scaleXSliderY;
  float m_scaleYSliderY;

  // SVG shape support
  Tab m_current_tab;
  bool m_is_svg_shape;
  std::string m_svg_shape_id;
  std::map<std::string, std::string> m_svg_parameters;

  // MVC connections
  whiteboard::PropertiesController *m_controller = nullptr;
  whiteboard::WhiteboardDocument *m_document = nullptr;
};
