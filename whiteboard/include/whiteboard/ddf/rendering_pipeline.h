#pragma once

#include "render_node.h"
#include <memory>
#include <vector>

// Forward declarations
struct NVGcontext;

namespace whiteboard {
class SVGShapeLibrary;
class SVGRenderer;

namespace ddf {

// Forward declaration
class DDFDocument;

/**
 * @brief Rendering pipeline for DDF documents
 *
 * The rendering pipeline converts DDF to rendered output using CSS-like processing.
 * It builds a render tree, computes styles, layouts, transforms, and paints.
 */
class RenderingPipeline {
public:
  explicit RenderingPipeline(NVGcontext *ctx);
  ~RenderingPipeline(); // Need explicit destructor for unique_ptr with forward-declared type

  // Main rendering pipeline
  void render(DDFDocument &doc);

  // Viewport management
  void set_viewport(float x, float y, float width, float height);
  void get_viewport(float &x, float &y, float &width, float &height) const;
  
  // SVG shape library
  void set_shape_library(SVGShapeLibrary *library) { svg_shape_library_ = library; }

private:
  NVGcontext *nvg_context_;
  SVGShapeLibrary *svg_shape_library_ = nullptr;
  std::unique_ptr<SVGRenderer> svg_renderer_;
  float viewport_x_ = 0.0f;
  float viewport_y_ = 0.0f;
  float viewport_width_ = 0.0f;
  float viewport_height_ = 0.0f;

  // Pipeline stages
  std::unique_ptr<RenderNode> build_render_tree(DDFDocument &doc);
  std::vector<RenderNode *> cull_viewport(RenderNode *root);

  // Shape rendering helpers
  void render_rect(const struct Shape *shape);
  void render_ellipse(const struct Shape *shape);
  void render_path(const struct Shape *shape);
  void render_text(const struct Shape *shape);
  void render_svg(const struct Shape *shape);
  void render_shape_text(const struct Shape *shape);
  void render_connector(const struct Connector *conn, DDFDocument &doc);
  void apply_style(const struct Shape *shape);
  struct NVGcolor parse_color(const std::string &color_str);
};

} // namespace ddf
} // namespace whiteboard
