#include <cctype>
#include <cmath>
#include <fmtlog.h>
#include <nanovg.h>
#include <sstream>
#include <whiteboard/ddf/connector_layer.h>
#include <whiteboard/ddf/ddf_document.h>
#include <whiteboard/ddf/rendering_pipeline.h>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/style_layer.h>
#include <whiteboard/svg/svg_renderer.h>
#include <whiteboard/svg/svg_shape_library.h>


namespace whiteboard {
namespace ddf {

RenderingPipeline::RenderingPipeline(NVGcontext *ctx)
    : nvg_context_(ctx), svg_renderer_(std::make_unique<SVGRenderer>()) {}

RenderingPipeline::~RenderingPipeline() = default;

void RenderingPipeline::render(DDFDocument &doc) {
  if (!nvg_context_)
    return;

  // 1. Build render tree from DDF shapes
  auto render_tree = build_render_tree(doc);
  if (!render_tree)
    return;

  // 2. Compute styles (CSS cascade)
  // TODO: Get merged stylesheet from style layer
  // render_tree->compute_styles(stylesheet);

  // 3. Compute layout (position children)
  render_tree->compute_layout();

  // 4. Compute transforms (accumulate from parents)
  render_tree->compute_transforms();

  // 5. Compute bounds (for culling)
  render_tree->compute_bounds();

  // 6. Cull invisible nodes
  auto visible_nodes = cull_viewport(render_tree.get());

  // 7. Paint (tree traversal)
  for (auto *node : visible_nodes) {
    node->render(nvg_context_);
  }
}

void RenderingPipeline::set_viewport(float x, float y, float width, float height) {
  viewport_x_ = x;
  viewport_y_ = y;
  viewport_width_ = width;
  viewport_height_ = height;
}

void RenderingPipeline::get_viewport(float &x, float &y, float &width, float &height) const {
  x = viewport_x_;
  y = viewport_y_;
  width = viewport_width_;
  height = viewport_height_;
}

std::unique_ptr<RenderNode> RenderingPipeline::build_render_tree(DDFDocument &doc) {
  // For now, directly render shapes without building a tree
  // This is a simplified implementation
  auto root = std::make_unique<RenderNode>();

  // Get all shapes from the document
  auto shapes = doc.shape_layer().get_all_shapes();

  // Render each shape directly
  for (const auto *shape : shapes) {
    if (!shape)
      continue;

    nvgSave(nvg_context_);

    // Apply transform if present
    if (!shape->transform.empty() && shape->transform.size() >= 6) {
      nvgTransform(nvg_context_, shape->transform[0], shape->transform[1], shape->transform[2],
                   shape->transform[3], shape->transform[4], shape->transform[5]);
    }

    // Compute styles from stylesheet (includes pseudo-states like :hover)
    auto computed_style = doc.style_layer().compute_style_for_shape(*shape);

    // Create a temporary merged style map for rendering
    std::map<std::string, std::string> merged_style = shape->inline_style;
    for (const auto &style_pair : computed_style) {
      if (merged_style.count(style_pair.first) == 0) {
        merged_style[style_pair.first] = style_pair.second;
      }
    }

    // Temporarily replace inline_style for rendering
    auto original_style = shape->inline_style;
    const_cast<Shape *>(shape)->inline_style = merged_style;

    // Render based on shape type
    if (shape->type == "rect") {
      render_rect(shape);
    } else if (shape->type == "circle" || shape->type == "ellipse") {
      render_ellipse(shape);
    } else if (shape->type == "path") {
      render_path(shape);
    } else if (shape->type == "text") {
      render_text(shape);
    } else if (shape->type == "svg") {
      render_svg(shape);
    }

    // Render text label if present (for non-text shapes)
    if (shape->type != "text" && !shape->text.empty()) {
      render_shape_text(shape);
    }

    // Restore original inline style
    const_cast<Shape *>(shape)->inline_style = original_style;

    nvgRestore(nvg_context_);
  }

  // Render connectors
  auto connectors = doc.connector_layer().get_all_connectors();
  for (const auto *conn : connectors) {
    if (!conn)
      continue;
    render_connector(conn, doc);
  }

  return root;
}

std::vector<RenderNode *> RenderingPipeline::cull_viewport(RenderNode *root) {
  // TODO: Implement viewport culling
  std::vector<RenderNode *> visible;
  if (root) {
    visible.push_back(root);
  }
  return visible;
}

void RenderingPipeline::render_rect(const Shape *shape) {
  if (!shape || shape->geometry.count("x") == 0)
    return;

  float x = shape->geometry.at("x");
  float y = shape->geometry.at("y");
  float w = shape->geometry.count("width") ? shape->geometry.at("width") : 100.0f;
  float h = shape->geometry.count("height") ? shape->geometry.at("height") : 50.0f;
  float rx = shape->geometry.count("rx") ? shape->geometry.at("rx") : 0.0f;

  nvgBeginPath(nvg_context_);
  if (rx > 0) {
    nvgRoundedRect(nvg_context_, x, y, w, h, rx);
  } else {
    nvgRect(nvg_context_, x, y, w, h);
  }

  apply_style(shape);
}

void RenderingPipeline::render_ellipse(const Shape *shape) {
  if (!shape || shape->geometry.count("cx") == 0)
    return;

  float cx = shape->geometry.at("cx");
  float cy = shape->geometry.at("cy");
  float rx = shape->geometry.count("rx")  ? shape->geometry.at("rx")
             : shape->geometry.count("r") ? shape->geometry.at("r")
                                          : 50.0f;
  float ry = shape->geometry.count("ry") ? shape->geometry.at("ry") : rx;

  nvgBeginPath(nvg_context_);
  nvgEllipse(nvg_context_, cx, cy, rx, ry);

  apply_style(shape);
}

void RenderingPipeline::render_path(const Shape *shape) {
  if (!shape || shape->inline_style.count("d") == 0)
    return;

  // Parse simple SVG path commands (M, L, Z)
  std::string path_data = shape->inline_style.at("d");

  nvgBeginPath(nvg_context_);

  // Simple parser for M (moveto), L (lineto), Z (closepath)
  std::istringstream iss(path_data);
  char cmd;
  float x, y;
  bool first = true;

  while (iss >> cmd) {
    if (cmd == 'M' || cmd == 'm') {
      // Move to
      if (iss >> x) {
        iss.ignore(1); // skip comma
        if (iss >> y) {
          nvgMoveTo(nvg_context_, x, y);
          first = false;
        }
      }
    } else if (cmd == 'L' || cmd == 'l') {
      // Line to
      if (iss >> x) {
        iss.ignore(1); // skip comma
        if (iss >> y) {
          nvgLineTo(nvg_context_, x, y);
        }
      }
    } else if (cmd == 'Z' || cmd == 'z') {
      // Close path
      nvgClosePath(nvg_context_);
    } else if (std::isdigit(cmd) || cmd == '-') {
      // Implicit lineto (number without command)
      iss.putback(cmd);
      if (iss >> x) {
        iss.ignore(1); // skip comma
        if (iss >> y) {
          if (first) {
            nvgMoveTo(nvg_context_, x, y);
            first = false;
          } else {
            nvgLineTo(nvg_context_, x, y);
          }
        }
      }
    }
  }

  apply_style(shape);
}

void RenderingPipeline::render_svg(const Shape *shape) {
  if (!shape)
    return;

  if (!svg_renderer_) {
    loge("SVG renderer not initialized!");
    return;
  }

  if (!shape->geometry.count("x") || !shape->geometry.count("y"))
    return;

  float x = shape->geometry.at("x");
  float y = shape->geometry.at("y");
  float w = shape->geometry.count("width") ? shape->geometry.at("width") : 80.0f;
  float h = shape->geometry.count("height") ? shape->geometry.at("height") : 80.0f;

  // Try to load SVG from library if shape_id is provided
  std::string svg_data = shape->svg_data;

  if (svg_data.empty() && !shape->svg_shape_id.empty()) {
    if (svg_shape_library_) {
      // Generate SVG from library
      svg_data = svg_shape_library_->generate_svg(shape->svg_shape_id, shape->svg_parameters);
    } else {
      // Library not set - log warning
      logi("SVG shape library not set, cannot load shape: {}", shape->svg_shape_id);
    }
  }

  if (!svg_data.empty()) {
    // Load SVG document using the SVG renderer
    auto svg_doc = svg_renderer_->load_svg(svg_data);
    if (svg_doc) {
      // Get actual SVG dimensions
      float svg_width = svg_doc->width();
      float svg_height = svg_doc->height();

      // Calculate scale factors to fit the desired size
      float scale_x = (svg_width > 0) ? (w / svg_width) : 1.0f;
      float scale_y = (svg_height > 0) ? (h / svg_height) : 1.0f;

      // Render using the SVG renderer (handles all the bitmap conversion properly)
      nanogui::Vector2f pos(x, y);
      svg_renderer_->render(nvg_context_, svg_doc.get(), pos, scale_x, scale_y, 0.0f, nullptr);

      logd("DDF: Rendered SVG via SVGRenderer: {} ({}x{} scaled to {}x{}) at ({}, {})",
           shape->svg_shape_id.empty() ? "inline" : shape->svg_shape_id, svg_width, svg_height, w,
           h, x, y);
    } else {
      logi("Failed to load SVG document for: {}", shape->svg_shape_id);
    }
  } else {
    // Fallback: Draw placeholder rectangle
    nvgBeginPath(nvg_context_);
    nvgRect(nvg_context_, x, y, w, h);
    nvgFillColor(nvg_context_, nvgRGBA(200, 200, 200, 100));
    nvgFill(nvg_context_);
    nvgStrokeColor(nvg_context_, nvgRGBA(150, 150, 150, 255));
    nvgStrokeWidth(nvg_context_, 2.0f);
    nvgStroke(nvg_context_);

    // Draw shape ID as text
    nvgFontSize(nvg_context_, 10.0f);
    nvgFontFace(nvg_context_, "sans");
    nvgTextAlign(nvg_context_, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(nvg_context_, nvgRGBA(100, 100, 100, 255));

    std::string label = shape->svg_shape_id.empty() ? "SVG" : shape->svg_shape_id;
    nvgText(nvg_context_, x + w / 2, y + h / 2, label.c_str(), nullptr);
  }
}

void RenderingPipeline::render_text(const Shape *shape) {
  if (!shape || shape->geometry.count("x") == 0)
    return;

  float x = shape->geometry.at("x");
  float y = shape->geometry.at("y");

  nvgFontSize(nvg_context_, 14.0f);
  nvgFontFace(nvg_context_, "sans");
  nvgTextAlign(nvg_context_, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  if (shape->inline_style.count("fill")) {
    auto color = parse_color(shape->inline_style.at("fill"));
    nvgFillColor(nvg_context_, color);
  } else {
    nvgFillColor(nvg_context_, nvgRGBA(0, 0, 0, 255));
  }

  nvgText(nvg_context_, x, y, shape->text.c_str(), nullptr);
}

void RenderingPipeline::render_shape_text(const Shape *shape) {
  if (!shape || shape->text.empty())
    return;

  // Calculate center position based on shape type
  float cx = 0, cy = 0;

  if (shape->type == "rect") {
    if (shape->geometry.count("x") && shape->geometry.count("y")) {
      float x = shape->geometry.at("x");
      float y = shape->geometry.at("y");
      float w = shape->geometry.count("width") ? shape->geometry.at("width") : 100.0f;
      float h = shape->geometry.count("height") ? shape->geometry.at("height") : 50.0f;
      cx = x + w / 2.0f;
      cy = y + h / 2.0f;
    }
  } else if (shape->type == "ellipse" || shape->type == "circle") {
    if (shape->geometry.count("cx") && shape->geometry.count("cy")) {
      cx = shape->geometry.at("cx");
      cy = shape->geometry.at("cy");
    }
  } else if (shape->type == "path") {
    // For paths, try to estimate center from bounding box
    // This is a simplified approach - ideally we'd parse the path
    if (shape->inline_style.count("d")) {
      // For now, just use a default position
      cx = 200.0f;
      cy = 260.0f;
    }
  }

  // Set text style
  float font_size = 12.0f;
  if (shape->inline_style.count("font-size")) {
    std::string fs = shape->inline_style.at("font-size");
    if (fs.find("px") != std::string::npos) {
      fs = fs.substr(0, fs.find("px"));
    }
    font_size = std::stof(fs);
  }

  nvgFontSize(nvg_context_, font_size);
  nvgFontFace(nvg_context_, "sans");
  nvgTextAlign(nvg_context_, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  // Choose text color based on background brightness
  NVGcolor text_color = nvgRGBA(255, 255, 255, 255); // Default white
  if (shape->inline_style.count("fill")) {
    auto bg_color = parse_color(shape->inline_style.at("fill"));
    // Calculate perceived brightness (0-1)
    float brightness = (bg_color.r * 299 + bg_color.g * 587 + bg_color.b * 114) / 1000.0f;
    if (brightness > 0.5f) {
      // Light background - use dark text
      text_color = nvgRGBA(44, 62, 80, 255); // #2c3e50
    }
  }
  nvgFillColor(nvg_context_, text_color);

  // Handle multi-line text (split by \n)
  std::string text = shape->text;
  size_t pos = 0;
  int line = 0;
  float line_height = font_size * 1.2f;

  while ((pos = text.find("\\n")) != std::string::npos) {
    std::string line_text = text.substr(0, pos);
    nvgText(nvg_context_, cx, cy + (line - 0.5f) * line_height, line_text.c_str(), nullptr);
    text = text.substr(pos + 2);
    line++;
  }
  nvgText(nvg_context_, cx, cy + (line - 0.5f) * line_height, text.c_str(), nullptr);
}

void RenderingPipeline::render_connector(const Connector *conn, DDFDocument &doc) {
  if (!conn)
    return;

  // Get the from and to shapes
  auto *from_shape = doc.shape_layer().get_shape(conn->from.shape_id);
  auto *to_shape = doc.shape_layer().get_shape(conn->to.shape_id);

  if (!from_shape || !to_shape)
    return;

  // Calculate connection points
  float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

  // Get from point
  if (from_shape->type == "rect") {
    float x = from_shape->geometry.count("x") ? from_shape->geometry.at("x") : 0;
    float y = from_shape->geometry.count("y") ? from_shape->geometry.at("y") : 0;
    float w = from_shape->geometry.count("width") ? from_shape->geometry.at("width") : 100;
    float h = from_shape->geometry.count("height") ? from_shape->geometry.at("height") : 50;

    if (conn->from.connection_point_id == "bottom") {
      x1 = x + w / 2;
      y1 = y + h;
    } else if (conn->from.connection_point_id == "top") {
      x1 = x + w / 2;
      y1 = y;
    } else if (conn->from.connection_point_id == "left") {
      x1 = x;
      y1 = y + h / 2;
    } else if (conn->from.connection_point_id == "right") {
      x1 = x + w;
      y1 = y + h / 2;
    }
  } else if (from_shape->type == "ellipse") {
    float cx = from_shape->geometry.count("cx") ? from_shape->geometry.at("cx") : 0;
    float cy = from_shape->geometry.count("cy") ? from_shape->geometry.at("cy") : 0;
    float ry = from_shape->geometry.count("ry") ? from_shape->geometry.at("ry") : 30;

    if (conn->from.connection_point_id == "bottom") {
      x1 = cx;
      y1 = cy + ry;
    } else if (conn->from.connection_point_id == "top") {
      x1 = cx;
      y1 = cy - ry;
    }
  }

  // Get to point
  if (to_shape->type == "rect") {
    float x = to_shape->geometry.count("x") ? to_shape->geometry.at("x") : 0;
    float y = to_shape->geometry.count("y") ? to_shape->geometry.at("y") : 0;
    float w = to_shape->geometry.count("width") ? to_shape->geometry.at("width") : 100;
    float h = to_shape->geometry.count("height") ? to_shape->geometry.at("height") : 50;

    if (conn->to.connection_point_id == "top") {
      x2 = x + w / 2;
      y2 = y;
    } else if (conn->to.connection_point_id == "bottom") {
      x2 = x + w / 2;
      y2 = y + h;
    } else if (conn->to.connection_point_id == "left") {
      x2 = x;
      y2 = y + h / 2;
    } else if (conn->to.connection_point_id == "right") {
      x2 = x + w;
      y2 = y + h / 2;
    }
  } else if (to_shape->type == "ellipse") {
    float cx = to_shape->geometry.count("cx") ? to_shape->geometry.at("cx") : 0;
    float cy = to_shape->geometry.count("cy") ? to_shape->geometry.at("cy") : 0;
    float ry = to_shape->geometry.count("ry") ? to_shape->geometry.at("ry") : 30;

    if (conn->to.connection_point_id == "top") {
      x2 = cx;
      y2 = cy - ry;
    } else if (conn->to.connection_point_id == "left") {
      x2 = cx - (to_shape->geometry.count("rx") ? to_shape->geometry.at("rx") : 60);
      y2 = cy;
    }
  }

  // Draw the line
  nvgBeginPath(nvg_context_);
  nvgMoveTo(nvg_context_, x1, y1);
  nvgLineTo(nvg_context_, x2, y2);

  // Apply style
  NVGcolor stroke_color = nvgRGBA(52, 73, 94, 255); // Default #34495e
  if (conn->style.count("stroke")) {
    stroke_color = parse_color(conn->style.at("stroke"));
  }
  nvgStrokeColor(nvg_context_, stroke_color);

  float stroke_width = 2.0f;
  if (conn->style.count("stroke-width")) {
    stroke_width = std::stof(conn->style.at("stroke-width"));
  }
  nvgStrokeWidth(nvg_context_, stroke_width);
  nvgStroke(nvg_context_);

  // Draw arrow head if specified
  if (conn->arrow_end == ArrowType::Arrow) {
    float angle = std::atan2(y2 - y1, x2 - x1);
    float arrow_size = 10.0f;

    nvgBeginPath(nvg_context_);
    nvgMoveTo(nvg_context_, x2, y2);
    nvgLineTo(nvg_context_, x2 - arrow_size * std::cos(angle - 0.5f),
              y2 - arrow_size * std::sin(angle - 0.5f));
    nvgLineTo(nvg_context_, x2 - arrow_size * std::cos(angle + 0.5f),
              y2 - arrow_size * std::sin(angle + 0.5f));
    nvgClosePath(nvg_context_);
    nvgFillColor(nvg_context_, stroke_color);
    nvgFill(nvg_context_);
  }

  // Draw label if present
  if (conn->label.has_value() && !conn->label->text.empty()) {
    float label_x = x1 + (x2 - x1) * conn->label->position;
    float label_y = y1 + (y2 - y1) * conn->label->position;

    nvgFontSize(nvg_context_, 10.0f);
    nvgFontFace(nvg_context_, "sans");
    nvgTextAlign(nvg_context_, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(nvg_context_, nvgRGBA(127, 140, 141, 255));
    nvgText(nvg_context_, label_x, label_y - 10, conn->label->text.c_str(), nullptr);
  }
}

void RenderingPipeline::apply_style(const Shape *shape) {
  // Apply fill
  if (shape->inline_style.count("fill")) {
    auto fill_str = shape->inline_style.at("fill");
    if (fill_str != "none") {
      auto color = parse_color(fill_str);
      nvgFillColor(nvg_context_, color);
      nvgFill(nvg_context_);
    }
  } else {
    nvgFillColor(nvg_context_, nvgRGBA(200, 200, 200, 255));
    nvgFill(nvg_context_);
  }

  // Apply stroke
  if (shape->inline_style.count("stroke")) {
    auto stroke_str = shape->inline_style.at("stroke");
    auto color = parse_color(stroke_str);
    nvgStrokeColor(nvg_context_, color);

    float stroke_width = 1.0f;
    if (shape->inline_style.count("stroke-width")) {
      stroke_width = std::stof(shape->inline_style.at("stroke-width"));
    }
    nvgStrokeWidth(nvg_context_, stroke_width);
    nvgStroke(nvg_context_);
  }
}

NVGcolor RenderingPipeline::parse_color(const std::string &color_str) {
  if (color_str.empty() || color_str[0] != '#') {
    return nvgRGBA(0, 0, 0, 255);
  }

  if (color_str.length() >= 7) {
    int r = std::stoi(color_str.substr(1, 2), nullptr, 16);
    int g = std::stoi(color_str.substr(3, 2), nullptr, 16);
    int b = std::stoi(color_str.substr(5, 2), nullptr, 16);
    return nvgRGBA(r, g, b, 255);
  }

  return nvgRGBA(0, 0, 0, 255);
}

} // namespace ddf
} // namespace whiteboard
