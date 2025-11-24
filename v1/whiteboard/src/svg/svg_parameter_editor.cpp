/**
 * \file svg_parameter_editor.cpp
 * \brief Implementation of SVG parameter editor dialog.
 */

#include "whiteboard/svg/svg_parameter_editor.h"
#include <fmtlog.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <iostream>

namespace whiteboard {

SVGParameterEditor::SVGParameterEditor(nanogui::Widget *parent, WhiteboardDocument *document,
                                       SVGShapeLibrary *library, int stroke_index)
    : nanogui::Window(parent, "Edit Shape Parameters"),
      m_document(document), m_library(library), m_stroke_index(stroke_index) {

  set_layout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill, 10, 10));
  set_modal(true);
  set_fixed_width(400);

  build_parameter_fields();

  // Button panel
  auto *button_panel = new nanogui::Widget(this);
  button_panel->set_layout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal,
                                                   nanogui::Alignment::Middle, 0, 10));

  auto *ok_btn = new nanogui::Button(button_panel, "OK");
  ok_btn->set_callback([this]() { on_ok_clicked(); });

  auto *cancel_btn = new nanogui::Button(button_panel, "Cancel");
  cancel_btn->set_callback([this]() { on_cancel_clicked(); });

  center();
}

void SVGParameterEditor::build_parameter_fields() {
  if (!m_document || !m_library) {
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (m_stroke_index < 0 || m_stroke_index >= static_cast<int>(strokes.size())) {
    return;
  }

  const Stroke &stroke = strokes[m_stroke_index];
  if (stroke.svg_shape_id.empty()) {
    return;
  }

  // Get shape definition
  const ShapeDefinition *shape = m_library->get_shape(stroke.svg_shape_id);
  if (!shape) {
    return;
  }

  // Create input fields for each parameter
  for (const auto &param : shape->parameters) {
    // Label
    new nanogui::Label(this, param.label + ":", "sans-bold");

    // Text box
    auto *textbox = new nanogui::TextBox(this);
    textbox->set_editable(true);
    textbox->set_alignment(nanogui::TextBox::Alignment::Left);

    // Set current value
    auto it = stroke.svg_parameters.find(param.name);
    if (it != stroke.svg_parameters.end()) {
      textbox->set_value(it->second);
    } else {
      textbox->set_value(param.default_value);
    }

    m_text_boxes[param.name] = textbox;
  }
}

void SVGParameterEditor::show() {
  set_visible(true);
  request_focus();
}

void SVGParameterEditor::hide() {
  set_visible(false);
}

void SVGParameterEditor::on_ok_clicked() {
  commit_changes();
  hide();
  dispose();
}

void SVGParameterEditor::on_cancel_clicked() {
  hide();
  dispose();
}

void SVGParameterEditor::commit_changes() {
  if (!m_document || !m_library) {
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (m_stroke_index < 0 || m_stroke_index >= static_cast<int>(strokes.size())) {
    return;
  }

  // Get current stroke
  Stroke stroke = strokes[m_stroke_index];

  // Update parameters from text boxes
  for (const auto &pair : m_text_boxes) {
    stroke.svg_parameters[pair.first] = pair.second->value();
  }

  // Log parameters before regeneration
  logi("SVGParameterEditor: Updating stroke {} with {} parameters", m_stroke_index, stroke.svg_parameters.size());
  for (const auto &pair : stroke.svg_parameters) {
    logi("  Parameter '{}' = '{}'", pair.first, pair.second);
  }

  // Regenerate SVG with new parameters
  stroke.svg_data = m_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);

  if (stroke.svg_data.empty()) {
    loge("Failed to regenerate SVG for shape: {}", stroke.svg_shape_id);
    return;
  }

  logi("Successfully regenerated SVG ({} bytes)", stroke.svg_data.size());

  // Update stroke in document
  m_document->update_stroke(m_stroke_index, stroke);

  logi("Updated SVG shape parameters for stroke {}", m_stroke_index);
}

} // namespace whiteboard
