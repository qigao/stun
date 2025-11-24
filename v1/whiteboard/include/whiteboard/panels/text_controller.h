/**
 * \file text_controller.h
 * \brief Controller component of Text Panel MVC.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include <string>

namespace whiteboard {

class TextView;

class TextController {
public:
  TextController(WhiteboardDocument *document, TextView *view);

  void change_font_face(const std::string &face);
  void change_font_size(float size);
  void toggle_bold();
  void toggle_italic();
  void set_alignment(int align);

private:
  WhiteboardDocument *m_document;
  TextView *m_view;
};

} // namespace whiteboard
