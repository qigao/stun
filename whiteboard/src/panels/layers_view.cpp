/**
 * \file layers_view.cpp
 * \brief Implementation of LayersView class.
 */

#include "whiteboard/panels/layers_view.h"
#include "whiteboard/panels/layers_controller.h"

namespace whiteboard {

LayersView::LayersView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr) {

  // Register as observer
  if (m_document) {
    m_document->add_observer(this);
  }
}

LayersView::~LayersView() {
  // Unregister from document
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void LayersView::on_strokes_changed() {
  // The existing LayersPanel will handle UI updates
  // This is just for future extensibility
}

void LayersView::on_selection_changed() {
  // The existing LayersPanel will handle UI updates
  // This is just for future extensibility
}

} // namespace whiteboard
