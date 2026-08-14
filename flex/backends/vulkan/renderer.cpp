#include "backends/vulkan/init.h"

#include "flex/render/engines/gcanvas.h"
#include <gcanvas/backends/vulkan.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace flex {
namespace vulkan_backend {

bool is_supported_format(VkFormat format) noexcept {
  return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB ||
         format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;
}

bool is_valid_canvas(const VulkanCanvas &canvas) noexcept {
  return canvas.instance != VK_NULL_HANDLE && canvas.physical_device != VK_NULL_HANDLE &&
         canvas.device != VK_NULL_HANDLE && canvas.queue != VK_NULL_HANDLE &&
         canvas.queue_family_index != VK_QUEUE_FAMILY_IGNORED &&
         canvas.command_buffer != VK_NULL_HANDLE && canvas.image != VK_NULL_HANDLE &&
         canvas.extent.width > 0 && canvas.extent.height > 0 &&
         canvas.extent.width <= static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) &&
         canvas.extent.height <= static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) &&
         is_supported_format(canvas.format) && canvas.final_layout != VK_IMAGE_LAYOUT_UNDEFINED &&
         canvas.final_stage_mask != 0;
}

namespace {

class VulkanRenderer final : public Renderer {
public:
  explicit VulkanRenderer(VulkanCanvas *canvas) : canvas_(canvas) {}

  ~VulkanRenderer() override = default;

  VulkanRenderer(const VulkanRenderer &) = delete;
  VulkanRenderer &operator=(const VulkanRenderer &) = delete;

  bool initialize() {
    if (!canvas_ || !is_valid_canvas(*canvas_)) {
      return false;
    }
    target_ = to_external_target(*canvas_);
    ::gcanvas::vulkan::ExternalCreateInfo create_info;
    create_info.metrics = {static_cast<int>(canvas_->extent.width),
                           static_cast<int>(canvas_->extent.height), 1.0f, 1.0f, 1.0f};
    create_info.target = &target_;
    context_ = ::gcanvas::vulkan::create_external_context(create_info);
    if (!context_) {
      return false;
    }
    delegate_ = render::engines::gcanvas::create_renderer(*context_);
    return delegate_ != nullptr;
  }

  void begin_frame(float width, float height, float pixel_ratio) override {
    if (frame_active_) {
      throw std::logic_error("Flex Vulkan begin_frame called before the previous frame ended");
    }
    validate_live_canvas();
    const auto frame_width = rounded_dimension(width);
    const auto frame_height = rounded_dimension(height);
    if (frame_width != canvas_->extent.width || frame_height != canvas_->extent.height) {
      throw std::invalid_argument("Flex Vulkan frame dimensions must match VulkanCanvas::extent");
    }
    sync_target();
    delegate_->begin_frame(width, height, pixel_ratio);
    frame_active_ = true;
  }

  void end_frame() override {
    if (!frame_active_) {
      throw std::logic_error("Flex Vulkan end_frame called without a matching begin_frame");
    }
    validate_live_canvas();
    if (canvas_->command_buffer != target_.command_buffer || canvas_->image != target_.image) {
      throw std::invalid_argument(
          "Flex Vulkan image and command buffer cannot change during an active frame");
    }
    delegate_->end_frame();
    canvas_->image_layout = target_.image_layout;
    frame_active_ = false;
  }

  bool requires_surface_recreation() const override {
    return !canvas_ || canvas_->instance != target_.instance ||
           canvas_->device != target_.device ||
           canvas_->physical_device != target_.physical_device ||
           canvas_->queue != target_.queue ||
           canvas_->queue_family_index != target_.queue_family_index ||
           canvas_->extent.width != target_.extent.width ||
           canvas_->extent.height != target_.extent.height ||
           canvas_->format != target_.format ||
           delegate_->requires_surface_recreation();
  }

  void acknowledge_surface_recreation() override { delegate_->acknowledge_surface_recreation(); }

  void set_retained_mode(bool enabled) override {
    delegate_->set_retained_mode(enabled);
  }

  void save() override { delegate_->save(); }
  void restore() override { delegate_->restore(); }
  void reset() override { delegate_->reset(); }
  void set_transform(const Transform &transform) override { delegate_->set_transform(transform); }
  void translate(float x, float y) override { delegate_->translate(x, y); }
  void rotate(float degrees) override { delegate_->rotate(degrees); }
  void scale(float sx, float sy) override { delegate_->scale(sx, sy); }
  void clip_rect(float x, float y, float w, float h) override { delegate_->clip_rect(x, y, w, h); }
  void reset_clip() override { delegate_->reset_clip(); }
  void set_global_alpha(float alpha) override { delegate_->set_global_alpha(alpha); }
  void set_shadow(const Shadow &shadow) override { delegate_->set_shadow(shadow); }
  void clear_shadow() override { delegate_->clear_shadow(); }
  void set_blur(const BlurFilter &blur) override { delegate_->set_blur(blur); }
  void clear_blur() override { delegate_->clear_blur(); }

  void fill_path(const std::string &d, const Paint &paint) override {
    delegate_->fill_path(d, paint);
  }
  void stroke_path(const std::string &d, const Paint &paint, float width) override {
    delegate_->stroke_path(d, paint, width);
  }
  void draw_line(float x1, float y1, float x2, float y2, const Paint &paint, float width) override {
    delegate_->draw_line(x1, y1, x2, y2, paint, width);
  }
  void draw_rect(float x, float y, float w, float h, float r, const Paint &fill,
                 const Paint &stroke, float stroke_width) override {
    delegate_->draw_rect(x, y, w, h, r, fill, stroke, stroke_width);
  }
  void draw_circle(float cx, float cy, float r, const Paint &fill, const Paint &stroke,
                   float stroke_width) override {
    delegate_->draw_circle(cx, cy, r, fill, stroke, stroke_width);
  }
  void draw_ellipse(float cx, float cy, float rx, float ry, const Paint &fill, const Paint &stroke,
                    float stroke_width) override {
    delegate_->draw_ellipse(cx, cy, rx, ry, fill, stroke, stroke_width);
  }
  void draw_text(const std::string &text, float x, float y, const std::string &font_family,
                 float font_size, bool bold, const Color &color) override {
    delegate_->draw_text(text, x, y, font_family, font_size, bold, color);
  }
  bool register_font(const std::string &family, const std::string &path) override {
    return delegate_->register_font(family, path);
  }
  void unregister_font(const std::string &family) override { delegate_->unregister_font(family); }
  void draw_image(const std::string &src, float x, float y, float width, float height) override {
    delegate_->draw_image(src, x, y, width, height);
  }
  void draw_svg(const std::string &src, float x, float y, float width, float height) override {
    delegate_->draw_svg(src, x, y, width, height);
  }
  void draw_svg_data(const std::string &data, float x, float y, float width,
                     float height) override {
    delegate_->draw_svg_data(data, x, y, width, height);
  }
  void clear(const Color &color) override { delegate_->clear(color); }
  Bounds viewport() const override { return delegate_->viewport(); }

  RendererCapabilities capabilities() const override {
    auto result = delegate_->capabilities();
    result.surface_recreation = false;
    return result;
  }

  bool supports_retained_mode() const override { return delegate_->supports_retained_mode(); }
  void set_retained_insertion_anchor(PaintHandle before) override {
    delegate_->set_retained_insertion_anchor(before);
  }
  void remove_cached(PaintHandle paint) override { delegate_->remove_cached(paint); }
  PaintHandle push_rect(float x, float y, float w, float h, float r, const Paint &fill,
                        const Paint &stroke, float stroke_width, const Transform &transform,
                        float alpha) override {
    return delegate_->push_rect(x, y, w, h, r, fill, stroke, stroke_width, transform, alpha);
  }
  PaintHandle push_circle(float cx, float cy, float r, const Paint &fill, const Paint &stroke,
                          float stroke_width, const Transform &transform, float alpha) override {
    return delegate_->push_circle(cx, cy, r, fill, stroke, stroke_width, transform, alpha);
  }
  PaintHandle push_ellipse(float cx, float cy, float rx, float ry, const Paint &fill,
                           const Paint &stroke, float stroke_width, const Transform &transform,
                           float alpha) override {
    return delegate_->push_ellipse(cx, cy, rx, ry, fill, stroke, stroke_width, transform, alpha);
  }
  PaintHandle push_polygon(int sides, float radius, const Paint &fill, const Paint &stroke,
                           float stroke_width, const Transform &transform, float alpha) override {
    return delegate_->push_polygon(sides, radius, fill, stroke, stroke_width, transform, alpha);
  }
  PaintHandle push_star(int points, float outer_radius, float inner_radius, const Paint &fill,
                        const Paint &stroke, float stroke_width, const Transform &transform,
                        float alpha) override {
    return delegate_->push_star(points, outer_radius, inner_radius, fill, stroke, stroke_width,
                                transform, alpha);
  }
  PaintHandle push_path(const std::string &d, const Paint &fill, const Paint &stroke,
                        float stroke_width, const Transform &transform, float alpha) override {
    return delegate_->push_path(d, fill, stroke, stroke_width, transform, alpha);
  }
  PaintHandle push_text(const std::string &text, const std::string &font_family, float font_size,
                        bool bold, const Color &color, const Transform &transform,
                        float alpha) override {
    return delegate_->push_text(text, font_family, font_size, bold, color, transform, alpha);
  }
  PaintHandle push_image(const std::string &src, float width, float height,
                         const Transform &transform, float alpha) override {
    return delegate_->push_image(src, width, height, transform, alpha);
  }
  PaintHandle push_svg(const std::string &src, float width, float height,
                       const Transform &transform, float alpha) override {
    return delegate_->push_svg(src, width, height, transform, alpha);
  }
  PaintHandle push_svg_data(const std::string &data, float width, float height,
                            const Transform &transform, float alpha) override {
    return delegate_->push_svg_data(data, width, height, transform, alpha);
  }
  void update_transform(PaintHandle paint, const Transform &transform) override {
    delegate_->update_transform(paint, transform);
  }

private:
  static std::uint32_t rounded_dimension(float value) {
    if (!std::isfinite(value) || value <= 0.0f ||
        value > static_cast<float>((std::numeric_limits<std::uint32_t>::max)())) {
      throw std::invalid_argument("Flex Vulkan frame dimension is invalid");
    }
    return static_cast<std::uint32_t>(std::round(static_cast<double>(value)));
  }

  static ::gcanvas::vulkan::ExternalTarget to_external_target(const VulkanCanvas &canvas) {
    ::gcanvas::vulkan::ExternalTarget target;
    target.instance = canvas.instance;
    target.api_version = canvas.api_version;
    target.physical_device = canvas.physical_device;
    target.device = canvas.device;
    target.queue = canvas.queue;
    target.queue_family_index = canvas.queue_family_index;
    target.command_buffer = canvas.command_buffer;
    target.image = canvas.image;
    target.extent = canvas.extent;
    target.format = canvas.format;
    target.image_layout = canvas.image_layout;
    target.final_layout = canvas.final_layout;
    target.final_stage_mask = canvas.final_stage_mask;
    target.final_access_mask = canvas.final_access_mask;
    return target;
  }

  void validate_live_canvas() const {
    if (!canvas_ || !is_valid_canvas(*canvas_)) {
      throw std::invalid_argument("Flex Vulkan canvas is incomplete or has an unsupported format");
    }
    if (canvas_->instance != target_.instance || canvas_->device != target_.device ||
        canvas_->physical_device != target_.physical_device || canvas_->queue != target_.queue ||
        canvas_->queue_family_index != target_.queue_family_index ||
        canvas_->extent.width != target_.extent.width ||
        canvas_->extent.height != target_.extent.height || canvas_->format != target_.format) {
      throw std::invalid_argument(
          "Flex Vulkan device, queue, extent and format cannot change during renderer lifetime");
    }
  }

  void sync_target() {
    target_.command_buffer = canvas_->command_buffer;
    target_.image = canvas_->image;
    target_.image_layout = canvas_->image_layout;
    target_.final_layout = canvas_->final_layout;
    target_.final_stage_mask = canvas_->final_stage_mask;
    target_.final_access_mask = canvas_->final_access_mask;
  }

  VulkanCanvas *canvas_ = nullptr;
  ::gcanvas::vulkan::ExternalTarget target_;
  std::unique_ptr<::gcanvas::Context> context_;
  std::unique_ptr<Renderer> delegate_;
  bool frame_active_ = false;
};

} // namespace
} // namespace vulkan_backend

std::unique_ptr<Renderer> create_vulkan_renderer(CanvasHandle canvas) {
  try {
    auto renderer = std::make_unique<vulkan_backend::VulkanRenderer>(
        static_cast<vulkan_backend::VulkanCanvas *>(canvas));
    if (!renderer->initialize()) {
      return nullptr;
    }
    return renderer;
  } catch (const std::exception &) {
    return nullptr;
  }
}

} // namespace flex
