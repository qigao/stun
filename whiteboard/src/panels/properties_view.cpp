/**
 * \file properties_view.cpp
 * \brief Implementation of PropertiesView class.
 */

#include "whiteboard/panels/properties_view.h"
#include "whiteboard/panels/properties_controller.h"
#include <fmtlog.h>
#include <nanogui/button.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/textbox.h>
#include <nanogui/vscrollpanel.h>

namespace whiteboard {

PropertiesView::PropertiesView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Widget(parent), m_document(document), m_controller(nullptr),
      m_properties_container(nullptr), m_current_tab(Tab::Properties), m_is_svg_shape(false) {
  if (m_document) {
    m_document->add_observer(this);
  }

  // Set up layout with padding
  set_layout(
      new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill, 10, 10));

  // Create title bar with custom styling
  auto *title_bar = new nanogui::Widget(this);
  title_bar->set_layout(
      new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 0, 10));

  auto *title = new nanogui::Label(title_bar, "Properties", "sans-bold", 16);
  title->set_color(nanogui::Color(255, 255, 255, 255));

  // Add spacer for tabs (tabs will be drawn in draw() method)
  auto *tab_spacer = new nanogui::Widget(this);
  tab_spacer->set_fixed_height(40);

  // Create scroll panel for properties
  auto *scroll = new nanogui::VScrollPanel(this);
  scroll->set_fixed_height(400);

  // Container for property items
  m_properties_container = new nanogui::Widget(scroll);
  m_properties_container->set_layout(
      new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill, 8, 8));

  // Initial build
  rebuild_properties();
}

PropertiesView::~PropertiesView() {
  if (m_document) {
    m_document->remove_observer(this);
  }
}

void PropertiesView::draw(NVGcontext *ctx) {
  // Draw custom styled panel background
  float px = static_cast<float>(m_pos.x());
  float py = static_cast<float>(m_pos.y());
  float pw = static_cast<float>(m_size.x());
  float ph = static_cast<float>(m_size.y());

  // Draw rounded panel background with shadow
  nvgSave(ctx);

  // Shadow
  NVGpaint shadow_paint =
      nvgBoxGradient(ctx, px, py + 2, pw, ph, 8, 10, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, px - 10, py - 10, pw + 20, ph + 30);
  nvgRoundedRect(ctx, px, py, pw, ph, 8);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow_paint);
  nvgFill(ctx);

  // Panel background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 8);
  nvgFillColor(ctx, nvgRGBA(248, 248, 252, 255));
  nvgFill(ctx);

  // Title bar background (blue gradient like in the image)
  nvgBeginPath(ctx);
  nvgRoundedRectVarying(ctx, px, py, pw, 40, 8, 8, 0, 0);
  NVGpaint title_paint = nvgLinearGradient(ctx, px, py, px, py + 40, nvgRGBA(41, 128, 185, 255),
                                           nvgRGBA(52, 152, 219, 255));
  nvgFillPaint(ctx, title_paint);
  nvgFill(ctx);

  // Border
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 8);
  nvgStrokeColor(ctx, nvgRGBA(200, 200, 210, 255));
  nvgStrokeWidth(ctx, 1.0f);
  nvgStroke(ctx);

  nvgRestore(ctx);

  // Draw tabs (if SVG shape is selected)
  draw_tabs(ctx);

  // Draw children
  Widget::draw(ctx);
}

void PropertiesView::on_selection_changed() { rebuild_properties(); }

void PropertiesView::on_strokes_changed() { rebuild_properties(); }

void PropertiesView::on_properties_changed() { rebuild_properties(); }

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
    m_is_svg_shape = false;
    auto *label = new nanogui::Label(m_properties_container,
                                     selected.empty() ? "No selection" : "Multiple items selected",
                                     "sans", 12);
    label->set_color(nanogui::Color(120, 120, 120, 255));
    return;
  }

  const auto &strokes = m_document->get_strokes();
  int index = selected[0];

  if (index < 0 || index >= static_cast<int>(strokes.size())) {
    m_is_svg_shape = false;
    return;
  }

  const auto &stroke = strokes[index];

  // Check if this is an SVG shape
  m_is_svg_shape = (stroke.tool == Tool::SVGShape);

  // Build appropriate tab content
  if (m_current_tab == Tab::Properties) {
    build_properties_tab(stroke, index);
  } else if (m_current_tab == Tab::SVGParameters && m_is_svg_shape) {
    build_svg_parameters_tab(stroke, index);
  } else {
    // Fallback to properties tab if SVG tab selected but not SVG shape
    m_current_tab = Tab::Properties;
    build_properties_tab(stroke, index);
  }
}

void PropertiesView::add_property_label(const std::string &name, const std::string &value) {
  auto *container = new nanogui::Widget(m_properties_container);
  container->set_layout(
      new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 5, 0));

  auto *name_label = new nanogui::Label(container, name + ":", "sans", 11);
  name_label->set_color(nanogui::Color(80, 80, 80, 255));
  name_label->set_fixed_width(80);

  auto *value_label = new nanogui::Label(container, value, "sans", 11);
  value_label->set_color(nanogui::Color(40, 40, 40, 255));
}

void PropertiesView::add_separator() {
  auto *sep = new nanogui::Widget(m_properties_container);
  sep->set_fixed_height(10);
}

std::string PropertiesView::get_tool_name(Tool tool) const {
  switch (tool) {
  case Tool::Pen:
    return "Pen";
  case Tool::Line:
    return "Line";
  case Tool::Arrow:
    return "Arrow";
  case Tool::Rectangle:
    return "Rectangle";
  case Tool::Diamond:
    return "Diamond";
  case Tool::Circle:
    return "Circle";
  case Tool::Text:
    return "Text";
  case Tool::Image:
    return "Image";
  case Tool::Sticky:
    return "Sticky Note";
  case Tool::SVGShape:
    return "SVG Shape";
  case Tool::Select:
    return "Selection";
  case Tool::Pan:
    return "Pan";
  default:
    return "Unknown";
  }
}

void PropertiesView::build_properties_tab(const Stroke &stroke, int index) {
  // Show common properties
  add_property_label("Type", get_tool_name(stroke.tool));

  // Position
  if (!stroke.points.empty()) {
    add_property_label("Position",
                       "X: " + std::to_string(static_cast<int>(stroke.points[0].x)) +
                           ", Y: " + std::to_string(static_cast<int>(stroke.points[0].y)));
  }

  add_separator();

  // TODO: Add opacity, stroke style, corner radius controls here
  // For now, just show basic properties
}

void PropertiesView::build_svg_parameters_tab(const Stroke &stroke, int index) {
  // SVG-specific properties tab
  auto *title = new nanogui::Label(m_properties_container, "SVG Shape Parameters", "sans-bold", 13);
  title->set_color(nanogui::Color(60, 60, 80, 255));

  add_separator();

  // Shape ID
  add_property_label("Shape ID", stroke.svg_shape_id);

  add_separator();

  // Display all SVG parameters
  if (!stroke.svg_parameters.empty()) {
    for (const auto &param : stroke.svg_parameters) {
      add_property_label(param.first, param.second);
    }

    add_separator();
  }

  // Scale controls
  add_property_label("Scale X", std::to_string(stroke.svg_scale_x));
  add_property_label("Scale Y", std::to_string(stroke.svg_scale_y));

  // Rotation
  add_property_label("Rotation", std::to_string(static_cast<int>(stroke.rotation)) + "°");

  add_separator();

  // Edit Parameters button
  auto *edit_btn = new nanogui::Button(m_properties_container, "Edit Parameters");
  edit_btn->set_callback([this, index]() {
    // TODO: This PropertiesView is not currently used - PropertiesPanelModule is used instead
    // If this view is re-enabled, implement edit_svg_parameters in PropertiesController
    (void)index; // Suppress unused warning
  });
}

void PropertiesView::draw_tabs(NVGcontext *ctx) {
  if (!m_is_svg_shape) {
    return; // No tabs for non-SVG shapes
  }

  float px = static_cast<float>(m_pos.x());
  float py = static_cast<float>(m_pos.y()) + 50; // Below title bar
  float tab_width = 140.0f;
  float tab_height = 35.0f;

  // Draw Properties tab
  nvgBeginPath(ctx);
  nvgRoundedRectVarying(ctx, px + 10, py, tab_width, tab_height, 6, 6, 0, 0);
  if (m_current_tab == Tab::Properties) {
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(230, 230, 240, 255));
  }
  nvgFill(ctx);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 12.0f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, px + 10 + tab_width / 2, py + tab_height / 2, "Properties", nullptr);

  // Draw SVG Parameters tab
  nvgBeginPath(ctx);
  nvgRoundedRectVarying(ctx, px + 10 + tab_width + 5, py, tab_width, tab_height, 6, 6, 0, 0);
  if (m_current_tab == Tab::SVGParameters) {
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(230, 230, 240, 255));
  }
  nvgFill(ctx);

  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgText(ctx, px + 10 + tab_width + 5 + tab_width / 2, py + tab_height / 2, "SVG Parameters",
          nullptr);
}

bool PropertiesView::mouse_button_event(const nanogui::Vector2i &p, int button, bool down,
                                        int modifiers) {
  if (!m_is_svg_shape || button != 0 || !down) {
    return Widget::mouse_button_event(p, button, down, modifiers);
  }

  // Check if click is in tab area
  float tab_width = 140.0f;
  float tab_height = 35.0f;

  float local_x = static_cast<float>(p.x() - m_pos.x());
  float local_y = static_cast<float>(p.y() - m_pos.y());

  // Check Properties tab
  if (local_x >= 10 && local_x <= 10 + tab_width && local_y >= 50 && local_y <= 50 + tab_height) {
    if (m_current_tab != Tab::Properties) {
      m_current_tab = Tab::Properties;
      rebuild_properties();
      logi("Switched to Properties tab");
    }
    return true;
  }

  // Check SVG Parameters tab
  if (local_x >= 10 + tab_width + 5 && local_x <= 10 + tab_width + 5 + tab_width && local_y >= 50 &&
      local_y <= 50 + tab_height) {
    if (m_current_tab != Tab::SVGParameters) {
      m_current_tab = Tab::SVGParameters;
      rebuild_properties();
      logi("Switched to SVG Parameters tab");
    }
    return true;
  }

  return Widget::mouse_button_event(p, button, down, modifiers);
}

} // namespace whiteboard
