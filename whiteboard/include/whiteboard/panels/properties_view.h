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
  
  void rebuild_properties();
  
  // Override draw for custom styling
  void draw(NVGcontext *ctx) override;
  
  // Mouse event for tab clicking
  bool mouse_button_event(const nanogui::Vector2i &p, int button, bool down, int modifiers) override;

private:
  enum class Tab { Properties, SVGParameters };
  
  void add_property_label(const std::string &name, const std::string &value);
  void add_separator();
  std::string get_tool_name(Tool tool) const;
  void build_properties_tab(const Stroke &stroke, int index);
  void build_svg_parameters_tab(const Stroke &stroke, int index);
  void draw_tabs(NVGcontext *ctx);

  WhiteboardDocument *m_document;
  PropertiesController *m_controller;
  nanogui::Widget *m_properties_container;
  Tab m_current_tab;
  bool m_is_svg_shape; // Track if current selection is SVG shape
};

} // namespace whiteboard
