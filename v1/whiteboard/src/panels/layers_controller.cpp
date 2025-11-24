/**
 * \file layers_controller.cpp
 * \brief Implementation of LayersController class.
 */

#include "whiteboard/panels/layers_controller.h"
#include "whiteboard/panels/layers_view.h"

namespace whiteboard {

LayersController::LayersController(WhiteboardDocument *document, LayersView *view)
    : m_document(document), m_view(view) {}

void LayersController::select_layer(int index, bool add_to_selection) {
  if (!m_document) {
    return;
  }

  if (add_to_selection) {
    // Add to existing selection
    auto selected = m_document->get_selected_indices();
    auto it = std::find(selected.begin(), selected.end(), index);
    if (it != selected.end()) {
      // Already selected, remove it
      selected.erase(it);
    } else {
      // Not selected, add it
      selected.push_back(index);
    }
    m_document->set_selection(selected);
  } else {
    // Single select
    m_document->set_selection({index});
  }
}

void LayersController::toggle_visibility(int index) {
  if (!m_document) {
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (index >= 0 && index < static_cast<int>(strokes.size())) {
    bool current_visibility = strokes[index].visible;
    m_document->set_stroke_visible(index, !current_visibility);
  }
}

void LayersController::toggle_lock(int index) {
  if (!m_document) {
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (index >= 0 && index < static_cast<int>(strokes.size())) {
    bool current_lock = strokes[index].locked;
    m_document->set_stroke_locked(index, !current_lock);
  }
}

void LayersController::rename_layer(int index, const std::string &name) {
  if (!m_document) {
    return;
  }

  m_document->set_stroke_name(index, name);
}

void LayersController::reorder_layer(int from_index, int to_index) {
  if (!m_document) {
    return;
  }

  m_document->reorder_stroke(from_index, to_index);
}

void LayersController::delete_selected_layers() {
  if (!m_document) {
    return;
  }

  const auto &selected = m_document->get_selected_indices();
  if (!selected.empty()) {
    m_document->remove_strokes(selected);
  }
}

} // namespace whiteboard
