#include <tinytest.h>
#undef group

#include <flexUI/render_command.h>
#include <flexUI/renderer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace flexUI;

namespace {

bool approx_eq(float a, float b, float eps = 0.001f) {
  return std::fabs(a - b) <= eps;
}

struct RectCall {
  float x;
  float y;
  float width;
  float height;
  flex::Color fill_color;
};

struct LineCall {
  float x1;
  float y1;
  float x2;
  float y2;
  flex::Color color;
  float width;
};

struct CircleCall {
  float cx;
  float cy;
  float radius;
  flex::Color fill_color;
  flex::Color stroke_color;
  float stroke_width;
};

struct EllipseCall {
  float cx;
  float cy;
  float radius_x;
  float radius_y;
  flex::Color fill_color;
  flex::Color stroke_color;
  float stroke_width;
};

struct TextCall {
  std::string text;
  float x;
  float y;
  flex::Color color;
};

struct ClipCall {
  float x;
  float y;
  float width;
  float height;
};

struct PathCall {
  std::string d;
  flex::Color color;
  float width;
};

struct ImageCall {
  std::string src;
  float x;
  float y;
  float width;
  float height;
};

class RecordingRenderer final : public flex::Renderer {
public:
  void begin_frame(float width, float height, float pixel_ratio) override {
    ++begin_frame_calls;
    viewport_ = {0.0f, 0.0f, width, height};
    frame_pixel_ratio = pixel_ratio;
    tx_ = 0.0f;
    ty_ = 0.0f;
    stack_.clear();
  }

  void end_frame() override { ++end_frame_calls; }
  void set_retained_mode(bool enabled) override { retained_mode_ = enabled; }
  void set_retained_insertion_anchor(flex::PaintHandle before) override {
    insertion_anchors.push_back(before);
  }

  void save() override { stack_.push_back({tx_, ty_}); }
  void restore() override {
    tx_ = stack_.back().first;
    ty_ = stack_.back().second;
    stack_.pop_back();
  }
  void reset() override {
    tx_ = 0.0f;
    ty_ = 0.0f;
  }
  void set_transform(const flex::Transform& transform) override {
    transforms.push_back(transform);
  }
  void translate(float x, float y) override {
    translations.push_back({x, y});
    tx_ += x;
    ty_ += y;
  }
  void rotate(float degrees) override { rotations.push_back(degrees); }
  void scale(float x, float y) override { scales.push_back({x, y}); }

  void clip_rect(float x, float y, float width, float height) override {
    clips.push_back({x + tx_, y + ty_, width, height});
  }
  void reset_clip() override { ++reset_clip_calls; }

  void set_global_alpha(float alpha) override { alphas.push_back(alpha); }
  void set_shadow(const flex::Shadow& shadow) override {
    shadows.push_back(shadow);
  }
  void clear_shadow() override { ++clear_shadow_calls; }
  void set_blur(const flex::BlurFilter& blur) override {
    blurs.push_back(blur);
  }
  void clear_blur() override { ++clear_blur_calls; }

  void fill_path(const std::string& d, const flex::Paint& paint) override {
    fill_paths.push_back({d, paint.color, 0.0f});
  }
  void stroke_path(const std::string& d, const flex::Paint& paint,
                   float width) override {
    stroke_paths.push_back({d, paint.color, width});
  }
  void draw_line(float x1, float y1, float x2, float y2,
                 const flex::Paint& paint, float width) override {
    lines.push_back({x1 + tx_, y1 + ty_, x2 + tx_, y2 + ty_, paint.color,
                     width});
  }

  void draw_rect(float x, float y, float width, float height, float,
                 const flex::Paint& fill, const flex::Paint&, float) override {
    rects.push_back({x + tx_, y + ty_, width, height, fill.color});
  }
  void draw_circle(float cx, float cy, float radius, const flex::Paint& fill,
                   const flex::Paint& stroke, float stroke_width) override {
    circles.push_back({cx + tx_, cy + ty_, radius, fill.color, stroke.color,
                       stroke_width});
  }
  void draw_ellipse(float cx, float cy, float radius_x, float radius_y,
                    const flex::Paint& fill, const flex::Paint& stroke,
                    float stroke_width) override {
    ellipses.push_back({cx + tx_, cy + ty_, radius_x, radius_y, fill.color,
                        stroke.color, stroke_width});
  }
  void draw_text(const std::string& text, float x, float y, const std::string&,
                 float, bool, const flex::Color& color) override {
    texts.push_back({text, x + tx_, y + ty_, color});
  }
  void draw_image(const std::string& src, float x, float y, float width,
                  float height) override {
    images.push_back({src, x + tx_, y + ty_, width, height});
  }
  void draw_svg(const std::string& src, float x, float y, float width,
                float height) override {
    svgs.push_back({src, x + tx_, y + ty_, width, height});
  }
  void draw_svg_data(const std::string& data, float x, float y, float width,
                     float height) override {
    svg_data.push_back({data, x + tx_, y + ty_, width, height});
  }

  void clear(const flex::Color& color) override {
    ++clear_calls;
    clear_color = color;
  }
  flex::Bounds viewport() const override { return viewport_; }
  flex::RendererCapabilities capabilities() const override {
    flex::RendererCapabilities caps;
    caps.retained_mode = retained_capable;
    return caps;
  }

  bool supports_retained_mode() const override { return retained_mode_; }
  void remove_cached(flex::PaintHandle paint) override {
    removed_handles.push_back(paint);
  }
  flex::PaintHandle push_rect(float, float, float, float, float,
                              const flex::Paint&, const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return record_push();
  }
  flex::PaintHandle push_circle(float, float, float, const flex::Paint&,
                                const flex::Paint&, float,
                                const flex::Transform&, float) override {
    return record_push();
  }
  flex::PaintHandle push_ellipse(float, float, float, float,
                                 const flex::Paint&, const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    return record_push();
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
    return record_push();
  }
  flex::PaintHandle push_text(const std::string&, const std::string&, float,
                              bool, const flex::Color&,
                              const flex::Transform&, float) override {
    return record_push();
  }
  flex::PaintHandle push_image(const std::string&, float, float,
                               const flex::Transform&, float) override {
    return record_push();
  }
  flex::PaintHandle push_svg(const std::string&, float, float,
                             const flex::Transform&, float) override {
    return record_push();
  }
  flex::PaintHandle push_svg_data(const std::string&, float, float,
                                  const flex::Transform&, float) override {
    return record_push();
  }
  void update_transform(flex::PaintHandle paint,
                        const flex::Transform& transform) override {
    updated_handles.push_back(paint);
    updated_transforms.push_back(transform);
  }

  bool retained_capable = false;
  int begin_frame_calls = 0;
  int clear_calls = 0;
  int end_frame_calls = 0;
  int reset_clip_calls = 0;
  int clear_shadow_calls = 0;
  int clear_blur_calls = 0;
  float frame_pixel_ratio = 0.0f;
  flex::Color clear_color{};
  std::vector<std::pair<float, float>> translations;
  std::vector<float> rotations;
  std::vector<std::pair<float, float>> scales;
  std::vector<flex::Transform> transforms;
  std::vector<float> alphas;
  std::vector<flex::Shadow> shadows;
  std::vector<flex::BlurFilter> blurs;
  std::vector<ClipCall> clips;
  std::vector<PathCall> fill_paths;
  std::vector<PathCall> stroke_paths;
  std::vector<RectCall> rects;
  std::vector<LineCall> lines;
  std::vector<CircleCall> circles;
  std::vector<EllipseCall> ellipses;
  std::vector<TextCall> texts;
  std::vector<ImageCall> images;
  std::vector<ImageCall> svgs;
  std::vector<ImageCall> svg_data;
  std::vector<flex::PaintHandle> pushed_handles;
  std::vector<flex::PaintHandle> removed_handles;
  std::vector<flex::PaintHandle> updated_handles;
  std::vector<flex::PaintHandle> insertion_anchors;
  std::vector<flex::Transform> updated_transforms;

private:
  flex::PaintHandle record_push() {
    auto handle = reinterpret_cast<flex::PaintHandle>(next_handle_++);
    pushed_handles.push_back(handle);
    return handle;
  }

  float tx_ = 0.0f;
  float ty_ = 0.0f;
  uintptr_t next_handle_ = 1;
  flex::Bounds viewport_{0.0f, 0.0f, 0.0f, 0.0f};
  std::vector<std::pair<float, float>> stack_;
};

void require_color(const flex::Color& color, float r, float g, float b,
                   float a = 1.0f) {
  check(approx_eq(color.r, r));
  check(approx_eq(color.g, g));
  check(approx_eq(color.b, b));
  check(approx_eq(color.a, a));
}

} // namespace

spec("RenderCommandList replays backend-neutral commands") {
  it("forwards frame state and primitive commands to the renderer") {
    RecordingRenderer backend;
    Renderer renderer(&backend);

    RenderCommandList commands(renderer.capabilities());
    commands.begin_frame(RenderViewport{160.0f, 90.0f, 1.5f});
    commands.clear(flex::Color{0.2f, 0.3f, 0.4f, 1.0f});
    commands.save();
    commands.set_transform(flex::make_translation(2.0f, 3.0f));
    commands.translate(8.0f, 6.0f);
    commands.rotate(15.0f);
    commands.scale(2.0f, 3.0f);
    commands.set_global_alpha(0.75f);
    commands.clip_rect(0.0f, 0.0f, 40.0f, 30.0f);
    commands.reset_clip();
    commands.fill_path("M0 0 L4 0 Z",
                       Paint::solid(flex::Color{0.3f, 0.4f, 0.5f, 1.0f}));
    commands.stroke_path("M1 1 L5 1",
                         Paint::solid(flex::Color{0.4f, 0.5f, 0.6f, 1.0f}),
                         1.5f);
    flex::Shadow shadow;
    shadow.offset_x = 1.0f;
    shadow.offset_y = 2.0f;
    shadow.blur = 3.0f;
    shadow.color = flex::Color{0.0f, 0.0f, 0.0f, 0.5f};
    commands.set_shadow(shadow);
    commands.clear_shadow();
    commands.set_blur(flex::BlurFilter(4.0f));
    commands.clear_blur();
    commands.draw_rect(1.0f, 2.0f, 20.0f, 10.0f, 3.0f,
                       Paint::solid(flex::Color{0.5f, 0.6f, 0.7f, 1.0f}),
                       Paint::none(), 0.0f);
    commands.draw_line(0.0f, 1.0f, 12.0f, 1.0f,
                       Paint::solid(flex::Color{0.1f, 0.2f, 0.3f, 1.0f}),
                       2.0f);
    commands.draw_circle(6.0f, 7.0f, 3.0f,
                         Paint::solid(flex::Color{0.2f, 0.3f, 0.4f, 1.0f}),
                         Paint::solid(flex::Color{0.5f, 0.6f, 0.7f, 1.0f}),
                         1.25f);
    commands.draw_ellipse(7.0f, 8.0f, 4.0f, 2.0f,
                          Paint::solid(flex::Color{0.3f, 0.4f, 0.5f, 1.0f}),
                          Paint::solid(flex::Color{0.6f, 0.7f, 0.8f, 1.0f}),
                          1.5f);
    commands.draw_text("ok", 4.0f, 5.0f, "Arial", 12.0f, false,
                       flex::Color{0.8f, 0.9f, 1.0f, 1.0f});
    commands.draw_image("image.png", 2.0f, 3.0f, 16.0f, 12.0f);
    commands.draw_svg("icon.svg", 3.0f, 4.0f, 10.0f, 8.0f);
    commands.draw_svg_data("<svg/>", 4.0f, 5.0f, 6.0f, 7.0f);
    commands.restore();
    commands.end_frame();

    commands.replay(renderer);

    check(commands.commands().size() == 26);
    check(backend.begin_frame_calls == 1);
    check(backend.clear_calls == 1);
    check(backend.end_frame_calls == 1);
    check(approx_eq(backend.frame_pixel_ratio, 1.5f));
    require_color(backend.clear_color, 0.2f, 0.3f, 0.4f);

    check(backend.clips.size() == 1);
    check(approx_eq(backend.clips[0].x, 8.0f));
    check(approx_eq(backend.clips[0].y, 6.0f));
    check(backend.reset_clip_calls == 1);

    check(backend.rotations.size() == 1);
    check(approx_eq(backend.rotations[0], 15.0f));
    check(backend.scales.size() == 1);
    check(approx_eq(backend.scales[0].first, 2.0f));
    check(approx_eq(backend.scales[0].second, 3.0f));
    check(backend.transforms.size() == 1);
    check(approx_eq(flex::tx(backend.transforms[0]), 2.0f));
    check(approx_eq(flex::ty(backend.transforms[0]), 3.0f));
    check(backend.alphas.size() == 1);
    check(approx_eq(backend.alphas[0], 0.75f));

    check(backend.fill_paths.size() == 1);
    check(backend.fill_paths[0].d == "M0 0 L4 0 Z");
    require_color(backend.fill_paths[0].color, 0.3f, 0.4f, 0.5f);
    check(backend.stroke_paths.size() == 1);
    check(backend.stroke_paths[0].d == "M1 1 L5 1");
    check(approx_eq(backend.stroke_paths[0].width, 1.5f));
    require_color(backend.stroke_paths[0].color, 0.4f, 0.5f, 0.6f);

    check(backend.shadows.size() == 1);
    check(approx_eq(backend.shadows[0].offset_x, 1.0f));
    check(approx_eq(backend.shadows[0].offset_y, 2.0f));
    check(approx_eq(backend.shadows[0].blur, 3.0f));
    check(backend.clear_shadow_calls == 1);
    check(backend.blurs.size() == 1);
    check(approx_eq(backend.blurs[0].radius, 4.0f));
    check(backend.clear_blur_calls == 1);

    check(backend.rects.size() == 1);
    check(approx_eq(backend.rects[0].x, 9.0f));
    check(approx_eq(backend.rects[0].y, 8.0f));
    require_color(backend.rects[0].fill_color, 0.5f, 0.6f, 0.7f);

    check(backend.lines.size() == 1);
    check(approx_eq(backend.lines[0].x1, 8.0f));
    check(approx_eq(backend.lines[0].y1, 7.0f));
    check(approx_eq(backend.lines[0].x2, 20.0f));
    check(approx_eq(backend.lines[0].y2, 7.0f));
    check(approx_eq(backend.lines[0].width, 2.0f));
    require_color(backend.lines[0].color, 0.1f, 0.2f, 0.3f);

    check(backend.circles.size() == 1);
    check(approx_eq(backend.circles[0].cx, 14.0f));
    check(approx_eq(backend.circles[0].cy, 13.0f));
    check(approx_eq(backend.circles[0].radius, 3.0f));
    check(approx_eq(backend.circles[0].stroke_width, 1.25f));
    require_color(backend.circles[0].fill_color, 0.2f, 0.3f, 0.4f);
    require_color(backend.circles[0].stroke_color, 0.5f, 0.6f, 0.7f);

    check(backend.ellipses.size() == 1);
    check(approx_eq(backend.ellipses[0].cx, 15.0f));
    check(approx_eq(backend.ellipses[0].cy, 14.0f));
    check(approx_eq(backend.ellipses[0].radius_x, 4.0f));
    check(approx_eq(backend.ellipses[0].radius_y, 2.0f));
    check(approx_eq(backend.ellipses[0].stroke_width, 1.5f));
    require_color(backend.ellipses[0].fill_color, 0.3f, 0.4f, 0.5f);
    require_color(backend.ellipses[0].stroke_color, 0.6f, 0.7f, 0.8f);

    check(backend.texts.size() == 1);
    check(backend.texts[0].text == "ok");
    check(approx_eq(backend.texts[0].x, 12.0f));
    check(approx_eq(backend.texts[0].y, 11.0f));
    require_color(backend.texts[0].color, 0.8f, 0.9f, 1.0f);

    check(backend.images.size() == 1);
    check(backend.images[0].src == "image.png");
    check(approx_eq(backend.images[0].x, 10.0f));
    check(approx_eq(backend.images[0].y, 9.0f));
    check(backend.svgs.size() == 1);
    check(backend.svgs[0].src == "icon.svg");
    check(approx_eq(backend.svgs[0].x, 11.0f));
    check(approx_eq(backend.svgs[0].y, 10.0f));
    check(backend.svg_data.size() == 1);
    check(backend.svg_data[0].src == "<svg/>");
    check(approx_eq(backend.svg_data[0].x, 12.0f));
    check(approx_eq(backend.svg_data[0].y, 11.0f));
  }

  it("drops no-op transform state commands") {
    RecordingRenderer backend;
    Renderer renderer(&backend);

    RenderCommandList commands(renderer.capabilities());
    commands.translate(0.0f, 0.0f);
    commands.rotate(0.0f);
    commands.scale(1.0f, 1.0f);
    commands.draw_rect(0.0f, 0.0f, 8.0f, 8.0f, 0.0f,
                       Paint::solid(flex::Color{0.0f, 1.0f, 0.0f, 1.0f}),
                       Paint::none(), 0.0f);

    commands.replay(renderer);

    check(commands.commands().size() == 1);
    check(backend.translations.empty());
    check(backend.rotations.empty());
    check(backend.scales.empty());
    check(backend.rects.size() == 1);
  }

  it("drops invisible and degenerate drawing commands before replay") {
    RecordingRenderer backend;
    Renderer renderer(&backend);

    RenderCommandList commands(renderer.capabilities());
    commands.fill_path("", Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}));
    commands.fill_path("M0 0 L1 1", Paint::none());
    commands.fill_path("M0 0 L1 1",
                       Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 0.0f}));
    commands.stroke_path("M0 0 L1 1",
                         Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                         0.0f);
    commands.draw_rect(0.0f, 0.0f, 0.0f, 10.0f, 0.0f,
                       Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                       Paint::none(), 0.0f);
    commands.draw_rect(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                       Paint::none(), Paint::none(), 0.0f);
    commands.draw_line(1.0f, 1.0f, 1.0f, 1.0f,
                       Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                       1.0f);
    commands.draw_circle(0.0f, 0.0f, 0.0f,
                         Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                         Paint::none(), 0.0f);
    commands.draw_ellipse(0.0f, 0.0f, 4.0f, 0.0f,
                          Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                          Paint::none(), 0.0f);
    commands.draw_text("", 0.0f, 0.0f, "Arial", 12.0f, false,
                       flex::Color{1.0f, 1.0f, 1.0f, 1.0f});
    commands.draw_text("hidden", 0.0f, 0.0f, "Arial", 12.0f, false,
                       flex::Color{1.0f, 1.0f, 1.0f, 0.0f});
    commands.draw_image("", 0.0f, 0.0f, 10.0f, 10.0f);
    commands.draw_svg("icon.svg", 0.0f, 0.0f, 0.0f, 10.0f);
    commands.draw_svg_data("<svg/>", 0.0f, 0.0f, 10.0f, 0.0f);

    commands.draw_rect(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                       Paint::solid(flex::Color{0.0f, 1.0f, 0.0f, 1.0f}),
                       Paint::none(), 0.0f);
    commands.draw_text("ok", 0.0f, 0.0f, "Arial", 12.0f, false,
                       flex::Color{1.0f, 1.0f, 1.0f, 1.0f});

    commands.replay(renderer);

    check(commands.commands().size() == 2);
    check(backend.rects.size() == 1);
    check(backend.texts.size() == 1);
    check(backend.fill_paths.empty());
    check(backend.stroke_paths.empty());
    check(backend.lines.empty());
    check(backend.circles.empty());
    check(backend.ellipses.empty());
    check(backend.images.empty());
    check(backend.svgs.empty());
    check(backend.svg_data.empty());
  }

  it("composes set_transform with a command-list transform prefix") {
    RecordingRenderer backend;
    Renderer renderer(&backend);

    RenderCommandList commands(
        flex::RendererCapabilities{},
        flex::make_translation(20.0f, 30.0f));
    commands.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    commands.save();
    commands.translate(5.0f, 6.0f);
    commands.set_transform(flex::make_translation(2.0f, 3.0f));
    commands.draw_rect(1.0f, 2.0f, 10.0f, 8.0f, 0.0f,
                       Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                       Paint::none(), 0.0f);
    commands.restore();
    commands.end_frame();

    commands.replay(renderer);

    check(backend.transforms.size() == 1);
    check(approx_eq(flex::tx(backend.transforms[0]), 22.0f));
    check(approx_eq(flex::ty(backend.transforms[0]), 33.0f));
    check(backend.rects.size() == 1);
    check(approx_eq(backend.rects[0].x, 6.0f));
    check(approx_eq(backend.rects[0].y, 8.0f));
  }

  it("scopes nested transform prefixes for embedded command emitters") {
    RecordingRenderer backend;
    Renderer renderer(&backend);

    RenderCommandList commands(flex::RendererCapabilities{},
                               flex::make_translation(10.0f, 20.0f));
    commands.push_transform_prefix(flex::make_translation(3.0f, 4.0f));
    commands.set_transform(flex::make_translation(1.0f, 2.0f));
    commands.pop_transform_prefix();
    commands.set_transform(flex::make_translation(5.0f, 6.0f));

    commands.replay(renderer);

    check(backend.transforms.size() == 2);
    if (backend.transforms.size() == 2) {
      check(approx_eq(flex::tx(backend.transforms[0]), 14.0f));
      check(approx_eq(flex::ty(backend.transforms[0]), 26.0f));
      check(approx_eq(flex::tx(backend.transforms[1]), 15.0f));
      check(approx_eq(flex::ty(backend.transforms[1]), 26.0f));
    }
  }

  it("can be reset and reused") {
    RecordingRenderer backend;
    Renderer renderer(&backend);

    RenderCommandList commands(flex::RendererCapabilities{},
                               flex::make_translation(7.0f, 8.0f));
    commands.clear(flex::Color{1.0f, 0.0f, 0.0f, 1.0f});
    commands.push_transform_prefix(flex::make_translation(30.0f, 40.0f));
    check(commands.commands().size() == 1);

    commands.reset();
    check(commands.commands().empty());

    commands.set_transform(flex::make_translation(1.0f, 2.0f));
    commands.end_frame();
    check(commands.commands().size() == 2);
    commands.replay(renderer);

    check(backend.transforms.size() == 1);
    if (backend.transforms.size() == 1) {
      check(approx_eq(flex::tx(backend.transforms[0]), 8.0f));
      check(approx_eq(flex::ty(backend.transforms[0]), 10.0f));
    }
  }

  it("produces stable signatures for identical command streams") {
    RecordingRenderer backend;
    Renderer renderer(&backend);

    RenderCommandList first(renderer.capabilities());
    first.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    first.clear(flex::Color{0.1f, 0.2f, 0.3f, 1.0f});
    first.draw_rect(1.0f, 2.0f, 20.0f, 10.0f, 3.0f,
                    Paint::solid(flex::Color{0.5f, 0.6f, 0.7f, 1.0f}),
                    Paint::none(), 0.0f);
    first.end_frame();

    RenderCommandList second(renderer.capabilities());
    second.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    second.clear(flex::Color{0.1f, 0.2f, 0.3f, 1.0f});
    second.draw_rect(1.0f, 2.0f, 20.0f, 10.0f, 3.0f,
                     Paint::solid(flex::Color{0.5f, 0.6f, 0.7f, 1.0f}),
                     Paint::none(), 0.0f);
    second.end_frame();

    RenderCommandList changed(renderer.capabilities());
    changed.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    changed.clear(flex::Color{0.1f, 0.2f, 0.3f, 1.0f});
    changed.draw_rect(1.0f, 2.0f, 21.0f, 10.0f, 3.0f,
                      Paint::solid(flex::Color{0.5f, 0.6f, 0.7f, 1.0f}),
                      Paint::none(), 0.0f);
    changed.end_frame();

    check(first.signature() == second.signature());
    check(first.signature() != changed.signature());
  }

  it("reuses retained command handles across identical content") {
    RecordingRenderer backend;
    backend.retained_capable = true;
    Renderer renderer(&backend);
    RenderCommandCache cache;

    RenderCommandList first(renderer.capabilities());
    first.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    first.clear(flex::Color{0.1f, 0.2f, 0.3f, 1.0f});
    first.set_transform(flex::make_translation(0.0f, 0.0f));
    first.draw_rect(1.0f, 2.0f, 20.0f, 10.0f, 3.0f,
                    Paint::solid(flex::Color{0.5f, 0.6f, 0.7f, 1.0f}),
                    Paint::none(), 0.0f);
    first.draw_text("ok", 4.0f, 5.0f, "Arial", 12.0f, false,
                    flex::Color{0.8f, 0.9f, 1.0f, 1.0f});
    first.end_frame();
    first.replay_retained(renderer, cache);

    check(backend.pushed_handles.size() == 2);
    check(backend.updated_handles.empty());
    check(backend.removed_handles.empty());

    first.replay_retained(renderer, cache);

    check(backend.pushed_handles.size() == 2);
    check(backend.updated_handles.empty());
    check(backend.removed_handles.empty());

    RenderCommandList moved(renderer.capabilities());
    moved.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    moved.clear(flex::Color{0.1f, 0.2f, 0.3f, 1.0f});
    moved.set_transform(flex::make_translation(10.0f, 0.0f));
    moved.draw_rect(1.0f, 2.0f, 20.0f, 10.0f, 3.0f,
                    Paint::solid(flex::Color{0.5f, 0.6f, 0.7f, 1.0f}),
                    Paint::none(), 0.0f);
    moved.draw_text("ok", 4.0f, 5.0f, "Arial", 12.0f, false,
                    flex::Color{0.8f, 0.9f, 1.0f, 1.0f});
    moved.end_frame();
    moved.replay_retained(renderer, cache);

    check(backend.pushed_handles.size() == 2);
    check(backend.updated_handles.size() == 2);
    check(backend.removed_handles.empty());
    if (backend.updated_transforms.size() == 2) {
      check(approx_eq(flex::tx(backend.updated_transforms[0]), 10.0f));
      check(approx_eq(flex::tx(backend.updated_transforms[1]), 14.0f));
    }
  }

  it("rebuilds only the changed retained slot when command count is stable") {
    RecordingRenderer backend;
    backend.retained_capable = true;
    Renderer renderer(&backend);
    RenderCommandCache cache;

    RenderCommandList first(renderer.capabilities());
    first.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    first.clear(flex::Color{0.1f, 0.2f, 0.3f, 1.0f});
    first.draw_rect(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                    Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                    Paint::none(), 0.0f);
    first.draw_rect(20.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                    Paint::solid(flex::Color{0.0f, 1.0f, 0.0f, 1.0f}),
                    Paint::none(), 0.0f);
    first.draw_rect(40.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                    Paint::solid(flex::Color{0.0f, 0.0f, 1.0f, 1.0f}),
                    Paint::none(), 0.0f);
    first.end_frame();
    first.replay_retained(renderer, cache);

    check(backend.pushed_handles.size() == 3);
    if (backend.pushed_handles.size() != 3) {
      return;
    }
    const auto first_handle = backend.pushed_handles[0];
    const auto changed_handle = backend.pushed_handles[1];
    const auto suffix_handle = backend.pushed_handles[2];

    RenderCommandList changed(renderer.capabilities());
    changed.begin_frame(RenderViewport{160.0f, 90.0f, 1.0f});
    changed.clear(flex::Color{0.1f, 0.2f, 0.3f, 1.0f});
    changed.draw_rect(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                      Paint::solid(flex::Color{1.0f, 0.0f, 0.0f, 1.0f}),
                      Paint::none(), 0.0f);
    changed.draw_rect(20.0f, 0.0f, 12.0f, 10.0f, 0.0f,
                      Paint::solid(flex::Color{0.0f, 1.0f, 0.0f, 1.0f}),
                      Paint::none(), 0.0f);
    changed.draw_rect(40.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                      Paint::solid(flex::Color{0.0f, 0.0f, 1.0f, 1.0f}),
                      Paint::none(), 0.0f);
    changed.end_frame();
    changed.replay_retained(renderer, cache);

    check(backend.pushed_handles.size() == 4);
    check(backend.removed_handles.size() == 1);
    check(backend.updated_handles.empty());
    if (backend.removed_handles.size() == 1) {
      check(backend.removed_handles[0] == changed_handle);
    }
    check(backend.insertion_anchors.size() == 1);
    if (backend.insertion_anchors.size() == 1) {
      check(backend.insertion_anchors[0] == suffix_handle);
    }
    check(std::find(backend.removed_handles.begin(),
                    backend.removed_handles.end(),
                    first_handle) == backend.removed_handles.end());
    check(std::find(backend.removed_handles.begin(),
                    backend.removed_handles.end(),
                    suffix_handle) == backend.removed_handles.end());
  }
}
