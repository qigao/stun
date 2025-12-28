/*
 * flexUI - ImageWidget
 *
 * Display images - 使用 flex::Renderer 渲染
 */

#ifndef FLEXUI_IMAGE_WIDGET_H
#define FLEXUI_IMAGE_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <functional>

namespace flexUI {

/**
 * ImageWidget - Image display widget
 *
 * Uses flex::Renderer's draw_image() for loading and rendering.
 *
 * CSS variables:
 *   --object-fit: "contain" | "cover" | "fill" | "none"
 *   --object-position: "center" | "top" | "bottom" | "left" | "right"
 */
class ImageWidget : public Widget {
public:
  explicit ImageWidget(const std::string& src = "");

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ImageWidget"; }

  // Image source
  const std::string& src() const { return src_; }
  void set_src(const std::string& src);

  // Image dimensions (original)
  float natural_width() const { return natural_width_; }
  float natural_height() const { return natural_height_; }

  // Load state
  bool is_loaded() const { return loaded_; }
  bool has_error() const { return error_; }

  // Callbacks
  using LoadCallback = std::function<void(bool success)>;
  void set_load_callback(LoadCallback cb) { on_load_ = std::move(cb); }

private:
  std::string src_;
  float natural_width_ = 0;
  float natural_height_ = 0;
  bool loaded_ = false;
  bool error_ = false;
  LoadCallback on_load_;
};

} // namespace flexUI

#endif // FLEXUI_IMAGE_WIDGET_H
