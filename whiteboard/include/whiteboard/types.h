#pragma once

#include <nanogui/vector.h>
#include <nanovg.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace whiteboard {

enum class Tool { Select, Pan, Pen, Text, Sticky, Rectangle, Circle, Line, Arrow, Image };

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

  Stroke()
      : width(3.0f), tool(Tool::Pen), fill_style(FillStyle::None), selected(false), rotation(0.0f),
        font_face("sans"), font_size(16.0f), text_align(NVG_ALIGN_LEFT | NVG_ALIGN_TOP),
        is_editing(false), nvg_image_handle(-1), image_width(0.0f), image_height(0.0f), name(),
        visible(true), locked(false) {}

  void get_bounds(float &min_x, float &min_y, float &max_x, float &max_y) const {
    if (points.empty())
      return;

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
    float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
    get_bounds(min_x, min_y, max_x, max_y);
    return x >= min_x - margin && x <= max_x + margin && y >= min_y - margin &&
           y <= max_y + margin;
  }

  void move(float dx, float dy) {
    for (auto &p : points) {
      p.x += dx;
      p.y += dy;
    }
  }
};

} // namespace whiteboard
