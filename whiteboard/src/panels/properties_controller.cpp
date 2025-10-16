/**
 * \file properties_controller.cpp
 * \brief Implementation of PropertiesController class.
 */

#include "whiteboard/panels/properties_controller.h"
#include "whiteboard/panels/properties_view.h"
#include <iostream>

namespace whiteboard {

PropertiesController::PropertiesController(WhiteboardDocument *document, PropertiesView *view)
    : m_document(document), m_view(view) {}

void PropertiesController::change_stroke_color(const nanogui::Color &color) {
  apply_to_selected([&color](Stroke &stroke) { stroke.color = color; });
}

void PropertiesController::change_stroke_width(float width) {
  apply_to_selected([width](Stroke &stroke) { stroke.width = width; });
}

void PropertiesController::change_fill_color(const nanogui::Color &color) {
  apply_to_selected([&color](Stroke &stroke) { stroke.fill_color = color; });
}

void PropertiesController::change_fill_style(FillStyle style) {
  apply_to_selected([style](Stroke &stroke) { stroke.fill_style = style; });
}

void PropertiesController::change_position(float x, float y) {
  if (!m_document)
    return;

  const auto &selected = m_document->get_selected_indices();
  const auto &strokes = m_document->get_strokes();

  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke modified = strokes[idx];
      if (!modified.points.empty()) {
        float dx = x - modified.points[0].x;
        float dy = y - modified.points[0].y;
        modified.move(dx, dy);
        m_document->update_stroke(idx, modified);
      }
    }
  }
}

void PropertiesController::change_size(float width, float height) {
  // TODO: Implement size change for selected strokes
}

void PropertiesController::change_rotation(float angle) {
  apply_to_selected([angle](Stroke &stroke) { stroke.rotation = angle; });
}

void PropertiesController::apply_to_selected(std::function<void(Stroke &)> modifier) {
  if (!m_document)
    return;

  const auto &selected = m_document->get_selected_indices();
  const auto &strokes = m_document->get_strokes();

  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke modified = strokes[idx];
      modifier(modified);
      m_document->update_stroke(idx, modified);
    }
  }
}

void PropertiesController::edit_svg_parameters(int stroke_index) {
  if (!m_document)
    return;

  const auto &strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(strokes.size()))
    return;

  const auto &stroke = strokes[stroke_index];
  
  // Only edit SVG shapes
  if (stroke.tool != Tool::SVGShape)
    return;

  // TODO: Show parameter editor dialog
  // This will be implemented when the SVGParameterEditor is integrated
  // For now, just log that the feature was requested
  std::cout << "Edit SVG parameters for stroke " << stroke_index << std::endl;
  std::cout << "Shape ID: " << stroke.svg_shape_id << std::endl;
  std::cout << "Parameters:" << std::endl;
  for (const auto &param : stroke.svg_parameters) {
    std::cout << "  " << param.first << " = " << param.second << std::endl;
  }
}

} // namespace whiteboard
