/**
 * \file properties_controller.cpp
 * \brief Implementation of PropertiesController class.
 */

#include "whiteboard/panels/properties_controller.h"
#include "whiteboard/panels/properties_view.h"

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

} // namespace whiteboard
