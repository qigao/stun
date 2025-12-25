/*
 * tvgbox2 - SpinnerWidget
 *
 * Animated loading indicator using Group/Shape composition.
 */

#ifndef TVGBOX2_SPINNER_WIDGET_H
#define TVGBOX2_SPINNER_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"

namespace tvgbox2 {

/**
 * SpinnerWidget - Loading animation
 *
 * CSS variables:
 *   --spinner-color: "r,g,b,a"     // Primary color
 *   --spinner-track: "r,g,b,a"     // Track color (optional)
 *   --spinner-size: "24"           // Size in pixels
 *   --spinner-stroke: "3"          // Stroke width
 *   --spinner-speed: "1000"        // Rotation speed (ms per revolution)
 *
 * Variants (via classes):
 *   .spinner-ring   - Circular arc (default)
 *   .spinner-dots   - Pulsing dots
 *   .spinner-bars   - Vertical bars
 */
class SpinnerWidget : public Widget {
public:
  enum class Variant { Ring, Dots, Bars };

  explicit SpinnerWidget(Variant variant = Variant::Ring);

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "SpinnerWidget"; }

  // Control
  void start() { spinning_ = true; dirty_ = true; }
  void stop() { spinning_ = false; dirty_ = true; }
  bool is_spinning() const { return spinning_; }

  // Variant
  Variant variant() const { return variant_; }
  void set_variant(Variant v) { variant_ = v; dirty_ = true; }

private:
  void render_ring(flex::Renderer& r, const Element& elem);
  void render_dots(flex::Renderer& r, const Element& elem);
  void render_bars(flex::Renderer& r, const Element& elem);

  Variant variant_ = Variant::Ring;
  bool spinning_ = true;
  float rotation_ = 0;      // For ring
  float phase_ = 0;         // For dots/bars animation
};

} // namespace tvgbox2

#endif // TVGBOX2_SPINNER_WIDGET_H
