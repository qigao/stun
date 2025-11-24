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

enum class StrokeStyle { Solid, Dashed, Dotted };

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
  float svg_width;                // Actual SVG document width
  float svg_height;               // Actual SVG document height

  // Enhanced visual properties (Task 12)
  float opacity;                  // Opacity (0.0 to 1.0, default 1.0)
  StrokeStyle stroke_style;       // Stroke style (Solid, Dashed, Dotted)
  float corner_radius;            // Corner radius for rectangles (default 0.0)

  // DDF integration fields (Task 15)
  std::string ddf_shape_id;              // ID of corresponding DDF shape (if any)
  std::string ddf_component_instance_id; // ID of DDF component instance (if any)

  Stroke()
      : width(3.0f), tool(Tool::Pen), fill_style(FillStyle::None), selected(false), rotation(0.0f),
        font_face("sans"), font_size(16.0f), text_align(NVG_ALIGN_LEFT | NVG_ALIGN_TOP),
        is_editing(false), nvg_image_handle(-1), image_width(0.0f), image_height(0.0f), name(),
        visible(true), locked(false), group_id(-1), svg_scale_x(1.0f), svg_scale_y(1.0f),
        svg_width(100.0f), svg_height(100.0f),
        opacity(1.0f), stroke_style(StrokeStyle::Solid), corner_radius(0.0f) {}

  /**
   * \brief Check if this stroke is a DDF element.
   * \return True if this stroke is linked to a DDF shape or component instance
   */
  bool is_ddf_element() const {
    return !ddf_shape_id.empty() || !ddf_component_instance_id.empty();
  }

  void get_bounds(float &min_x, float &min_y, float &max_x, float &max_y) const {
    if (points.empty())
      return;

    // Special handling for SVG shapes
    if (tool == Tool::SVGShape && !svg_data.empty()) {
      // For SVG shapes, use actual SVG dimensions and scale
      float scaled_width = svg_width * svg_scale_x;
      float scaled_height = svg_height * svg_scale_y;
      
      // Position is at first point (top-left of unrotated shape)
      float pos_x = points[0].x;
      float pos_y = points[0].y;
      
      // If no rotation, use simple axis-aligned bounds
      if (std::abs(rotation) < 0.001f) {
        min_x = pos_x;
        min_y = pos_y;
        max_x = pos_x + scaled_width;
        max_y = pos_y + scaled_height;
        return;
      }
      
      // Calculate rotated bounding box (rotation around center)
      float center_x = pos_x + scaled_width / 2.0f;
      float center_y = pos_y + scaled_height / 2.0f;
      
      // Get the four corners of the unrotated rectangle
      float corners_x[4] = {pos_x, pos_x + scaled_width, pos_x + scaled_width, pos_x};
      float corners_y[4] = {pos_y, pos_y, pos_y + scaled_height, pos_y + scaled_height};
      
      // Rotate each corner around the center
      float cos_r = std::cos(rotation);
      float sin_r = std::sin(rotation);
      
      min_x = std::numeric_limits<float>::max();
      min_y = std::numeric_limits<float>::max();
      max_x = std::numeric_limits<float>::lowest();
      max_y = std::numeric_limits<float>::lowest();
      
      for (int i = 0; i < 4; i++) {
        // Translate to origin (center is pivot)
        float tx = corners_x[i] - center_x;
        float ty = corners_y[i] - center_y;
        
        // Rotate
        float rx = tx * cos_r - ty * sin_r;
        float ry = tx * sin_r + ty * cos_r;
        
        // Translate back
        float final_x = rx + center_x;
        float final_y = ry + center_y;
        
        // Update bounds
        min_x = std::min(min_x, final_x);
        min_y = std::min(min_y, final_y);
        max_x = std::max(max_x, final_x);
        max_y = std::max(max_y, final_y);
      }
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

  // Get dash pattern for NanoVG based on stroke style
  std::vector<float> get_dash_pattern() const {
    switch (stroke_style) {
      case StrokeStyle::Dashed:
        return {10.0f, 5.0f};  // 10px dash, 5px gap
      case StrokeStyle::Dotted:
        return {2.0f, 4.0f};   // 2px dot, 4px gap
      case StrokeStyle::Solid:
      default:
        return {};  // Empty = solid line
    }
  }
};

} // namespace whiteboard
