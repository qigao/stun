/**
 * \file properties_view.cpp
 * \brief Implementation of PropertiesView class.
 */

#include "whiteboard/panels/properties_view.h"
#include "whiteboard/panels/properties_controller.h"
#include <nanogui/label.h>
#include <nanogui/button.h>
#include <nanogui/textbox.h>
#include <nanogui/layout.h>
#include <nanogui/vscrollpanel.h>

namespace whiteboard {

PropertiesView::PropertiesView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr),
      m_properties_container(nullptr) {
  if (m_document) {
    m_document->add_observer(this);
  }
  
  // Set up layout
  set_layout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill, 5, 5));
  
  // Create title
  auto *title = new nanogui::Label(this, "Properties", "sans-bold", 16);
  title->set_color(nanogui::Color(60, 60, 60, 255));
  
  // Create scroll panel for properties
  auto *scroll = new nanogui::VScrollPanel(this);
  scroll->set_fixed_height(300);
  
  // Container for property items
  m_properties_container = new nanogui::Widget(scroll);
  m_properties_container->set_layout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, 
                                                             nanogui::Alignment::Fill, 5, 5));
  
  // Initial build
  rebuild_properties();
}

PropertiesView::~PropertiesView() {
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void PropertiesView::on_selection_changed() {
  rebuild_properties();
}

void PropertiesView::on_strokes_changed() {
  rebuild_properties();
}

void PropertiesView::on_properties_changed() {
  rebuild_properties();
}

void PropertiesView::rebuild_properties() {
  if (!m_document || !m_properties_container) {
    return;
  }
  
  // Clear existing properties
  while (m_properties_container->child_count() > 0) {
    m_properties_container->remove_child_at(0);
  }
  
  const auto &selected = m_document->get_selected_indices();
  
  // Show properties only if exactly one item is selected
  if (selected.size() != 1) {
    auto *label = new nanogui::Label(m_properties_container, 
                                     selected.empty() ? "No selection" : "Multiple items selected",
                                     "sans", 12);
    label->set_color(nanogui::Color(120, 120, 120, 255));
    return;
  }
  
  const auto &strokes = m_document->get_strokes();
  int index = selected[0];
  
  if (index < 0 || index >= static_cast<int>(strokes.size())) {
    return;
  }
  
  const auto &stroke = strokes[index];
  
  // Show common properties
  add_property_label("Type", get_tool_name(stroke.tool));
  
  // Position
  if (!stroke.points.empty()) {
    add_property_label("Position", 
                      "X: " + std::to_string(static_cast<int>(stroke.points[0].x)) + 
                      ", Y: " + std::to_string(static_cast<int>(stroke.points[0].y)));
  }
  
  // SVG-specific properties
  if (stroke.tool == Tool::SVGShape) {
    add_separator();
    
    auto *svg_label = new nanogui::Label(m_properties_container, "SVG Shape", "sans-bold", 14);
    svg_label->set_color(nanogui::Color(60, 60, 60, 255));
    
    if (!stroke.svg_shape_id.empty()) {
      add_property_label("Shape ID", stroke.svg_shape_id);
    }
    
    // Scale
    add_property_label("Scale X", std::to_string(stroke.svg_scale_x));
    add_property_label("Scale Y", std::to_string(stroke.svg_scale_y));
    
    // Rotation
    add_property_label("Rotation", std::to_string(static_cast<int>(stroke.rotation)) + "°");
    
    // Edit Parameters button (if shape has parameters)
    if (!stroke.svg_parameters.empty()) {
      add_separator();
      
      auto *edit_btn = new nanogui::Button(m_properties_container, "Edit Parameters");
      edit_btn->set_callback([this, index]() {
        if (m_controller) {
          m_controller->edit_svg_parameters(index);
        }
      });
    }
  }
}

void PropertiesView::add_property_label(const std::string &name, const std::string &value) {
  auto *container = new nanogui::Widget(m_properties_container);
  container->set_layout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, 
                                               nanogui::Alignment::Middle, 5, 0));
  
  auto *name_label = new nanogui::Label(container, name + ":", "sans", 11);
  name_label->set_color(nanogui::Color(80, 80, 80, 255));
  name_label->set_fixed_width(80);
  
  auto *value_label = new nanogui::Label(container, value, "sans", 11);
  value_label->set_color(nanogui::Color(40, 40, 40, 255));
}

void PropertiesView::add_separator() {
  auto *sep = new nanogui::Widget(m_properties_container);
  sep->set_fixed_height(10);
  // Note: NanoGUI Widget doesn't have set_background_color, separator is just spacing
}

std::string PropertiesView::get_tool_name(Tool tool) const {
  switch (tool) {
    case Tool::Pen: return "Pen";
    case Tool::Line: return "Line";
    case Tool::Arrow: return "Arrow";
    case Tool::Rectangle: return "Rectangle";
    case Tool::Diamond: return "Diamond";
    case Tool::Circle: return "Circle";
    case Tool::Text: return "Text";
    case Tool::Image: return "Image";
    case Tool::Sticky: return "Sticky Note";
    case Tool::SVGShape: return "SVG Shape";
    case Tool::Select: return "Selection";
    case Tool::Pan: return "Pan";
    default: return "Unknown";
  }
}

} // namespace whiteboard
