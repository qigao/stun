/**
 * \file shape_library_view.cpp
 * \brief Implementation of ShapeLibraryView class.
 */

#include "whiteboard/panels/shape_library_view.h"
#include "whiteboard/panels/shape_library_controller.h"
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <iostream>

namespace whiteboard {

ShapeLibraryView::ShapeLibraryView(nanogui::Widget *parent, WhiteboardDocument *document,
                                   SVGShapeLibrary *library)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr), m_library(library),
      m_current_category("") {

  // Set layout
  set_layout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill, 5, 5));

  // Title
  new nanogui::Label(this, "Shape Library", "sans-bold", 16);

  // Category bar
  m_category_bar = new nanogui::Widget(this);
  m_category_bar->set_layout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, 
                                                     nanogui::Alignment::Fill, 2, 2));

  // Scroll panel for shapes
  m_scroll_panel = new nanogui::VScrollPanel(this);
  m_scroll_panel->set_fixed_height(400);

  // Shape grid container
  m_shape_grid = new nanogui::Widget(m_scroll_panel);
  m_shape_grid->set_layout(new nanogui::GridLayout(nanogui::Orientation::Horizontal, 2,
                                                    nanogui::Alignment::Fill, 5, 5));

  // Build UI
  build_category_bar();
  build_shape_grid();

  // Register as observer
  if (m_document) {
    m_document->add_observer(this);
  }
}

ShapeLibraryView::~ShapeLibraryView() {
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void ShapeLibraryView::on_tool_changed() {
  // Update UI if needed when tool changes
}

void ShapeLibraryView::set_category(const std::string& category) {
  if (m_current_category != category) {
    m_current_category = category;
    rebuild_shape_grid();
  }
}

void ShapeLibraryView::rebuild_shape_grid() {
  build_shape_grid();
  // Layout will be updated on next frame
}

void ShapeLibraryView::build_category_bar() {
  // Clear existing buttons
  while (m_category_bar->child_count() > 0) {
    m_category_bar->remove_child_at(0);
  }
  m_category_buttons.clear();

  if (!m_library || !m_library->is_loaded()) {
    return;
  }

  // Add "All" button
  auto *all_btn = new nanogui::Button(m_category_bar, "All");
  all_btn->set_fixed_width(60);
  all_btn->set_callback([this]() { on_category_clicked(""); });
  m_category_buttons.push_back(all_btn);

  // Add category buttons
  auto categories = m_library->get_categories();
  for (const auto& category : categories) {
    auto *btn = new nanogui::Button(m_category_bar, category);
    btn->set_fixed_width(80);
    btn->set_callback([this, category]() { on_category_clicked(category); });
    m_category_buttons.push_back(btn);
  }
}

void ShapeLibraryView::build_shape_grid() {
  // Clear existing buttons
  while (m_shape_grid->child_count() > 0) {
    m_shape_grid->remove_child_at(0);
  }
  m_shape_buttons.clear();

  if (!m_library || !m_library->is_loaded()) {
    auto *label = new nanogui::Label(m_shape_grid, "No shapes loaded", "sans", 14);
    return;
  }

  // Get shapes for current category
  auto shapes = m_library->get_shapes(m_current_category);

  if (shapes.empty()) {
    auto *label = new nanogui::Label(m_shape_grid, "No shapes in category", "sans", 14);
    return;
  }

  // Create shape buttons
  for (const auto& shape : shapes) {
    auto *btn = new nanogui::Button(m_shape_grid, shape.name);
    btn->set_fixed_size(nanogui::Vector2i(100, 80));
    btn->set_tooltip(shape.description);
    btn->set_callback([this, shape]() { on_shape_clicked(shape.id); });
    m_shape_buttons.push_back(btn);
  }
}

void ShapeLibraryView::on_category_clicked(const std::string& category) {
  set_category(category);
  
  // Update button states
  for (size_t i = 0; i < m_category_buttons.size(); ++i) {
    bool is_active = (i == 0 && category.empty()) || 
                     (i > 0 && m_library->get_categories()[i-1] == category);
    m_category_buttons[i]->set_pushed(is_active);
  }
}

void ShapeLibraryView::on_shape_clicked(const std::string& shape_id) {
  if (m_controller) {
    m_controller->place_shape(shape_id);
  }
}

} // namespace whiteboard
