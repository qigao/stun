/**
 * \file serialization.cpp
 * \brief Implementation of serialization utilities
 */

#include "whiteboard/serialization.h"
#include <nanovg.h>

namespace whiteboard {

json stroke_to_json(const Stroke& stroke) {
  json j;
  
  // Basic properties
  j["tool"] = static_cast<int>(stroke.tool);
  j["color"] = color_to_json(stroke.color);
  j["width"] = stroke.width;
  j["fill_style"] = static_cast<int>(stroke.fill_style);
  j["fill_color"] = color_to_json(stroke.fill_color);
  j["rotation"] = stroke.rotation;
  
  // Geometry - points
  j["points"] = json::array();
  for (const auto& p : stroke.points) {
    j["points"].push_back(point_to_json(p));
  }
  
  // Text properties (if applicable)
  if (!stroke.text.empty()) {
    j["text"] = stroke.text;
    j["font_face"] = stroke.font_face;
    j["font_size"] = stroke.font_size;
    j["text_align"] = stroke.text_align;
  }
  
  // Image properties (if applicable)
  if (!stroke.file_path.empty()) {
    j["file_path"] = stroke.file_path;
    j["image_width"] = stroke.image_width;
    j["image_height"] = stroke.image_height;
  }
  
  // Layer properties
  j["name"] = stroke.name;
  j["visible"] = stroke.visible;
  j["locked"] = stroke.locked;
  j["group_id"] = stroke.group_id;
  
  // SVG properties (if applicable)
  if (!stroke.svg_data.empty()) {
    j["svg_data"] = stroke.svg_data;
    j["svg_shape_id"] = stroke.svg_shape_id;
    j["svg_scale_x"] = stroke.svg_scale_x;
    j["svg_scale_y"] = stroke.svg_scale_y;
    
    // Serialize SVG parameters
    if (!stroke.svg_parameters.empty()) {
      j["svg_parameters"] = stroke.svg_parameters;
    }
  }
  
  return j;
}

Stroke stroke_from_json(const json& j) {
  Stroke stroke;
  
  // Basic properties
  stroke.tool = static_cast<Tool>(j["tool"].get<int>());
  stroke.color = color_from_json(j["color"]);
  stroke.width = j["width"].get<float>();
  stroke.fill_style = static_cast<FillStyle>(j["fill_style"].get<int>());
  stroke.fill_color = color_from_json(j["fill_color"]);
  stroke.rotation = j.value("rotation", 0.0f);
  
  // Geometry - points
  if (j.contains("points")) {
    for (const auto& p : j["points"]) {
      stroke.points.push_back(point_from_json(p));
    }
  }
  
  // Text properties
  stroke.text = j.value("text", "");
  stroke.font_face = j.value("font_face", "sans");
  stroke.font_size = j.value("font_size", 16.0f);
  stroke.text_align = j.value("text_align", NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  
  // Image properties
  stroke.file_path = j.value("file_path", "");
  stroke.image_width = j.value("image_width", 0.0f);
  stroke.image_height = j.value("image_height", 0.0f);
  // Note: nvg_image_handle is not serialized (runtime resource)
  stroke.nvg_image_handle = -1;
  
  // Layer properties
  stroke.name = j.value("name", "");
  stroke.visible = j.value("visible", true);
  stroke.locked = j.value("locked", false);
  stroke.group_id = j.value("group_id", -1);
  
  // SVG properties
  stroke.svg_data = j.value("svg_data", "");
  stroke.svg_shape_id = j.value("svg_shape_id", "");
  stroke.svg_scale_x = j.value("svg_scale_x", 1.0f);
  stroke.svg_scale_y = j.value("svg_scale_y", 1.0f);
  
  if (j.contains("svg_parameters")) {
    stroke.svg_parameters = j["svg_parameters"].get<std::map<std::string, std::string>>();
  }
  
  // Runtime properties (not serialized)
  stroke.selected = false;
  stroke.is_editing = false;
  
  return stroke;
}

} // namespace whiteboard
