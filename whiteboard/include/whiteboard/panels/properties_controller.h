/**
 * \file properties_controller.h
 * \brief Controller component of Properties Panel MVC.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include <nanogui/common.h>

namespace whiteboard {

class PropertiesView;

class PropertiesController {
public:
  PropertiesController(WhiteboardDocument *document, PropertiesView *view);

  void change_stroke_color(const nanogui::Color &color);
  void change_stroke_width(float width);
  void change_fill_color(const nanogui::Color &color);
  void change_fill_style(FillStyle style);
  void change_position(float x, float y);
  void change_size(float width, float height);
  void change_rotation(float angle);

private:
  WhiteboardDocument *m_document;
  PropertiesView *m_view;

  void apply_to_selected(std::function<void(Stroke &)> modifier);
};

} // namespace whiteboard
