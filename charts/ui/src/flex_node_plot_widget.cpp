#include <charts/ui/flex_node_plot_widget.h>

#include <flex.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/widget.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace charts::ui {
namespace {

class CommandListRenderer final : public flex::Renderer {
 public:
  CommandListRenderer(flexUI::RenderCommandList& commands, float width,
                      float height)
      : commands_(commands), viewport_{0.0f, 0.0f, width, height} {}

  void begin_frame(float, float, float) override {}
  void end_frame() override {}
  void set_retained_mode(bool enabled) override { retained_mode_ = enabled; }
  void save() override { commands_.save(); }
  void restore() override { commands_.restore(); }
  void reset() override { commands_.set_transform(flex::Transform{}); }
  void set_transform(const flex::Transform& transform) override {
    commands_.set_transform(transform);
  }
  void translate(float x, float y) override { commands_.translate(x, y); }
  void rotate(float degrees) override { commands_.rotate(degrees); }
  void scale(float sx, float sy) override { commands_.scale(sx, sy); }
  void clip_rect(float x, float y, float width, float height) override {
    commands_.clip_rect(x, y, width, height);
  }
  void reset_clip() override { commands_.reset_clip(); }
  void set_global_alpha(float alpha) override {
    commands_.set_global_alpha(alpha);
  }
  void set_shadow(const flex::Shadow& shadow) override {
    commands_.set_shadow(shadow);
  }
  void clear_shadow() override { commands_.clear_shadow(); }
  void set_blur(const flex::BlurFilter& blur) override {
    commands_.set_blur(blur);
  }
  void clear_blur() override { commands_.clear_blur(); }
  void fill_path(const std::string& path, const flex::Paint& paint) override {
    commands_.fill_path(path, paint);
  }
  void stroke_path(const std::string& path, const flex::Paint& paint,
                   float width) override {
    commands_.stroke_path(path, paint, width);
  }
  void draw_line(float x1, float y1, float x2, float y2,
                 const flex::Paint& paint, float width) override {
    commands_.draw_line(x1, y1, x2, y2, paint, width);
  }
  void draw_rect(float x, float y, float width, float height, float radius,
                 const flex::Paint& fill, const flex::Paint& stroke,
                 float stroke_width) override {
    commands_.draw_rect(x, y, width, height, radius, fill, stroke,
                        stroke_width);
  }
  void draw_circle(float x, float y, float radius, const flex::Paint& fill,
                   const flex::Paint& stroke, float stroke_width) override {
    commands_.draw_circle(x, y, radius, fill, stroke, stroke_width);
  }
  void draw_ellipse(float x, float y, float radius_x, float radius_y,
                    const flex::Paint& fill, const flex::Paint& stroke,
                    float stroke_width) override {
    commands_.draw_ellipse(x, y, radius_x, radius_y, fill, stroke,
                           stroke_width);
  }
  void draw_text(const std::string& value, float x, float y,
                 const std::string& font, float size, bool bold,
                 const flex::Color& color) override {
    commands_.draw_text(value, x, y, font, size, bold, color);
  }
  void draw_image(const std::string& source, float x, float y, float width,
                  float height) override {
    commands_.draw_image(source, x, y, width, height);
  }
  void draw_svg(const std::string& source, float x, float y, float width,
                float height) override {
    commands_.draw_svg(source, x, y, width, height);
  }
  void draw_svg_data(const std::string& data, float x, float y, float width,
                     float height) override {
    commands_.draw_svg_data(data, x, y, width, height);
  }
  void clear(const flex::Color&) override {}
  flex::Bounds viewport() const override { return viewport_; }
  flex::RendererCapabilities capabilities() const override { return {}; }
  bool supports_retained_mode() const override { return false; }
  void remove_cached(flex::PaintHandle) override {}
  flex::PaintHandle push_rect(float, float, float, float, float,
                              const flex::Paint&, const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_circle(float, float, float, const flex::Paint&,
                                const flex::Paint&, float,
                                const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_ellipse(float, float, float, float,
                                 const flex::Paint&, const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_polygon(int, float, const flex::Paint&,
                                 const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_star(int, float, float, const flex::Paint&,
                              const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_path(const std::string&, const flex::Paint&,
                              const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  void update_transform(flex::PaintHandle,
                        const flex::Transform&) override {}

 private:
  flexUI::RenderCommandList& commands_;
  flex::Bounds viewport_;
};

class FlexNodePlotWidget final : public flexUI::Widget {
 public:
  FlexNodePlotWidget(std::shared_ptr<flex::Instance> instance,
                     flex::Group* plot, float width, float height,
                     bool perform_layout)
      : instance_(std::move(instance)), width_(width), height_(height) {
    if (!instance_ || !plot) {
      throw std::invalid_argument("Flex plot requires an Instance and Group");
    }
    if (!std::isfinite(width_) || !std::isfinite(height_) || width_ <= 0.0f ||
        height_ <= 0.0f) {
      throw std::invalid_argument("Flex plot dimensions must be finite and positive");
    }
    plot->set_layout_size(width_, height_);
    instance_->scene()->root()->add_child(plot);
    if (perform_layout) {
      instance_->scene()->root()->perform_layout();
    }
  }

  void emit_render_commands(const flexUI::Element&,
                            flexUI::RenderCommandList& commands) override {
    CommandListRenderer renderer(commands, width_, height_);
    instance_->render(renderer);
  }

  void update(float delta_ms, flexUI::Element& element) override {
    instance_->advance(delta_ms / 1000.0f);
    element.mark_paint_dirty();
  }

  bool measure_intrinsic_size(const flexUI::Element&, float, float,
                              float& out_width,
                              float& out_height) const override {
    out_width = width_;
    out_height = height_;
    return true;
  }

  const char* type_name() const override { return "FlexNodePlotWidget"; }

 private:
  std::shared_ptr<flex::Instance> instance_;
  float width_;
  float height_;
};

}  // namespace

std::unique_ptr<flexUI::Widget> create_flex_node_plot_widget(
    std::shared_ptr<flex::Instance> instance, flex::Group* plot, float width,
    float height, bool perform_layout) {
  return std::make_unique<FlexNodePlotWidget>(std::move(instance), plot, width,
                                              height, perform_layout);
}

}  // namespace charts::ui
