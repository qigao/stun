/**
 * \file toolbar_controller.cpp
 * \brief Implementation of ToolbarController class.
 */

#include "whiteboard/toolbar/toolbar_controller.h"
#include "whiteboard/toolbar/toolbar_view.h"

namespace whiteboard {

ToolbarController::ToolbarController(WhiteboardDocument *document, ToolbarView *view)
    : m_document(document), m_view(view) {}

void ToolbarController::select_tool(Tool tool) {
  if (m_document) {
    m_document->set_current_tool(tool);
  }
}

void ToolbarController::change_stroke_color(const nanogui::Color &color) {
  if (m_document) {
    m_document->set_stroke_color(color);
  }
}

void ToolbarController::change_stroke_width(float width) {
  if (m_document) {
    m_document->set_stroke_width(width);
  }
}

void ToolbarController::change_fill_style(FillStyle style) {
  if (m_document) {
    m_document->set_fill_style(style);
  }
}

void ToolbarController::change_fill_color(const nanogui::Color &color) {
  if (m_document) {
    m_document->set_fill_color(color);
  }
}

} // namespace whiteboard
