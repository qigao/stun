/**
 * \file whiteboard_ddf_integration.cpp
 * \brief Implementation of WhiteboardDDFIntegration.
 */

#include "whiteboard/ddf/whiteboard_ddf_integration.h"
#include <iomanip>
#include <sstream>

namespace whiteboard {

WhiteboardDDFIntegration::WhiteboardDDFIntegration() {}

// === Stroke to DDF Conversion ===

std::optional<ddf::Shape> WhiteboardDDFIntegration::stroke_to_shape(const Stroke &stroke) const {
  ddf::Shape shape;

  // Generate unique ID
  shape.id = generate_shape_id();

  // Convert tool to shape type
  shape.type = tool_to_shape_type(stroke.tool);
  if (shape.type.empty()) {
    return std::nullopt; // Unsupported tool type
  }

  // Extract geometry
  shape.geometry = extract_geometry(stroke);

  // Convert styling
  shape.inline_style["fill"] = color_to_hex(stroke.fill_color);
  shape.inline_style["stroke"] = color_to_hex(stroke.color);
  shape.inline_style["stroke-width"] = std::to_string(stroke.width);
  shape.inline_style["opacity"] = std::to_string(stroke.opacity);

  // Handle fill style
  if (stroke.fill_style == FillStyle::None) {
    shape.inline_style["fill"] = "none";
  }

  // Handle stroke style
  if (stroke.stroke_style == StrokeStyle::Dashed) {
    shape.inline_style["stroke-dasharray"] = "10,5";
  } else if (stroke.stroke_style == StrokeStyle::Dotted) {
    shape.inline_style["stroke-dasharray"] = "2,4";
  }

  // Handle corner radius for rectangles
  if (stroke.tool == Tool::Rectangle && stroke.corner_radius > 0.0f) {
    shape.geometry["rx"] = stroke.corner_radius;
    shape.geometry["ry"] = stroke.corner_radius;
  }

  // Handle text
  if (stroke.tool == Tool::Text) {
    shape.inline_style["font-size"] = std::to_string(stroke.font_size) + "px";
    shape.inline_style["font-family"] = stroke.font_face;
    shape.geometry["text"] = 0.0f; // Placeholder, actual text stored separately
  }

  // Handle rotation
  if (std::abs(stroke.rotation) > 0.001f) {
    // Store rotation as 2D affine transform (a, b, c, d, e, f)
    // For rotation: [cos, sin, -sin, cos, 0, 0]
    float cos_r = std::cos(stroke.rotation);
    float sin_r = std::sin(stroke.rotation);
    shape.transform = {cos_r, sin_r, -sin_r, cos_r, 0.0f, 0.0f};
  }

  // Handle visibility and locked state
  if (!stroke.visible) {
    shape.inline_style["visibility"] = "hidden";
  }

  // Set pseudo-states based on selection
  if (stroke.selected) {
    shape.pseudo_states.insert("selected");
  }

  return shape;
}

std::vector<ddf::Shape> WhiteboardDDFIntegration::strokes_to_shapes(
    const std::vector<Stroke> &strokes) const {
  std::vector<ddf::Shape> shapes;
  shapes.reserve(strokes.size());

  for (const auto &stroke : strokes) {
    auto shape_opt = stroke_to_shape(stroke);
    if (shape_opt) {
      shapes.push_back(*shape_opt);
    }
  }

  return shapes;
}

// === DDF to Stroke Conversion ===

Stroke WhiteboardDDFIntegration::shape_to_stroke(const ddf::Shape &shape) const {
  Stroke stroke;

  // Convert shape type to tool
  stroke.tool = shape_type_to_tool(shape.type);

  // Apply geometry
  apply_geometry(stroke, shape.geometry);

  // Convert styling
  if (shape.inline_style.count("fill")) {
    const auto &fill = shape.inline_style.at("fill");
    if (fill == "none") {
      stroke.fill_style = FillStyle::None;
    } else {
      stroke.fill_style = FillStyle::Solid;
      stroke.fill_color = hex_to_color(fill);
    }
  }

  if (shape.inline_style.count("stroke")) {
    stroke.color = hex_to_color(shape.inline_style.at("stroke"));
  }

  if (shape.inline_style.count("stroke-width")) {
    stroke.width = std::stof(shape.inline_style.at("stroke-width"));
  }

  if (shape.inline_style.count("opacity")) {
    stroke.opacity = std::stof(shape.inline_style.at("opacity"));
  }

  // Handle stroke style
  if (shape.inline_style.count("stroke-dasharray")) {
    const auto &dasharray = shape.inline_style.at("stroke-dasharray");
    if (dasharray.find("10") != std::string::npos) {
      stroke.stroke_style = StrokeStyle::Dashed;
    } else if (dasharray.find("2") != std::string::npos) {
      stroke.stroke_style = StrokeStyle::Dotted;
    }
  }

  // Handle corner radius
  if (shape.geometry.count("rx")) {
    stroke.corner_radius = shape.geometry.at("rx");
  }

  // Handle text properties
  if (shape.inline_style.count("font-size")) {
    std::string font_size_str = shape.inline_style.at("font-size");
    // Remove "px" suffix if present
    if (font_size_str.size() > 2 && font_size_str.substr(font_size_str.size() - 2) == "px") {
      font_size_str = font_size_str.substr(0, font_size_str.size() - 2);
    }
    stroke.font_size = std::stof(font_size_str);
  }

  if (shape.inline_style.count("font-family")) {
    stroke.font_face = shape.inline_style.at("font-family");
  }

  // Handle visibility
  if (shape.inline_style.count("visibility")) {
    stroke.visible = (shape.inline_style.at("visibility") != "hidden");
  }

  // Handle selection state
  stroke.selected = shape.pseudo_states.count("selected") > 0;

  // Handle rotation from transform
  // Extract rotation angle from 2D affine transform [a, b, c, d, e, f]
  if (!shape.transform.empty() && shape.transform.size() >= 4) {
    // For rotation: [cos, sin, -sin, cos, tx, ty]
    float cos_theta = shape.transform[0];
    float sin_theta = shape.transform[1];
    stroke.rotation = std::atan2(sin_theta, cos_theta);
  }

  return stroke;
}

std::vector<Stroke> WhiteboardDDFIntegration::shapes_to_strokes(
    const std::vector<ddf::Shape> &shapes) const {
  std::vector<Stroke> strokes;
  strokes.reserve(shapes.size());

  for (const auto &shape : shapes) {
    strokes.push_back(shape_to_stroke(shape));
  }

  return strokes;
}

// === Synchronization ===

void WhiteboardDDFIntegration::sync_stroke_from_shape(Stroke &stroke,
                                                       const ddf::Shape &shape) const {
  // Update stroke properties from shape
  Stroke updated = shape_to_stroke(shape);

  // Preserve certain stroke-specific fields
  updated.name = stroke.name;
  updated.locked = stroke.locked;
  updated.group_id = stroke.group_id;
  updated.nvg_image_handle = stroke.nvg_image_handle;
  updated.file_path = stroke.file_path;
  updated.svg_data = stroke.svg_data;
  updated.svg_shape_id = stroke.svg_shape_id;
  updated.svg_parameters = stroke.svg_parameters;

  stroke = updated;
}

void WhiteboardDDFIntegration::sync_shape_from_stroke(ddf::Shape &shape,
                                                       const Stroke &stroke) const {
  auto updated_opt = stroke_to_shape(stroke);
  if (updated_opt) {
    // Preserve shape ID
    std::string original_id = shape.id;
    shape = *updated_opt;
    shape.id = original_id;
  }
}

// === Component Instance Conversion ===

std::vector<Stroke> WhiteboardDDFIntegration::component_instance_to_strokes(
    const ddf::ComponentInstance &instance,
    ddf::ComponentLayer &component_layer) const {
  // Get the component definition
  auto *component = component_layer.get_component(instance.component_id);
  if (!component) {
    return {};
  }

  // Instantiate shapes from component
  auto shapes = component_layer.instantiate_shapes(instance.id);

  // Convert shapes to strokes
  return shapes_to_strokes(shapes);
}

// === Helper Methods ===

std::string WhiteboardDDFIntegration::tool_to_shape_type(Tool tool) const {
  switch (tool) {
  case Tool::Rectangle:
    return "rect";
  case Tool::Circle:
    return "circle";
  case Tool::Line:
  case Tool::Arrow:
    return "line";
  case Tool::Pen:
    return "path";
  case Tool::Text:
    return "text";
  case Tool::Diamond:
    return "path"; // Diamond is a special path
  case Tool::SVGShape:
    return "svg";
  default:
    return ""; // Unsupported
  }
}

Tool WhiteboardDDFIntegration::shape_type_to_tool(const std::string &type) const {
  if (type == "rect")
    return Tool::Rectangle;
  if (type == "circle")
    return Tool::Circle;
  if (type == "line")
    return Tool::Line;
  if (type == "path")
    return Tool::Pen;
  if (type == "text")
    return Tool::Text;
  if (type == "svg")
    return Tool::SVGShape;
  return Tool::Select; // Default
}

std::map<std::string, float> WhiteboardDDFIntegration::extract_geometry(const Stroke &stroke) const {
  std::map<std::string, float> geometry;

  if (stroke.points.empty()) {
    return geometry;
  }

  switch (stroke.tool) {
  case Tool::Rectangle:
  case Tool::Diamond:
    if (stroke.points.size() >= 2) {
      float x1 = stroke.points[0].x;
      float y1 = stroke.points[0].y;
      float x2 = stroke.points[1].x;
      float y2 = stroke.points[1].y;
      geometry["x"] = std::min(x1, x2);
      geometry["y"] = std::min(y1, y2);
      geometry["width"] = std::abs(x2 - x1);
      geometry["height"] = std::abs(y2 - y1);
    }
    break;

  case Tool::Circle:
    if (stroke.points.size() >= 2) {
      float cx = stroke.points[0].x;
      float cy = stroke.points[0].y;
      float dx = stroke.points[1].x - cx;
      float dy = stroke.points[1].y - cy;
      float radius = std::sqrt(dx * dx + dy * dy);
      geometry["cx"] = cx;
      geometry["cy"] = cy;
      geometry["r"] = radius;
    }
    break;

  case Tool::Line:
  case Tool::Arrow:
    if (stroke.points.size() >= 2) {
      geometry["x1"] = stroke.points[0].x;
      geometry["y1"] = stroke.points[0].y;
      geometry["x2"] = stroke.points[1].x;
      geometry["y2"] = stroke.points[1].y;
    }
    break;

  case Tool::Text:
    if (!stroke.points.empty()) {
      geometry["x"] = stroke.points[0].x;
      geometry["y"] = stroke.points[0].y;
    }
    break;

  case Tool::SVGShape:
    if (!stroke.points.empty()) {
      geometry["x"] = stroke.points[0].x;
      geometry["y"] = stroke.points[0].y;
      geometry["width"] = stroke.svg_width * stroke.svg_scale_x;
      geometry["height"] = stroke.svg_height * stroke.svg_scale_y;
    }
    break;

  case Tool::Pen:
    // For pen strokes, store path data
    // This is simplified - full implementation would convert to SVG path
    if (!stroke.points.empty()) {
      geometry["x"] = stroke.points[0].x;
      geometry["y"] = stroke.points[0].y;
    }
    break;

  default:
    break;
  }

  return geometry;
}

void WhiteboardDDFIntegration::apply_geometry(Stroke &stroke,
                                               const std::map<std::string, float> &geometry) const {
  stroke.points.clear();

  switch (stroke.tool) {
  case Tool::Rectangle:
  case Tool::Diamond:
    if (geometry.count("x") && geometry.count("y") && geometry.count("width") &&
        geometry.count("height")) {
      float x = geometry.at("x");
      float y = geometry.at("y");
      float w = geometry.at("width");
      float h = geometry.at("height");
      stroke.points.push_back(Point(x, y));
      stroke.points.push_back(Point(x + w, y + h));
    }
    break;

  case Tool::Circle:
    if (geometry.count("cx") && geometry.count("cy") && geometry.count("r")) {
      float cx = geometry.at("cx");
      float cy = geometry.at("cy");
      float r = geometry.at("r");
      stroke.points.push_back(Point(cx, cy));
      stroke.points.push_back(Point(cx + r, cy)); // Point on circle edge
    }
    break;

  case Tool::Line:
  case Tool::Arrow:
    if (geometry.count("x1") && geometry.count("y1") && geometry.count("x2") &&
        geometry.count("y2")) {
      stroke.points.push_back(Point(geometry.at("x1"), geometry.at("y1")));
      stroke.points.push_back(Point(geometry.at("x2"), geometry.at("y2")));
    }
    break;

  case Tool::Text:
    if (geometry.count("x") && geometry.count("y")) {
      stroke.points.push_back(Point(geometry.at("x"), geometry.at("y")));
    }
    break;

  case Tool::SVGShape:
    if (geometry.count("x") && geometry.count("y")) {
      stroke.points.push_back(Point(geometry.at("x"), geometry.at("y")));
      if (geometry.count("width") && geometry.count("height")) {
        stroke.svg_width = geometry.at("width");
        stroke.svg_height = geometry.at("height");
      }
    }
    break;

  default:
    break;
  }
}

std::string WhiteboardDDFIntegration::color_to_hex(const nanogui::Color &color) const {
  std::ostringstream oss;
  oss << "#" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(color.r() * 255)
      << std::setw(2) << static_cast<int>(color.g() * 255) << std::setw(2)
      << static_cast<int>(color.b() * 255);
  return oss.str();
}

nanogui::Color WhiteboardDDFIntegration::hex_to_color(const std::string &hex) const {
  if (hex.empty() || hex[0] != '#' || hex.size() < 7) {
    return nanogui::Color(0, 0, 0, 255); // Default to black
  }

  int r = std::stoi(hex.substr(1, 2), nullptr, 16);
  int g = std::stoi(hex.substr(3, 2), nullptr, 16);
  int b = std::stoi(hex.substr(5, 2), nullptr, 16);

  return nanogui::Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

std::string WhiteboardDDFIntegration::generate_shape_id() const {
  return "shape_" + std::to_string(m_next_shape_id++);
}

} // namespace whiteboard
