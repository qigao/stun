/*
 * tvgbox2 - ImageWidget Implementation
 */

#include <tvgbox2/widgets/image_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <cmath>

namespace tvgbox2 {

ImageWidget::ImageWidget(const std::string& src) : src_(src) {
  if (!src_.empty()) {
    load_image();
  }
}

ImageWidget::~ImageWidget() {
  release_picture();
}

void ImageWidget::release_picture() {
  if (picture_) {
    tvg::Paint::rel(picture_);
    picture_ = nullptr;
  }
}

void ImageWidget::set_src(const std::string& src) {
  if (src_ != src) {
    src_ = src;
    loaded_ = false;
    error_ = false;
    release_picture();
    load_image();
    dirty_ = true;
  }
}

void ImageWidget::load_image() {
  if (src_.empty()) return;

  auto* pic = tvg::Picture::gen();
  if (pic->load(src_.c_str()) == tvg::Result::Success) {
    float w, h;
    pic->size(&w, &h);
    natural_width_ = w;
    natural_height_ = h;
    release_picture();
    picture_ = pic;
    loaded_ = true;
    error_ = false;
    if (on_load_) on_load_(true);
  } else {
    tvg::Paint::rel(pic);
    loaded_ = false;
    error_ = true;
    if (on_load_) on_load_(false);
  }
}

void ImageWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  if (!loaded_ || !picture_) return;
  render_image(scene, elem);
}

void ImageWidget::render_image(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  // Get object-fit mode
  std::string fit = "contain";
  if (style) {
    fit = style->get_variable("--object-fit", "contain");
  }

  float elem_w = elem.width();
  float elem_h = elem.height();
  float img_w = natural_width_;
  float img_h = natural_height_;

  if (img_w <= 0 || img_h <= 0) return;

  float scale_x = 1.0f, scale_y = 1.0f;
  float offset_x = 0, offset_y = 0;

  if (fit == "fill") {
    // Stretch to fill
    scale_x = elem_w / img_w;
    scale_y = elem_h / img_h;
  } else if (fit == "cover") {
    // Scale to cover, may crop
    float scale = std::max(elem_w / img_w, elem_h / img_h);
    scale_x = scale_y = scale;
    offset_x = (elem_w - img_w * scale) / 2;
    offset_y = (elem_h - img_h * scale) / 2;
  } else if (fit == "none") {
    // No scaling, center
    offset_x = (elem_w - img_w) / 2;
    offset_y = (elem_h - img_h) / 2;
  } else {
    // contain (default) - fit within bounds
    float scale = std::min(elem_w / img_w, elem_h / img_h);
    scale_x = scale_y = scale;
    offset_x = (elem_w - img_w * scale) / 2;
    offset_y = (elem_h - img_h * scale) / 2;
  }

  // Clone picture for rendering - scene takes ownership
  auto* pic = static_cast<tvg::Picture*>(picture_->duplicate());
  if (!pic) return;

  pic->size(img_w * scale_x, img_h * scale_y);
  pic->translate(offset_x, offset_y);

  scene->push(pic);
}

bool ImageWidget::handle_event(const Event& event, Element& elem) {
  return false;  // Images don't handle events by default
}

void ImageWidget::update(float delta_ms, Element& elem) {
  // No animation by default
}

} // namespace tvgbox2
