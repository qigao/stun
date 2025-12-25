/*
 * tvgbox2 - DividerWidget
 *
 * Horizontal or vertical separator line using Group/Shape composition.
 */

#ifndef TVGBOX2_DIVIDER_WIDGET_H
#define TVGBOX2_DIVIDER_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>

namespace tvgbox2 {

/**
 * DividerWidget - Separator line
 *
 * CSS variables:
 *   --divider-color: "r,g,b,a"     // Line color
 *   --divider-thickness: "1"       // Line thickness
 *   --divider-style: "solid" | "dashed" | "dotted"
 *   --divider-orientation: "horizontal" | "vertical"
 */
class DividerWidget : public Widget {
public:
  enum class Orientation { Horizontal, Vertical };
  enum class Style { Solid, Dashed, Dotted };

  explicit DividerWidget(Orientation orientation = Orientation::Horizontal);

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "DividerWidget"; }

  // Orientation
  Orientation orientation() const { return orientation_; }
  void set_orientation(Orientation o) { orientation_ = o; dirty_ = true; }

  // Style
  Style style() const { return style_; }
  void set_style(Style s) { style_ = s; dirty_ = true; }

  // Optional label
  const std::string& label() const { return label_; }
  void set_label(const std::string& label) { label_ = label; dirty_ = true; }

private:
  Orientation orientation_ = Orientation::Horizontal;
  Style style_ = Style::Solid;
  std::string label_;
};

} // namespace tvgbox2

#endif // TVGBOX2_DIVIDER_WIDGET_H
