/*
 * flexUI - ImageWidget Implementation
 */

#include <flexUI/widgets/image_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <cmath>

namespace flexUI {

ImageWidget::ImageWidget(const std::string& src) : src_(src) {
  // Image loading is deferred to render time via flex::Renderer
  // The renderer handles caching internally
  if (!src_.empty()) {
    loaded_ = true;  // Assume success, renderer will handle errors
  }
}

void ImageWidget::set_src(const std::string& src) {
  if (src_ != src) {
    src_ = src;
    loaded_ = !src.empty();
    error_ = false;
    natural_width_ = 0;
    natural_height_ = 0;
    dirty_ = true;
  }
}

void ImageWidget::render(const Element& elem, Renderer& renderer) {
  if (src_.empty()) return;

  auto& r = renderer.flex();
  auto* style = elem.computed_style;

  // Get object-fit mode
  std::string fit = "contain";
  if (style) {
    fit = style->get_variable("--object-fit", "contain");
  }

  float elem_w = elem.width();
  float elem_h = elem.height();

  // For now, use element dimensions as image dimensions
  // The renderer's draw_image handles the actual loading and sizing
  float img_w = elem_w;
  float img_h = elem_h;

  float draw_x = 0, draw_y = 0;
  float draw_w = elem_w, draw_h = elem_h;

  if (fit == "fill") {
    // Stretch to fill - use element dimensions directly
    draw_w = elem_w;
    draw_h = elem_h;
  } else if (fit == "cover") {
    // Scale to cover, may crop - for now just fill
    draw_w = elem_w;
    draw_h = elem_h;
  } else if (fit == "none") {
    // No scaling, center - use natural dimensions if known
    if (natural_width_ > 0 && natural_height_ > 0) {
      draw_w = natural_width_;
      draw_h = natural_height_;
      draw_x = (elem_w - draw_w) / 2;
      draw_y = (elem_h - draw_h) / 2;
    }
  } else {
    // contain (default) - fit within bounds maintaining aspect ratio
    // Without knowing natural dimensions, just fill the element
    draw_w = elem_w;
    draw_h = elem_h;
  }

  // Use flex::Renderer's draw_image
  r.draw_image(src_, draw_x, draw_y, draw_w, draw_h);
}

bool ImageWidget::handle_event(const Event& event, Element& elem) {
  return false;  // Images don't handle events by default
}

void ImageWidget::update(float delta_ms, Element& elem) {
  // No animation by default
}

} // namespace flexUI
