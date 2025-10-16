/**
 * \file text_controller.cpp
 * \brief Implementation of TextController class.
 */

#include "whiteboard/panels/text_controller.h"
#include "whiteboard/panels/text_view.h"

namespace whiteboard {

TextController::TextController(WhiteboardDocument *document, TextView *view)
    : m_document(document), m_view(view) {}

void TextController::change_font_face(const std::string &face) {
  if (m_document) {
    m_document->set_font_face(face);
  }
}

void TextController::change_font_size(float size) {
  if (m_document) {
    m_document->set_font_size(size);
  }
}

void TextController::toggle_bold() {
  // TODO: Implement bold toggle
}

void TextController::toggle_italic() {
  // TODO: Implement italic toggle
}

void TextController::set_alignment(int align) {
  if (m_document) {
    m_document->set_text_align(align);
  }
}

} // namespace whiteboard
