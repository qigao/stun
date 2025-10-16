/**
 * \file toolbar_controller.h
 * \brief Controller component of Toolbar MVC - handles toolbar actions.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/types.h"
#include <nanogui/common.h>

namespace whiteboard {

// Forward declaration
class ToolbarView;

/**
 * \class ToolbarController
 * \brief The Controller in Toolbar MVC - processes toolbar actions.
 *
 * ToolbarController is responsible for:
 * - Processing tool selection
 * - Processing property changes
 * - Updating the model
 */
class ToolbarController {
public:
  /**
   * \brief Constructor.
   * \param document The shared document model
   * \param view The toolbar view
   */
  ToolbarController(WhiteboardDocument *document, ToolbarView *view);

  // === User Actions ===

  /**
   * \brief Select a tool.
   * \param tool Tool to select
   */
  void select_tool(Tool tool);

  /**
   * \brief Change stroke color.
   * \param color New color
   */
  void change_stroke_color(const nanogui::Color &color);

  /**
   * \brief Change stroke width.
   * \param width New width
   */
  void change_stroke_width(float width);

  /**
   * \brief Change fill style.
   * \param style New fill style
   */
  void change_fill_style(FillStyle style);

  /**
   * \brief Change fill color.
   * \param color New fill color
   */
  void change_fill_color(const nanogui::Color &color);

private:
  WhiteboardDocument *m_document;
  ToolbarView *m_view;
};

} // namespace whiteboard
