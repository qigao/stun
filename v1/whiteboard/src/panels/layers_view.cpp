/**
 * \file layers_view.cpp
 * \brief Implementation of LayersView class.
 */

#include "whiteboard/panels/layers_view.h"
#include "whiteboard/panels/layers_controller.h"
#include <nanogui/label.h>
#include <nanogui/button.h>
#include <nanogui/vscrollpanel.h>
#include <nanogui/layout.h>

namespace whiteboard {

LayersView::LayersView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr) {

  // Register as observer
  if (m_document) {
    m_document->add_observer(this);
  }
  
  // Set fixed size
  set_fixed_size(nanogui::Vector2i(240, 500));
  
  // Set up layout
  set_layout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill, 5, 5));
  
  // Create title
  auto *title = new nanogui::Label(this, "Layers", "sans-bold", 16);
  title->set_color(nanogui::Color(60, 60, 60, 255));
  
  // Create scroll panel for layers
  auto *scroll = new nanogui::VScrollPanel(this);
  scroll->set_fixed_height(400);
  
  // Container for layer items
  m_layer_container = new nanogui::Widget(scroll);
  m_layer_container->set_layout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, 
                                                        nanogui::Alignment::Fill, 2, 2));
  
  // Initial build
  rebuild_layer_list();
}

LayersView::~LayersView() {
  // Unregister from document
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void LayersView::draw(NVGcontext *ctx) {
  // Draw background
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgFillColor(ctx, nvgRGBA(250, 250, 252, 255));
  nvgFill(ctx);
  
  // Draw border
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgStrokeColor(ctx, nvgRGBA(200, 200, 210, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);
  
  // Draw children
  Widget::draw(ctx);
}

void LayersView::on_strokes_changed() {
  rebuild_layer_list();
}

void LayersView::on_selection_changed() {
  rebuild_layer_list();
}

void LayersView::rebuild_layer_list() {
  if (!m_document || !m_layer_container) {
    return;
  }
  
  // Clear existing layer items
  while (m_layer_container->child_count() > 0) {
    m_layer_container->remove_child_at(0);
  }
  
  const auto &strokes = m_document->get_strokes();
  const auto &selected = m_document->get_selected_indices();
  
  // Create layer items in reverse order (top layer first)
  for (int i = static_cast<int>(strokes.size()) - 1; i >= 0; --i) {
    const auto &stroke = strokes[i];
    
    // Create layer item container
    auto *item = new nanogui::Widget(m_layer_container);
    item->set_layout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, 
                                            nanogui::Alignment::Middle, 2, 2));
    
    // Check if selected (visual indication would require custom rendering)
    bool is_selected = std::find(selected.begin(), selected.end(), i) != selected.end();
    
    // Visibility toggle button (using text instead of icon)
    auto *vis_btn = new nanogui::Button(item, stroke.visible ? "V" : "H");
    vis_btn->set_fixed_size(nanogui::Vector2i(24, 24));
    vis_btn->set_font_size(12);
    vis_btn->set_callback([this, i]() {
      if (m_controller) {
        m_controller->toggle_visibility(i);
      }
    });
    
    // Lock toggle button (using text instead of icon)
    auto *lock_btn = new nanogui::Button(item, stroke.locked ? "L" : "U");
    lock_btn->set_fixed_size(nanogui::Vector2i(24, 24));
    lock_btn->set_font_size(12);
    lock_btn->set_callback([this, i]() {
      if (m_controller) {
        m_controller->toggle_lock(i);
      }
    });
    
    // Layer name button
    std::string layer_name = get_layer_display_name(stroke);
    
    auto *name_btn = new nanogui::Button(item, layer_name);
    name_btn->set_font_size(12);
    name_btn->set_callback([this, i]() {
      if (m_controller) {
        m_controller->select_layer(i, false);
      }
    });
  }
}

std::string LayersView::get_layer_display_name(const Stroke &stroke) const {
  // If stroke has a custom name, use it
  if (!stroke.name.empty()) {
    return stroke.name;
  }
  
  // For SVG shapes, use the shape ID or a friendly name
  if (stroke.tool == Tool::SVGShape && !stroke.svg_shape_id.empty()) {
    // Extract friendly name from shape ID (e.g., "uml.class" -> "UML Class")
    std::string shape_id = stroke.svg_shape_id;
    size_t dot_pos = shape_id.find('.');
    if (dot_pos != std::string::npos) {
      std::string category = shape_id.substr(0, dot_pos);
      std::string type = shape_id.substr(dot_pos + 1);
      
      // Capitalize first letter
      if (!category.empty()) {
        category[0] = std::toupper(category[0]);
      }
      if (!type.empty()) {
        type[0] = std::toupper(type[0]);
      }
      
      return category + " " + type;
    }
    return shape_id;
  }
  
  // Default names based on tool type
  switch (stroke.tool) {
    case Tool::Pen: return "Pen Stroke";
    case Tool::Line: return "Line";
    case Tool::Arrow: return "Arrow";
    case Tool::Rectangle: return "Rectangle";
    case Tool::Diamond: return "Diamond";
    case Tool::Circle: return "Circle";
    case Tool::Text: return stroke.text.empty() ? "Text" : stroke.text.substr(0, 20);
    case Tool::Image: return "Image";
    case Tool::Sticky: return "Sticky Note";
    default: return "Shape";
  }
}



} // namespace whiteboard
