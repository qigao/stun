/**
 * \file shape_library_controller.h
 * \brief Controller component of Shape Library Panel MVC.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/svg/svg_shape_library.h"
#include "whiteboard/types.h"
#include <string>

namespace whiteboard {

class ShapeLibraryView;

/**
 * \class ShapeLibraryController
 * \brief The Controller in Shape Library Panel MVC - handles shape placement.
 *
 * ShapeLibraryController is responsible for:
 * - Creating shapes from library definitions
 * - Adding shapes to the document
 * - Managing shape placement workflow
 */
class ShapeLibraryController {
public:
  /**
   * \brief Constructor.
   * \param document The shared document model
   * \param library The shape library
   * \param view The shape library view
   */
  ShapeLibraryController(WhiteboardDocument *document, SVGShapeLibrary *library,
                        ShapeLibraryView *view);

  /**
   * \brief Place a shape from the library.
   * \param shape_id Shape identifier
   *
   * Creates a stroke from the shape definition and adds it to the document
   * at a default position (center of canvas).
   */
  void place_shape(const std::string& shape_id);

  /**
   * \brief Select a category to filter shapes.
   * \param category Category name (empty for all)
   */
  void select_category(const std::string& category);

private:
  WhiteboardDocument *m_document;
  SVGShapeLibrary *m_library;
  ShapeLibraryView *m_view;

  /**
   * \brief Calculate default placement position.
   * \return Position in canvas coordinates
   */
  Point get_default_position();
};

} // namespace whiteboard
