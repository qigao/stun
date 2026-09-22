#include "flex/render/engines/gcanvas.h"

#include <gcanvas/context.hpp>
#include <tinytest.hpp>

#include <stdexcept>
#include <string>
#include <memory>
#include <limits>
#include <vector>

namespace {

struct RectCall {
  float x;
  float y;
  float width;
  float height;
  float radius;
  bool stroke;
};

struct ShadowCall {
  enum class Type { Rect, RoundedRect, Circle };

  Type type;
  float x;
  float y;
  float width;
  float height;
  float radius;
  float blur;
  float alpha;
};

class RecordingImage final : public gcanvas::Image {
public:
  RecordingImage(int width, int height, int components, const unsigned char* data,
                 std::size_t size) {
    _width = width;
    _height = height;
    _components = components;
    assign_pixels(data, size);
  }
};

class RecordingFont final : public gcanvas::Font {
public:
  explicit RecordingFont(std::string source) : source_(std::move(source)) {}
  const std::string& source() const noexcept { return source_; }

protected:
  std::unique_ptr<gcanvas::Image> create_atlas(int, int, int,
                                               const unsigned char*) override {
    return nullptr;
  }

private:
  std::string source_;
};

class RecordingContext final : public gcanvas::Context {
public:
  RecordingContext(int width, int height,
                   gcanvas::ResourceLimits limits = {})
      : Context(gcanvas::CanvasMetrics{width, height}, limits) {}

  gcanvas::Backend backend() const noexcept override { return gcanvas::Backend::OpenGL; }
  int font_size() const noexcept { return _font_size; }
  std::string selected_font_source() const {
    return _font ? static_cast<RecordingFont*>(_font)->source() : std::string{};
  }
  gcanvas::Image& cache_gradient(const gcanvas::Paint& paint,
                                 gcanvas::vec2 minimum, gcanvas::vec2 maximum) {
    return acquire_cached_path_paint_texture(paint, minimum, maximum);
  }
  void close_recording_frame() { reset_transient_path_resources(); }

  void stroke_rect(float x, float y, float width, float height) override {
    rects.push_back({x, y, width, height, 0.0f, true});
  }
  void stroke_rounded_rect(float x, float y, float width, float height,
                           float radius) override {
    rects.push_back({x, y, width, height, radius, true});
  }
  void stroke_rounded_rect(float x, float y, float width, float height,
                           float radius_nw, float, float, float) override {
    stroke_rounded_rect(x, y, width, height, radius_nw);
  }
  void stroke_circle(float, float, float) override { ++stroke_circles; }
  void stroke_ellipse(float, float, float, float) override { ++stroke_ellipses; }

  void fill_rect(float x, float y, float width, float height) override {
    rects.push_back({x, y, width, height, 0.0f, false});
  }
  void fill_rounded_rect(float x, float y, float width, float height,
                         float radius) override {
    rects.push_back({x, y, width, height, radius, false});
  }
  void fill_rounded_rect(float x, float y, float width, float height,
                         float radius_nw, float, float, float) override {
    fill_rounded_rect(x, y, width, height, radius_nw);
  }
  void fill_circle(float, float, float) override { ++fill_circles; }
  void fill_ellipse(float, float, float, float) override { ++fill_ellipses; }

  void draw_text(float, float, std::string) override {
    ++texts;
    last_text_alpha = _fill_color.af();
    record_selected_font();
  }
  void draw_text(float, float, std::u32string) override {
    ++texts;
    last_text_alpha = _fill_color.af();
    record_selected_font();
  }
  void draw_text(float, float, std::string, const gcanvas::Transform& transform) override {
    ++texts;
    last_text_alpha = _fill_color.af();
    last_text_transform = transform;
    record_selected_font();
  }
  void draw_text(float, float, std::u32string,
                 const gcanvas::Transform& transform) override {
    ++texts;
    last_text_alpha = _fill_color.af();
    last_text_transform = transform;
    record_selected_font();
  }
  void draw_image(float, float, float, float, gcanvas::Image&, bool tint) override {
    ++images;
    last_image_alpha = _fill_color.af();
    last_image_tint = tint;
  }
  void draw_image(float, float, float, float, gcanvas::Image&,
                  const gcanvas::Transform& transform, bool tint) override {
    ++images;
    last_image_alpha = _fill_color.af();
    last_image_tint = tint;
    last_image_transform = transform;
  }
  void draw_image(float, float, float, float, gcanvas::Image&, float, float, float,
                  float, bool) override {
    ++images;
  }
  void draw_rounded_image(float, float, float, float, gcanvas::Image&, float,
                          bool) override {
    ++images;
  }
  void draw_rounded_image(float, float, float, float, gcanvas::Image&, float, float,
                          float, float, bool) override {
    ++images;
  }
  void draw_rounded_image(float, float, float, float, gcanvas::Image&, float, float,
                          float, float, float, float, float, float, bool) override {
    ++images;
  }

  void draw_circle_shadow(float x, float y, float radius, float blur) override {
    shadows.push_back({ShadowCall::Type::Circle, x, y, radius * 2.0f,
                       radius * 2.0f, radius, blur, _fill_color.af()});
  }
  void draw_rect_shadow(float x, float y, float width, float height,
                        float blur) override {
    shadows.push_back({ShadowCall::Type::Rect, x, y, width, height, 0.0f,
                       blur, _fill_color.af()});
  }
  void draw_rounded_rect_shadow(float x, float y, float width, float height,
                                float radius, float blur) override {
    shadows.push_back({ShadowCall::Type::RoundedRect, x, y, width, height,
                       radius, blur, _fill_color.af()});
  }
  void draw_rounded_rect_shadow(float x, float y, float width, float height,
                                float radius_nw, float, float, float,
                                float blur) override {
    draw_rounded_rect_shadow(x, y, width, height, radius_nw, blur);
  }

  void set_clear_color(gcanvas::color) override { ++clear_colors; }
  void set_rect_mask(float x, float y, float width, float height) override {
    masks.push_back({x, y, width, height});
  }
  void set_convex_mask(const std::vector<gcanvas::vec2>& vertices) override {
    convex_masks.push_back(vertices);
  }
  void remove_rect_mask() override { ++mask_removals; }
  void draw_frame() override {
    ++frames;
    reset_transient_path_resources();
  }
  void present_frame() override { ++presents; }
  std::vector<std::uint8_t> read_pixels() override { return {}; }
  void resize_context(int width, int height) override {
    if (width <= 0 || height <= 0) {
      throw std::invalid_argument("recording context dimensions must be positive");
    }
    _width = width;
    _height = height;
  }
  void set_vsync(bool) override {}

  gcanvas::Image& create_image(const std::string&, gcanvas::ImageConfig) override {
    static constexpr unsigned char white[] = {255, 255, 255, 255};
    return own_image(std::make_unique<RecordingImage>(1, 1, 4, white, sizeof(white)));
  }
  gcanvas::Image& create_image(int width, int height, int components,
                               const unsigned char* data, std::size_t size,
                               gcanvas::ImageConfig) override {
    ++created_images;
    return own_image(std::make_unique<RecordingImage>(width, height, components, data, size));
  }
  gcanvas::Font& create_font(const std::string& source) override {
    return own_font(std::make_unique<RecordingFont>(source));
  }
  gcanvas::Font& create_font(const unsigned char*, std::size_t) override {
    throw std::logic_error("recording context has no fonts");
  }

  std::vector<RectCall> rects;
  std::vector<ShadowCall> shadows;
  std::vector<flex::Bounds> masks;
  std::vector<std::vector<gcanvas::vec2>> convex_masks;
  int fill_circles = 0;
  int stroke_circles = 0;
  int fill_ellipses = 0;
  int stroke_ellipses = 0;
  int texts = 0;
  int images = 0;
  int created_images = 0;
  int updated_images = 0;
  int clear_colors = 0;
  int mask_removals = 0;
  int frames = 0;
  int presents = 0;
  int paths = 0;
  float last_text_alpha = 1.0f;
  gcanvas::Transform last_text_transform;
  float last_image_alpha = 1.0f;
  bool last_image_tint = false;
  gcanvas::Transform last_image_transform;
  int text_masks = 0;
  int image_masks = 0;
  int path_inset_masks = 0;
  int path_eroded_masks = 0;
  int path_blur_samples = 0;
  int text_inset_masks = 0;
  int image_inset_masks = 0;
  int inset_shadows = 0;
  std::string last_font_source;

private:
  void record_selected_font() {
    last_font_source =
        _font ? static_cast<RecordingFont*>(_font)->source() : std::string{};
  }
  void register_image(gcanvas::Image*) override {}
  void register_font(gcanvas::Font*) override {}
  void update_image(gcanvas::Image*) override { ++updated_images; }
  void draw_tessellated_path(const gcanvas::Path&, const gcanvas::Paint&, float,
                             const gcanvas::Transform&) override {
    ++paths;
  }
  void draw_tessellated_path_blur(
      const gcanvas::Path&, const gcanvas::Paint&, float,
      const ShadowKernel& kernel, float, const gcanvas::Transform&) override {
    path_blur_samples += static_cast<int>(kernel.count);
  }
  void draw_text_alpha_mask(float, float, std::u32string, float,
                            const gcanvas::Transform& transform) override {
    ++text_masks;
    last_text_alpha = _fill_color.af();
    last_text_transform = transform;
  }
  void draw_image_alpha_mask(float, float, float, float, gcanvas::Image&,
                             float, const gcanvas::Transform& transform) override {
    ++image_masks;
    last_image_alpha = _fill_color.af();
    last_image_transform = transform;
  }
  void draw_tessellated_path_eroded_shadow(
      const gcanvas::Path&, float, const ShadowKernel& kernel,
      const gcanvas::Transform&) override {
    path_eroded_masks += static_cast<int>(kernel.count);
  }
  void draw_tessellated_path_inset_shadow(
      const gcanvas::Path&, float, float, float, const ShadowKernel& kernel,
      float, const gcanvas::Transform&) override {
    path_inset_masks += static_cast<int>(kernel.count);
  }
  void draw_text_inset_alpha_mask(float, float, std::u32string, float, float, float,
                                  const gcanvas::Transform&) override {
    ++text_inset_masks;
  }
  void draw_image_inset_alpha_mask(float, float, float, float, gcanvas::Image&, float,
                                   float, float, const gcanvas::Transform&) override {
    ++image_inset_masks;
  }
  void draw_inset_shadow(const InsetShadow&) override { ++inset_shadows; }
  void prepare() override {}
};

} // namespace

suite("Flex gCanvas renderer") {
  it("pins bounded gradient cache entries for a frame and reuses them after reset") {
    gcanvas::ResourceLimits limits;
    limits.max_path_surfaces = 1;
    RecordingContext canvas(32, 32, limits);
    const auto first = gcanvas::Paint::linear_gradient(
        0.0f, 0.0f, 1.0f, 0.0f,
        {{0.0f, gcanvas::color(255, 0, 0)}, {1.0f, gcanvas::color(0, 0, 255)}});
    const auto second = gcanvas::Paint::linear_gradient(
        0.0f, 0.0f, 1.0f, 0.0f,
        {{0.0f, gcanvas::color(0, 255, 0)}, {1.0f, gcanvas::color(0, 0, 255)}});

    gcanvas::Image& first_texture =
        canvas.cache_gradient(first, {0.0f, 0.0f}, {1.0f, 1.0f});
    check(&canvas.cache_gradient(first, {0.0f, 0.0f}, {1.0f, 1.0f}) ==
          &first_texture);
    check_throws_as(canvas.cache_gradient(second, {0.0f, 0.0f}, {1.0f, 1.0f}),
                    std::length_error);
    check_equal(canvas.created_images, 1);
    check_equal(canvas.updated_images, 0);

    canvas.close_recording_frame();
    check(&canvas.cache_gradient(second, {0.0f, 0.0f}, {1.0f, 1.0f}) ==
          &first_texture);
    check_equal(canvas.created_images, 1);
    check_equal(canvas.updated_images, 1);
  }

  it("routes axis-aligned solid ellipses through analytic GPU primitives") {
    RecordingContext canvas(64, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(64.0f, 64.0f, 1.0f);
    renderer->draw_ellipse(24.0f, 20.0f, 12.0f, 6.0f,
                           flex::Paint::solid(flex::Color::Green),
                           flex::Paint::solid(flex::Color::White), 2.0f);
    renderer->end_frame();

    check_equal(canvas.fill_ellipses, 1);
    check_equal(canvas.stroke_ellipses, 1);
    check_equal(canvas.paths, 0);
  }

  it("reports the exact supported capability subset") {
    RecordingContext canvas(320, 160);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);
    const auto caps = renderer->capabilities();

    check_false(caps.retained_mode);
    check_false(caps.surface_recreation);
    check_true(caps.path_drawing);
    check_true(caps.raster_images);
    check_true(caps.svg_images);
    check_true(caps.rotation);
    check_true(caps.scaling);
    check_true(caps.shadow);
    check_true(caps.blur);
  }

  it("uses logical metrics without replacing a borrowed physical extent") {
    RecordingContext canvas(120, 80);
    canvas.set_metrics({96, 64, 1.0f, 1.0f, 0.0f, 0.0f, 1.25f});
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    check_equal(canvas.get_width(), 120);
    check_equal(canvas.get_height(), 80);
    check_nothrow(renderer->begin_frame(96.0f, 64.0f, 1.25f));
    renderer->end_frame();
    check_throws_as(renderer->begin_frame(120.0f, 80.0f, 1.0f),
                    std::invalid_argument);
  }

  it("maps bounded blur state to path text image and save restore") {
    RecordingContext canvas(96, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(96.0f, 64.0f, 1.0f);
    check_throws_as(renderer->set_blur(flex::BlurFilter{-1.0f}),
                    std::invalid_argument);
    renderer->set_blur(flex::BlurFilter{4.0f});
    renderer->draw_rect(8.0f, 8.0f, 16.0f, 12.0f, 2.0f,
                        flex::Paint::solid(flex::Color::Red),
                        flex::Paint::none(), 0.0f);
    renderer->draw_text("blur", 32.0f, 20.0f, "sans-serif", 12.0f,
                        false, flex::Color::White);
    renderer->draw_image("blur.png", 48.0f, 8.0f, 12.0f, 12.0f);
    const int blurred_paths = canvas.path_blur_samples;
    const int blurred_texts = canvas.texts;
    const int blurred_images = canvas.images;
    renderer->save();
    renderer->clear_blur();
    renderer->draw_rect(68.0f, 8.0f, 12.0f, 12.0f, 0.0f,
                        flex::Paint::solid(flex::Color::Green),
                        flex::Paint::none(), 0.0f);
    renderer->restore();
    renderer->draw_rect(68.0f, 28.0f, 12.0f, 12.0f, 0.0f,
                        flex::Paint::solid(flex::Color::Blue),
                        flex::Paint::none(), 0.0f);
    renderer->end_frame();

    check_true(blurred_paths > 1);
    check_true(blurred_texts > 1);
    check_true(blurred_images > 1);
    check_equal(canvas.rects.size(), 1);
    check_true(canvas.path_blur_samples > blurred_paths);
  }

  it("measures strict UTF-8 with the same registered font resolution") {
    RecordingContext canvas(96, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    check_true(renderer->register_font("ui", "ui-regular.ttf"));
    check_true(renderer->register_font("ui-bold", "ui-bold.ttf"));

    flex::TextMetrics metrics{123.0f, 456.0f};
    check_true(renderer->measure_text("Hello", "ui", 14.0f, true, metrics));
    check_equal(canvas.selected_font_source(), "ui-bold.ttf");
    check_equal(canvas.font_size(), 14);
    check_true(metrics.width >= 0.0f);
    check_true(metrics.height >= 0.0f);

    flex::TextMetrics unchanged{123.0f, 456.0f};
    check_throws_as(renderer->measure_text(
                        std::string("\xF0\x28\x8C\x28", 4), "ui", 14.0f,
                        false, unchanged),
                    std::range_error);
    check_within(unchanged.width, 123.0f, 0.001f);
    check_within(unchanged.height, 456.0f, 0.001f);
  }

  it("resolves registered bold faces before regular and sans-serif fallbacks") {
    RecordingContext canvas(96, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    check_true(renderer->register_font("ui", "ui-regular.ttf"));
    check_true(renderer->register_font("ui-bold", "ui-bold.ttf"));
    check_true(renderer->register_font("sans-serif", "sans-regular.ttf"));
    check_true(renderer->register_font("sans-serif-bold", "sans-bold.ttf"));

    renderer->begin_frame(96.0f, 64.0f, 1.0f);
    renderer->draw_text("Bold", 0.0f, 0.0f, "ui", 14.0f, true,
                        flex::Color::White);
    check_equal(canvas.last_font_source, "ui-bold.ttf");
    renderer->unregister_font("ui-bold");
    renderer->draw_text("Fallback", 0.0f, 16.0f, "ui", 14.0f, true,
                        flex::Color::White);
    check_equal(canvas.last_font_source, "ui-regular.ttf");
    renderer->draw_text("Sans", 0.0f, 32.0f, "missing", 14.0f, true,
                        flex::Color::White);
    check_equal(canvas.last_font_source, "sans-bold.ttf");
    renderer->end_frame();
  }

  it("fails before queuing blur commands when the configured tap bound is exceeded") {
    gcanvas::ResourceLimits limits;
    limits.max_blur_samples = 1;
    RecordingContext canvas(64, 64, limits);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(64.0f, 64.0f, 1.0f);
    check_throws_as(renderer->set_blur(
                        flex::BlurFilter{(std::numeric_limits<float>::infinity)()}),
                    std::invalid_argument);
    renderer->set_blur(flex::BlurFilter{4.0f});
    check_throws_as(renderer->draw_rect(
                        8.0f, 8.0f, 16.0f, 12.0f, 0.0f,
                        flex::Paint::solid(flex::Color::Red),
                        flex::Paint::none(), 0.0f),
                    std::length_error);
    check_equal(canvas.path_blur_samples, 0);
    renderer->clear_blur();
    renderer->end_frame();
  }

  it("maps nested Flex transforms and clips to immediate gCanvas calls") {
    RecordingContext canvas(320, 160);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(320.0f, 160.0f, 1.0f);
    renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
    renderer->save();
    renderer->translate(10.0f, 20.0f);
    renderer->clip_rect(0.0f, 0.0f, 32.0f, 16.0f);
    renderer->save();
    renderer->translate(4.0f, 4.0f);
    renderer->set_global_alpha(0.5f);
    renderer->draw_rect(0.0f, 0.0f, 20.0f, 10.0f, 2.0f,
                        flex::Paint::solid(flex::Color{1.0f, 0.2f, 0.2f, 1.0f}),
                        flex::Paint::none(), 0.0f);
    renderer->restore();
    renderer->restore();
    renderer->end_frame();

    check(canvas.rects.size() == std::size_t{1});
    check_within(canvas.rects[0].x, 14.0f, 0.001f);
    check_within(canvas.rects[0].y, 24.0f, 0.001f);
    check_within(canvas.rects[0].width, 20.0f, 0.001f);
    check_within(canvas.rects[0].height, 10.0f, 0.001f);
    check_within(canvas.rects[0].radius, 2.0f, 0.001f);
    check_false(canvas.rects[0].stroke);
    check_true(canvas.masks.size() >= std::size_t{1});
    check_within(canvas.masks.front().x, 10.0f, 0.001f);
    check_within(canvas.masks.front().y, 20.0f, 0.001f);
    check_within(canvas.masks.front().width, 32.0f, 0.001f);
    check_within(canvas.masks.front().height, 16.0f, 0.001f);
    check(canvas.clear_colors == 1);
    check(canvas.frames == 1);
    check(canvas.presents == 0);
  }

  it("intersects transformed clips and forwards their convex polygon") {
    RecordingContext canvas(160, 160);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(160.0f, 160.0f, 1.0f);
    renderer->translate(80.0f, 40.0f);
    renderer->rotate(45.0f);
    renderer->clip_rect(-20.0f, -20.0f, 40.0f, 40.0f);
    renderer->save();
    renderer->translate(10.0f, 0.0f);
    renderer->clip_rect(-20.0f, -20.0f, 40.0f, 40.0f);
    renderer->restore();
    renderer->end_frame();

    check_equal(canvas.convex_masks.size(), std::size_t{3});
    check_equal(canvas.convex_masks[0].size(), std::size_t{4});
    check_true(canvas.convex_masks[1].size() >= std::size_t{3});
    check_equal(canvas.convex_masks[2].size(), std::size_t{4});
    check_within(canvas.convex_masks[0][0].get_x(), 80.0f, 0.001f);
    check_within(canvas.convex_masks[0][0].get_y(), 11.7157f, 0.001f);
  }

  it("maps paths gradients ellipses lines and affine transforms") {
    RecordingContext canvas(64, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);
    flex::LinearGradient gradient(0.0f, 0.0f, 0.64f, 0.0f);
    gradient.add_stop(0.0f, flex::Color::Red);
    gradient.add_stop(0.5f, flex::Color::Green);
    gradient.add_stop(1.0f, flex::Color::Blue);

    renderer->begin_frame(64.0f, 64.0f, 1.0f);
    renderer->translate(32.0f, 16.0f);
    renderer->rotate(30.0f);
    check_nothrow(renderer->fill_path("M0 0 L20 0 L10 20 Z", flex::Paint(gradient)));
    check_nothrow(renderer->draw_line(0.0f, 0.0f, 20.0f, 20.0f,
                                      flex::Paint::solid(flex::Color::White), 2.0f));
    check_nothrow(renderer->draw_ellipse(10.0f, 10.0f, 8.0f, 4.0f,
                                         flex::Paint::solid(flex::Color::Yellow),
                                         flex::Paint::none(), 0.0f));
    renderer->end_frame();

    check_equal(canvas.paths, 3);
    check_equal(canvas.frames, 1);
  }

  it("maps drop shadow state, opacity, offset, spread, and save restore") {
    RecordingContext canvas(320, 160);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);
    flex::Shadow shadow = flex::Shadow::drop(
        3.0f, 4.0f, 5.0f, flex::Color{0.0f, 0.0f, 0.0f, 0.5f});
    shadow.spread = 2.0f;

    renderer->begin_frame(320.0f, 160.0f, 1.0f);
    renderer->translate(10.0f, 20.0f);
    renderer->scale(2.0f, 2.0f);
    renderer->set_global_alpha(0.5f);
    renderer->set_shadow(shadow);
    renderer->draw_rect(1.0f, 2.0f, 10.0f, 6.0f, 1.0f,
                        flex::Paint::solid(flex::Color::White), flex::Paint::none(), 0.0f);
    renderer->save();
    renderer->clear_shadow();
    renderer->draw_circle(20.0f, 10.0f, 3.0f,
                          flex::Paint::solid(flex::Color::White), flex::Paint::none(), 0.0f);
    renderer->restore();
    renderer->draw_circle(20.0f, 10.0f, 3.0f,
                          flex::Paint::solid(flex::Color::White), flex::Paint::none(), 0.0f);
    renderer->end_frame();

    check_equal(canvas.shadows.size(), std::size_t{2});
    const ShadowCall& rect = canvas.shadows[0];
    check_true(rect.type == ShadowCall::Type::RoundedRect);
    check_within(rect.x, 14.0f, 0.001f);
    check_within(rect.y, 28.0f, 0.001f);
    check_within(rect.width, 28.0f, 0.001f);
    check_within(rect.height, 20.0f, 0.001f);
    check_within(rect.radius, 6.0f, 0.001f);
    check_within(rect.blur, 10.0f, 0.001f);
    check_within(rect.alpha, 0.25f, 0.001f);

    const ShadowCall& circle = canvas.shadows[1];
    check_true(circle.type == ShadowCall::Type::Circle);
    check_within(circle.x, 56.0f, 0.001f);
    check_within(circle.y, 48.0f, 0.001f);
    check_within(circle.radius, 10.0f, 0.001f);
    check_within(circle.blur, 10.0f, 0.001f);
    check_within(circle.alpha, 0.25f, 0.001f);
  }

  it("supports signed-spread sampled shadows and transformed primitives") {
    RecordingContext canvas(64, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(64.0f, 64.0f, 1.0f);
    flex::Shadow invalid = flex::Shadow::drop(
        0.0f, 0.0f, -1.0f, flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
    check_throws_as(renderer->set_shadow(invalid), std::invalid_argument);
    check_nothrow(renderer->set_shadow(flex::Shadow::inner(
        1.0f, 1.0f, 2.0f, flex::Color{0.0f, 0.0f, 0.0f, 1.0f})));
    check_nothrow(renderer->draw_rect(0.0f, 0.0f, 8.0f, 8.0f, 2.0f,
                                      flex::Paint::solid(flex::Color::White),
                                      flex::Paint::none(), 0.0f));
    check_equal(canvas.inset_shadows, 1);
    check_nothrow(renderer->fill_path("M0 0 L4 0 L0 4 Z",
                                      flex::Paint::solid(flex::Color::White)));
    check_nothrow(renderer->draw_text("inset", 0.0f, 0.0f, "sans-serif", 12.0f,
                                      false, flex::Color::White));
    check_nothrow(renderer->draw_image("image.png", 0.0f, 0.0f, 4.0f, 4.0f));
    check_nothrow(renderer->draw_ellipse(4.0f, 4.0f, 3.0f, 2.0f,
                                         flex::Paint::none(),
                                         flex::Paint::solid(flex::Color::White), 1.0f));
    check(canvas.path_inset_masks > 0);
    check(canvas.text_inset_masks > 0);
    check(canvas.image_inset_masks > 0);

    renderer->set_shadow(flex::Shadow::drop(
        1.0f, 1.0f, 2.0f, flex::Color{0.0f, 0.0f, 0.0f, 1.0f}));
    check_nothrow(renderer->fill_path("M0 0 L4 0 L0 4 Z",
                                      flex::Paint::solid(flex::Color::White)));
    check(canvas.paths > 1);
    flex::Shadow contracted = flex::Shadow::drop(
        1.0f, 1.0f, 2.0f, flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
    contracted.spread = -1.0f;
    renderer->set_shadow(contracted);
    check_nothrow(renderer->fill_path("M0 0 L4 0 L0 4 Z",
                                      flex::Paint::solid(flex::Color::White)));
    check(canvas.path_eroded_masks > 0);
    check_nothrow(renderer->draw_text("contracted", 0.0f, 0.0f, "sans-serif", 12.0f,
                                      false, flex::Color::White));
    check_nothrow(renderer->draw_image("image.png", 0.0f, 0.0f, 4.0f, 4.0f));
    contracted.inset = true;
    renderer->set_shadow(contracted);
    check_nothrow(renderer->fill_path("M0 0 L4 0 L0 4 Z",
                                      flex::Paint::solid(flex::Color::White)));
    check_nothrow(renderer->draw_rect(0.0f, 0.0f, 8.0f, 8.0f, 1.0f,
                                      flex::Paint::none(),
                                      flex::Paint::solid(flex::Color::White), 1.0f));
    check_nothrow(renderer->draw_circle(4.0f, 4.0f, 3.0f, flex::Paint::none(),
                                        flex::Paint::solid(flex::Color::White), 1.0f));
    renderer->set_shadow(flex::Shadow::drop(
        1.0f, 1.0f, 2.0f, flex::Color{0.0f, 0.0f, 0.0f, 1.0f}));
    renderer->rotate(10.0f);
    check_nothrow(renderer->draw_rect(0.0f, 0.0f, 8.0f, 8.0f, 0.0f,
                                      flex::Paint::solid(flex::Color::White),
                                      flex::Paint::none(), 0.0f));
    renderer->end_frame();
  }

  it("maps ellipse text and image shadows through GPU alpha masks") {
    RecordingContext canvas(64, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(64.0f, 64.0f, 1.0f);
    flex::Shadow shadow = flex::Shadow::drop(
        2.0f, 3.0f, 2.0f, flex::Color{0.25f, 0.5f, 0.75f, 0.6f});
    shadow.spread = 1.0f;
    renderer->set_shadow(shadow);
    renderer->draw_ellipse(12.0f, 12.0f, 7.0f, 4.0f,
                           flex::Paint::solid(flex::Color::White),
                           flex::Paint::none(), 0.0f);
    renderer->draw_text("alpha", 2.0f, 20.0f, "sans-serif", 12.0f,
                        false, flex::Color::White);
    renderer->draw_image("image.png", 24.0f, 24.0f, 12.0f, 8.0f);
    renderer->end_frame();

    check(canvas.paths > 1);
    check(canvas.text_masks > 1);
    check(canvas.image_masks > 1);
    check_equal(canvas.texts, 1);
    check_equal(canvas.images, 1);
  }

  it("preserves alpha and still fails fast for unsupported semantics") {
    RecordingContext canvas(320, 160);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    check_throws_as(renderer->set_retained_mode(true), std::logic_error);
    check_throws_as(renderer->begin_frame(100.0f, 100.0f, 1.0f),
                    std::invalid_argument);

    renderer->begin_frame(320.0f, 160.0f, 1.0f);
    renderer->save();
    check_nothrow(renderer->rotate(10.0f));
    renderer->restore();
    check_nothrow(renderer->set_shadow(flex::Shadow{}));
    renderer->set_global_alpha(0.5f);
    check_nothrow(renderer->draw_text("alpha", 0.0f, 0.0f, "sans-serif", 12.0f,
                                      false, flex::Color::White));
    check_nothrow(renderer->draw_image("image.png", 0.0f, 0.0f, 10.0f, 10.0f));
    renderer->draw_rect(0.0f, 0.0f, 10.0f, 10.0f, 0.0f, flex::Paint::none(),
                        flex::Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 0.5f}), 1.0f);
    renderer->end_frame();

    check_equal(canvas.texts, 1);
    check_equal(canvas.images, 1);
    check_within(canvas.last_text_alpha, 0.5f, 0.001f);
    check_within(canvas.last_image_alpha, 0.5f, 0.001f);
    check_true(canvas.last_image_tint);
    check_true(canvas.rects.size() >= std::size_t{1});
    check_true(canvas.rects.back().stroke);
  }

  it("rasterizes inline SVG through PlutoSVG and reuses the gCanvas image") {
    RecordingContext canvas(320, 160);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);
    const std::string svg =
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='4'>"
        "<rect width='8' height='4' fill='#ff0000'/></svg>";

    renderer->begin_frame(320.0f, 160.0f, 1.0f);
    renderer->set_global_alpha(0.5f);
    check_nothrow(renderer->draw_svg_data(svg, 2.0f, 3.0f, 16.0f, 8.0f));
    check_nothrow(renderer->draw_svg_data(svg, 20.0f, 3.0f, 16.0f, 8.0f));
    renderer->end_frame();

    check_equal(canvas.images, 2);
    check_equal(canvas.created_images, 1);
    check_within(canvas.last_image_alpha, 0.5f, 0.001f);
    check_true(canvas.last_image_tint);
  }

  it("forwards affine image transforms without reducing them to axis-aligned bounds") {
    RecordingContext canvas(64, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(64.0f, 64.0f, 1.0f);
    renderer->translate(12.0f, 7.0f);
    renderer->rotate(90.0f);
    check_nothrow(renderer->draw_image("image.png", 0.0f, 0.0f, 20.0f, 8.0f));
    renderer->end_frame();

    check_equal(canvas.images, 1);
    check_within(canvas.last_image_transform.a, 0.0f, 0.001f);
    check_within(canvas.last_image_transform.b, 1.0f, 0.001f);
    check_within(canvas.last_image_transform.c, -1.0f, 0.001f);
    check_within(canvas.last_image_transform.d, 0.0f, 0.001f);
    check_within(canvas.last_image_transform.e, 12.0f, 0.001f);
    check_within(canvas.last_image_transform.f, 7.0f, 0.001f);
  }

  it("forwards affine text transforms without pre-scaling the glyph size") {
    RecordingContext canvas(64, 64);
    auto renderer = flex::render::engines::gcanvas::create_renderer(canvas);

    renderer->begin_frame(64.0f, 64.0f, 1.0f);
    renderer->translate(18.0f, 9.0f);
    renderer->rotate(90.0f);
    renderer->scale(2.0f, 0.5f);
    check_nothrow(renderer->draw_text("GPU", 0.0f, 0.0f, "sans-serif", 16.0f,
                                     false, flex::Color::White));
    renderer->end_frame();

    check_equal(canvas.texts, 1);
    check_equal(canvas.font_size(), 16);
    check_within(canvas.last_text_transform.a, 0.0f, 0.001f);
    check_within(canvas.last_text_transform.b, 2.0f, 0.001f);
    check_within(canvas.last_text_transform.c, -0.5f, 0.001f);
    check_within(canvas.last_text_transform.d, 0.0f, 0.001f);
    check_within(canvas.last_text_transform.e, 18.0f, 0.001f);
    check_within(canvas.last_text_transform.f, 9.0f, 0.001f);
  }
}
