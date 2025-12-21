/*
 * tvgbox2 - ImageWidget
 *
 * Display images (PNG, JPG, SVG) using ThorVG
 */

#ifndef TVGBOX2_IMAGE_WIDGET_H
#define TVGBOX2_IMAGE_WIDGET_H

#include "../widget.h"
#include <string>
#include <memory>
#include <functional>

namespace tvg {
class Picture;
}

namespace tvgbox2 {

/**
 * ImageWidget - Image display widget
 *
 * Supports PNG, JPG, SVG via ThorVG's native loaders.
 *
 * CSS variables:
 *   --object-fit: "contain" | "cover" | "fill" | "none"
 *   --object-position: "center" | "top" | "bottom" | "left" | "right"
 *
 * Example:
 *   auto* img = box->create_widget<ImageWidget>("image", "logo", "assets/logo.svg");
 */
class ImageWidget : public Widget {
public:
  explicit ImageWidget(const std::string& src = "");
  ~ImageWidget() override;

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
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
  void load_image();
  void render_image(tvg::Scene* scene, const Element& elem);
  void release_picture();

  std::string src_;
  tvg::Picture* picture_ = nullptr;  // Owned, release with Paint::rel()
  float natural_width_ = 0;
  float natural_height_ = 0;
  bool loaded_ = false;
  bool error_ = false;
  LoadCallback on_load_;
};

} // namespace tvgbox2

#endif // TVGBOX2_IMAGE_WIDGET_H
