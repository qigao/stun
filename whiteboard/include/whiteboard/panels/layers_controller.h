/**
 * \file layers_controller.h
 * \brief Controller component of Layers Panel MVC - handles layer operations.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include <string>
#include <vector>

namespace whiteboard {

// Forward declaration
class LayersView;

/**
 * \class LayersController
 * \brief The Controller in Layers Panel MVC - processes layer operations.
 *
 * LayersController is responsible for:
 * - Processing layer selection
 * - Processing visibility/lock toggles
 * - Processing layer reordering
 * - Processing layer deletion
 */
class LayersController {
public:
  /**
   * \brief Constructor.
   * \param document The shared document model
   * \param view The layers view
   */
  LayersController(WhiteboardDocument *document, LayersView *view);

  // === User Actions ===

  /**
   * \brief Select a layer.
   * \param index Layer index
   * \param add_to_selection Whether to add to existing selection (shift-click)
   */
  void select_layer(int index, bool add_to_selection);

  /**
   * \brief Toggle layer visibility.
   * \param index Layer index
   */
  void toggle_visibility(int index);

  /**
   * \brief Toggle layer lock.
   * \param index Layer index
   */
  void toggle_lock(int index);

  /**
   * \brief Rename a layer.
   * \param index Layer index
   * \param name New name
   */
  void rename_layer(int index, const std::string &name);

  /**
   * \brief Reorder a layer.
   * \param from_index Current index
   * \param to_index Target index
   */
  void reorder_layer(int from_index, int to_index);

  /**
   * \brief Delete selected layers.
   */
  void delete_selected_layers();

private:
  WhiteboardDocument *m_document;
  LayersView *m_view;
};

} // namespace whiteboard
