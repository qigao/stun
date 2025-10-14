#include "whiteboard/properties_panel.h"

namespace whiteboard {

void PropertiesPanel::on_stroke_color_changed(const Color &color) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("stroke_color", color);
}

void PropertiesPanel::on_fill_color_changed(const Color &color) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("fill_color", color);
}

void PropertiesPanel::on_fill_none_changed(bool none) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("fill_none", none);
}

void PropertiesPanel::on_stroke_width_changed(float width) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("stroke_width", width);
}

void PropertiesPanel::on_opacity_changed(float opacity) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("opacity", opacity);
}

void PropertiesPanel::on_rotation_changed(float radians) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("rotation", radians);
}

void PropertiesPanel::on_position_changed(float x, float y) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("position", x, y);
}

void PropertiesPanel::on_size_changed(float width, float height) {
  if (m_updating || !m_canvas)
    return;
  m_canvas->apply_property_to_selected("size", width, height);
}

} // namespace whiteboard
