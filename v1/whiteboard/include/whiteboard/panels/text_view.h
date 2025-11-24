/**
 * \file text_view.h
 * \brief View component of Text Panel MVC.
 */

#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/whiteboard_document.h"
#include <nanogui/widget.h>

namespace whiteboard {

class TextController;

class TextView : public nanogui::Widget, public IDocumentObserver {
public:
  TextView(nanogui::Widget *parent, WhiteboardDocument *document);
  ~TextView() override;

  void on_tool_changed() override;
  void on_selection_changed() override;
  void on_properties_changed() override;

  void set_controller(TextController *controller) { m_controller = controller; }

private:
  WhiteboardDocument *m_document;
  TextController *m_controller;
};

} // namespace whiteboard
