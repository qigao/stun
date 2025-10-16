#pragma once

#include <nanogui/vector.h>
#include <nanovg.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace whiteboard {

enum class Tool { Select, Pan, Pen, Text, Sticky, Rectangle, Diamond, Circle, Line, Arrow, Image, SVGShape };

enum class FillStyle { None, Solid };

struct Point {
  float x;
  float y;

  Point(float xx = 0.f, float yy = 0.f) : x(xx), y(yy) {}
};

struct Guide {
  enum Type { Horizontal, Vertical };

  Guide(Type t, float pos)
      : type(t), position(pos), color(nanogui::Color(0, 120, 215, 200)), visible(true) {}

  Type type;
  float position;
  nanogui::Color color;
  bool visible;
};

struct Stroke {
  std::vector<Point> points;
  nanogui::Color color;
  float width;
  Tool tool;
  FillStyle fill_style;
  nanogui::Color fill_color;
  bool selected;
  float rotation;
  std::string text;
  std::string font_face;
  float font_size;
  int text_align;
  bool is_editing;
  int nvg_image_handle;
  std::string file_path;
  float image_width;
  float image_height;
  std::string name;
  bool visible;
  bool locked;
  int group_id;

  // SVG-specific fields
  std::string svg_data;           // SVG content as string
  std::string svg_shape_id;       // Shape identifier (e.g., "uml.class", "flowchart.decision")
  std::map<std::string, std::string> svg_parameters;  // Editable parameters (e.g., {"className": "Customer"})
  float svg_scale_x;              // Horizontal scale factor
  float svg_scale_y;              // Vertical scale factor

  Stroke()
      : width(3.0f), tool(Tool::Pen), fill_style(FillStyle::None), selected(false), rotation(0.0f),
        font_face("sans"), font_size(16.0f), text_align(NVG_ALIGN_LEFT | NVG_ALIGN_TOP),
        is_editing(false), nvg_image_handle(-1), image_width(0.0f), image_height(0.0f), name(),
        visible(true), locked(false), group_id(-1), svg_scale_x(1.0f), svg_scale_y(1.0f) {}

  void get_bounds(float &min_x, float &min_y, float &max_x, float &max_y) const {
    if (points.empty())
      return;

    // Special handling for SVG shapes
    if (tool == Tool::SVGShape && !svg_data.empty()) {
      // For SVG shapes, we need to calculate bounds based on SVG dimensions and scale
      // Default SVG size (will be overridden by actual SVG bounds in full implementation)
      float svg_width = 100.0f;
      float svg_height = 100.0f;
      
      // Apply scale factors
      float scaled_width = svg_width * svg_scale_x;
      float scaled_height = svg_height * svg_scale_y;
      
      // Position is at first point
      float pos_x = points[0].x;
      float pos_y = points[0].y;
      
      // Calculate bounds (simplified - doesn't account for rotation)
      // TODO: Query ThorVG for actual picture bounds
      // TODO: Apply rotation transform to bounds
      min_x = pos_x;
      min_y = pos_y;
      max_x = pos_x + scaled_width;
      max_y = pos_y + scaled_height;
      return;
    }

    // Special handling for text - estimate bounds based on text length
    if (tool == Tool::Text && !text.empty()) {
      // Approximate text bounds (will be refined during rendering)
      float char_width = font_size * 0.6f; // Approximate character width
      float text_width = text.length() * char_width;
      float text_height = font_size * 1.2f;

      min_x = points[0].x;
      min_y = points[0].y - text_height * 0.8f; // Text baseline offset
      max_x = points[0].x + text_width;
      max_y = points[0].y + text_height * 0.2f;
      return;
    }

    // Special handling for circles - calculate actual bounding box from center and radius
    if (tool == Tool::Circle && points.size() >= 2) {
      float cx = points[0].x; // Center X
      float cy = points[0].y; // Center Y
      float dx = points[1].x - cx;
      float dy = points[1].y - cy;
      float radius = std::sqrt(dx * dx + dy * dy);

      min_x = cx - radius;
      max_x = cx + radius;
      min_y = cy - radius;
      max_y = cy + radius;
      return;
    }

    // For rectangles, lines, arrows, and other shapes - use point bounds
    min_x = max_x = points[0].x;
    min_y = max_y = points[0].y;
    for (const auto &p : points) {
      min_x = std::min(min_x, p.x);
      max_x = std::max(max_x, p.x);
      min_y = std::min(min_y, p.y);
      max_y = std::max(max_y, p.y);
    }
  }

  bool contains_point(float x, float y, float margin = 10.0f) const {
    // Special handling for SVG shapes
    if (tool == Tool::SVGShape && !svg_data.empty() && !points.empty()) {
      // Use bounding box for SVG shapes
      // In a full implementation, we would query ThorVG for precise hit testing
      float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
      get_bounds(min_x, min_y, max_x, max_y);
      
      // Apply rotation if needed (simplified - assumes axis-aligned for now)
      // TODO: Implement proper rotated rectangle hit testing
      return x >= min_x - margin && x <= max_x + margin && 
             y >= min_y - margin && y <= max_y + margin;
    }
    
    // Default bounding box hit testing for other shapes
    float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
    get_bounds(min_x, min_y, max_x, max_y);
    return x >= min_x - margin && x <= max_x + margin && y >= min_y - margin && y <= max_y + margin;
  }

  void move(float dx, float dy) {
    for (auto &p : points) {
      p.x += dx;
      p.y += dy;
    }
  }
};

} // namespace whiteboard
