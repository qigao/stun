/**
 * \file properties_view.h
 * \brief View component of Properties Panel MVC.
 */

#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/whiteboard_document.h"
#include <nanogui/widget.h>

namespace whiteboard {

class PropertiesController;

class PropertiesView : public nanogui::Widget, public IDocumentObserver {
public:
  PropertiesView(nanogui::Widget *parent, WhiteboardDocument *document);
  ~PropertiesView() override;

  void on_selection_changed() override;
  void on_strokes_changed() override;
  void on_properties_changed() override;

  void set_controller(PropertiesController *controller) { m_controller = controller; }

private:
  WhiteboardDocument *m_document;
  PropertiesController *m_controller;
};

} // namespace whiteboard
