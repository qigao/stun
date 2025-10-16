/**
 * \file svg_parameter_editor.h
 * \brief Dialog for editing SVG shape parameters.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/svg/svg_shape_library.h"
#include <nanogui/window.h>
#include <nanogui/textbox.h>
#include <nanogui/button.h>
#include <map>
#include <string>

namespace whiteboard {

/**
 * \class SVGParameterEditor
 * \brief Dialog window for editing SVG shape parameters.
 *
 * Displays editable fields for all parameters of an SVG shape.
 * Supports text, list, and number parameter types.
 */
class SVGParameterEditor : public nanogui::Window {
public:
  /**
   * \brief Constructor.
   * \param parent Parent widget
   * \param document The document model
   * \param library The shape library
   * \param stroke_index Index of stroke being edited
   */
  SVGParameterEditor(nanogui::Widget *parent, WhiteboardDocument *document,
                     SVGShapeLibrary *library, int stroke_index);

  /**
   * \brief Show the editor dialog.
   */
  void show();

  /**
   * \brief Hide the editor dialog.
   */
  void hide();

private:
  WhiteboardDocument *m_document;
  SVGShapeLibrary *m_library;
  int m_stroke_index;

  std::map<std::string, nanogui::TextBox*> m_text_boxes;

  /**
   * \brief Build the parameter input fields.
   */
  void build_parameter_fields();

  /**
   * \brief Handle OK button click - commit changes.
   */
  void on_ok_clicked();

  /**
   * \brief Handle Cancel button click - discard changes.
   */
  void on_cancel_clicked();

  /**
   * \brief Update the stroke with new parameter values.
   */
  void commit_changes();
};

} // namespace whiteboard
