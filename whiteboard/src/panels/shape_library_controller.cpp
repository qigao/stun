/**
 * \file shape_library_controller.cpp
 * \brief Implementation of ShapeLibraryController class.
 */

#include "whiteboard/panels/shape_library_controller.h"
#include "whiteboard/panels/shape_library_view.h"
#include <iostream>

namespace whiteboard {

ShapeLibraryController::ShapeLibraryController(WhiteboardDocument *document,
                                               SVGShapeLibrary *library,
                                               ShapeLibraryView *view)
    : m_document(document), m_library(library), m_view(view) {}

void ShapeLibraryController::place_shape(const std::string& shape_id) {
  if (!m_document || !m_library) {
    std::cerr << "Cannot place shape: document or library is null" << std::endl;
    return;
  }

  // Get default position (center of canvas)
  Point position = get_default_position();

  // Create stroke from shape
  Stroke stroke = m_library->create_shape(shape_id, position);

  if (stroke.svg_data.empty()) {
    std::cerr << "Failed to create shape: " << shape_id << std::endl;
    return;
  }

  // Add to document
  m_document->add_stroke(stroke);

  std::cout << "Placed shape: " << shape_id << " at (" << position.x << ", " << position.y << ")" << std::endl;
}

void ShapeLibraryController::select_category(const std::string& category) {
  if (m_view) {
    m_view->set_category(category);
  }
}

Point ShapeLibraryController::get_default_position() {
  // Place at center of visible canvas
  // In a full implementation, this would account for current zoom and pan
  // For now, use a simple default position
  return Point(400.0f, 300.0f);
}

} // namespace whiteboard
