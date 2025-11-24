/**
 * \file text_view.cpp
 * \brief Implementation of TextView class.
 */

#include "whiteboard/panels/text_view.h"
#include "whiteboard/panels/text_controller.h"

namespace whiteboard {

TextView::TextView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr) {
  if (m_document) {
    m_document->add_observer(this);
  }
}

TextView::~TextView() {
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void TextView::on_tool_changed() {
  // Show/hide based on tool
  if (m_document) {
    bool is_text_tool = m_document->get_current_tool() == Tool::Text;
    set_visible(is_text_tool);
  }
}

void TextView::on_selection_changed() {
  // Legacy panel handles UI updates
}

void TextView::on_properties_changed() {
  // Legacy panel handles UI updates
}

} // namespace whiteboard
