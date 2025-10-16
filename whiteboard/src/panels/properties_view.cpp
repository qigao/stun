/**
 * \file properties_view.cpp
 * \brief Implementation of PropertiesView class.
 */

#include "whiteboard/panels/properties_view.h"
#include "whiteboard/panels/properties_controller.h"

namespace whiteboard {

PropertiesView::PropertiesView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr) {
  if (m_document) {
    m_document->add_observer(this);
  }
}

PropertiesView::~PropertiesView() {
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void PropertiesView::on_selection_changed() {
  // Legacy panel handles UI updates
}

void PropertiesView::on_strokes_changed() {
  // Legacy panel handles UI updates
}

void PropertiesView::on_properties_changed() {
  // Legacy panel handles UI updates
}

} // namespace whiteboard
