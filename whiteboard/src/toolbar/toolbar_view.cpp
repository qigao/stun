/**
 * \file toolbar_view.cpp
 * \brief Implementation of ToolbarView class.
 */

#include "whiteboard/toolbar/toolbar_view.h"
#include "whiteboard/toolbar/toolbar_controller.h"

namespace whiteboard {

ToolbarView::ToolbarView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr) {

  // Register as observer
  if (m_document) {
    m_document->add_observer(this);
  }
}

ToolbarView::~ToolbarView() {
  // Unregister from document
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void ToolbarView::on_tool_changed() {
  // The existing ToolbarPanelModule will handle UI updates
  // This is just for future extensibility
}

void ToolbarView::on_properties_changed() {
  // The existing property panels will handle UI updates
  // This is just for future extensibility
}

} // namespace whiteboard
