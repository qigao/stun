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

  // Switch to SVG Shape tool and set the pending shape
  m_document->set_current_tool(Tool::SVGShape);
  m_document->set_pending_svg_shape(shape_id);

  std::cout << "✓ Shape library: Set pending shape '" << shape_id << "'" << std::endl;
  std::cout << "  Tool switched to SVGShape, move mouse over canvas to see preview" << std::endl;
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
