#include <tinytest.h>
#undef group
#include "test_support.h"

#include <flexUI/box.h>
#include <flexUI/render_command.h>
#include <flexUI/render_manager.h>
#include <flexUI/renderer.h>
#include <flexUI/shapes.h>
#include <flexUI/text_layout.h>
#include <flexUI/transition.h>
#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/datepicker_widget.h>
#include <flexUI/widgets/dialog_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/image_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/listview_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/menu_widget.h>
#include <flexUI/widgets/notification_widget.h>
#include <flexUI/widgets/panel_widget.h>
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/widgets/popover_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/radio_widget.h>
#include <flexUI/widgets/scrollview_widget.h>
#include <flexUI/widgets/searchbox_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/sidebar_widget.h>
#include <flexUI/widgets/stepper_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/widgets/timepicker_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/toolbar_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/tree_widget.h>
#include <flexUI/element.h>
#include <flexUI/widget.h>

#include <flex/runtime/renderer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>
#include <vector>

using namespace flexUI;

namespace {

Event ctrl_key(KeyCode key) {
  return Event::key_down(key, static_cast<int>(KeyMod::Control));
}

void render_widget(Widget& widget, const Element& elem, Renderer& renderer) {
  RenderCommandList commands(
      renderer.capabilities(),
      flex::make_translation(elem.absolute_x(), elem.absolute_y()));
  commands.save();
  commands.translate(elem.absolute_x(), elem.absolute_y());
  widget.emit_render_commands(elem, commands);
  commands.restore();
  commands.replay(renderer);
}

void render_widget_overlay(Widget& widget, const Element& elem,
                           Renderer& renderer) {
  RenderCommandList commands(renderer.capabilities());
  widget.emit_overlay_commands(elem, commands);
  commands.replay(renderer);
}

struct DrawRectCall {
  float x;
  float y;
  float w;
  float h;
  flex::Paint::Type fill_type;
  flex::Color fill_color;
  flex::LinearGradient fill_linear;
  flex::RadialGradient fill_radial;
  flex::Color stroke_color;
  float stroke_width;
};

struct DrawCircleCall {
  float cx;
  float cy;
  float radius;
  flex::Paint::Type fill_type;
  flex::Color fill_color;
  flex::Color stroke_color;
  float stroke_width;
};

struct ClipRectCall {
  float x;
  float y;
  float w;
  float h;
};

struct DrawImageCall {
  std::string src;
  float x;
  float y;
  float w;
  float h;
};

struct DrawSvgCall {
  std::string src;
  float x;
  float y;
  float w;
  float h;
};

struct TextCall {
  std::string text;
  std::string font;
  float x;
  float y;
  float size;
  flex::Color color;
  bool bold;
};

struct LineCall {
  float x1;
  float y1;
  float x2;
  float y2;
  flex::Paint::Type paint_type;
  flex::Color color;
  float width;
};

struct FillPathCall {
  std::string d;
  flex::Paint::Type paint_type;
  flex::Color color;
};

struct StrokePathCall {
  std::string d;
  flex::Paint::Type paint_type;
  flex::Color color;
  float width;
};

struct ShadowCall {
  flex::Shadow shadow;
};

class RecordingRenderer final : public flex::Renderer {
public:
  void begin_frame(float width, float height, float pixel_ratio) override {
    ++begin_frame_calls;
    frame_pixel_ratio = pixel_ratio;
    viewport_ = {0.0f, 0.0f, width, height};
    tx_ = 0.0f;
    ty_ = 0.0f;
    save_stack_.clear();
    alphas.clear();
    translations.clear();
  }

  void end_frame() override { ++end_frame_calls; }
  void set_retained_mode(bool enabled) override { retained_mode_ = enabled; }

  void save() override { save_stack_.push_back({tx_, ty_}); }

  void restore() override {
    if (save_stack_.empty()) return;
    tx_ = save_stack_.back().x;
    ty_ = save_stack_.back().y;
    save_stack_.pop_back();
  }

  void reset() override {
    tx_ = 0.0f;
    ty_ = 0.0f;
  }

  void set_transform(const flex::Transform& transform) override {
    transforms.push_back(transform);
    tx_ = flex::tx(transform);
    ty_ = flex::ty(transform);
  }

  void translate(float x, float y) override {
    translations.push_back({x, y});
    tx_ += x;
    ty_ += y;
  }

  void rotate(float degrees) override { rotations.push_back(degrees); }
  void scale(float sx, float sy) override { scales.push_back({sx, sy}); }

  void clip_rect(float x, float y, float w, float h) override {
    clips.push_back({x + tx_, y + ty_, w, h});
  }

  void reset_clip() override {}
  void set_global_alpha(float alpha) override { alphas.push_back(alpha); }
  void set_shadow(const flex::Shadow& shadow) override {
    shadows.push_back({shadow});
  }
  void clear_shadow() override { ++clear_shadow_calls; }
  void set_blur(const flex::BlurFilter& blur) override {
    blur_radii.push_back(blur.radius);
  }
  void clear_blur() override { ++clear_blur_calls; }
  void fill_path(const std::string& d, const flex::Paint& paint) override {
    const auto color =
        paint.type == flex::Paint::Type::Solid ? paint.color : flex::Color{};
    fill_paths.push_back({d, paint.type, color});
  }
  void stroke_path(const std::string& d, const flex::Paint& paint,
                   float width) override {
    const auto color =
        paint.type == flex::Paint::Type::Solid ? paint.color : flex::Color{};
    stroke_paths.push_back({d, paint.type, color, width});
  }
  void draw_line(float x1, float y1, float x2, float y2, const flex::Paint& paint,
                 float width) override {
    const auto color =
        paint.type == flex::Paint::Type::Solid ? paint.color : flex::Color{};
    lines.push_back({x1 + tx_, y1 + ty_, x2 + tx_, y2 + ty_, paint.type, color,
                     width});
  }

  void draw_rect(float x, float y, float w, float h, float,
                 const flex::Paint& fill, const flex::Paint& stroke, float stroke_width) override {
    const auto fill_color =
        fill.type == flex::Paint::Type::Solid ? fill.color : flex::Color{};
    const auto fill_linear =
        fill.type == flex::Paint::Type::Linear ? fill.linear : flex::LinearGradient{};
    const auto fill_radial =
        fill.type == flex::Paint::Type::Radial ? fill.radial : flex::RadialGradient{};
    const auto stroke_color =
        stroke.type == flex::Paint::Type::Solid ? stroke.color : flex::Color{};
    rects.push_back(
        {x + tx_, y + ty_, w, h, fill.type, fill_color, fill_linear, fill_radial, stroke_color,
         stroke_width});
  }

  void draw_circle(float cx, float cy, float radius, const flex::Paint& fill,
                   const flex::Paint& stroke, float stroke_width) override {
    const auto fill_color =
        fill.type == flex::Paint::Type::Solid ? fill.color : flex::Color{};
    const auto stroke_color =
        stroke.type == flex::Paint::Type::Solid ? stroke.color : flex::Color{};
    circles.push_back({cx + tx_, cy + ty_, radius, fill.type, fill_color,
                       stroke_color, stroke_width});
  }
  void draw_ellipse(float, float, float, float, const flex::Paint&, const flex::Paint&, float) override {}
  void draw_text(const std::string& text, float x, float y, const std::string& font,
                 float size, bool bold, const flex::Color& color) override {
    texts.push_back({text, font, x + tx_, y + ty_, size, color, bold});
  }
  void draw_image(const std::string& src, float x, float y, float w, float h) override {
    images.push_back({src, x + tx_, y + ty_, w, h});
  }
  void draw_svg(const std::string& src, float x, float y, float w, float h) override {
    svgs.push_back({src, x + tx_, y + ty_, w, h});
  }
  void draw_svg_data(const std::string&, float, float, float, float) override {}
  void clear(const flex::Color& color) override {
    ++clear_calls;
    clear_color = color;
  }
  flex::Bounds viewport() const override { return viewport_; }
  flex::RendererCapabilities capabilities() const override { return capabilities_; }

  bool supports_retained_mode() const override {
    return retained_mode_ && capabilities_.retained_mode;
  }
  void remove_cached(flex::PaintHandle) override {}
  flex::PaintHandle push_rect(float, float, float, float, float, const flex::Paint&,
                              const flex::Paint&, float, const flex::Transform&, float) override {
    ++retained_push_count;
    return reinterpret_cast<flex::PaintHandle>(next_handle_++);
  }
  flex::PaintHandle push_circle(float, float, float, const flex::Paint&, const flex::Paint&, float,
                                const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_ellipse(float, float, float, float, const flex::Paint&, const flex::Paint&,
                                 float, const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_polygon(int, float, const flex::Paint&, const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_star(int, float, float, const flex::Paint&, const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_path(const std::string&, const flex::Paint&, const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  void update_transform(flex::PaintHandle, const flex::Transform&) override {}

  std::vector<DrawRectCall> rects;
  std::vector<DrawCircleCall> circles;
  std::vector<ShadowCall> shadows;
  std::vector<ClipRectCall> clips;
  std::vector<float> rotations;
  std::vector<std::pair<float, float>> scales;
  std::vector<std::pair<float, float>> translations;
  std::vector<DrawImageCall> images;
  std::vector<DrawSvgCall> svgs;
  std::vector<TextCall> texts;
  std::vector<LineCall> lines;
  std::vector<FillPathCall> fill_paths;
  std::vector<StrokePathCall> stroke_paths;
  std::vector<flex::Transform> transforms;
  std::vector<float> alphas;
  std::vector<float> blur_radii;
  int begin_frame_calls = 0;
  int end_frame_calls = 0;
  int clear_calls = 0;
  int clear_blur_calls = 0;
  int clear_shadow_calls = 0;
  float frame_pixel_ratio = 0.0f;
  flex::Color clear_color{};
  flex::RendererCapabilities capabilities_{};
  int retained_push_count = 0;

private:
  struct SavedOffset {
    float x;
    float y;
  };

  float tx_ = 0.0f;
  float ty_ = 0.0f;
  flex::Bounds viewport_{0.0f, 0.0f, 800.0f, 600.0f};
  uintptr_t next_handle_ = 1;
  std::vector<SavedOffset> save_stack_;
};

std::vector<TextCall> render_plain_text(const ComputedStyle& style,
                                      const std::string& text, float width) {
  RecordingRenderer backend;
  backend.begin_frame(800.0f, 600.0f, 1.0f);
  Renderer renderer(&backend);
  RenderManager render_manager(&renderer);

  Element elem;
  elem.set_layout_bounds(20.0f, 30.0f, width, 120.0f);
  elem.set_text(text);
  *elem.computed_style = style;
  render_manager.render_tree(&elem);
  return backend.texts;
}

void require_color(const flex::Color& color, float r, float g, float b,
                   float a = 1.0f) {
  check(approx_eq(color.r, r, 0.001f));
  check(approx_eq(color.g, g, 0.001f));
  check(approx_eq(color.b, b, 0.001f));
  check(approx_eq(color.a, a, 0.001f));
}

const DrawRectCall* find_fill_rect(const RecordingRenderer& backend) {
  const auto it = std::find_if(
      backend.rects.begin(), backend.rects.end(),
      [](const DrawRectCall& call) { return call.fill_color.a > 0.0f; });
  return it != backend.rects.end() ? &(*it) : nullptr;
}

const DrawRectCall* find_stroke_rect(const RecordingRenderer& backend) {
  const auto it = std::find_if(
      backend.rects.begin(), backend.rects.end(),
      [](const DrawRectCall& call) { return call.stroke_width > 0.0f; });
  return it != backend.rects.end() ? &(*it) : nullptr;
}

} // namespace

spec("RenderManager owns backend frame lifecycle") {
  it("renders a laid out tree inside one backend frame") {
    Box box(nullptr);
    check(!box.renderer_capabilities().shadow);

    RecordingRenderer capability_backend;
    capability_backend.capabilities_.shadow = true;
    Box backend_box(&capability_backend);
    check(backend_box.renderer_capabilities().shadow);

    auto* root = box.create("div", "root");
    box.set_root(root);
    box.set_viewport(200.0f, 120.0f);

    box.load_css(R"(
      #root {
        width: 80px;
        height: 40px;
        background-color: #336699;
      }
    )");
    box.update();

    RecordingRenderer backend;
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    RenderFrame frame;
    frame.root = root;
    frame.viewport = {200.0f, 120.0f, 2.0f};
    frame.clear_color = flex::Color{0.1f, 0.2f, 0.3f, 1.0f};
    render_manager.render_frame(frame);

    check(backend.begin_frame_calls == 1);
    check(backend.clear_calls == 1);
    check(backend.end_frame_calls == 1);
    check(approx_eq(backend.frame_pixel_ratio, 2.0f, 0.0f));
    require_color(backend.clear_color, 0.1f, 0.2f, 0.3f);

    const auto* fill = find_fill_rect(backend);
    check(fill != nullptr);
    check(approx_eq(fill->w, 80.0f, 0.0f));
    check(approx_eq(fill->h, 40.0f, 0.0f));
    require_color(fill->fill_color, 0x33 / 255.0f, 0x66 / 255.0f,
                  0x99 / 255.0f);
  }

  it("submits unchanged frames because the host surface may have changed") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    box.set_viewport(200.0f, 120.0f);
    box.load_css(R"(
      #root {
        width: 80px;
        height: 40px;
        background-color: #336699;
      }
    )");
    box.update();

    RecordingRenderer backend;
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    RenderFrame frame;
    frame.root = root;
    frame.viewport = {200.0f, 120.0f, 1.0f};
    frame.clear_color = flex::Color{0.1f, 0.2f, 0.3f, 1.0f};

    render_manager.render_frame(frame);
    render_manager.render_frame(frame);

    check(backend.begin_frame_calls == 2);
    check(backend.clear_calls == 2);
    check(backend.end_frame_calls == 2);

    frame.clear_color = flex::Color{0.2f, 0.2f, 0.3f, 1.0f};
    render_manager.render_frame(frame);

    check(backend.begin_frame_calls == 3);
    check(backend.clear_calls == 3);
    check(backend.end_frame_calls == 3);
    require_color(backend.clear_color, 0.2f, 0.2f, 0.3f);
  }

  it("reuses retained draw commands across frame state changes") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    box.set_viewport(200.0f, 120.0f);
    box.load_css(R"(
      #root {
        width: 80px;
        height: 40px;
        background-color: #336699;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.capabilities_.retained_mode = true;
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    RenderFrame frame;
    frame.root = root;
    frame.viewport = {200.0f, 120.0f, 1.0f};
    frame.clear_color = flex::Color{0.1f, 0.2f, 0.3f, 1.0f};
    render_manager.render_frame(frame);

    check(backend.begin_frame_calls == 1);
    check(backend.clear_calls == 1);
    check(backend.retained_push_count == 1);

    render_manager.render_frame(frame);

    check(backend.begin_frame_calls == 2);
    check(backend.clear_calls == 2);
    check(backend.retained_push_count == 1);

    frame.clear_color = flex::Color{0.2f, 0.2f, 0.3f, 1.0f};
    render_manager.render_frame(frame);

    check(backend.begin_frame_calls == 3);
    check(backend.clear_calls == 3);
    check(backend.retained_push_count == 1);
  }

  it("renders overlays inside the same backend frame") {
    Element root;
    root.set_layout_bounds(0.0f, 0.0f, 200.0f, 120.0f);

    Element select_elem;
    select_elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 32.0f);
    SelectWidget select({"One", "Two"}, 0);
    select.set_expanded(true);
    select.update(1000.0f, select_elem);
    select_elem.widget = &select;
    root.append(&select_elem);

    RecordingRenderer backend;
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    RenderFrame frame;
    frame.root = &root;
    frame.viewport = {200.0f, 120.0f, 1.0f};
    frame.clear_color = flex::Color{0.0f, 0.0f, 0.0f, 1.0f};

    render_manager.render_frame(frame);

    check(backend.begin_frame_calls == 1);
    check(backend.end_frame_calls == 1);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 20.0f, 0.001f) &&
                 approx_eq(call.y, 62.0f, 0.001f) &&
                 approx_eq(call.w, 100.0f, 0.001f) &&
                 approx_eq(call.h, 64.0f, 0.001f);
        });
    check(overlay != backend.rects.end());
  }

  it("ignores active animations inside hidden subtrees when deciding frame dirtiness") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div", "root");
    auto* hidden_panel = box.create("div", "hidden-panel");
    auto* hidden_child = box.create("div", "hidden-child");
    hidden_panel->add_class("hidden");
    hidden_child->add_class("animated");
    hidden_panel->append(hidden_child);
    root->append(hidden_panel);
    box.set_root(root);
    box.set_viewport(200.0f, 120.0f);

    box.load_css(R"(
      @keyframes fade-in {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #root {
        width: 200px;
        height: 120px;
      }
      #hidden-panel,
      #hidden-child {
        width: 40px;
        height: 20px;
      }
      .hidden {
        display: none;
      }
      .animated {
        animation: fade-in 1s linear infinite;
      }
    )");
    box.update();
    check_false(box.is_dirty());

    const auto hidden_id = reinterpret_cast<std::uintptr_t>(hidden_child);
    check(box.animations().has_active(hidden_id, box.time()));

    box.update_time(16.0f);
    check_false(box.is_dirty());
  }
}

spec("Semantic widget parts settle after layout-driven geometry sync") {
  it("does not request another frame after an unchanged slider render") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div", "root");
    auto* slider = box.create_widget<SliderWidget>(
        "slider", "slider", 0.0f, 100.0f, 50.0f, 1.0f);
    auto* progress = box.create_widget<ProgressBarWidget>(
        "progress", "progress", 65.0f, false);
    root->append(slider);
    root->append(progress);
    box.set_root(root);
    box.set_viewport(240.0f, 100.0f);
    box.load_css(R"(
      #root { width: 240px; height: 100px; }
      #slider { width: 180px; height: 28px; }
      #progress { width: 180px; height: 20px; }
    )");

    box.update();
    check_false(box.is_dirty());
    const int rendered_frames = backend.begin_frame_calls;

    box.update();
    check_false(box.is_dirty());
    check(backend.begin_frame_calls == rendered_frames);
  }
}

spec("Element emits backend-neutral render commands") {
  it("emits visual state for visible descendants only") {
    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 100.0f, 50.0f);
    root.set_opacity(0.5f);
    root.set_clip(true);

    Element child;
    child.set_layout_bounds(5.0f, 7.0f, 20.0f, 10.0f);
    root.append(&child);

    Element hidden_child;
    hidden_child.set_layout_bounds(30.0f, 40.0f, 10.0f, 10.0f);
    hidden_child.computed_style->display = Display::None;
    root.append(&hidden_child);

    RenderCommandList commands(flex::RendererCapabilities{});
    root.emit_render_commands(commands);

    size_t save_count = 0;
    size_t restore_count = 0;
    std::vector<flex::Transform> transforms;
    std::vector<ClipRectCommand> clips;
    std::vector<float> alphas;

    for (const auto& command : commands.commands()) {
      if (std::holds_alternative<SaveCommand>(command)) {
        ++save_count;
      } else if (std::holds_alternative<RestoreCommand>(command)) {
        ++restore_count;
      } else if (const auto* set_transform =
                     std::get_if<SetTransformCommand>(&command)) {
        transforms.push_back(set_transform->transform);
      } else if (const auto* clip = std::get_if<ClipRectCommand>(&command)) {
        clips.push_back(*clip);
      } else if (const auto* alpha =
                     std::get_if<SetGlobalAlphaCommand>(&command)) {
        alphas.push_back(alpha->alpha);
      }
    }

    check(save_count == 2);
    check(restore_count == 2);
    check(transforms.size() == 2);
    check(approx_eq(flex::tx(transforms[0]), 10.0f, 0.001f));
    check(approx_eq(flex::ty(transforms[0]), 20.0f, 0.001f));
    check(approx_eq(flex::tx(transforms[1]), 15.0f, 0.001f));
    check(approx_eq(flex::ty(transforms[1]), 27.0f, 0.001f));

    check(alphas.size() == 1);
    check(approx_eq(alphas[0], 0.5f, 0.001f));
    check(clips.size() == 1);
    check(approx_eq(clips[0].x, 0.0f, 0.001f));
    check(approx_eq(clips[0].y, 0.0f, 0.001f));
    check(approx_eq(clips[0].width, 100.0f, 0.001f));
    check(approx_eq(clips[0].height, 50.0f, 0.001f));
  }

  it("composes local element transforms with command-list prefixes") {
    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 100.0f, 50.0f);

    Element child;
    child.set_layout_bounds(5.0f, 7.0f, 20.0f, 10.0f);
    root.append(&child);

    RenderCommandList commands(flex::RendererCapabilities{},
                               flex::make_translation(100.0f, 200.0f));
    root.emit_render_commands(commands);

    std::vector<flex::Transform> transforms;
    for (const auto& command : commands.commands()) {
      if (const auto* set_transform =
              std::get_if<SetTransformCommand>(&command)) {
        transforms.push_back(set_transform->transform);
      }
    }

    check(transforms.size() == 2);
    if (transforms.size() == 2) {
      check(approx_eq(flex::tx(transforms[0]), 110.0f, 0.001f));
      check(approx_eq(flex::ty(transforms[0]), 220.0f, 0.001f));
      check(approx_eq(flex::tx(transforms[1]), 115.0f, 0.001f));
      check(approx_eq(flex::ty(transforms[1]), 227.0f, 0.001f));
    }
  }
}

spec("RenderManager resolves CSS math lengths for pseudo element geometry") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* badge = box.create("div", "badge");
    root->append(badge);
    box.set_root(root);
    box.set_viewport(200.0f, 120.0f);

    box.load_css(R"(
      #root {
        width: 120px;
        height: 80px;
      }

      #badge {
        width: 100px;
        height: 50px;
      }

      #badge::before {
        content: "";
        background-color: #336699;
        width: calc(50% - 10px);
        height: clamp(8px, 20%, 30px);
        left: calc(10% + 4px);
        top: max(2px, 0.25rem);
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(root);

    const auto pseudo = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f);
        });
    check(pseudo != backend.rects.end());
    if (pseudo == backend.rects.end()) {
      return;
    }
    check(approx_eq(pseudo->x, 14.0f, 0.001f));
    check(approx_eq(pseudo->y, 4.0f, 0.001f));
    check(approx_eq(pseudo->w, 40.0f, 0.001f));
    check(approx_eq(pseudo->h, 10.0f, 0.001f));
  }
}

spec("RenderManager resolves viewport units for pseudo element geometry") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* badge = box.create("div", "badge");
    root->append(badge);
    box.set_root(root);
    box.set_viewport(200.0f, 120.0f);

    box.load_css(R"(
      #root {
        width: 120px;
        height: 80px;
      }

      #badge {
        width: 100px;
        height: 50px;
      }

      #badge::before {
        content: "";
        background-color: #336699;
        width: 10vw;
        height: 10dvh;
        left: calc(50vw - 10px);
        top: 5svh;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(root);

    const auto pseudo = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f) &&
                 approx_eq(call.w, 20.0f, 0.001f) &&
                 approx_eq(call.h, 12.0f, 0.001f);
        });
    check(pseudo != backend.rects.end());
    if (pseudo == backend.rects.end()) {
      return;
    }
    check(approx_eq(pseudo->x, 90.0f, 0.001f));
    check(approx_eq(pseudo->y, 6.0f, 0.001f));
  }
}

spec("RenderManager segments pseudo element content through shared text layout") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* badge = box.create("div", "badge");
    root->append(badge);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
    badge->set_layout_bounds(20.0f, 30.0f, 120.0f, 40.0f);

    box.load_css(R"(
      #badge::before {
        content: "A🙂B";
        left: 4px;
        top: 2px;
        width: 80px;
        height: 20px;
        font-size: 16px;
        color: #223344;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(root);

    check(backend.texts.size() >= 3);
    if (backend.texts.size() >= 3) {
      check(backend.texts[0].text == "A");
      check(backend.texts[1].text == "🙂");
      check(backend.texts[2].text == "B");
      check(backend.texts[1].x > backend.texts[0].x);
      check(backend.texts[2].x > backend.texts[1].x);
    }
  }
}

spec("RenderManager overlays use global coordinates without double translation") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element dropdown_elem;
    dropdown_elem.set_layout_bounds(30.0f, 40.0f, 120.0f, 24.0f);
    DropdownWidget dropdown("Pick one");
    dropdown.add_option("Alpha", "alpha");
    dropdown.open();
    dropdown_elem.widget = &dropdown;
    root.append(&dropdown_elem);

    render_manager.render_overlays(&root);

    check_false(backend.rects.empty());
    const auto it = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 40.0f, 0.001f) &&
                 approx_eq(call.y, 88.0f, 0.001f) &&
                 approx_eq(call.w, 120.0f, 0.001f);
        });
    check(it != backend.rects.end());
    check(approx_eq(it->x, 40.0f, 0.001f));
    check(approx_eq(it->y, 88.0f, 0.001f));
  }
}

spec("SelectWidget overlay emits global coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element select_elem;
    select_elem.set_layout_bounds(30.0f, 40.0f, 120.0f, 32.0f);
    SelectWidget select({"Alpha", "Beta"});
    select.set_selected_index(0);
    select.set_expanded(true);
    select.update(200.0f, select_elem);
    select_elem.widget = &select;
    root.append(&select_elem);

    render_manager.render_overlays(&root);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 40.0f, 0.001f) &&
                 approx_eq(call.y, 92.0f, 0.001f) &&
                 approx_eq(call.w, 120.0f, 0.001f);
        });
    check(overlay != backend.rects.end());
  }
}

spec("DatePickerWidget overlay translates local calendar commands") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element date_elem;
    date_elem.set_layout_bounds(30.0f, 40.0f, 160.0f, 32.0f);
    DatePickerWidget datepicker({2024, 4, 22});
    datepicker.set_open(true);
    date_elem.widget = &datepicker;
    root.append(&date_elem);

    render_manager.render_overlays(&root);

    const auto overlay_bg_count = std::count_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 40.0f, 0.001f) &&
                 approx_eq(call.y, 96.0f, 0.001f) &&
                 approx_eq(call.w, 280.0f, 0.001f) &&
                 approx_eq(call.h, 280.0f, 0.001f);
        });
    check(overlay_bg_count >= 2);
  }
}

spec("SearchBoxWidget overlay emits global coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element search_elem;
    search_elem.set_layout_bounds(30.0f, 40.0f, 180.0f, 32.0f);
    SearchBoxWidget search("Find");
    search.add_suggestion("alpha", "Alpha");
    search.set_dropdown_open(true);
    search_elem.widget = &search;
    root.append(&search_elem);

    render_manager.render_overlays(&root);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 40.0f, 0.001f) &&
                 approx_eq(call.y, 94.0f, 0.001f) &&
                 approx_eq(call.w, 180.0f, 0.001f);
        });
    check(overlay != backend.rects.end());
  }
}

spec("TimePickerWidget overlay emits global coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element time_elem;
    time_elem.set_layout_bounds(30.0f, 40.0f, 160.0f, 32.0f);
    TimePickerWidget timepicker({8, 9, 0});
    time_elem.widget = &timepicker;
    root.append(&time_elem);

    check(timepicker.handle_event(Event::mouse_down(41.0f, 61.0f), time_elem));
    render_manager.render_overlays(&root);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 40.0f, 0.001f) &&
                 approx_eq(call.y, 96.0f, 0.001f) &&
                 approx_eq(call.w, 136.0f, 0.001f) &&
                 approx_eq(call.h, 180.0f, 0.001f);
        });
    check(overlay != backend.rects.end());
  }
}

spec("ToolbarWidget dropdown overlay emits global coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element toolbar_elem;
    toolbar_elem.set_layout_bounds(30.0f, 40.0f, 180.0f, 40.0f);
    ToolbarWidget toolbar;
    toolbar.add_dropdown("more", "M", {{"a", "Alpha"}, {"b", "Beta"}});
    toolbar_elem.widget = &toolbar;
    root.append(&toolbar_elem);

    render_manager.render_tree(&root);
    toolbar.set_open_dropdown("more");
    render_manager.render_overlays(&root);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 44.0f, 0.001f) &&
                 approx_eq(call.y, 98.0f, 0.001f) &&
                 approx_eq(call.w, 120.0f, 0.001f);
        });
    check(overlay != backend.rects.end());
  }
}

spec("PopoverWidget overlay emits global coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element popover_elem;
    popover_elem.set_layout_bounds(30.0f, 40.0f, 100.0f, 30.0f);
    PopoverWidget popover("Details", PopoverWidget::Position::Bottom);
    popover.set_size(120.0f, 80.0f);
    popover.show();
    popover_elem.widget = &popover;
    root.append(&popover_elem);

    render_manager.render_overlays(&root);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 30.0f, 0.001f) &&
                 approx_eq(call.y, 98.0f, 0.001f) &&
                 approx_eq(call.w, 120.0f, 0.001f) &&
                 approx_eq(call.h, 80.0f, 0.001f);
        });
    check(overlay != backend.rects.end());
  }
}

spec("MenuWidget overlay uses shown global coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element menu_elem;
    menu_elem.set_layout_bounds(30.0f, 40.0f, 1.0f, 1.0f);
    MenuWidget menu;
    menu.add_item("alpha", "Alpha");
    menu.show(140.0f, 90.0f);
    menu_elem.widget = &menu;
    root.append(&menu_elem);

    render_manager.render_overlays(&root);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 140.0f, 0.001f) &&
                 approx_eq(call.y, 90.0f, 0.001f) &&
                 call.w > 0.0f && call.h > 0.0f;
        });
    check(overlay != backend.rects.end());
  }
}

spec("NotificationWidget overlay uses viewport coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element notification_elem;
    notification_elem.set_layout_bounds(30.0f, 40.0f, 1.0f, 1.0f);
    NotificationWidget notifications(NotificationWidget::Position::TopRight);
    notifications.notify("Done", "Saved");
    notifications.update(250.0f, notification_elem);
    notification_elem.widget = &notifications;
    root.append(&notification_elem);

    render_manager.render_overlays(&root);

    const auto overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 464.0f, 0.001f) &&
                 approx_eq(call.y, 16.0f, 0.001f) &&
                 approx_eq(call.w, 320.0f, 0.001f) &&
                 approx_eq(call.h, 80.0f, 0.001f);
        });
    check(overlay != backend.rects.end());
  }
}

spec("ListView clips in local coordinates under RenderManager transforms") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element list_elem;
    list_elem.set_layout_bounds(30.0f, 40.0f, 120.0f, 80.0f);
    ListViewWidget listview;
    listview.add_item("1", "1");
    listview.add_item("2", "Beta");
    list_elem.widget = &listview;
    root.append(&list_elem);

    render_manager.render_tree(&root);

    check_false(backend.clips.empty());
    check(approx_eq(backend.clips.front().x, 40.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 60.0f, 0.001f));
    check(approx_eq(backend.clips.front().w, 120.0f, 0.001f));
    check(approx_eq(backend.clips.front().h, 80.0f, 0.001f));
  }
}

spec("ScrollView prefixes nested transforms inside scrolled content") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element scroll_elem;
    scroll_elem.set_layout_bounds(0.0f, 0.0f, 80.0f, 60.0f);

    Element child;
    child.set_layout_bounds(0.0f, 0.0f, 200.0f, 160.0f);
    scroll_elem.append(&child);

    ScrollViewWidget scroll;
    RenderCommandList commands(renderer.capabilities());
    scroll.emit_render_commands(scroll_elem, commands);
    scroll.scroll_to(10.0f, 15.0f);
    scroll.begin_scroll(scroll_elem, commands);
    commands.set_transform(flex::make_translation(2.0f, 3.0f));
    scroll.end_scroll(commands);
    commands.replay(renderer);

    check_false(backend.transforms.empty());
    if (!backend.transforms.empty()) {
      check(approx_eq(flex::tx(backend.transforms.back()), -8.0f, 0.001f));
      check(approx_eq(flex::ty(backend.transforms.back()), -12.0f, 0.001f));
    }
  }
}

spec("ListView consumes standard scrollbar CSS bridge variables") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* list_elem = box.create_widget<ListViewWidget>("div", "list");
    auto* list = static_cast<ListViewWidget*>(list_elem->widget);
    for (int i = 0; i < 10; ++i) {
      list->add_item(std::to_string(i), "Item " + std::to_string(i));
    }
    root->append(list_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #list {
        width: 120px;
        height: 80px;
        scrollbar-width: thin;
        scrollbar-color: #336699 #111111;
      }
    )");
    box.update();

    const auto track = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 6.0f, 0.001f) &&
                 approx_eq(call.h, 80.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 0x11 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x11 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x11 / 255.0f, 0.001f);
        });
    check(track != backend.rects.end());
    if (track == backend.rects.end()) {
      return;
    }

    const auto thumb = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 6.0f, 0.001f) &&
                 call.h >= 20.0f &&
                 approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f);
        });
    check(thumb != backend.rects.end());
    if (thumb == backend.rects.end()) {
      return;
    }

    check(approx_eq(track->x, 114.0f, 0.001f));
    check(approx_eq(track->y, 0.0f, 0.001f));
    check(approx_eq(thumb->x, 114.0f, 0.001f));
    check(approx_eq(thumb->y, 0.0f, 0.001f));
  }
}

spec("ListView hides CSS scrollbar-width none without degenerate scrollbar draws") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* list_elem = box.create_widget<ListViewWidget>("div", "list");
    auto* list = static_cast<ListViewWidget*>(list_elem->widget);
    for (int i = 0; i < 10; ++i) {
      list->add_item(std::to_string(i), "Item " + std::to_string(i));
    }
    root->append(list_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #list {
        width: 120px;
        height: 80px;
        scrollbar-width: none;
        scrollbar-color: #336699 #111111;
      }
    )");
    box.update();

    const auto scrollbar_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          const bool scrollbar_color =
              (approx_eq(call.fill_color.r, 0x11 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.g, 0x11 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.b, 0x11 / 255.0f, 0.001f)) ||
              (approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f));
          return scrollbar_color || approx_eq(call.w, 0.0f, 0.001f);
        });
    check(scrollbar_rect == backend.rects.end());
  }
}

spec("RenderManager clips overflow hidden elements in local coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 300.0f, 200.0f);

    Element child;
    child.set_layout_bounds(30.0f, 40.0f, 120.0f, 80.0f);
    child.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    child.computed_style->overflow_x = Overflow::Hidden;
    root.append(&child);

    render_manager.render_tree(&root);

    check_false(backend.clips.empty());
    check(approx_eq(backend.clips.front().x, 40.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 60.0f, 0.001f));
    check(approx_eq(backend.clips.front().w, 120.0f, 0.001f));
    check(approx_eq(backend.clips.front().h, 80.0f, 0.001f));
  }
}

spec("RenderManager skips visibility collapse elements") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->visibility = Visibility::Collapse;

    render_manager.render_tree(&elem);

    check(backend.rects.empty());
    check(backend.texts.empty());
  }
}

spec("RenderManager skips CSS visibility hidden elements") {
  it("runs") {
    Box box(nullptr);
    auto* panel = box.create("div", "panel");
    box.set_root(panel);
    box.set_viewport(160.0f, 120.0f);

    box.load_css(R"(
      #panel {
        width: 80px;
        height: 40px;
        background-color: #336699;
        visibility: hidden;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(panel);

    check(backend.rects.empty());
    check(backend.texts.empty());
  }
}

spec("RenderManager applies parsed CSS opacity to global alpha") {
  it("runs") {
    Box box(nullptr);
    auto* panel = box.create("div", "panel");
    box.set_root(panel);
    box.set_viewport(160.0f, 120.0f);

    box.load_css(R"(
      #panel {
        width: 80px;
        height: 40px;
        background-color: #336699;
        opacity: 0.25;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(panel);

    check_false(backend.alphas.empty());
    check(approx_eq(backend.alphas.back(), 0.25f, 0.001f));
    check(backend.rects.size() == 1);
  }
}

spec("RenderManager applies clip-path inset in local coordinates") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 300.0f, 200.0f);

    Element child;
    child.set_layout_bounds(30.0f, 40.0f, 120.0f, 80.0f);
    child.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    child.computed_style->variables[Symbol("--clip-path")] =
        "inset(10px 20% 5px 4px)";
    root.append(&child);

    render_manager.render_tree(&root);

    check_false(backend.clips.empty());
    check(approx_eq(backend.clips.front().x, 44.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 70.0f, 0.001f));
    check(approx_eq(backend.clips.front().w, 92.0f, 0.001f));
    check(approx_eq(backend.clips.front().h, 65.0f, 0.001f));
  }
}

spec("RenderManager applies CSS clip-path inset round as rectangular clip") {
  it("runs") {
    Box box(nullptr);
    auto* panel = box.create("div", "panel");
    box.set_root(panel);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #panel {
        width: 120px;
        height: 80px;
        background-color: #336699;
        clip-path: inset(10px 20% 5px 4px round 8px);
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(panel);

    check_false(backend.clips.empty());
    check(approx_eq(backend.clips.front().x, 4.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 10.0f, 0.001f));
    check(approx_eq(backend.clips.front().w, 92.0f, 0.001f));
    check(approx_eq(backend.clips.front().h, 65.0f, 0.001f));
  }
}

spec("RenderManager culls against transformed render bounds") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(1000.0f, 20.0f, 60.0f, 30.0f);
    elem.computed_style->transform_x = -950.0f;
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};

    render_manager.render_tree(&elem);

    check_false(backend.rects.empty());
    if (!backend.rects.empty()) {
      check(approx_eq(backend.rects.back().x, 50.0f, 0.001f));
      check(approx_eq(backend.rects.back().y, 20.0f, 0.001f));
      check(approx_eq(backend.rects.back().w, 60.0f, 0.001f));
      check(approx_eq(backend.rects.back().h, 30.0f, 0.001f));
    }
  }
}

spec("RenderManager offsets overflow scroll container contents by scroll state") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);

    Element scroll;
    scroll.set_layout_bounds(10.0f, 20.0f, 120.0f, 60.0f);
    scroll.computed_style->overflow_y = Overflow::Scroll;
    scroll.set_scroll_metrics(true, 120.0f, 180.0f, 0.0f, 120.0f);
    scroll.set_scroll_offset(0.0f, 30.0f);

    Element child;
    child.set_layout_bounds(0.0f, 50.0f, 120.0f, 20.0f);
    child.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};

    root.append(&scroll);
    scroll.append(&child);

    render_manager.render_tree(&root);

    check_false(backend.clips.empty());
    check(approx_eq(backend.clips.front().x, 10.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 20.0f, 0.001f));
    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.back().x, 10.0f, 0.001f));
    check(approx_eq(backend.rects.back().y, 40.0f, 0.001f));
  }
}

spec("RenderManager prefixes widget transforms with overflow scroll offset") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);

    Element scroll;
    scroll.set_layout_bounds(10.0f, 20.0f, 120.0f, 60.0f);
    scroll.computed_style->overflow_y = Overflow::Scroll;
    scroll.set_scroll_metrics(true, 120.0f, 180.0f, 0.0f, 120.0f);
    scroll.set_scroll_offset(0.0f, 30.0f);

    Element button_elem;
    button_elem.set_layout_bounds(0.0f, 50.0f, 80.0f, 24.0f);
    ButtonWidget button("Inside");
    button_elem.widget = &button;

    root.append(&scroll);
    scroll.append(&button_elem);

    render_manager.render_tree(&root);

    const auto transform = std::find_if(
        backend.transforms.begin(), backend.transforms.end(),
        [](const flex::Transform& transform) {
          return approx_eq(flex::tx(transform), 10.0f, 0.001f) &&
                 approx_eq(flex::ty(transform), 40.0f, 0.001f);
        });
    check(transform != backend.transforms.end());
  }
}

spec("RenderManager offsets horizontal overflow scroll container contents by scroll state") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);

    Element scroll;
    scroll.set_layout_bounds(10.0f, 20.0f, 120.0f, 60.0f);
    scroll.computed_style->overflow_x = Overflow::Scroll;
    scroll.set_scroll_metrics(true, 220.0f, 60.0f, 100.0f, 0.0f);
    scroll.set_scroll_offset(25.0f, 0.0f);

    Element child;
    child.set_layout_bounds(40.0f, 10.0f, 20.0f, 20.0f);
    child.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};

    root.append(&scroll);
    scroll.append(&child);

    render_manager.render_tree(&root);

    check_false(backend.clips.empty());
    check(approx_eq(backend.clips.front().x, 10.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 20.0f, 0.001f));
    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.back().x, 25.0f, 0.001f));
    check(approx_eq(backend.rects.back().y, 30.0f, 0.001f));
  }
}

spec("RenderManager draws sticky headers at the scroll container top after scrolling") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* scroll = box.create("div", "scroll");
    auto* intro = box.create("div", "intro");
    auto* header = box.create("div", "header");
    auto* body = box.create("div", "body");
    root->append(scroll);
    scroll->append(intro);
    scroll->append(header);
    scroll->append(body);
    box.set_root(root);
    box.set_viewport(160.0f, 120.0f);

    box.load_css(R"(
      #scroll {
        width: 120px;
        height: 60px;
        overflow: auto;
      }

      #intro {
        width: 120px;
        height: 30px;
        background-color: #111111;
      }

      #header {
        width: 120px;
        height: 20px;
        position: sticky;
        top: 0;
        background-color: #336699;
      }

      #body {
        width: 120px;
        height: 120px;
        background-color: #222222;
      }
    )");
    box.update();
    check(scroll->set_scroll_offset(0.0f, 40.0f));
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(root);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return std::fabs(call.fill_color.r - 0x33 / 255.0f) <= 0.001f &&
                                    std::fabs(call.fill_color.g - 0x66 / 255.0f) <= 0.001f &&
                                    std::fabs(call.fill_color.b - 0x99 / 255.0f) <= 0.001f &&
                                    std::fabs(call.w - 120.0f) <= 0.001f &&
                                    std::fabs(call.h - 20.0f) <= 0.001f;
                           });
    check(it != backend.rects.end());
    if (it == backend.rects.end()) {
      return;
    }
    check(approx_eq(it->x, 0.0f, 0.001f));
    check(approx_eq(it->y, 0.0f, 0.001f));
  }
}

spec("RenderManager leaves overflow visible elements unclipped") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 80.0f);
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->overflow_x = Overflow::Visible;
    elem.computed_style->overflow_y = Overflow::Visible;

    render_manager.render_tree(&elem);

    check(backend.clips.empty());
  }
}

spec("ScrollViewWidget reuses standard scrollbar CSS bridge variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* scroll = box.create_widget<ScrollViewWidget>("div", "scroll");
    auto* body = box.create("div", "body");
    root->append(scroll);
    scroll->append(body);
    box.set_root(root);
    box.set_viewport(160.0f, 120.0f);

    box.load_css(R"(
      #scroll {
        width: 80px;
        height: 40px;
        scrollbar-width: thin;
        scrollbar-color: #336699 #111111;
      }

      #body {
        position: relative;
        left: 0;
        top: 0;
        width: 80px;
        height: 140px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(root);

    auto track = std::find_if(backend.rects.begin(), backend.rects.end(),
                              [](const DrawRectCall& call) {
                                return std::fabs(call.fill_color.r - 0x11 / 255.0f) <= 0.001f &&
                                       std::fabs(call.fill_color.g - 0x11 / 255.0f) <= 0.001f &&
                                       std::fabs(call.fill_color.b - 0x11 / 255.0f) <= 0.001f &&
                                       std::fabs(call.w - 6.0f) <= 0.001f;
                              });
    check(track != backend.rects.end());

    auto thumb = std::find_if(backend.rects.begin(), backend.rects.end(),
                              [](const DrawRectCall& call) {
                                return std::fabs(call.fill_color.r - 0x33 / 255.0f) <= 0.001f &&
                                       std::fabs(call.fill_color.g - 0x66 / 255.0f) <= 0.001f &&
                                       std::fabs(call.fill_color.b - 0x99 / 255.0f) <= 0.001f &&
                                       std::fabs(call.w - 6.0f) <= 0.001f;
                              });
    check(thumb != backend.rects.end());
  }
}

spec("ScrollViewWidget hides CSS scrollbar-width none without degenerate scrollbar draws") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* scroll = box.create_widget<ScrollViewWidget>("div", "scroll");
    auto* body = box.create("div", "body");
    root->append(scroll);
    scroll->append(body);
    box.set_root(root);
    box.set_viewport(160.0f, 120.0f);

    box.load_css(R"(
      #scroll {
        width: 80px;
        height: 40px;
        scrollbar-width: none;
        scrollbar-color: #336699 #111111;
      }

      #body {
        position: relative;
        left: 0;
        top: 0;
        width: 80px;
        height: 140px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(root);

    const auto scrollbar_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          const bool scrollbar_color =
              (approx_eq(call.fill_color.r, 0x11 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.g, 0x11 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.b, 0x11 / 255.0f, 0.001f)) ||
              (approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
               approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f));
          return scrollbar_color || approx_eq(call.w, 0.0f, 0.001f);
        });
    check(scrollbar_rect == backend.rects.end());
  }
}

spec("RenderManager positions plain text using padding and line-height") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 40.0f);
    elem.set_text("Hello");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    elem.computed_style->variables[Symbol("--line-height")] = "2";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    check(approx_eq(backend.texts.front().x, 26.0f, 0.001f));
    check(approx_eq(backend.texts.front().y, 47.0f, 0.001f));
  }
}

spec("RenderManager applies parsed CSS line-height to plain text") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("Hello\nWorld");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 120px;
        height: 60px;
        font-size: 10px;
        line-height: 2;
        padding-top: 4px;
        padding-left: 6px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "Hello");
    check(backend.texts[1].text == "World");
    check(approx_eq(backend.texts[0].x, 6.0f, 0.001f));
    check(approx_eq(backend.texts[1].x, 6.0f, 0.001f));
    check(approx_eq(backend.texts[1].y - backend.texts[0].y, 20.0f,
                    0.001f));
  }
}

spec("RenderManager applies parsed CSS font-family to plain text") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("Hello");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 120px;
        height: 30px;
        font-family: "Consolas", monospace;
        font-size: 10px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 1);
    if (backend.texts.empty()) {
      return;
    }
    check(backend.texts.front().text == "Hello");
    check(backend.texts.front().font == "Consolas");
  }
}

spec("RenderManager aligns plain text using text-align") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 30.0f);
    elem.set_text("Hello");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_align = TextAlign::Center;

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    check(approx_eq(backend.texts.front().x, 63.0f, 1.0f));
  }
}

spec("RenderManager applies vertical-align to plain text layout") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element top;
    top.set_layout_bounds(20.0f, 30.0f, 120.0f, 40.0f);
    top.set_text("Hello");
    top.computed_style->font_size = 10.0f;
    top.computed_style->variables[Symbol("--vertical-align")] = "top";

    Element middle;
    middle.set_layout_bounds(20.0f, 80.0f, 120.0f, 40.0f);
    middle.set_text("Hello");
    middle.computed_style->font_size = 10.0f;
    middle.computed_style->variables[Symbol("--vertical-align")] = "middle";

    Element bottom;
    bottom.set_layout_bounds(20.0f, 130.0f, 120.0f, 40.0f);
    bottom.set_text("Hello");
    bottom.computed_style->font_size = 10.0f;
    bottom.computed_style->variables[Symbol("--vertical-align")] = "bottom";

    render_manager.render_tree(&top);
    render_manager.render_tree(&middle);
    render_manager.render_tree(&bottom);

    check(backend.texts.size() == 3);
    if (backend.texts.size() != 3) {
      return;
    }
    const float top_offset = backend.texts[0].y - 30.0f;
    const float middle_offset = backend.texts[1].y - 80.0f;
    const float bottom_offset = backend.texts[2].y - 130.0f;
    check(middle_offset > top_offset);
    check(bottom_offset > middle_offset);
    check(approx_eq(middle_offset - top_offset, 12.0f, 1.0f));
    check(approx_eq(bottom_offset - top_offset, 24.0f, 1.0f));
  }
}

spec("RenderManager applies parsed CSS vertical-align values") {
  it("runs") {
    Box box(nullptr);
    Element* root = box.create("div", "root");
    Element* top = box.create("div", "top");
    Element* middle = box.create("div", "middle");
    Element* bottom = box.create("div", "bottom");
    top->set_text("Hello");
    middle->set_text("Hello");
    bottom->set_text("Hello");
    root->append(top);
    root->append(middle);
    root->append(bottom);
    box.set_root(root);
    box.set_viewport(240.0f, 220.0f);

    box.load_css(R"(
      #root {
        width: 160px;
        height: 180px;
        display: flex;
        flex-direction: column;
      }
      #top,
      #middle,
      #bottom {
        width: 120px;
        height: 40px;
        font-size: 10px;
      }
      #top {
        vertical-align: top;
      }
      #middle {
        vertical-align: middle;
      }
      #bottom {
        vertical-align: bottom;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    check(backend.texts.size() == 3);
    if (backend.texts.size() != 3) {
      return;
    }
    const float top_offset = backend.texts[0].y - top->y();
    const float middle_offset = backend.texts[1].y - middle->y();
    const float bottom_offset = backend.texts[2].y - bottom->y();
    check(middle_offset > top_offset);
    check(bottom_offset > middle_offset);
    check(approx_eq(middle_offset - top_offset, 12.0f, 1.0f));
    check(approx_eq(bottom_offset - top_offset, 24.0f, 1.0f));
  }
}

spec("RenderManager justifies non-final text lines") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 80.0f);
    elem.set_text("A B\nC D");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_align = TextAlign::Justify;

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 3);
    if (backend.texts.size() != 3) {
      return;
    }
    check(backend.texts[0].text == "A");
    check(backend.texts[1].text == "B");
    check(backend.texts[2].text == "C D");
    const float natural_b_x =
        20.0f + approximate_text_width(elem.computed_style, "A ");
    check(backend.texts[1].x > natural_b_x + 1.0f);
    check(approx_eq(backend.texts[2].x, 20.0f, 0.001f));
  }
}

spec("RenderManager applies text-align-last to final text line") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 80.0f);
    elem.set_text("A B\nC D");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_align = TextAlign::Justify;
    elem.computed_style->variables[Symbol("--text-align-last")] = "center";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 3);
    if (backend.texts.size() != 3) {
      return;
    }
    check(backend.texts[0].text == "A");
    check(backend.texts[1].text == "B");
    check(backend.texts[2].text == "C D");
    const float expected_last_x =
        20.0f +
        (80.0f - approximate_text_width(elem.computed_style, "C D")) * 0.5f;
    check(backend.texts[1].x >
          20.0f + approximate_text_width(elem.computed_style, "A ") + 1.0f);
    check(approx_eq(backend.texts[2].x, expected_last_x, 1.0f));
  }
}

spec("RenderManager applies parsed CSS text-align-last") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("A B\nC D");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 80px;
        height: 80px;
        font-size: 10px;
        text-align: justify;
        text-align-last: center;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 3);
    if (backend.texts.size() != 3) {
      return;
    }
    check(backend.texts[0].text == "A");
    check(backend.texts[1].text == "B");
    check(backend.texts[2].text == "C D");
    const float expected_last_x =
        (80.0f - approximate_text_width(label->computed_style, "C D")) * 0.5f;
    check(backend.texts[1].x >
          approximate_text_width(label->computed_style, "A ") + 1.0f);
    check(approx_eq(backend.texts[2].x, expected_last_x, 1.0f));
  }
}

spec("RenderManager resolves text-align start and end with direction") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element ltr_start;
    ltr_start.set_layout_bounds(20.0f, 30.0f, 120.0f, 30.0f);
    ltr_start.set_text("Hello");
    ltr_start.computed_style->font_size = 10.0f;
    ltr_start.computed_style->direction = Direction::Ltr;
    ltr_start.computed_style->text_align = TextAlign::Start;

    Element rtl_start;
    rtl_start.set_layout_bounds(20.0f, 70.0f, 120.0f, 30.0f);
    rtl_start.set_text("Hello");
    rtl_start.computed_style->font_size = 10.0f;
    rtl_start.computed_style->direction = Direction::Rtl;
    rtl_start.computed_style->text_align = TextAlign::Start;

    Element rtl_end;
    rtl_end.set_layout_bounds(20.0f, 110.0f, 120.0f, 30.0f);
    rtl_end.set_text("Hello");
    rtl_end.computed_style->font_size = 10.0f;
    rtl_end.computed_style->direction = Direction::Rtl;
    rtl_end.computed_style->text_align = TextAlign::End;

    render_manager.render_tree(&ltr_start);
    render_manager.render_tree(&rtl_start);
    render_manager.render_tree(&rtl_end);

    check(backend.texts.size() == 3);
    check(approx_eq(backend.texts[0].x, 20.0f, 1.0f));
    check(approx_eq(backend.texts[1].x, 106.0f, 1.0f));
    check(approx_eq(backend.texts[2].x, 20.0f, 1.0f));
  }
}

spec("RenderManager applies text transform and letter spacing to plain text layout") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 60.0f, 30.0f);
    elem.set_text("sale");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_align = TextAlign::Center;
    elem.computed_style->text_transform = TextTransform::Uppercase;
    elem.computed_style->letter_spacing = 4.0f;

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    check(backend.texts.front().text == "SALE");
    check(approx_eq(backend.texts.front().x, 30.4f, 1.0f));
  }
}

spec("RenderManager applies parsed CSS text-transform to plain text") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("hello world");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 160px;
        height: 30px;
        font-size: 10px;
        text-transform: uppercase;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 1);
    if (!backend.texts.empty()) {
      check(backend.texts.front().text == "HELLO WORLD");
    }
  }
}

spec("RenderManager applies parsed CSS letter spacing to plain text") {
  it("runs") {
    Box base_box(nullptr);
    Element* base = base_box.create("div", "base");
    base->set_text("A🙂B");
    base_box.set_root(base);
    base_box.set_viewport(240.0f, 120.0f);
    base_box.load_css(R"(
      #base {
        width: 160px;
        height: 40px;
        font-size: 10px;
      }
    )");
    base_box.update();

    RecordingRenderer base_backend;
    base_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer base_renderer(&base_backend);
    RenderManager base_render_manager(&base_renderer);
    base_render_manager.render_tree(base);

    Box spaced_box(nullptr);
    Element* spaced = spaced_box.create("div", "spaced");
    spaced->set_text("A🙂B");
    spaced_box.set_root(spaced);
    spaced_box.set_viewport(240.0f, 120.0f);
    spaced_box.load_css(R"(
      #spaced {
        width: 160px;
        height: 40px;
        font-size: 10px;
        letter-spacing: 4px;
      }
    )");
    spaced_box.update();

    RecordingRenderer spaced_backend;
    spaced_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer spaced_renderer(&spaced_backend);
    RenderManager spaced_render_manager(&spaced_renderer);
    spaced_render_manager.render_tree(spaced);

    check(base_backend.texts.size() == 3);
    check(spaced_backend.texts.size() == 3);
    if (base_backend.texts.size() != 3 || spaced_backend.texts.size() != 3) {
      return;
    }
    check(base_backend.texts[0].text == "A");
    check(base_backend.texts[1].text == "🙂");
    check(base_backend.texts[2].text == "B");
    check(spaced_backend.texts[0].text == "A");
    check(spaced_backend.texts[1].text == "🙂");
    check(spaced_backend.texts[2].text == "B");
    check(spaced_backend.texts[1].x > base_backend.texts[1].x + 3.0f);
    check(spaced_backend.texts[2].x > base_backend.texts[2].x + 7.0f);
  }
}

spec("RenderManager applies word spacing to plain text layout") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 160.0f, 30.0f);
    elem.set_text("Hello world");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->word_spacing = 8.0f;

    ComputedStyle measure_style = *elem.computed_style;
    measure_style.word_spacing = 0.0f;
    const float expected_world_x =
        20.0f + approximate_text_width(&measure_style, "Hello ") + 8.0f;

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "Hello");
    check(backend.texts[1].text == "world");
    check(approx_eq(backend.texts[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.texts[1].x, expected_world_x, 0.001f));
  }
}

spec("RenderManager applies parsed CSS word spacing to plain text") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("Hello world");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 160px;
        height: 30px;
        font-size: 10px;
        word-spacing: 8px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    ComputedStyle measure_style = *label->computed_style;
    measure_style.word_spacing = 0.0f;
    const float expected_world_x =
        approximate_text_width(&measure_style, "Hello ") + 8.0f;

    render_manager.render_tree(label);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "Hello");
    check(backend.texts[1].text == "world");
    check(approx_eq(backend.texts[0].x, 0.0f, 0.001f));
    check(approx_eq(backend.texts[1].x, expected_world_x, 0.001f));
  }
}

spec("RenderManager applies text indent to the first text layout line") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_text("First\nSecond");
    elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 80.0f);
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_indent = 12.0f;

    render_manager.render_tree(&elem);

    const auto first = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "First"; });
    const auto second = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Second"; });
    check(first != backend.texts.end());
    check(second != backend.texts.end());
    if (first == backend.texts.end() || second == backend.texts.end()) {
      return;
    }
    check(approx_eq(first->x, second->x + 12.0f, 0.001f));
  }
}

spec("RenderManager applies parsed CSS text-indent to the first line") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("First\nSecond");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 180px;
        height: 80px;
        font-size: 10px;
        text-indent: 12px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    const auto first = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "First"; });
    const auto second = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Second"; });
    check(first != backend.texts.end());
    check(second != backend.texts.end());
    if (first == backend.texts.end() || second == backend.texts.end()) {
      return;
    }
    check(approx_eq(first->x, second->x + 12.0f, 0.001f));
  }
}

spec("RenderManager applies letter spacing across segmented emoji text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 160.0f, 40.0f);
    elem.set_text("A🙂B");
    elem.computed_style->font_size = 16.0f;

    render_manager.render_tree(&elem);
    check(backend.texts.size() >= 3);
    if (backend.texts.size() < 3) {
      return;
    }
    const auto base_a = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "A"; });
    const auto base_emoji = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto base_b = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(base_a != backend.texts.end());
    check(base_emoji != backend.texts.end());
    check(base_b != backend.texts.end());
    if (base_a == backend.texts.end() || base_emoji == backend.texts.end() ||
        base_b == backend.texts.end()) {
      return;
    }
    const float base_emoji_x = base_emoji->x;
    const float base_b_x = base_b->x;
    const size_t base_count = backend.texts.size();

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    elem.computed_style->letter_spacing = 4.0f;
    render_manager.render_tree(&elem);
    check(backend.texts.size() >= base_count + 3);
    if (backend.texts.size() < base_count + 3) {
      return;
    }
    const auto spaced_a = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(base_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "A"; });
    const auto spaced_emoji = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(base_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto spaced_b = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(base_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(spaced_a != backend.texts.end());
    check(spaced_emoji != backend.texts.end());
    check(spaced_b != backend.texts.end());
    if (spaced_a == backend.texts.end() || spaced_emoji == backend.texts.end() ||
        spaced_b == backend.texts.end()) {
      return;
    }
    check(spaced_emoji->x > base_emoji_x + 3.5f);
    check(spaced_b->x > base_b_x + 7.5f);
  }
}

spec("TextShape segments emoji runs through shared text utilities") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);

    TextShape shape(10.0f, 20.0f, "A🙂B");
    shape.set_font_size(16.0f);
    Renderer renderer(&backend);
    RenderCommandList commands(flex::RendererCapabilities{});
    shape.draw(commands, Transform{}, 1.0f);
    commands.replay(renderer);

    check(backend.texts.size() >= 3);
    if (backend.texts.size() >= 3) {
      check(backend.texts[0].text == "A");
      check(backend.texts[1].text == "🙂");
      check(backend.texts[2].text == "B");
      check(backend.texts[1].x > backend.texts[0].x);
      check(backend.texts[2].x > backend.texts[1].x);
    }
    const auto bounds = shape.local_bounds();
    check(bounds.width > 0.0f);
  }
}

spec("RenderManager truncates plain text with ellipsis for nowrap overflow") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 60.0f, 30.0f);
    elem.set_text("HelloWorld");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    check(backend.texts.front().text.size() >= 3);
    check(backend.texts.front().text.substr(backend.texts.front().text.size() - 3) ==
          "...");
  }
}

spec("RenderManager clips plain text for explicit nowrap text-overflow clip") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 30.0f, 30.0f);
    elem.set_text("HelloWorld");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    elem.computed_style->variables[Symbol("--text-overflow")] = "clip";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    if (backend.texts.empty()) {
      return;
    }
    check(backend.texts.front().text != "HelloWorld");
    check(backend.texts.front().text.find("...") == std::string::npos);
    check(approximate_text_width(elem.computed_style,
                                 backend.texts.front().text) <= 30.0f);
  }
}

spec("RenderManager preserves UTF-8 scalar boundaries in constrained text") {
  it("clips CJK to a whole scalar or an empty line") {
    ComputedStyle style;
    style.font_size = 10.0f;
    style.variables[Symbol("--white-space")] = "nowrap";
    style.variables[Symbol("--text-overflow")] = "clip";
    const std::string text = "\xE4\xB8\xAD\xE6\x96\x87";
    const struct {
      float width;
      const char* expected;
    } cases[] = {{30.0f, "\xE4\xB8\xAD"}, {14.0f, ""}};

    for (const auto& item : cases) {
      const auto calls = render_plain_text(style, text, item.width);
      const std::string expected = item.expected;
      check(calls.size() == (expected.empty() ? 0u : 1u));
      std::string emitted;
      for (const auto& call : calls) {
        emitted += call.text;
      }
      check(emitted == expected);
    }
  }

  it("retains only whole CJK scalars before the ellipsis") {
    ComputedStyle style;
    style.font_size = 10.0f;
    style.variables[Symbol("--white-space")] = "nowrap";
    style.variables[Symbol("--text-overflow")] = "ellipsis";
    const std::string text = "\xE4\xB8\xAD\xE6\x96\x87";
    const struct {
      float width;
      const char* expected;
    } cases[] = {{36.0f, "\xE4\xB8\xAD..."}, {20.0f, "..."}};

    for (const auto& item : cases) {
      const auto calls = render_plain_text(style, text, item.width);
      check(calls.size() == 1);
      if (calls.size() == 1) {
        check(calls.front().text == item.expected);
        check(approximate_text_width(&style, calls.front().text) <= item.width);
      }
    }
  }

  it("keeps emoji intact when anywhere and break-all wrap narrower than a scalar") {
    const std::vector<std::string> expected = {
        "A", "\xF0\x9F\x98\x80", "\xF0\x9F\x9A\x80", "B"};
    std::string text;
    for (const auto& scalar : expected) {
      text += scalar;
    }
    const struct {
      const char* property;
      const char* value;
    } modes[] = {{"--overflow-wrap", "anywhere"}, {"--word-break", "break-all"}};

    for (const auto& mode : modes) {
      ComputedStyle style;
      style.font_size = 10.0f;
      style.variables[Symbol(mode.property)] = mode.value;
      const auto calls = render_plain_text(style, text, 20.0f);
      check(calls.size() == expected.size());
      std::string emitted;
      for (size_t i = 0; i < calls.size(); ++i) {
        emitted += calls[i].text;
        if (i < expected.size()) {
          check(calls[i].text == expected[i]);
        }
        if (i > 0) {
          check(calls[i].y > calls[i - 1].y);
        }
      }
      check(emitted == text);
    }
  }


  it("uses Unicode line-break opportunities between CJK ideographs") {
    ComputedStyle style;
    style.font_size = 10.0f;
    const std::string first = "\xE4\xB8\xAD";
    const std::string second = "\xE6\x96\x87";
    const std::string text = first + second;
    const float width = approximate_text_width(&style, first) + 0.01f;

    const auto block =
        layout_text_block(&style, text, 0.0f, 0.0f, width, 80.0f, Color{});
    check(block.lines.size() == 2);
    if (block.lines.size() == 2) {
      check(block.lines[0].text == first);
      check(block.lines[1].text == second);
    }
  }


  it("does not invent a break around non-breaking space") {
    ComputedStyle style;
    style.font_size = 10.0f;
    const std::string text = std::string("A") + "\xC2\xA0" + "B";
    const float width = approximate_text_width(&style, "A") + 0.01f;

    const auto block =
        layout_text_block(&style, text, 0.0f, 0.0f, width, 80.0f, Color{});
    check(block.lines.size() == 1);
    if (block.lines.size() == 1) {
      check(block.lines[0].text == text);
    }
  }


  it("does not break across Unicode word joiner") {
    ComputedStyle style;
    style.font_size = 10.0f;
    const std::string text =
        std::string("A") + "\xE2\x81\xA0" + "B";  // U+2060 WORD JOINER
    const float width = approximate_text_width(&style, "A") + 0.01f;

    const auto block =
        layout_text_block(&style, text, 0.0f, 0.0f, width, 80.0f, Color{});
    check(block.lines.size() == 1);
    if (block.lines.size() == 1) {
      check(block.lines[0].text == text);
    }
  }


  it("continues after an oversized unbreakable word at the next Unicode break") {
    ComputedStyle style;
    style.font_size = 10.0f;
    const std::string first = "SuperLongToken";
    const std::string second = "next";
    const std::string text = first + " " + second;
    const float width = approximate_text_width(&style, "Super") + 0.01f;

    const auto block =
        layout_text_block(&style, text, 0.0f, 0.0f, width, 80.0f, Color{});
    check(block.lines.size() == 2);
    if (block.lines.size() == 2) {
      check(block.lines[0].text == first);
      check(block.lines[1].text == second);
    }
  }

  it("keeps combining and ZWJ graphemes intact for break-all and anywhere") {
    const std::string combining = std::string("e") + "\xCC\x81";
    const std::string woman_technologist =
        "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB";
    const struct {
      const char* property;
      const char* value;
    } modes[] = {{"--overflow-wrap", "anywhere"}, {"--word-break", "break-all"}};

    for (const auto& cluster : {combining, woman_technologist}) {
      for (const auto& mode : modes) {
        ComputedStyle style;
        style.font_size = 10.0f;
        style.variables[Symbol(mode.property)] = mode.value;
        const std::string text = cluster + "B";
        const float width = approximate_text_width(&style, cluster) + 0.01f;

        const auto block =
            layout_text_block(&style, text, 0.0f, 0.0f, width, 80.0f, Color{});
        check(block.lines.size() == 2);
        if (block.lines.size() == 2) {
          check(block.lines[0].text == cluster);
          check(block.lines[1].text == "B");
        }
      }
    }
  }
}

spec("RenderManager applies parsed CSS text-overflow modes") {
  it("runs") {
    Box ellipsis_box(nullptr);
    Element* ellipsis = ellipsis_box.create("div", "ellipsis");
    ellipsis->set_text("HelloWorld");
    ellipsis_box.set_root(ellipsis);
    ellipsis_box.set_viewport(240.0f, 120.0f);
    ellipsis_box.load_css(R"(
      #ellipsis {
        width: 60px;
        height: 30px;
        font-size: 10px;
        white-space: nowrap;
        text-overflow: ellipsis;
      }
    )");
    ellipsis_box.update();

    RecordingRenderer ellipsis_backend;
    ellipsis_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer ellipsis_renderer(&ellipsis_backend);
    RenderManager ellipsis_render_manager(&ellipsis_renderer);
    ellipsis_render_manager.render_tree(ellipsis);

    check(ellipsis_backend.texts.size() == 1);
    if (ellipsis_backend.texts.empty()) {
      return;
    }
    check(ellipsis_backend.texts.front().text.size() >= 3);
    check(ellipsis_backend.texts.front().text.substr(
              ellipsis_backend.texts.front().text.size() - 3) == "...");

    Box clip_box(nullptr);
    Element* clip = clip_box.create("div", "clip");
    clip->set_text("HelloWorld");
    clip_box.set_root(clip);
    clip_box.set_viewport(240.0f, 120.0f);
    clip_box.load_css(R"(
      #clip {
        width: 30px;
        height: 30px;
        font-size: 10px;
        white-space: nowrap;
        text-overflow: clip;
      }
    )");
    clip_box.update();

    RecordingRenderer clip_backend;
    clip_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer clip_renderer(&clip_backend);
    RenderManager clip_render_manager(&clip_renderer);
    clip_render_manager.render_tree(clip);

    check(clip_backend.texts.size() == 1);
    if (clip_backend.texts.empty()) {
      return;
    }
    check(clip_backend.texts.front().text != "HelloWorld");
    check(clip_backend.texts.front().text.find("...") == std::string::npos);
    check(approximate_text_width(clip->computed_style,
                                 clip_backend.texts.front().text) <= 30.0f);
  }
}

spec("RenderManager wraps plain text to multiple lines by width") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 60.0f, 80.0f);
    elem.set_text("Alpha Beta Gamma");
    elem.computed_style->font_size = 10.0f;

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 3);
    check(backend.texts[0].text == "Alpha");
    check(backend.texts[1].text == "Beta");
    check(backend.texts[2].text == "Gamma");
    check(backend.texts[1].y > backend.texts[0].y);
    check(backend.texts[2].y > backend.texts[1].y);
  }
}

spec("RenderManager applies white-space pre-line semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 80.0f);
    elem.set_text("Alpha   Beta\nGamma");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "pre-line";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 2);
    check(backend.texts[0].text == "Alpha Beta");
    check(backend.texts[1].text == "Gamma");
  }
}

spec("RenderManager applies parsed CSS white-space pre-line") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("Alpha   Beta\nGamma");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 120px;
        height: 80px;
        font-size: 10px;
        white-space: pre-line;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "Alpha Beta");
    check(backend.texts[1].text == "Gamma");
  }
}

spec("RenderManager preserves white-space pre and nowrap lines") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element pre;
    pre.set_layout_bounds(20.0f, 30.0f, 24.0f, 80.0f);
    pre.set_text("A  B\nLongToken");
    pre.computed_style->font_size = 10.0f;
    pre.computed_style->variables[Symbol("--white-space")] = "pre";

    Element nowrap;
    nowrap.set_layout_bounds(20.0f, 130.0f, 24.0f, 80.0f);
    nowrap.set_text("LongToken");
    nowrap.computed_style->font_size = 10.0f;
    nowrap.computed_style->variables[Symbol("--white-space")] = "nowrap";

    render_manager.render_tree(&pre);
    const size_t pre_count = backend.texts.size();
    render_manager.render_tree(&nowrap);

    check(pre_count == 2);
    if (pre_count == 2) {
      check(backend.texts[0].text == "A  B");
      check(backend.texts[1].text == "LongToken");
      check(backend.texts[1].y > backend.texts[0].y);
    }
    check(backend.texts.size() == pre_count + 1);
    if (backend.texts.size() == pre_count + 1) {
      check(backend.texts[pre_count].text == "LongToken");
    }
  }
}

spec("RenderManager applies text-wrap nowrap without changing whitespace collapse") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element text_wrap;
    text_wrap.set_layout_bounds(20.0f, 30.0f, 60.0f, 80.0f);
    text_wrap.set_text("Alpha   Beta Gamma");
    text_wrap.computed_style->font_size = 10.0f;
    text_wrap.computed_style->variables[Symbol("--text-wrap")] = "nowrap";

    Element text_wrap_mode;
    text_wrap_mode.set_layout_bounds(20.0f, 130.0f, 60.0f, 80.0f);
    text_wrap_mode.set_text("Alpha   Beta Gamma");
    text_wrap_mode.computed_style->font_size = 10.0f;
    text_wrap_mode.computed_style->variables[Symbol("--text-wrap-mode")] =
        "nowrap";

    render_manager.render_tree(&text_wrap);
    const size_t text_wrap_count = backend.texts.size();
    render_manager.render_tree(&text_wrap_mode);

    check(text_wrap_count == 1);
    if (text_wrap_count == 1) {
      check(backend.texts[0].text == "Alpha Beta Gamma");
    }
    check(backend.texts.size() == text_wrap_count + 1);
    if (backend.texts.size() == text_wrap_count + 1) {
      check(backend.texts[text_wrap_count].text == "Alpha Beta Gamma");
    }
  }
}

spec("RenderManager applies parsed CSS text-wrap nowrap semantics") {
  it("runs") {
    Box box(nullptr);
    Element* root = box.create("div", "root");
    Element* text_wrap = box.create("div", "text-wrap");
    Element* text_wrap_mode = box.create("div", "text-wrap-mode");
    text_wrap->set_text("Alpha   Beta Gamma");
    text_wrap_mode->set_text("Alpha   Beta Gamma");
    root->append(text_wrap);
    root->append(text_wrap_mode);
    box.set_root(root);
    box.set_viewport(240.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 160px;
        height: 180px;
        display: flex;
        flex-direction: column;
      }
      #text-wrap,
      #text-wrap-mode {
        width: 60px;
        height: 80px;
        font-size: 10px;
      }
      #text-wrap {
        text-wrap: nowrap;
      }
      #text-wrap-mode {
        text-wrap-mode: nowrap;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "Alpha Beta Gamma");
    check(backend.texts[1].text == "Alpha Beta Gamma");
  }
}

spec("RenderManager preserves and wraps white-space break-spaces") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 12.0f, 80.0f);
    elem.set_text("A  B");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "break-spaces";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "A ");
    check(backend.texts[1].text == " B");
    check(backend.texts[1].y > backend.texts[0].y);
  }
}

spec("RenderManager applies overflow-wrap and word-break semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element break_word;
    break_word.set_layout_bounds(20.0f, 30.0f, 40.0f, 80.0f);
    break_word.set_text("SuperLongToken");
    break_word.computed_style->font_size = 10.0f;
    break_word.computed_style->variables[Symbol("--overflow-wrap")] = "break-word";

    Element anywhere;
    anywhere.set_layout_bounds(20.0f, 80.0f, 40.0f, 80.0f);
    anywhere.set_text("SuperLongToken");
    anywhere.computed_style->font_size = 10.0f;
    anywhere.computed_style->variables[Symbol("--overflow-wrap")] = "anywhere";

    Element break_all;
    break_all.set_layout_bounds(20.0f, 130.0f, 20.0f, 80.0f);
    break_all.set_text("ABCD");
    break_all.computed_style->font_size = 10.0f;
    break_all.computed_style->variables[Symbol("--word-break")] = "break-all";

    render_manager.render_tree(&break_word);
    const size_t break_word_count = backend.texts.size();
    render_manager.render_tree(&anywhere);
    const size_t anywhere_count = backend.texts.size();
    render_manager.render_tree(&break_all);

    check(break_word_count >= 2);
    check(backend.texts[0].text == "Super");
    check(backend.texts[1].y > backend.texts[0].y);
    check(anywhere_count >= break_word_count + 2);
    check(backend.texts[break_word_count].text == "Super");
    check(backend.texts[break_word_count + 1].y >
          backend.texts[break_word_count].y);
    check(backend.texts.size() >= anywhere_count + 2);
    check(backend.texts[anywhere_count].text == "AB");
    check(backend.texts[anywhere_count + 1].text == "CD");
  }
}

spec("RenderManager applies parsed CSS overflow wrapping semantics") {
  it("runs") {
    Box break_word_box(nullptr);
    Element* break_word = break_word_box.create("div", "break-word");
    break_word->set_text("SuperLongToken");
    break_word_box.set_root(break_word);
    break_word_box.set_viewport(240.0f, 120.0f);
    break_word_box.load_css(R"(
      #break-word {
        width: 40px;
        height: 80px;
        font-size: 10px;
        overflow-wrap: break-word;
      }
    )");
    break_word_box.update();

    RecordingRenderer break_word_backend;
    break_word_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer break_word_renderer(&break_word_backend);
    RenderManager break_word_render_manager(&break_word_renderer);
    break_word_render_manager.render_tree(break_word);

    check(break_word_backend.texts.size() >= 2);
    if (break_word_backend.texts.size() < 2) {
      return;
    }
    check(break_word_backend.texts[0].text == "Super");
    check(break_word_backend.texts[1].y > break_word_backend.texts[0].y);

    Box anywhere_box(nullptr);
    Element* anywhere = anywhere_box.create("div", "anywhere");
    anywhere->set_text("SuperLongToken");
    anywhere_box.set_root(anywhere);
    anywhere_box.set_viewport(240.0f, 120.0f);
    anywhere_box.load_css(R"(
      #anywhere {
        width: 40px;
        height: 80px;
        font-size: 10px;
        overflow-wrap: anywhere;
      }
    )");
    anywhere_box.update();

    RecordingRenderer anywhere_backend;
    anywhere_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer anywhere_renderer(&anywhere_backend);
    RenderManager anywhere_render_manager(&anywhere_renderer);
    anywhere_render_manager.render_tree(anywhere);

    check(anywhere_backend.texts.size() >= 2);
    if (anywhere_backend.texts.size() < 2) {
      return;
    }
    check(anywhere_backend.texts[0].text == "Super");
    check(anywhere_backend.texts[1].y > anywhere_backend.texts[0].y);

    Box break_all_box(nullptr);
    Element* break_all = break_all_box.create("div", "break-all");
    break_all->set_text("ABCD");
    break_all_box.set_root(break_all);
    break_all_box.set_viewport(240.0f, 120.0f);
    break_all_box.load_css(R"(
      #break-all {
        width: 20px;
        height: 80px;
        font-size: 10px;
        word-break: break-all;
      }
    )");
    break_all_box.update();

    RecordingRenderer break_all_backend;
    break_all_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer break_all_renderer(&break_all_backend);
    RenderManager break_all_render_manager(&break_all_renderer);
    break_all_render_manager.render_tree(break_all);

    check(break_all_backend.texts.size() >= 2);
    if (break_all_backend.texts.size() < 2) {
      return;
    }
    check(break_all_backend.texts[0].text == "AB");
    check(break_all_backend.texts[1].text == "CD");
  }
}

spec("RenderManager treats overflow-wrap anywhere as character breakable") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element break_word;
    break_word.set_layout_bounds(20.0f, 30.0f, 25.0f, 80.0f);
    break_word.set_text("ab-cd");
    break_word.computed_style->font_size = 10.0f;
    break_word.computed_style->variables[Symbol("--overflow-wrap")] =
        "break-word";

    Element anywhere;
    anywhere.set_layout_bounds(20.0f, 80.0f, 25.0f, 80.0f);
    anywhere.set_text("ab-cd");
    anywhere.computed_style->font_size = 10.0f;
    anywhere.computed_style->variables[Symbol("--overflow-wrap")] =
        "anywhere";

    render_manager.render_tree(&break_word);
    const size_t break_word_count = backend.texts.size();
    render_manager.render_tree(&anywhere);

    check(break_word_count == 2);
    if (break_word_count == 2) {
      check(backend.texts[0].text == "ab-");
      check(backend.texts[1].text == "cd");
    }
    check(backend.texts.size() == break_word_count + 2);
    if (backend.texts.size() == break_word_count + 2) {
      check(backend.texts[break_word_count].text == "ab-c");
      check(backend.texts[break_word_count + 1].text == "d");
    }
  }
}

spec("Shared text layout applies tabular numeric width semantics") {
  it("runs") {
    ComputedStyle proportional_style;
    proportional_style.font_size = 20.0f;

    const float narrow_digits = approximate_text_width(&proportional_style, "11");
    const float wide_digits = approximate_text_width(&proportional_style, "88");
    check(narrow_digits < wide_digits);

    ComputedStyle tabular_style = proportional_style;
    tabular_style.variables[Symbol("--font-variant-numeric")] = "tabular-nums";
    const float tabular_narrow = approximate_text_width(&tabular_style, "11");
    const float tabular_wide = approximate_text_width(&tabular_style, "88");
    check(approx_eq(tabular_narrow, tabular_wide, 0.001f));
  }
}

spec("RenderManager applies parsed CSS tabular numeric text widths") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("11\n88");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 120px;
        height: 80px;
        font-size: 20px;
        text-align: center;
        font-variant-numeric: tabular-nums;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "11");
    check(backend.texts[1].text == "88");
    check(approx_eq(backend.texts[0].x, backend.texts[1].x, 0.001f));
    check(approx_eq(approximate_text_width(label->computed_style, "11"),
                    approximate_text_width(label->computed_style, "88"),
                    0.001f));
  }
}

spec("Shared text layout applies tab-size to tab advances") {
  it("runs") {
    ComputedStyle compact;
    compact.font_size = 10.0f;
    compact.tab_size = 2.0f;

    ComputedStyle wide = compact;
    wide.tab_size = 6.0f;

    check(approximate_text_width(&wide, "A\tB") >
          approximate_text_width(&compact, "A\tB") + 10.0f);

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 160.0f, 40.0f);
    elem.set_text("A\tB");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->tab_size = 4.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "pre";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "A");
    check(backend.texts[1].text == "B");
    const float expected_b_x =
        20.0f + approximate_text_width(elem.computed_style, "A\t");
    check(approx_eq(backend.texts[1].x, expected_b_x, 0.001f));
  }
}

spec("RenderManager applies CSS tab-size to plain text tabs") {
  it("runs") {
    Box box(nullptr);
    auto* label = box.create("div", "label");
    label->set_text("A\tB");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 160px;
        height: 40px;
        font-size: 10px;
        tab-size: 6;
        white-space: pre;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "A");
    check(backend.texts[1].text == "B");
    const float expected_b_x =
        approximate_text_width(label->computed_style, "A\t");
    check(approx_eq(backend.texts[1].x, expected_b_x, 0.001f));
  }
}

spec("StepperWidget recenters numeric text with tabular numeric semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 120.0f, 32.0f);
    elem.computed_style->font_size = 20.0f;
    StepperWidget stepper(11, 0, 100, 1);
    elem.widget = &stepper;

    render_widget(stepper, elem, renderer);
    const auto default_11 = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "11"; });
    check(default_11 != backend.texts.end());
    if (default_11 == backend.texts.end()) {
      return;
    }
    const float default_11_x = default_11->x;

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    stepper.set_value(88);
    render_widget(stepper, elem, renderer);
    const auto default_88 = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "88"; });
    check(default_88 != backend.texts.end());
    if (default_88 == backend.texts.end()) {
      return;
    }
    const float default_88_x = default_88->x;
    check(default_11_x > default_88_x);

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    elem.computed_style->variables[Symbol("--font-variant-numeric")] =
        "tabular-nums";
    stepper.set_value(11);
    render_widget(stepper, elem, renderer);
    const auto tabular_11 = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "11"; });
    check(tabular_11 != backend.texts.end());
    if (tabular_11 == backend.texts.end()) {
      return;
    }
    const float tabular_11_x = tabular_11->x;

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    stepper.set_value(88);
    render_widget(stepper, elem, renderer);
    const auto tabular_88 = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "88"; });
    check(tabular_88 != backend.texts.end());
    if (tabular_88 == backend.texts.end()) {
      return;
    }
    const float tabular_delta = std::fabs(tabular_11_x - tabular_88->x);
    const float proportional_delta = std::fabs(default_11_x - default_88_x);
    check(tabular_delta <= proportional_delta);
    check(approximate_text_width(elem.computed_style, "11") ==
          approximate_text_width(elem.computed_style, "88"));
  }
}

spec("StepperWidget and PaginationWidget center control glyphs with shared widths") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element stepper_elem;
    stepper_elem.set_layout_bounds(0.0f, 0.0f, 120.0f, 32.0f);
    stepper_elem.computed_style->font_size = 20.0f;
    StepperWidget stepper(11, 0, 100, 1);
    render_widget(stepper, stepper_elem, renderer);

    const auto minus_call = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "-"; });
    const auto plus_call = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "+"; });
    check(minus_call != backend.texts.end());
    check(plus_call != backend.texts.end());
    if (minus_call == backend.texts.end() || plus_call == backend.texts.end()) {
      return;
    }

    const float button_width = 32.0f;
    const float expected_minus_x =
        (button_width -
         approximate_segmented_text_width(stepper_elem.computed_style, "-")) /
        2.0f;
    const float expected_plus_x =
        stepper_elem.width() - button_width +
        (button_width -
         approximate_segmented_text_width(stepper_elem.computed_style, "+")) /
            2.0f;
    check(approx_eq(minus_call->x, expected_minus_x, 0.001f));
    check(approx_eq(plus_call->x, expected_plus_x, 0.001f));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Element pagination_elem;
    pagination_elem.set_layout_bounds(0.0f, 40.0f, 260.0f, 32.0f);
    pagination_elem.computed_style->font_size = 20.0f;
    PaginationWidget pagination(5, 3);
    render_widget(pagination, pagination_elem, renderer);

    const auto prev_call = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "<"; });
    const auto next_call = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == ">"; });
    check(prev_call != backend.texts.end());
    check(next_call != backend.texts.end());
    if (prev_call == backend.texts.end() || next_call == backend.texts.end()) {
      return;
    }

    const float pagination_button_size = 32.0f;
    const float pagination_gap = 4.0f;
    const float next_button_x =
        (pagination_button_size + pagination_gap) *
        6.0f;
    const float expected_prev_x =
        (pagination_button_size -
         approximate_segmented_text_width(pagination_elem.computed_style, "<")) /
        2.0f;
    const float expected_next_x =
        next_button_x +
        (pagination_button_size -
         approximate_segmented_text_width(pagination_elem.computed_style, ">")) /
            2.0f;
    check(approx_eq(prev_call->x, expected_prev_x, 0.001f));
    check(approx_eq(next_call->x, expected_next_x, 0.001f));
  }
}

spec("BadgeWidget reuses shared text transform semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 60.0f, 24.0f);
    BadgeWidget badge("new");
    elem.widget = &badge;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_transform = TextTransform::Uppercase;

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    check(backend.texts.front().text == "NEW");
  }
}

spec("RenderManager draws CSS text-shadow before plain text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 30.0f);
    elem.set_text("Hello");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_color = {0.7f, 0.8f, 0.9f, 1.0f};
    elem.computed_style->has_text_shadow = true;
    elem.computed_style->text_shadows = {
        TextShadow{2.0f, 3.0f, 4.0f, {0.1f, 0.2f, 0.3f, 0.4f}},
        TextShadow{-1.0f, 1.0f, 0.0f, {0.5f, 0.4f, 0.3f, 0.2f}}};
    elem.computed_style->text_shadow =
        elem.computed_style->text_shadows.front();

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 3);
    if (backend.texts.size() != 3) {
      return;
    }

    check(backend.texts[0].text == "Hello");
    check(backend.texts[1].text == "Hello");
    check(backend.texts[2].text == "Hello");
    check(approx_eq(backend.texts[0].x, backend.texts[2].x + 2.0f,
                    0.001f));
    check(approx_eq(backend.texts[0].y, backend.texts[2].y + 3.0f,
                    0.001f));
    check(approx_eq(backend.texts[1].x, backend.texts[2].x - 1.0f,
                    0.001f));
    check(approx_eq(backend.texts[1].y, backend.texts[2].y + 1.0f,
                    0.001f));
    require_color(backend.texts[0].color, 0.1f, 0.2f, 0.3f, 0.4f);
    require_color(backend.texts[1].color, 0.5f, 0.4f, 0.3f, 0.2f);
    require_color(backend.texts[2].color, 0.7f, 0.8f, 0.9f, 1.0f);
    check(backend.blur_radii.size() == 1);
    if (!backend.blur_radii.empty()) {
      check(approx_eq(backend.blur_radii.front(), 4.0f, 0.001f));
    }
  }
}

spec("RenderManager draws parsed CSS text-shadow before plain text") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("Hello");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 120px;
        height: 30px;
        font-size: 10px;
        color: #b3cce6;
        text-shadow: 2px 3px 4px rgba(26, 51, 77, 0.4),
                     -1px 1px 0 rgba(128, 102, 77, 0.2);
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 3);
    if (backend.texts.size() != 3) {
      return;
    }

    check(backend.texts[0].text == "Hello");
    check(backend.texts[1].text == "Hello");
    check(backend.texts[2].text == "Hello");
    check(approx_eq(backend.texts[0].x, backend.texts[2].x + 2.0f,
                    0.001f));
    check(approx_eq(backend.texts[0].y, backend.texts[2].y + 3.0f,
                    0.001f));
    check(approx_eq(backend.texts[1].x, backend.texts[2].x - 1.0f,
                    0.001f));
    check(approx_eq(backend.texts[1].y, backend.texts[2].y + 1.0f,
                    0.001f));
    require_color(backend.texts[0].color, 26.0f / 255.0f,
                  51.0f / 255.0f, 77.0f / 255.0f, 0.4f);
    require_color(backend.texts[1].color, 128.0f / 255.0f,
                  102.0f / 255.0f, 77.0f / 255.0f, 0.2f);
    require_color(backend.texts[2].color, 179.0f / 255.0f,
                  204.0f / 255.0f, 230.0f / 255.0f, 1.0f);
    check(backend.blur_radii.size() == 1);
    if (!backend.blur_radii.empty()) {
      check(approx_eq(backend.blur_radii.front(), 4.0f, 0.001f));
    }
  }
}

spec("RenderManager draws underline and line-through for plain text decorations") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 30.0f);
    elem.set_text("Hello");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--text-decoration")] =
        "underline line-through";
    elem.computed_style->variables[Symbol("--text-decoration-color")] =
        "51, 102, 153, 255";
    elem.computed_style->variables[Symbol("--text-decoration-thickness")] = "3px";
    elem.computed_style->variables[Symbol("--text-underline-offset")] = "4px";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    check(backend.lines.size() == 2);
    check(backend.lines[0].paint_type == flex::Paint::Type::Solid);
    check(backend.lines[1].paint_type == flex::Paint::Type::Solid);
    check(approx_eq(backend.lines[0].width, 3.0f, 0.001f));
    check(approx_eq(backend.lines[1].width, 3.0f, 0.001f));
    check(approx_eq(backend.lines[0].color.r, 51.0f / 255.0f, 0.001f));
    check(approx_eq(backend.lines[0].color.g, 102.0f / 255.0f, 0.001f));
    check(approx_eq(backend.lines[0].color.b, 153.0f / 255.0f, 0.001f));
    check(approx_eq(backend.lines[0].x1, 20.0f, 0.001f));
    check(backend.lines[0].x2 > backend.lines[0].x1);
    check(backend.lines[0].y1 > backend.lines[1].y1);
  }
}

spec("RenderManager draws CSS text-decoration shorthand") {
  it("runs") {
    Box box(nullptr);
    auto* label = box.create("div", "label");
    label->set_text("Hello");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 120px;
        height: 30px;
        font-size: 10px;
        text-decoration: underline dashed #336699 2px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 1);
    check(backend.lines.size() > 1);
    if (backend.lines.empty()) {
      return;
    }
    require_color(backend.lines.front().color, 0x33 / 255.0f,
                  0x66 / 255.0f, 0x99 / 255.0f);
    check(approx_eq(backend.lines.front().width, 2.0f, 0.001f));
    check(backend.lines.front().x2 < backend.lines[1].x1);
  }
}

spec("RenderManager draws parsed CSS text-decoration longhands") {
  it("runs") {
    Box box(nullptr);
    Element* label = box.create("div", "label");
    label->set_text("Hello");
    box.set_root(label);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #label {
        width: 120px;
        height: 30px;
        font-size: 10px;
        text-decoration-line: underline overline;
        text-decoration-style: dashed;
        text-decoration-color: #336699;
        text-decoration-thickness: 2px;
        text-underline-offset: 4px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(label);

    check(backend.texts.size() == 1);
    check(backend.lines.size() > 2);
    if (backend.lines.size() <= 2) {
      return;
    }
    require_color(backend.lines.front().color, 0x33 / 255.0f,
                  0x66 / 255.0f, 0x99 / 255.0f);
    check(approx_eq(backend.lines.front().width, 2.0f, 0.001f));
    check(backend.lines.front().x2 < backend.lines[1].x1);
    float min_y = backend.lines.front().y1;
    float max_y = backend.lines.front().y1;
    for (const auto& line : backend.lines) {
      min_y = std::min(min_y, line.y1);
      max_y = std::max(max_y, line.y1);
    }
    check(max_y > min_y);
  }
}

spec("RenderManager draws overline and patterned text decorations") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 30.0f);
    elem.set_text("Hello");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--text-decoration")] =
        "overline underline";
    elem.computed_style->variables[Symbol("--text-decoration-style")] =
        "dashed";
    elem.computed_style->variables[Symbol("--text-decoration-color")] =
        "51, 102, 153, 255";
    elem.computed_style->variables[Symbol("--text-decoration-thickness")] =
        "2px";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 1);
    check(backend.lines.size() > 2);
    if (backend.lines.size() <= 2) {
      return;
    }
    for (const auto& line : backend.lines) {
      check(line.paint_type == flex::Paint::Type::Solid);
      check(approx_eq(line.width, 2.0f, 0.001f));
      check(approx_eq(line.color.r, 51.0f / 255.0f, 0.001f));
      check(approx_eq(line.color.g, 102.0f / 255.0f, 0.001f));
      check(approx_eq(line.color.b, 153.0f / 255.0f, 0.001f));
    }
    check(backend.lines.front().y1 < backend.lines.back().y1);
    check(backend.lines[0].x2 < backend.lines[1].x1);
  }
}

spec("RenderManager draws dotted text decorations as short line segments") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 30.0f);
    elem.set_text("Hello");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--text-decoration")] = "underline";
    elem.computed_style->variables[Symbol("--text-decoration-style")] =
        "dotted";
    elem.computed_style->variables[Symbol("--text-decoration-thickness")] =
        "2px";

    render_manager.render_tree(&elem);

    check(backend.lines.size() > 2);
    if (backend.lines.size() <= 2) {
      return;
    }
    const float first_length = backend.lines[0].x2 - backend.lines[0].x1;
    const float second_gap = backend.lines[1].x1 - backend.lines[0].x2;
    check(approx_eq(first_length, 2.0f, 0.001f));
    check(approx_eq(second_gap, 3.0f, 0.001f));
  }
}

spec("RenderManager draws double and wavy text decorations with line commands") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element double_elem;
    double_elem.set_layout_bounds(20.0f, 30.0f, 120.0f, 30.0f);
    double_elem.set_text("Hello");
    double_elem.computed_style->font_size = 10.0f;
    double_elem.computed_style->variables[Symbol("--text-decoration")] =
        "underline";
    double_elem.computed_style->variables[Symbol("--text-decoration-style")] =
        "double";
    double_elem.computed_style->variables[Symbol("--text-decoration-thickness")] =
        "2px";

    Element wavy_elem;
    wavy_elem.set_layout_bounds(20.0f, 80.0f, 120.0f, 30.0f);
    wavy_elem.set_text("Hello");
    wavy_elem.computed_style->font_size = 10.0f;
    wavy_elem.computed_style->variables[Symbol("--text-decoration")] =
        "underline";
    wavy_elem.computed_style->variables[Symbol("--text-decoration-style")] =
        "wavy";
    wavy_elem.computed_style->variables[Symbol("--text-decoration-thickness")] =
        "2px";

    render_manager.render_tree(&double_elem);
    const size_t double_line_count = backend.lines.size();
    render_manager.render_tree(&wavy_elem);

    check(double_line_count == 2);
    if (double_line_count == 2) {
      check(backend.lines[0].y1 < backend.lines[1].y1);
      check(approx_eq(backend.lines[0].width, 2.0f, 0.001f));
      check(approx_eq(backend.lines[1].width, 2.0f, 0.001f));
    }
    check(backend.lines.size() > double_line_count + 2);
    if (backend.lines.size() > double_line_count + 2) {
      check(backend.lines[double_line_count].y2 <
            backend.lines[double_line_count + 1].y2);
    }
  }
}

spec("LabelWidget reuses shared text ellipsis and decoration semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 60.0f, 30.0f);
    LabelWidget label("HelloWorld");
    elem.widget = &label;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_align = TextAlign::Center;
    elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";
    elem.computed_style->variables[Symbol("--text-decoration")] = "underline";

    render_manager.render_tree(&elem);

    if (backend.texts.size() != 1) {
      check(false);
      return;
    }
    check(backend.texts.front().text.size() >= 3);
    check(backend.texts.front().text.substr(backend.texts.front().text.size() - 3) ==
          "...");
    check(approx_eq(backend.texts.front().x, 23.0f, 1.5f));
    if (backend.lines.size() != 1) {
      check(false);
      return;
    }
    check(approx_eq(backend.lines.front().x1, backend.texts.front().x, 0.001f));
    check(backend.lines.front().x2 > backend.lines.front().x1);
  }
}

spec("RenderManager does not draw widget host text twice") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    box.set_viewport(200.0f, 100.0f);
    box.load_css(R"(
      #root {
        width: 200px;
        height: 100px;
        display: flex;
      }
      label {
        width: 120px;
        height: 30px;
        font-size: 10px;
      }
    )");

    auto* root = box.create("div", "root");
    auto* label = box.create_widget<LabelWidget>("label", "status", "Status Text");
    root->append(label);
    box.set_root(root);

    box.update();

    size_t status_text_count = 0;
    for (const auto& text : backend.texts) {
      if (text.text == "Status Text") {
        ++status_text_count;
      }
    }
    check(status_text_count == 1);
  }
}

spec("LabelWidget clamps multiline text through shared layout helper") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 90.0f, 50.0f);
    LabelWidget label("One\nTwo\nThree");
    elem.widget = &label;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--line-clamp")] = "2";

    render_manager.render_tree(&elem);

    if (backend.texts.size() < 2) {
      check(false);
      return;
    }
    check(backend.texts[0].text == "One");
    check(backend.texts[1].text == "Two...");
    check(backend.texts[1].y > backend.texts[0].y);
  }
}

spec("RenderManager clamps plain text with max-lines") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 90.0f, 60.0f);
    elem.set_text("One\nTwo\nThree");
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--max-lines")] = "2";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 2);
    if (backend.texts.size() != 2) {
      return;
    }
    check(backend.texts[0].text == "One");
    check(backend.texts[1].text == "Two...");
  }
}

spec("RenderManager applies parsed CSS text line clamps") {
  it("runs") {
    Box max_lines_box(nullptr);
    Element* max_lines = max_lines_box.create("div", "max-lines");
    max_lines->set_text("One\nTwo\nThree");
    max_lines_box.set_root(max_lines);
    max_lines_box.set_viewport(240.0f, 120.0f);
    max_lines_box.load_css(R"(
      #max-lines {
        width: 90px;
        height: 60px;
        font-size: 10px;
        max-lines: 2;
      }
    )");
    max_lines_box.update();

    RecordingRenderer max_lines_backend;
    max_lines_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer max_lines_renderer(&max_lines_backend);
    RenderManager max_lines_render_manager(&max_lines_renderer);
    max_lines_render_manager.render_tree(max_lines);

    check(max_lines_backend.texts.size() == 2);
    if (max_lines_backend.texts.size() != 2) {
      return;
    }
    check(max_lines_backend.texts[0].text == "One");
    check(max_lines_backend.texts[1].text == "Two...");

    Box webkit_box(nullptr);
    Element* webkit = webkit_box.create("div", "webkit-clamp");
    webkit->set_text("One\nTwo\nThree");
    webkit_box.set_root(webkit);
    webkit_box.set_viewport(240.0f, 120.0f);
    webkit_box.load_css(R"(
      #webkit-clamp {
        width: 90px;
        height: 60px;
        font-size: 10px;
        -webkit-line-clamp: 2;
      }
    )");
    webkit_box.update();

    RecordingRenderer webkit_backend;
    webkit_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer webkit_renderer(&webkit_backend);
    RenderManager webkit_render_manager(&webkit_renderer);
    webkit_render_manager.render_tree(webkit);

    check(webkit_backend.texts.size() == 2);
    if (webkit_backend.texts.size() != 2) {
      return;
    }
    check(webkit_backend.texts[0].text == "One");
    check(webkit_backend.texts[1].text == "Two...");
  }
}

spec("DropdownWidget reuses shared text ellipsis for trigger and menu items") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 90.0f, 28.0f);
    DropdownWidget dropdown("Pick something");
    dropdown.add_option("ExtremelyLongOptionLabel", "alpha");
    dropdown.add_option("AnotherVeryLongOption", "beta");
    dropdown.set_selected_index(0);
    dropdown.open();
    elem.widget = &dropdown;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

    render_manager.render_tree(&elem);
    render_manager.render_overlays(&elem);

    std::vector<std::string> ellipsis_texts;
    for (const auto& call : backend.texts) {
      if (call.text.find("...") != std::string::npos) {
        ellipsis_texts.push_back(call.text);
      }
    }
    const auto overlay_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.y, 62.0f, 0.001f) && call.h >= 32.0f;
        });
    check(overlay_rect != backend.rects.end());
    if (ellipsis_texts.size() < 3) {
      check(false);
      return;
    }
    check(ellipsis_texts[0].find("...") != std::string::npos);
    check(ellipsis_texts[1].find("...") != std::string::npos);
    check(ellipsis_texts[2].find("...") != std::string::npos);
  }
}

spec("SelectWidget reuses shared text ellipsis for trigger and expanded items") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 90.0f, 28.0f);
    SelectWidget select({"ExtremelyLongOptionLabel", "AnotherVeryLongOption"});
    select.set_selected_index(0);
    select.set_expanded(true);
    elem.widget = &select;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";
    select.update(200.0f, elem);

    render_manager.render_tree(&elem);
    render_manager.render_overlays(&elem);

    std::vector<std::string> ellipsis_texts;
    for (const auto& call : backend.texts) {
      if (call.text.find("...") != std::string::npos) {
        ellipsis_texts.push_back(call.text);
      }
    }
    if (ellipsis_texts.size() < 3) {
      check(false);
      return;
    }
    check(ellipsis_texts[0].find("...") != std::string::npos);
    check(ellipsis_texts[1].find("...") != std::string::npos);
    check(ellipsis_texts[2].find("...") != std::string::npos);
  }
}

spec("BreadcrumbWidget and PaginationWidget render active semantics with shared colors") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element breadcrumb_elem;
    breadcrumb_elem.set_layout_bounds(0.0f, 0.0f, 240.0f, 24.0f);
    breadcrumb_elem.computed_style->font_size = 14.0f;
    breadcrumb_elem.computed_style->variables[Symbol("--breadcrumb-color")] =
        "100, 116, 139, 255";
    breadcrumb_elem.computed_style->variables[Symbol("--breadcrumb-color-active")] =
        "15, 23, 42, 255";
    breadcrumb_elem.computed_style->variables[Symbol("--breadcrumb-separator-color")] =
        "148, 163, 184, 255";
    BreadcrumbWidget breadcrumb;
    breadcrumb.set_items({{"home", "Home"}, {"settings", "Settings"}, {"billing", "Billing"}});
    breadcrumb.set_separator("/");
    breadcrumb_elem.widget = &breadcrumb;

    render_manager.render_tree(&breadcrumb_elem);

    const auto billing_text = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Billing"; });
    check(billing_text != backend.texts.end());
    if (billing_text != backend.texts.end()) {
      require_color(billing_text->color, 15.0f / 255.0f, 23.0f / 255.0f,
                    42.0f / 255.0f);
    }

    const auto separator_text = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "/"; });
    check(separator_text != backend.texts.end());
    if (separator_text != backend.texts.end()) {
      require_color(separator_text->color, 148.0f / 255.0f, 163.0f / 255.0f,
                    184.0f / 255.0f);
    }

    backend.begin_frame(800.0f, 600.0f, 1.0f);

    Element pagination_elem;
    pagination_elem.set_layout_bounds(0.0f, 40.0f, 220.0f, 32.0f);
    pagination_elem.computed_style->font_size = 14.0f;
    pagination_elem.computed_style->variables[Symbol("--pagination-bg-active")] =
        "59, 130, 246, 255";
    pagination_elem.computed_style->variables[Symbol("--pagination-text-active")] =
        "255, 255, 255, 255";
    pagination_elem.computed_style->variables[Symbol("--pagination-border")] =
        "203, 213, 225, 255";
    PaginationWidget pagination(5, 3);
    pagination_elem.widget = &pagination;

    render_manager.render_tree(&pagination_elem);

    const auto active_page_text = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "3"; });
    check(active_page_text != backend.texts.end());
    if (active_page_text != backend.texts.end()) {
      require_color(active_page_text->color, 1.0f, 1.0f, 1.0f);
    }

    const auto active_page_bg = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 59.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 130.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 246.0f / 255.0f, 0.001f) &&
                 approx_eq(call.w, 32.0f, 0.001f) &&
                 approx_eq(call.h, 32.0f, 0.001f);
        });
    check(active_page_bg != backend.rects.end());
  }
}

spec("BadgeWidget reuses shared text alignment and decoration semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 60.0f, 24.0f);
    BadgeWidget badge("99+");
    elem.widget = &badge;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->text_align = TextAlign::Center;
    elem.computed_style->variables[Symbol("--text-decoration")] = "line-through";

    render_manager.render_tree(&elem);

    if (backend.texts.size() != 1) {
      check(false);
      return;
    }
    check(approx_eq(backend.texts.front().x, 39.8f, 1.5f));
    if (backend.lines.size() != 1) {
      check(false);
      return;
    }
    check(backend.lines.front().x2 > backend.lines.front().x1);
  }
}

spec("TabsWidget reuses shared text ellipsis and decoration semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 150.0f, 28.0f);
    TabsWidget tabs;
    tabs.add_tab("ExtremelyLongTabLabel", "first");
    tabs.add_tab("AnotherVeryLongTabLabel", "second");
    elem.widget = &tabs;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";
    elem.computed_style->variables[Symbol("--text-decoration")] = "underline";

    render_manager.render_tree(&elem);

    std::vector<std::string> ellipsis_texts;
    for (const auto& call : backend.texts) {
      if (call.text.find("...") != std::string::npos) {
        ellipsis_texts.push_back(call.text);
      }
    }
    if (ellipsis_texts.size() < 2) {
      check(false);
      return;
    }
    check(backend.lines.size() >= 2);
  }
}

spec("Tooltip and toast reuse shared text layout semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element tooltip_target;
    tooltip_target.set_hover(true);

    Element tooltip_elem;
    tooltip_elem.set_layout_bounds(20.0f, 30.0f, 90.0f, 28.0f);
    tooltip_target.append(&tooltip_elem);
    TooltipWidget tooltip("VeryLongTooltipText");
    tooltip.update(600.0f, tooltip_elem);
    tooltip_elem.widget = &tooltip;
    tooltip_elem.computed_style->font_size = 10.0f;
    tooltip_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    tooltip_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";
    tooltip_elem.computed_style->variables[Symbol("--text-decoration")] = "underline";

    Element toast_elem;
    toast_elem.set_layout_bounds(20.0f, 70.0f, 120.0f, 32.0f);
    ToastWidget toast("VeryLongToastMessage", ToastWidget::Type::Info);
    toast.show();
    toast.update(200.0f, toast_elem);
    toast_elem.widget = &toast;
    toast_elem.computed_style->font_size = 10.0f;
    toast_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    toast_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

    render_manager.render_tree(&tooltip_target);
    render_manager.render_tree(&toast_elem);

    std::vector<std::string> ellipsis_texts;
    for (const auto& call : backend.texts) {
      if (call.text.find("...") != std::string::npos) {
        ellipsis_texts.push_back(call.text);
      }
    }
    if (ellipsis_texts.size() < 2) {
      check(false);
      return;
    }
    check(backend.lines.size() >= 1);
  }
}

spec("Modal and divider reuse shared text layout semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element modal_elem;
    modal_elem.set_layout_bounds(0.0f, 0.0f, 240.0f, 160.0f);
    ModalWidget modal("VeryLongModalTitleThatNeedsEllipsis");
    modal.open();
    modal.update(1000.0f, modal_elem);
    modal_elem.widget = &modal;
    modal_elem.computed_style->font_size = 10.0f;
    modal_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    modal_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

    Element divider_elem;
    divider_elem.set_layout_bounds(20.0f, 180.0f, 140.0f, 24.0f);
    DividerWidget divider(DividerWidget::Orientation::Horizontal);
    divider.set_label("VeryLongDividerLabel");
    divider_elem.widget = &divider;
    divider_elem.computed_style->font_size = 10.0f;
    divider_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    divider_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

    render_manager.render_tree(&modal_elem);
    render_manager.render_tree(&divider_elem);

    std::vector<std::string> ellipsis_texts;
    for (const auto& call : backend.texts) {
      if (call.text.find("...") != std::string::npos) {
        ellipsis_texts.push_back(call.text);
      }
    }
    if (ellipsis_texts.empty() || backend.texts.size() < 2) {
      check(false);
      return;
    }
  }
}

spec("ModalWidget anchors side sheet geometry against the requested edge") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div", "root");
    auto* elem = box.create_widget<ModalWidget>("div", "sheet", "Settings");
    auto* modal = static_cast<ModalWidget*>(elem->widget);
    modal->set_side(ModalWidget::Side::Right);
    modal->set_show_close_button(true);
    modal->open();
    root->append(elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
    box.load_css(
        "#sheet { width: 320px; height: 200px; --modal-width: 180px; "
        "--modal-height: 200px; --modal-bg: #f8fafc; }");
    box.update_time(100.0f);
    box.update();

    const auto sheet_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 248.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 250.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 252.0f / 255.0f, 0.001f) &&
                 call.x >= 140.0f && call.x <= 150.0f &&
                 approx_eq(call.y, 0.0f, 0.001f) &&
                 call.w >= 170.0f &&
                 call.h >= 190.0f;
        });
    check(sheet_rect != backend.rects.end());

    check(elem->attribute("data-side") != nullptr);
    if (const std::string* side = elem->attribute("data-side")) {
      check(*side == "right");
    }
  }
}

spec("TabsWidget uses shared segmented widths for tab allocation") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 260.0f, 32.0f);
    elem.computed_style->font_size = 16.0f;
    TabsWidget tabs;
    tabs.add_tab("A🙂B", "emoji");
    tabs.add_tab("Wide", "wide");
    elem.widget = &tabs;

    render_widget(tabs, elem, renderer);

    ComputedStyle measure_style = *elem.computed_style;
    measure_style.font_size = 16.0f;
    const float first_width =
        approximate_segmented_text_width(&measure_style, "A🙂B") + 32.0f;
    const auto second_tab = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Wide"; });
    check(second_tab != backend.texts.end());
    if (second_tab != backend.texts.end()) {
      check(second_tab->x >= first_width - 0.001f);
    }
  }
}

spec("Breadcrumb divider and toggle group use shared segmented text widths") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element breadcrumb_elem;
    breadcrumb_elem.set_layout_bounds(0.0f, 0.0f, 240.0f, 24.0f);
    breadcrumb_elem.computed_style->font_size = 16.0f;
    BreadcrumbWidget breadcrumb;
    breadcrumb.set_items({{"home", "Home"}, {"emoji", "A🙂B"}, {"tail", "Tail"}});
    breadcrumb.set_separator("›");
    breadcrumb_elem.widget = &breadcrumb;

    Element divider_elem;
    divider_elem.set_layout_bounds(20.0f, 40.0f, 160.0f, 24.0f);
    divider_elem.computed_style->font_size = 16.0f;
    DividerWidget divider(DividerWidget::Orientation::Horizontal);
    divider.set_label("A🙂B");
    divider_elem.widget = &divider;

    Element toggle_elem;
    toggle_elem.set_layout_bounds(0.0f, 80.0f, 120.0f, 32.0f);
    toggle_elem.computed_style->font_size = 16.0f;
    ToggleGroupWidget toggle(
        {{"left", "A🙂B"}, {"right", "Wide"}});
    toggle_elem.widget = &toggle;

    render_manager.render_tree(&breadcrumb_elem);
    const size_t breadcrumb_count = backend.texts.size();
    render_manager.render_tree(&divider_elem);
    const size_t divider_count = backend.texts.size();
    render_manager.render_tree(&toggle_elem);

    const float gap = 8.0f;
    const float home_width =
        approximate_segmented_text_width(breadcrumb_elem.computed_style, "Home");
    const float separator_width =
        approximate_segmented_text_width(breadcrumb_elem.computed_style, "›");
    const float expected_breadcrumb_x = home_width + gap + separator_width + gap;
    const auto breadcrumb_a = std::find_if(
        backend.texts.begin(), backend.texts.begin() + static_cast<std::ptrdiff_t>(breadcrumb_count),
        [](const TextCall& call) { return call.text == "A"; });
    check(breadcrumb_a != backend.texts.begin() + static_cast<std::ptrdiff_t>(breadcrumb_count));
    if (breadcrumb_a != backend.texts.begin() + static_cast<std::ptrdiff_t>(breadcrumb_count)) {
      check(approx_eq(breadcrumb_a->x, expected_breadcrumb_x, 0.001f));
    }

    const float divider_label_width =
        approximate_segmented_text_width(divider_elem.computed_style, "A🙂B");
    const float expected_divider_x =
        divider_elem.absolute_x() + divider_elem.width() / 2.0f - divider_label_width / 2.0f;
    const auto divider_a = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(breadcrumb_count),
        backend.texts.begin() + static_cast<std::ptrdiff_t>(divider_count),
        [](const TextCall& call) { return call.text == "A"; });
    check(divider_a != backend.texts.begin() + static_cast<std::ptrdiff_t>(divider_count));
    if (divider_a != backend.texts.begin() + static_cast<std::ptrdiff_t>(divider_count)) {
      check(approx_eq(divider_a->x, expected_divider_x, 0.001f));
    }

    const float toggle_width = toggle_elem.width() / 2.0f;
    const float toggle_label_width =
        approximate_segmented_text_width(toggle_elem.computed_style, "A🙂B");
    const float expected_toggle_x = (toggle_width - toggle_label_width) / 2.0f;
    const auto toggle_a = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(divider_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "A"; });
    check(toggle_a != backend.texts.end());
    if (toggle_a != backend.texts.end()) {
      check(approx_eq(toggle_a->x, expected_toggle_x, 0.001f));
    }
  }
}

spec("LabelWidget clamps auto-wrapped text through shared layout helper") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 60.0f, 50.0f);
    LabelWidget label("Alpha Beta Gamma");
    elem.widget = &label;
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->variables[Symbol("--line-clamp")] = "2";

    render_manager.render_tree(&elem);

    check(backend.texts.size() == 2);
    check(backend.texts[0].text == "Alpha");
    check(backend.texts[1].text == "Beta...");
  }
}

spec("Accordion table tree and calendar reuse shared text layout semantics") {
  RecordingRenderer backend;
  backend.begin_frame(800.0f, 600.0f, 1.0f);
  Renderer renderer(&backend);
  RenderManager render_manager(&renderer);

  Element accordion_elem;
  accordion_elem.set_layout_bounds(20.0f, 20.0f, 120.0f, 80.0f);
  AccordionWidget accordion;
  accordion.add_section("ExtremelyLongAccordionSectionTitle", "section");
  accordion.expand("section");
  accordion_elem.widget = &accordion;
  accordion_elem.computed_style->font_size = 10.0f;
  accordion_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
  accordion_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

  Element table_elem;
  table_elem.set_layout_bounds(20.0f, 120.0f, 160.0f, 100.0f);
  TableWidget table;
  table.add_column("ExtremelyLongColumnHeader", 80.0f);
  table.add_row({"VeryLongCellValue"});
  table_elem.widget = &table;
  table_elem.computed_style->font_size = 10.0f;
  table_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
  table_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

  Element tree_elem;
  tree_elem.set_layout_bounds(200.0f, 20.0f, 120.0f, 100.0f);
  TreeWidget tree;
  tree.add_node("root", "VeryLongTreeNodeLabel");
  tree_elem.widget = &tree;
  tree_elem.computed_style->font_size = 10.0f;
  tree_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
  tree_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

  Element calendar_elem;
  calendar_elem.set_layout_bounds(200.0f, 140.0f, 180.0f, 160.0f);
  CalendarWidget calendar;
  calendar.set_view_date({2024, 9, 1});
  calendar_elem.widget = &calendar;
  calendar_elem.computed_style->font_size = 10.0f;
  calendar_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
  calendar_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";

  render_manager.render_tree(&accordion_elem);
  render_manager.render_tree(&table_elem);
  render_manager.render_tree(&tree_elem);
  render_manager.render_tree(&calendar_elem);

  std::vector<std::string> ellipsis_texts;
  for (const auto& call : backend.texts) {
    if (call.text.find("...") != std::string::npos) {
      ellipsis_texts.push_back(call.text);
    }
  }
  if (ellipsis_texts.size() < 4) {
    check(false);
    return;
  }
}

spec("CalendarWidget centers weekday and day labels in cells") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 320.0f, 340.0f);
    CalendarWidget calendar;
    elem.widget = &calendar;
    elem.computed_style->font_size = 10.0f;

    render_manager.render_tree(&elem);

    auto day_one =
        std::find_if(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "1"; });
    if (day_one == backend.texts.end()) {
      check(false);
      return;
    }
    check(day_one->x > 80.0f);
  }
}

spec("CalendarWidget maps pointer hits to the rendered day grid") {
  it("selects the clicked day with non-zero element offsets") {
    Element elem;
    elem.set_layout_bounds(100.0f, 200.0f, 320.0f, 340.0f);
    CalendarWidget calendar;
    calendar.set_view_date({2024, 1, 1});
    elem.widget = &calendar;
    calendar.bind_host_element(&elem);

    const float start_y = 80.0f;
    const float cell_w = elem.width() / 7.0f;
    const float cell_h = (elem.height() - start_y) / 6.0f;
    const int first_day = 1;
    const int day = 14;
    const int pos = first_day + day - 1;
    const int row = pos / 7;
    const int col = pos % 7;
    const float hit_x = elem.absolute_x() + col * cell_w + cell_w * 0.5f;
    const float hit_y = elem.absolute_y() + start_y + row * cell_h + cell_h * 0.5f;

    check(calendar.handle_event(Event::mouse_down(hit_x, hit_y), elem));
    check(calendar.selected_date().day == 14);
  }

  it("selects the day under the rendered text coordinates") {
    Element elem;
    elem.set_layout_bounds(100.0f, 200.0f, 320.0f, 340.0f);
    elem.computed_style->font_size = 10.0f;
    CalendarWidget calendar;
    calendar.set_view_date({2024, 1, 1});
    elem.widget = &calendar;
    calendar.bind_host_element(&elem);

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(&elem);

    const auto day_14 =
        std::find_if(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "14"; });
    check(day_14 != backend.texts.end());
    if (day_14 == backend.texts.end()) {
      return;
    }

    check(calendar.handle_event(
        Event::mouse_down(day_14->x + 2.0f, day_14->y + day_14->size * 0.5f),
        elem));
    check(calendar.selected_date().day == 14);
  }

  it("bridges the hovered day without changing widget paint output") {
    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 320.0f, 340.0f);
    elem.computed_style->font_size = 10.0f;
    CalendarWidget calendar;
    calendar.set_view_date({2024, 1, 1});
    elem.widget = &calendar;
    calendar.bind_host_element(&elem);

    const float start_y = 80.0f;
    const float cell_w = elem.width() / 7.0f;
    const float cell_h = (elem.height() - start_y) / 6.0f;
    const int first_day = 1;
    const int hover_day = 30;
    const int pos = first_day + hover_day - 1;
    const int row = pos / 7;
    const int col = pos % 7;
    const float hover_x = elem.absolute_x() + col * cell_w + cell_w * 0.5f;
    const float hover_y = elem.absolute_y() + start_y + row * cell_h + cell_h * 0.5f;

    check(calendar.handle_event(Event::mouse_move(hover_x, hover_y), elem));
    check(elem.attribute("data-hover-day") != nullptr);
    if (const auto* hovered = elem.attribute("data-hover-day")) {
      check(*hovered == "30");
    }

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    render_widget(calendar, elem, renderer);

    check(backend.circles.size() == 1);
    if (!backend.circles.empty()) {
      require_color(backend.circles.front().fill_color, 0.23f, 0.51f, 0.96f);
    }

    check(calendar.handle_event(
        Event::mouse_move(elem.absolute_x() + 2.0f,
                          elem.absolute_y() + start_y - 2.0f),
        elem));
    check(elem.attribute("data-hover-day") == nullptr);
  }
}

spec("RenderManager draws siblings in z-index order") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    root.computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};

    Element low;
    low.set_layout_bounds(10.0f, 20.0f, 40.0f, 30.0f);
    low.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    low.computed_style->z_index = 1;
    low.set_z_index(1);

    Element high;
    high.set_layout_bounds(70.0f, 20.0f, 40.0f, 30.0f);
    high.computed_style->background_color = {0.8f, 0.2f, 0.1f, 1.0f};
    high.computed_style->z_index = 5;
    high.set_z_index(5);

    root.append(&high);
    root.append(&low);

    render_manager.render_tree(&root);

    check(backend.rects.size() == 2);
    check(approx_eq(backend.rects[0].x, 10.0f, 0.001f));
    check(approx_eq(backend.rects[1].x, 70.0f, 0.001f));
  }
}

spec("RenderManager draws CSS z-index siblings in paint order") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* high = box.create("div", "high");
    auto* low = box.create("div", "low");
    root->append(high);
    root->append(low);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        position: relative;
        width: 220px;
        height: 80px;
      }
      #low {
        position: absolute;
        left: 10px;
        top: 20px;
        width: 40px;
        height: 30px;
        background-color: #336699;
        z-index: 1;
      }
      #high {
        position: absolute;
        left: 70px;
        top: 20px;
        width: 40px;
        height: 30px;
        background-color: #cc331a;
        z-index: 5;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    std::vector<DrawRectCall> child_rects;
    for (const auto& rect : backend.rects) {
      if (approx_eq(rect.w, 40.0f, 0.001f) &&
          approx_eq(rect.h, 30.0f, 0.001f) && rect.fill_color.a > 0.99f) {
        child_rects.push_back(rect);
      }
    }

    check(child_rects.size() == 2);
    if (child_rects.size() != 2) {
      return;
    }

    require_color(child_rects[0].fill_color, 0x33 / 255.0f, 0x66 / 255.0f,
                  0x99 / 255.0f);
    require_color(child_rects[1].fill_color, 0xcc / 255.0f, 0x33 / 255.0f,
                  0x1a / 255.0f);
  }
}

spec("RenderManager paints CSS positioned elements at resolved offsets") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* absolute = box.create("div", "absolute");
    auto* fixed = box.create("div", "fixed");
    root->append(absolute);
    root->append(fixed);
    box.set_root(root);
    box.set_viewport(500.0f, 400.0f);

    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
      }
      #absolute {
        position: absolute;
        inset: 10px 20px 30px 40px;
        width: 40px;
        height: 30px;
        background-color: #336699;
      }
      #fixed {
        position: fixed;
        right: 10px;
        bottom: 20px;
        width: 50px;
        height: 30px;
        background-color: #cc331a;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto absolute_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f);
        });
    check(absolute_rect != backend.rects.end());
    if (absolute_rect != backend.rects.end()) {
      check(approx_eq(absolute_rect->x, 40.0f, 0.001f));
      check(approx_eq(absolute_rect->y, 10.0f, 0.001f));
      check(approx_eq(absolute_rect->w, 40.0f, 0.001f));
      check(approx_eq(absolute_rect->h, 30.0f, 0.001f));
    }

    const auto fixed_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0xcc / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x1a / 255.0f, 0.001f);
        });
    check(fixed_rect != backend.rects.end());
    if (fixed_rect != backend.rects.end()) {
      check(approx_eq(fixed_rect->x, 440.0f, 0.001f));
      check(approx_eq(fixed_rect->y, 350.0f, 0.001f));
      check(approx_eq(fixed_rect->w, 50.0f, 0.001f));
      check(approx_eq(fixed_rect->h, 30.0f, 0.001f));
    }
  }
}

spec("RenderManager paints fixed CSS inset stretch against viewport") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* fixed = box.create("div", "fixed");
    root->append(fixed);
    box.set_root(root);
    box.set_viewport(500.0f, 400.0f);

    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
      }
      #fixed {
        position: fixed;
        left: 10px;
        right: 20px;
        top: 15px;
        bottom: 25px;
        background-color: #336699;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto fixed_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f);
        });
    check(fixed_rect != backend.rects.end());
    if (fixed_rect != backend.rects.end()) {
      check(approx_eq(fixed_rect->x, 10.0f, 0.001f));
      check(approx_eq(fixed_rect->y, 15.0f, 0.001f));
      check(approx_eq(fixed_rect->w, 470.0f, 0.001f));
      check(approx_eq(fixed_rect->h, 360.0f, 0.001f));
    }
  }
}

spec("RenderManager paints logical CSS inset offsets after direction mapping") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* ltr = box.create("div", "ltr");
    auto* rtl = box.create("div", "rtl");
    root->append(ltr);
    root->append(rtl);
    box.set_root(root);
    box.set_viewport(360.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 240px;
        height: 120px;
      }
      #ltr {
        position: absolute;
        inset-inline: 5px 15px;
        inset-block-start: 7px;
        width: 30px;
        height: 20px;
        background-color: #336699;
      }
      #rtl {
        direction: rtl;
        position: absolute;
        inset-inline: 5px 15px;
        inset-block-start: 47px;
        width: 30px;
        height: 20px;
        background-color: #cc331a;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto ltr_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f);
        });
    check(ltr_rect != backend.rects.end());
    if (ltr_rect != backend.rects.end()) {
      check(approx_eq(ltr_rect->x, 5.0f, 0.001f));
      check(approx_eq(ltr_rect->y, 7.0f, 0.001f));
    }

    const auto rtl_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0xcc / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x1a / 255.0f, 0.001f);
        });
    check(rtl_rect != backend.rects.end());
    if (rtl_rect != backend.rects.end()) {
      check(approx_eq(rtl_rect->x, 15.0f, 0.001f));
      check(approx_eq(rtl_rect->y, 47.0f, 0.001f));
    }
  }
}

spec("RenderManager paints CSS flex and grid layouts at computed positions") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* flex = box.create("div", "flex");
    auto* flex_a = box.create("div", "flex-a");
    auto* flex_b = box.create("div", "flex-b");
    auto* grid = box.create("div", "grid");
    auto* grid_a = box.create("div", "grid-a");
    auto* grid_b = box.create("div", "grid-b");
    root->append(flex);
    root->append(grid);
    flex->append(flex_a);
    flex->append(flex_b);
    grid->append(grid_a);
    grid->append(grid_b);
    box.set_root(root);
    box.set_viewport(360.0f, 240.0f);

    box.load_css(R"(
      #root {
        width: 320px;
        height: 200px;
      }
      #flex {
        display: flex;
        flex-direction: row;
        gap: 10px;
        width: 180px;
        height: 30px;
      }
      #flex-a {
        width: 30px;
        height: 20px;
        background-color: #336699;
      }
      #flex-b {
        width: 40px;
        height: 20px;
        background-color: #cc331a;
      }
      #grid {
        display: grid;
        grid-template-columns: 50px 60px;
        column-gap: 12px;
        row-gap: 8px;
        width: 140px;
        height: 40px;
      }
      #grid-a {
        width: 50px;
        height: 20px;
        background-color: #22aa66;
      }
      #grid-b {
        grid-column: 2;
        width: 60px;
        height: 20px;
        background-color: #8855cc;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto flex_a_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f);
        });
    const auto flex_b_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0xcc / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x1a / 255.0f, 0.001f);
        });
    check(flex_a_rect != backend.rects.end());
    check(flex_b_rect != backend.rects.end());
    if (flex_a_rect != backend.rects.end() &&
        flex_b_rect != backend.rects.end()) {
      check(approx_eq(flex_a_rect->x, 0.0f, 0.001f));
      check(approx_eq(flex_b_rect->x, 40.0f, 0.001f));
    }

    const auto grid_a_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x22 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0xaa / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x66 / 255.0f, 0.001f);
        });
    const auto grid_b_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x88 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x55 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0xcc / 255.0f, 0.001f);
        });
    check(grid_a_rect != backend.rects.end());
    check(grid_b_rect != backend.rects.end());
    if (grid_a_rect != backend.rects.end() &&
        grid_b_rect != backend.rects.end()) {
      check(approx_eq(grid_a_rect->x, 0.0f, 0.001f));
      check(approx_eq(grid_b_rect->x, 62.0f, 0.001f));
    }
  }
}

spec("RenderManager paints CSS box sizing and aspect ratio dimensions") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* border_box = box.create("div", "border-box");
    auto* content_box = box.create("div", "content-box");
    auto* ratio = box.create("div", "ratio");
    root->append(border_box);
    root->append(content_box);
    root->append(ratio);
    box.set_root(root);
    box.set_viewport(360.0f, 260.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        gap: 10px;
        width: 320px;
        height: 240px;
      }
      #border-box,
      #content-box {
        width: 100px;
        height: 50px;
        padding: 10px 20px;
        border-width: 5px;
      }
      #border-box {
        box-sizing: border-box;
        background-color: #336699;
      }
      #content-box {
        box-sizing: content-box;
        background-color: #cc331a;
      }
      #ratio {
        width: 160px;
        aspect-ratio: 16 / 9;
        background-color: #22aa66;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto border_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x66 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x99 / 255.0f, 0.001f);
        });
    check(border_rect != backend.rects.end());
    if (border_rect != backend.rects.end()) {
      check(approx_eq(border_rect->w, 100.0f, 0.001f));
      check(approx_eq(border_rect->h, 50.0f, 0.001f));
    }

    const auto content_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0xcc / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x1a / 255.0f, 0.001f);
        });
    check(content_rect != backend.rects.end());
    if (content_rect != backend.rects.end()) {
      check(approx_eq(content_rect->w, 150.0f, 0.001f));
      check(approx_eq(content_rect->h, 80.0f, 0.001f));
    }

    const auto ratio_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x22 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0xaa / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x66 / 255.0f, 0.001f);
        });
    check(ratio_rect != backend.rects.end());
    if (ratio_rect != backend.rects.end()) {
      check(approx_eq(ratio_rect->w, 160.0f, 0.001f));
      check(approx_eq(ratio_rect->h, 90.0f, 0.001f));
    }
  }
}

spec("RenderManager uses active opacity transitions during rendering") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
    button->set_layout_bounds(0.0f, 0.0f, 100.0f, 40.0f);

    box.load_css(R"(
      .btn {
        opacity: 1;
        transition: opacity 0.2s linear;
        width: 100px;
        height: 40px;
        background-color: #336699;
      }
      .btn:hover {
        opacity: 0.2;
      }
    )");

    box.update();
    button->set_hover(true);
    box.update();

    check_false(backend.alphas.empty());
    check(approx_eq(backend.alphas.back(), 1.0f, 0.001f));

    box.update_time(100.0f);
    box.update();

    check_false(backend.alphas.empty());
    check(approx_eq(backend.alphas.back(), 0.6f, 0.02f));
  }
}

spec("RenderManager uses active transform transitions during rendering") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .btn {
        transition: transform 0.2s linear;
        transform: translate(0, 0);
        width: 100px;
        height: 40px;
        background-color: #336699;
      }
      .btn:hover {
        transform: translate(20, 10);
      }
    )");

    box.update();
    button->set_hover(true);
    box.update();

    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.back().x, 0.0f, 0.001f));
    check(approx_eq(backend.rects.back().y, 0.0f, 0.001f));

    box.update_time(100.0f);
    box.update();

    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.back().x, 10.0f, 0.02f));
    check(approx_eq(backend.rects.back().y, 5.0f, 0.02f));
  }
}

spec("RenderManager keeps CSS rotate transitions in degrees") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .btn {
        transition: transform 0.2s linear;
        transform-origin: left top;
        transform: rotate(0deg);
        width: 100px;
        height: 40px;
        background-color: #336699;
      }
      .btn:hover {
        transform: rotate(90deg);
      }
    )");

    box.update();
    button->set_hover(true);
    box.update();
    box.update_time(100.0f);
    box.update();

    check_false(backend.rotations.empty());
    if (!backend.rotations.empty()) {
      check(approx_eq(backend.rotations.back(), 45.0f, 0.02f));
    }
  }
}

spec("RenderManager applies transform-origin to scale transforms") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 50.0f);
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->transform_scale = 2.0f;
    elem.computed_style->transform_origin_x = 1.0f;
    elem.computed_style->transform_origin_y = 1.0f;
    elem.computed_style->transform_origin_x_percent = true;
    elem.computed_style->transform_origin_y_percent = true;

    render_manager.render_tree(&elem);

    check(backend.translations.size() >= 3);
    check(approx_eq(backend.translations[0].first, 20.0f, 0.001f));
    check(approx_eq(backend.translations[0].second, 30.0f, 0.001f));
    check(approx_eq(backend.translations[1].first, 100.0f, 0.001f));
    check(approx_eq(backend.translations[1].second, 50.0f, 0.001f));
    check(approx_eq(backend.translations[2].first, -100.0f, 0.001f));
    check(approx_eq(backend.translations[2].second, -50.0f, 0.001f));
  }
}

spec("RenderManager applies variable-backed transform-origin from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* panel = box.create("div", "panel");
    box.set_root(panel);
    box.set_viewport(200.0f, 120.0f);

    box.load_css(R"(
      #panel {
        --radix-tooltip-content-transform-origin: right bottom;
        width: 100px;
        height: 50px;
        background-color: #336699;
        transform: scale(2);
        transform-origin: var(--radix-tooltip-content-transform-origin);
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.capabilities_.scaling = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(panel);

    check(backend.translations.size() >= 2);
    if (backend.translations.size() < 2) {
      return;
    }
    check(approx_eq(backend.translations[0].first, 100.0f, 0.001f));
    check(approx_eq(backend.translations[0].second, 50.0f, 0.001f));
    check(approx_eq(backend.translations[1].first, -100.0f, 0.001f));
    check(approx_eq(backend.translations[1].second, -50.0f, 0.001f));
  }
}

spec("RenderManager forwards CSS individual transform properties") {
  it("runs") {
    Box box(nullptr);
    auto* panel = box.create("div", "panel");
    box.set_root(panel);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #panel {
        width: 100px;
        height: 50px;
        background-color: #336699;
        transform-origin: left top;
        translate: 8px 13px;
        scale: 1.25 0.5;
        rotate: 0.5turn;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(panel);

    check_false(backend.translations.empty());
    if (!backend.translations.empty()) {
      check(approx_eq(backend.translations.front().first, 8.0f, 0.001f));
      check(approx_eq(backend.translations.front().second, 13.0f, 0.001f));
    }
    check_false(backend.scales.empty());
    if (!backend.scales.empty()) {
      check(approx_eq(backend.scales.front().first, 1.25f, 0.001f));
      check(approx_eq(backend.scales.front().second, 0.5f, 0.001f));
    }
    check_false(backend.rotations.empty());
    if (!backend.rotations.empty()) {
      check(approx_eq(backend.rotations.front(), 180.0f, 0.001f));
    }
  }
}

spec("RenderManager emits affine CSS transform matrices") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 50.0f);
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->has_transform_matrix = true;
    elem.computed_style->transform_origin_x = 0.0f;
    elem.computed_style->transform_origin_y = 0.0f;
    elem.computed_style->transform_origin_x_percent = false;
    elem.computed_style->transform_origin_y_percent = false;
    elem.computed_style->transform_matrix.data[0] = 1.0f;
    elem.computed_style->transform_matrix.data[1] = 0.25f;
    elem.computed_style->transform_matrix.data[2] = 7.0f;
    elem.computed_style->transform_matrix.data[3] = 0.5f;
    elem.computed_style->transform_matrix.data[4] = 1.0f;
    elem.computed_style->transform_matrix.data[5] = 9.0f;

    render_manager.render_tree(&elem);

    check_false(backend.transforms.empty());
    if (backend.transforms.empty()) {
      return;
    }
    const auto& transform = backend.transforms.front();
    check(approx_eq(transform.data[0], 1.0f, 0.001f));
    check(approx_eq(transform.data[1], 0.25f, 0.001f));
    check(approx_eq(transform.data[2], 27.0f, 0.001f));
    check(approx_eq(transform.data[3], 0.5f, 0.001f));
    check(approx_eq(transform.data[4], 1.0f, 0.001f));
    check(approx_eq(transform.data[5], 39.0f, 0.001f));
  }
}

spec("RenderManager emits CSS parsed affine transform matrices") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* matrix = box.create("div", "matrix");
    auto* skewed = box.create("div", "skewed");
    root->append(matrix);
    root->append(skewed);
    box.set_root(root);
    box.set_viewport(240.0f, 180.0f);

    box.load_css(R"(
      #root {
        width: 200px;
        height: 120px;
      }
      #matrix {
        width: 100px;
        height: 40px;
        background-color: #336699;
        transform-origin: left top;
        transform: matrix(1, 0.5, 0.25, 1, 7px, 9px);
      }
      #skewed {
        width: 100px;
        height: 40px;
        background-color: #669933;
        transform-origin: left top;
        transform: skewX(45deg);
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    check(backend.transforms.size() >= 2);
    if (backend.transforms.size() < 2) {
      return;
    }

    const auto& matrix_transform = backend.transforms[0];
    check(approx_eq(matrix_transform.data[0], 1.0f, 0.001f));
    check(approx_eq(matrix_transform.data[1], 0.25f, 0.001f));
    check(approx_eq(matrix_transform.data[2], 7.0f, 0.001f));
    check(approx_eq(matrix_transform.data[3], 0.5f, 0.001f));
    check(approx_eq(matrix_transform.data[4], 1.0f, 0.001f));
    check(approx_eq(matrix_transform.data[5], 9.0f, 0.001f));

    const auto& skew_transform = backend.transforms[1];
    check(approx_eq(skew_transform.data[1], 1.0f, 0.001f));
    check(approx_eq(skew_transform.data[3], 0.0f, 0.001f));
  }
}

spec("RenderManager prefixes widget transforms with ancestor CSS transforms") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.scaling = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 120.0f, 80.0f);
    root.computed_style->transform_scale = 2.0f;
    root.computed_style->transform_origin_x = 0.0f;
    root.computed_style->transform_origin_y = 0.0f;

    Element child;
    child.set_layout_bounds(5.0f, 7.0f, 60.0f, 30.0f);
    ButtonWidget button("Save");
    child.widget = &button;
    root.append(&child);

    render_manager.render_tree(&root);

    check_false(backend.transforms.empty());
    if (!backend.transforms.empty()) {
      check(approx_eq(flex::tx(backend.transforms.front()), 20.0f, 0.001f));
      check(approx_eq(flex::ty(backend.transforms.front()), 34.0f, 0.001f));
    }
  }
}

spec("RenderManager uses active background-color transitions during rendering") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    Box box(nullptr);

    Element root;
    root.owner_box_ = &box;
    root.set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    root.computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};

    Element child;
    child.set_layout_bounds(10.0f, 20.0f, 100.0f, 40.0f);
    child.computed_style->background_color = {0.6f, 0.8f, 0.2f, 1.0f};
    root.append(&child);

    const auto element_id = reinterpret_cast<std::uintptr_t>(&child);
    TransitionDef def;
    def.property = "background-color";
    def.duration_ms = 200.0f;
    def.delay_ms = 0.0f;
    def.easing = EasingType::Linear;
    box.transitions().start(element_id, "background-color-r", 0.2f, 0.6f, def,
                            0.0f);
    box.transitions().start(element_id, "background-color-g", 0.4f, 0.8f, def,
                            0.0f);
    box.transitions().start(element_id, "background-color-b", 0.6f, 0.2f, def,
                            0.0f);
    box.transitions().start(element_id, "background-color-a", 1.0f, 1.0f, def,
                            0.0f);
    box.update_time(100.0f);
    render_manager.render_tree(&root);

    check_false(backend.rects.empty());
    require_color(backend.rects.back().fill_color, 0.4f, 0.6f, 0.4f);
  }
}

spec("RenderManager draws CSS linear-gradient backgrounds with linear paint") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient.angle = 90.0f;
    elem.computed_style->gradient.stop_count = 2;
    elem.computed_style->gradient.stops[0] = {0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    elem.computed_style->gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};

    render_manager.render_tree(&elem);

    check_false(backend.rects.empty());
    const auto& fill = backend.rects.front();
    check(fill.fill_type == flex::Paint::Type::Linear);
    check(fill.fill_linear.stops.size() == 2);
    check(approx_eq(fill.fill_linear.x1, 0.0f, 0.001f));
    check(approx_eq(fill.fill_linear.y1, 0.5f, 0.001f));
    check(approx_eq(fill.fill_linear.x2, 1.0f, 0.001f));
    check(approx_eq(fill.fill_linear.y2, 0.5f, 0.001f));
    check(approx_eq(fill.fill_linear.stops[0].offset, 0.0f, 0.001f));
    check(approx_eq(fill.fill_linear.stops[1].offset, 1.0f, 0.001f));
  }
}

spec("RenderManager draws CSS radial-gradient backgrounds with radial paint") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient.stop_count = 2;
    elem.computed_style->gradient.stops[0] = {0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    elem.computed_style->gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 1.0f, 0.0f)};
    elem.computed_style->variables[Symbol("__flex_background_gradient_type")] = "radial";
    elem.computed_style->variables[Symbol("__flex_background_radial_position")] = "right top";
    elem.computed_style->variables[Symbol("__flex_background_radial_size")] = "closest-side";

    render_manager.render_tree(&elem);

    check(backend.rects.size() >= 1);
    const auto& fill = backend.rects.back();
    check(fill.fill_type == flex::Paint::Type::Radial);
    check(approx_eq(fill.fill_radial.cx, 1.0f, 0.001f));
    check(approx_eq(fill.fill_radial.cy, 0.0f, 0.001f));
    check(approx_eq(fill.fill_radial.radius, 0.001f, 0.0001f));
    check(fill.fill_radial.stops.size() == 2);
    check(approx_eq(fill.fill_radial.stops[1].offset, 1.0f, 0.001f));
  }
}

spec("RenderManager applies background position size and repeat to gradients") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient.angle = 90.0f;
    elem.computed_style->gradient.stop_count = 2;
    elem.computed_style->gradient.stops[0] = {0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    elem.computed_style->gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};
    elem.computed_style->variables[Symbol("__flex_background_size")] = "40px 20px";
    elem.computed_style->variables[Symbol("__flex_background_position")] = "center";
    elem.computed_style->variables[Symbol("__flex_background_repeat")] = "no-repeat";

    render_manager.render_tree(&elem);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return call.fill_type == flex::Paint::Type::Linear &&
                                    std::fabs(call.w - 40.0f) <= 0.001f &&
                                    std::fabs(call.h - 20.0f) <= 0.001f;
                           });
    check(it != backend.rects.end());
    check(approx_eq(it->x, 50.0f, 0.001f));
    check(approx_eq(it->y, 60.0f, 0.001f));
  }
}

spec("RenderManager resolves four-value background-position offsets") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient.angle = 90.0f;
    elem.computed_style->gradient.stop_count = 2;
    elem.computed_style->gradient.stops[0] = {
        0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    elem.computed_style->gradient.stops[1] = {
        1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};
    elem.computed_style->variables[Symbol("__flex_background_size")] =
        "20px 10px";
    elem.computed_style->variables[Symbol("__flex_background_position")] =
        "right 10px bottom 5px";
    elem.computed_style->variables[Symbol("__flex_background_repeat")] =
        "no-repeat";

    render_manager.render_tree(&elem);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return call.fill_type == flex::Paint::Type::Linear;
                           });
    check(it != backend.rects.end());
    if (it == backend.rects.end()) {
      return;
    }
    check(approx_eq(it->x, 90.0f, 0.001f));
    check(approx_eq(it->y, 95.0f, 0.001f));
    check(approx_eq(it->w, 20.0f, 0.001f));
    check(approx_eq(it->h, 10.0f, 0.001f));
  }
}

spec("RenderManager resolves background-repeat round and space geometry") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    BackgroundImageLayer round;
    round.has_gradient = true;
    round.gradient_type = "linear";
    round.position = "left top";
    round.size = "33px 80px";
    round.repeat = "round no-repeat";
    round.gradient.angle = 90.0f;
    round.gradient.stop_count = 2;
    round.gradient.stops[0] = {
        0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    round.gradient.stops[1] = {
        1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};

    BackgroundImageLayer space = round;
    space.size = "20px 80px";
    space.repeat = "space no-repeat";

    elem.computed_style->background_layers = {space, round};

    render_manager.render_tree(&elem);

    std::vector<DrawRectCall> space_tiles;
    std::vector<DrawRectCall> round_tiles;
    for (const auto& call : backend.rects) {
      if (call.fill_type != flex::Paint::Type::Linear) {
        continue;
      }
      if (approx_eq(call.w, 20.0f, 0.001f)) {
        space_tiles.push_back(call);
      } else if (approx_eq(call.w, 100.0f / 3.0f, 0.001f)) {
        round_tiles.push_back(call);
      }
    }

    check(space_tiles.size() == 5);
    check(round_tiles.size() == 3);
    if (space_tiles.size() != 5 || round_tiles.size() != 3) {
      return;
    }

    check(approx_eq(space_tiles[0].x, 20.0f, 0.001f));
    check(approx_eq(space_tiles[1].x, 40.0f, 0.001f));
    check(approx_eq(space_tiles[4].x, 100.0f, 0.001f));
    check(approx_eq(round_tiles[0].x, 20.0f, 0.001f));
    check(approx_eq(round_tiles[1].x, 20.0f + 100.0f / 3.0f, 0.001f));
    check(approx_eq(round_tiles[2].x, 20.0f + 200.0f / 3.0f, 0.001f));
  }
}

spec("RenderManager treats background-size contain on gradients as positioning-area sized") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient.angle = 90.0f;
    elem.computed_style->gradient.stop_count = 2;
    elem.computed_style->gradient.stops[0] = {
        0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    elem.computed_style->gradient.stops[1] = {
        1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};
    elem.computed_style->variables[Symbol("__flex_background_size")] = "contain";
    elem.computed_style->variables[Symbol("__flex_background_position")] = "center";
    elem.computed_style->variables[Symbol("__flex_background_repeat")] = "no-repeat";

    render_manager.render_tree(&elem);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return call.fill_type == flex::Paint::Type::Linear;
                           });
    check(it != backend.rects.end());
    if (it == backend.rects.end()) {
      return;
    }
    check(approx_eq(it->x, 20.0f, 0.001f));
    check(approx_eq(it->y, 30.0f, 0.001f));
    check(approx_eq(it->w, 100.0f, 0.001f));
    check(approx_eq(it->h, 80.0f, 0.001f));
  }
}

spec("RenderManager treats single-value background-size height as auto") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    Box box(&backend);
    auto* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #card {
        width: 70px;
        height: 40px;
        background-image: url(tile.png);
        background-size: 20px;
        background-repeat: no-repeat;
        background-position: left top;
      }
    )");
    box.update();

    check(backend.images.size() == 1);
    if (backend.images.size() != 1) {
      return;
    }
    check(backend.images[0].src == "tile.png");
    check(approx_eq(backend.images[0].x, 0.0f, 0.001f));
    check(approx_eq(backend.images[0].y, 0.0f, 0.001f));
    check(approx_eq(backend.images[0].w, 20.0f, 0.001f));
    check(approx_eq(backend.images[0].h, 40.0f, 0.001f));
  }
}

spec("RenderManager resolves background-size auto from SVG intrinsic ratio") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    BackgroundImageLayer image;
    image.has_image_url = true;
    image.image_url = "<svg width=\"100\" height=\"50\"></svg>";
    image.position = "left top";
    image.size = "20px auto";
    image.repeat = "no-repeat";
    elem.computed_style->background_layers = {image};

    render_manager.render_tree(&elem);

    check(backend.svgs.size() == 1);
    if (backend.svgs.size() != 1) {
      return;
    }
    check(approx_eq(backend.svgs[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.svgs[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.svgs[0].w, 20.0f, 0.001f));
    check(approx_eq(backend.svgs[0].h, 10.0f, 0.001f));
  }
}

spec("RenderManager draws layered gradient backgrounds from back to front") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    BackgroundImageLayer top;
    top.has_gradient = true;
    top.gradient_type = "linear";
    top.position = "left top";
    top.size = "100% 100%";
    top.repeat = "no-repeat";
    top.gradient.angle = 90.0f;
    top.gradient.stop_count = 2;
    top.gradient.stops[0] = {0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    top.gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};

    BackgroundImageLayer back;
    back.has_gradient = true;
    back.gradient_type = "radial";
    back.radial_position = "center";
    back.radial_size = "farthest-corner";
    back.position = "center";
    back.size = "40px 20px";
    back.repeat = "no-repeat";
    back.gradient.stop_count = 2;
    back.gradient.stops[0] = {0.0f, flex::Color(0.0f, 1.0f, 0.0f, 1.0f)};
    back.gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 0.0f, 0.0f)};

    elem.computed_style->background_layers = {top, back};
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient = top.gradient;

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 2);
    if (backend.rects.size() != 2) {
      return;
    }

    check(backend.rects[0].fill_type == flex::Paint::Type::Radial);
    check(approx_eq(backend.rects[0].x, 50.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 60.0f, 0.001f));
    check(approx_eq(backend.rects[0].w, 40.0f, 0.001f));
    check(approx_eq(backend.rects[0].h, 20.0f, 0.001f));

    check(backend.rects[1].fill_type == flex::Paint::Type::Linear);
    check(approx_eq(backend.rects[1].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[1].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[1].w, 100.0f, 0.001f));
    check(approx_eq(backend.rects[1].h, 80.0f, 0.001f));
  }
}

spec("RenderManager applies background-clip to background fills and gradients") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->padding[0] = 5.0f;
    elem.computed_style->padding[1] = 7.0f;
    elem.computed_style->padding[2] = 5.0f;
    elem.computed_style->padding[3] = 7.0f;
    elem.computed_style->background_clip = BackgroundClip::ContentBox;
    elem.computed_style->background_origin = BackgroundClip::ContentBox;
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient.angle = 90.0f;
    elem.computed_style->gradient.stop_count = 2;
    elem.computed_style->gradient.stops[0] = {0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    elem.computed_style->gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};
    elem.computed_style->variables[Symbol("__flex_background_repeat")] = "no-repeat";

    render_manager.render_tree(&elem);

    check(backend.rects.size() >= 2);
    check(approx_eq(backend.rects[0].x, 29.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 37.0f, 0.001f));
    check(approx_eq(backend.rects[0].w, 82.0f, 0.001f));
    check(approx_eq(backend.rects[0].h, 66.0f, 0.001f));
    require_color(backend.rects[0].fill_color, 0.2f, 0.4f, 0.6f, 1.0f);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return call.fill_type == flex::Paint::Type::Linear;
                           });
    check(it != backend.rects.end());
    if (it == backend.rects.end()) {
      return;
    }
    check(approx_eq(it->x, 29.0f, 0.001f));
    check(approx_eq(it->y, 37.0f, 0.001f));
    check(approx_eq(it->w, 82.0f, 0.001f));
    check(approx_eq(it->h, 66.0f, 0.001f));
  }
}

spec("RenderManager applies CSS background-origin and clip boxes") {
  it("runs") {
    Box box(nullptr);
    auto* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #card {
        width: 100px;
        height: 80px;
        border: 2px solid transparent;
        padding: 5px 7px;
        background-image: linear-gradient(90deg, red, blue);
        background-size: 100% 100%;
        background-repeat: no-repeat;
        background-origin: content-box;
        background-clip: content-box;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(card);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return call.fill_type == flex::Paint::Type::Linear;
                           });
    check(it != backend.rects.end());
    if (it == backend.rects.end()) {
      return;
    }
    check(approx_eq(it->x, 9.0f, 0.001f));
    check(approx_eq(it->y, 7.0f, 0.001f));
    check(approx_eq(it->w, 100.0f, 0.001f));
    check(approx_eq(it->h, 80.0f, 0.001f));
  }
}

spec("RenderManager uses background-origin for background positioning") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->padding[0] = 5.0f;
    elem.computed_style->padding[1] = 7.0f;
    elem.computed_style->padding[2] = 5.0f;
    elem.computed_style->padding[3] = 7.0f;
    elem.computed_style->background_clip = BackgroundClip::BorderBox;
    elem.computed_style->background_origin = BackgroundClip::ContentBox;
    elem.computed_style->has_gradient = true;
    elem.computed_style->gradient.angle = 90.0f;
    elem.computed_style->gradient.stop_count = 2;
    elem.computed_style->gradient.stops[0] = {
        0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    elem.computed_style->gradient.stops[1] = {
        1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};
    elem.computed_style->variables[Symbol("__flex_background_size")] =
        "40px 20px";
    elem.computed_style->variables[Symbol("__flex_background_position")] =
        "left top";
    elem.computed_style->variables[Symbol("__flex_background_repeat")] =
        "no-repeat";

    render_manager.render_tree(&elem);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return call.fill_type == flex::Paint::Type::Linear;
                           });
    check(it != backend.rects.end());
    if (it == backend.rects.end()) {
      return;
    }

    check(approx_eq(it->x, 29.0f, 0.001f));
    check(approx_eq(it->y, 37.0f, 0.001f));
    check(approx_eq(it->w, 40.0f, 0.001f));
    check(approx_eq(it->h, 20.0f, 0.001f));
  }
}

spec("RenderManager separates background-origin from background-clip") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->padding[0] = 5.0f;
    elem.computed_style->padding[1] = 7.0f;
    elem.computed_style->padding[2] = 5.0f;
    elem.computed_style->padding[3] = 7.0f;
    elem.computed_style->background_clip = BackgroundClip::ContentBox;

    BackgroundImageLayer image;
    image.has_image_url = true;
    image.image_url = "hero.png";
    image.origin = "border-box";
    image.position = "left top";
    image.size = "100% 100%";
    image.repeat = "no-repeat";
    elem.computed_style->background_layers = {image};

    render_manager.render_tree(&elem);

    check(!backend.clips.empty());
    check(backend.images.size() == 1);
    if (backend.clips.empty() || backend.images.size() != 1) {
      return;
    }

    check(approx_eq(backend.clips.front().x, 29.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 37.0f, 0.001f));
    check(approx_eq(backend.clips.front().w, 82.0f, 0.001f));
    check(approx_eq(backend.clips.front().h, 66.0f, 0.001f));
    check(approx_eq(backend.images.front().x, 20.0f, 0.001f));
    check(approx_eq(backend.images.front().y, 30.0f, 0.001f));
    check(approx_eq(backend.images.front().w, 100.0f, 0.001f));
    check(approx_eq(backend.images.front().h, 80.0f, 0.001f));
  }
}

spec("RenderManager draws shorthand background gradients with tiled placement") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* child = box.create("div", "child");
    root->append(child);
    box.set_root(root);

    box.load_css(R"(
      #child {
        background: radial-gradient(circle at right top, #ff0000 0%, transparent 70%)
                    center / 40px 20px no-repeat #112233;
      }
    )");
    box.update();

    child->set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(child);

    check(backend.rects.size() >= 2);
    require_color(backend.rects.front().fill_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);

    auto it = std::find_if(backend.rects.begin(), backend.rects.end(),
                           [](const DrawRectCall& call) {
                             return call.fill_type == flex::Paint::Type::Radial &&
                                    std::fabs(call.w - 40.0f) <= 0.001f &&
                                    std::fabs(call.h - 20.0f) <= 0.001f;
                           });
    check(it != backend.rects.end());
    check(approx_eq(it->x, 50.0f, 0.001f));
    check(approx_eq(it->y, 60.0f, 0.001f));
  }
}

spec("RenderManager draws layered background shorthand gradients from back to front") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* child = box.create("div", "child");
    root->append(child);
    box.set_root(root);

    box.load_css(R"(
      #child {
        background:
          linear-gradient(90deg, #ff0000 0%, #0000ff 100%) left top / 100% 100% no-repeat,
          radial-gradient(circle at right top, #00ff00 0%, transparent 70%) center / 40px 20px no-repeat,
          #112233;
      }
    )");
    box.update();

    child->set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    render_manager.render_tree(child);

    check(backend.rects.size() >= 3);
    if (backend.rects.size() < 3) {
      return;
    }

    require_color(backend.rects[0].fill_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
    check(backend.rects[1].fill_type == flex::Paint::Type::Radial);
    check(approx_eq(backend.rects[1].x, 50.0f, 0.001f));
    check(approx_eq(backend.rects[1].y, 60.0f, 0.001f));
    check(approx_eq(backend.rects[1].w, 40.0f, 0.001f));
    check(approx_eq(backend.rects[1].h, 20.0f, 0.001f));
    check(backend.rects[2].fill_type == flex::Paint::Type::Linear);
    check(approx_eq(backend.rects[2].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[2].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[2].w, 100.0f, 0.001f));
    check(approx_eq(backend.rects[2].h, 80.0f, 0.001f));
  }
}

spec("RenderManager draws layered url background images with tiling metadata") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    BackgroundImageLayer image;
    image.has_image_url = true;
    image.image_url = "hero.png";
    image.position = "center";
    image.size = "24px 12px";
    image.repeat = "no-repeat";

    BackgroundImageLayer gradient;
    gradient.has_gradient = true;
    gradient.gradient_type = "linear";
    gradient.position = "left top";
    gradient.size = "100% 100%";
    gradient.repeat = "no-repeat";
    gradient.gradient.angle = 90.0f;
    gradient.gradient.stop_count = 2;
    gradient.gradient.stops[0] = {0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    gradient.gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};

    elem.computed_style->background_layers = {image, gradient};

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 1);
    check(backend.images.size() == 1);
    if (backend.rects.size() != 1 || backend.images.size() != 1) {
      return;
    }

    check(backend.rects[0].fill_type == flex::Paint::Type::Linear);
    check(backend.images[0].src == "hero.png");
    check(approx_eq(backend.images[0].x, 58.0f, 0.001f));
    check(approx_eq(backend.images[0].y, 64.0f, 0.001f));
    check(approx_eq(backend.images[0].w, 24.0f, 0.001f));
    check(approx_eq(backend.images[0].h, 12.0f, 0.001f));
  }
}

spec("RenderManager applies CSS background-position-x and y longhands") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    Box box(&backend);
    auto* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #card {
        width: 100px;
        height: 80px;
        background-image: url(hero.png);
        background-size: 20px 10px;
        background-repeat: no-repeat;
        background-position-x: right 10px;
        background-position-y: bottom 5px;
      }
    )");
    box.update();

    check(backend.images.size() == 1);
    if (backend.images.size() != 1) {
      return;
    }
    check(backend.images[0].src == "hero.png");
    check(approx_eq(backend.images[0].x, 70.0f, 0.001f));
    check(approx_eq(backend.images[0].y, 65.0f, 0.001f));
    check(approx_eq(backend.images[0].w, 20.0f, 0.001f));
    check(approx_eq(backend.images[0].h, 10.0f, 0.001f));
  }
}

spec("RenderManager applies CSS background-size and repeat longhands") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    Box box(&backend);
    auto* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #card {
        width: 70px;
        height: 40px;
        background-image: url(tile.png);
        background-size: 20px 10px;
        background-repeat: repeat-x;
        background-position: left 0px top 5px;
      }
    )");
    box.update();

    check(backend.images.size() == 4);
    if (backend.images.size() != 4) {
      return;
    }
    for (size_t i = 0; i < backend.images.size(); ++i) {
      check(backend.images[i].src == "tile.png");
      check(approx_eq(backend.images[i].x, static_cast<float>(i) * 20.0f,
                      0.001f));
      check(approx_eq(backend.images[i].y, 5.0f, 0.001f));
      check(approx_eq(backend.images[i].w, 20.0f, 0.001f));
      check(approx_eq(backend.images[i].h, 10.0f, 0.001f));
    }
  }
}

spec("RenderManager applies CSS background repeat-y longhand") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    Box box(&backend);
    auto* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #card {
        width: 40px;
        height: 70px;
        background-image: url(tile.png);
        background-size: 10px 20px;
        background-repeat: repeat-y;
        background-position: left 5px top 0px;
      }
    )");
    box.update();

    check(backend.images.size() == 4);
    if (backend.images.size() != 4) {
      return;
    }
    for (size_t i = 0; i < backend.images.size(); ++i) {
      check(backend.images[i].src == "tile.png");
      check(approx_eq(backend.images[i].x, 5.0f, 0.001f));
      check(approx_eq(backend.images[i].y, static_cast<float>(i) * 20.0f,
                      0.001f));
      check(approx_eq(backend.images[i].w, 10.0f, 0.001f));
      check(approx_eq(backend.images[i].h, 20.0f, 0.001f));
    }
  }
}

spec("RenderManager falls back conservatively for cover image backgrounds without intrinsic size") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->background_clip = BackgroundClip::ContentBox;
    elem.computed_style->background_origin = BackgroundClip::ContentBox;
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->padding[0] = 5.0f;
    elem.computed_style->padding[1] = 7.0f;
    elem.computed_style->padding[2] = 5.0f;
    elem.computed_style->padding[3] = 7.0f;

    BackgroundImageLayer image;
    image.has_image_url = true;
    image.image_url = "hero.png";
    image.position = "center";
    image.size = "cover";
    image.repeat = "no-repeat";
    elem.computed_style->background_layers = {image};

    render_manager.render_tree(&elem);

    check(backend.images.size() == 1);
    if (backend.images.size() != 1) {
      return;
    }
    check(approx_eq(backend.images[0].x, 29.0f, 0.001f));
    check(approx_eq(backend.images[0].y, 37.0f, 0.001f));
    check(approx_eq(backend.images[0].w, 82.0f, 0.001f));
    check(approx_eq(backend.images[0].h, 66.0f, 0.001f));
  }
}

spec("RenderManager applies per-layer background-clip boxes") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->padding[0] = 5.0f;
    elem.computed_style->padding[1] = 7.0f;
    elem.computed_style->padding[2] = 5.0f;
    elem.computed_style->padding[3] = 7.0f;

    BackgroundImageLayer top;
    top.has_gradient = true;
    top.gradient_type = "linear";
    top.position = "left top";
    top.size = "100% 100%";
    top.repeat = "no-repeat";
    top.clip = "border-box";
    top.gradient.angle = 90.0f;
    top.gradient.stop_count = 2;
    top.gradient.stops[0] = {0.0f, flex::Color(1.0f, 0.0f, 0.0f, 1.0f)};
    top.gradient.stops[1] = {1.0f, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)};

    BackgroundImageLayer back = top;
    back.clip = "content-box";
    elem.computed_style->background_layers = {top, back};

    render_manager.render_tree(&elem);

    check(backend.clips.size() >= 2);
    if (backend.clips.size() < 2) {
      return;
    }

    check(approx_eq(backend.clips[0].x, 29.0f, 0.001f));
    check(approx_eq(backend.clips[0].y, 37.0f, 0.001f));
    check(approx_eq(backend.clips[0].w, 82.0f, 0.001f));
    check(approx_eq(backend.clips[0].h, 66.0f, 0.001f));
    check(approx_eq(backend.clips[1].x, 20.0f, 0.001f));
    check(approx_eq(backend.clips[1].y, 30.0f, 0.001f));
    check(approx_eq(backend.clips[1].w, 100.0f, 0.001f));
    check(approx_eq(backend.clips[1].h, 80.0f, 0.001f));
  }
}

spec("RenderManager applies per-layer background-clip to image layers") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->background_clip = BackgroundClip::BorderBox;
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->padding[0] = 5.0f;
    elem.computed_style->padding[1] = 7.0f;
    elem.computed_style->padding[2] = 5.0f;
    elem.computed_style->padding[3] = 7.0f;

    BackgroundImageLayer image;
    image.has_image_url = true;
    image.image_url = "hero.png";
    image.position = "left top";
    image.size = "100% 100%";
    image.repeat = "no-repeat";
    image.origin = "border-box";
    image.clip = "content-box";
    elem.computed_style->background_layers = {image};

    render_manager.render_tree(&elem);

    check(!backend.clips.empty());
    check(backend.images.size() == 1);
    if (backend.clips.empty() || backend.images.size() != 1) {
      return;
    }
    check(approx_eq(backend.clips.front().x, 29.0f, 0.001f));
    check(approx_eq(backend.clips.front().y, 37.0f, 0.001f));
    check(approx_eq(backend.clips.front().w, 82.0f, 0.001f));
    check(approx_eq(backend.clips.front().h, 66.0f, 0.001f));
    check(approx_eq(backend.images.front().x, 20.0f, 0.001f));
    check(approx_eq(backend.images.front().y, 30.0f, 0.001f));
  }
}

spec("RenderManager resolves cover and contain for inline svg background images") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = false;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    const std::string svg_src =
        "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' "
        "viewBox='0 0 40 20'></svg>";

    BackgroundImageLayer contain;
    contain.has_image_url = true;
    contain.image_url = svg_src;
    contain.position = "center";
    contain.size = "contain";
    contain.repeat = "no-repeat";

    BackgroundImageLayer cover = contain;
    cover.size = "cover";

    elem.computed_style->background_layers = {contain, cover};

    render_manager.render_tree(&elem);

    check(backend.images.empty());
    check(backend.svgs.size() == 2);
    if (backend.svgs.size() != 2) {
      return;
    }

    const auto contain_it = std::find_if(
        backend.svgs.begin(), backend.svgs.end(),
        [](const DrawSvgCall& call) {
          return approx_eq(call.w, 100.0f, 0.001f) &&
                 approx_eq(call.h, 50.0f, 0.001f);
        });
    check(contain_it != backend.svgs.end());
    if (contain_it == backend.svgs.end()) {
      return;
    }
    check(approx_eq(contain_it->x, 20.0f, 0.001f));
    check(approx_eq(contain_it->y, 45.0f, 0.001f));

    const auto cover_it = std::find_if(
        backend.svgs.begin(), backend.svgs.end(),
        [](const DrawSvgCall& call) {
          return approx_eq(call.w, 160.0f, 0.001f) &&
                 approx_eq(call.h, 80.0f, 0.001f);
        });
    check(cover_it != backend.svgs.end());
    if (cover_it == backend.svgs.end()) {
      return;
    }
    check(approx_eq(cover_it->x, -10.0f, 0.001f));
    check(approx_eq(cover_it->y, 30.0f, 0.001f));
  }
}

spec("RenderManager routes svg background images when raster images are unavailable") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = false;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    BackgroundImageLayer image;
    image.has_image_url = true;
    image.image_url = "hero.svg";
    image.position = "left top";
    image.size = "20px 10px";
    image.repeat = "no-repeat";
    elem.computed_style->background_layers = {image};

    render_manager.render_tree(&elem);

    check(backend.images.empty());
    check(backend.svgs.size() == 1);
    if (backend.svgs.size() != 1) {
      return;
    }
    check(backend.svgs[0].src == "hero.svg");
    check(approx_eq(backend.svgs[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.svgs[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.svgs[0].w, 20.0f, 0.001f));
    check(approx_eq(backend.svgs[0].h, 10.0f, 0.001f));
  }
}

spec("RenderManager prefers svg background rendering for svg sources") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);

    BackgroundImageLayer image;
    image.has_image_url = true;
    image.image_url = "hero.svg";
    image.position = "left top";
    image.size = "20px 10px";
    image.repeat = "no-repeat";
    elem.computed_style->background_layers = {image};

    render_manager.render_tree(&elem);

    check(backend.images.empty());
    check(backend.svgs.size() == 1);
    if (backend.svgs.size() != 1) {
      return;
    }
    check(backend.svgs[0].src == "hero.svg");
    check(approx_eq(backend.svgs[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.svgs[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.svgs[0].w, 20.0f, 0.001f));
    check(approx_eq(backend.svgs[0].h, 10.0f, 0.001f));
  }
}

spec("RenderManager applies CSS filter blur when backend supports blur") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.blur = true;
    Box box(&backend);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        width: 60px;
        height: 30px;
        background-color: #336699;
        filter: blur(6px);
      }
    )");

    box.update();

    check_false(backend.blur_radii.empty());
    check(approx_eq(backend.blur_radii.back(), 6.0f, 0.001f));
    check(backend.clear_blur_calls >= 1);
  }
}

spec("RenderManager applies CSS filter drop-shadow alongside blur") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.shadow = true;
    backend.capabilities_.blur = true;
    Box box(&backend);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        width: 60px;
        height: 30px;
        background-color: #336699;
        filter: blur(6px) drop-shadow(2px 4px 3px rgba(10, 20, 30, 0.5));
      }
    )");

    box.update();

    check(backend.shadows.size() == 1);
    if (backend.shadows.size() != 1 || backend.rects.size() < 2 ||
        backend.blur_radii.size() < 3) {
      return;
    }
    check(approx_eq(backend.blur_radii.front(), 6.0f, 0.001f));
    check(approx_eq(backend.blur_radii[1], 3.0f, 0.001f));
    check(approx_eq(backend.blur_radii.back(), 6.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.offset_x, 2.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.offset_y, 4.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.blur, 3.0f, 0.001f));
    require_color(backend.shadows[0].shadow.color, 10.0f / 255.0f,
                  20.0f / 255.0f, 30.0f / 255.0f, 0.5f);
    check(approx_eq(backend.rects[0].x, 2.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 4.0f, 0.001f));
    check(approx_eq(backend.rects[0].w, 60.0f, 0.001f));
    check(approx_eq(backend.rects[0].h, 30.0f, 0.001f));
    require_color(backend.rects[0].fill_color, 10.0f / 255.0f,
                  20.0f / 255.0f, 30.0f / 255.0f, 0.5f);
    check(approx_eq(backend.rects[1].x, 0.0f, 0.001f));
    check(approx_eq(backend.rects[1].y, 0.0f, 0.001f));
    check(approx_eq(backend.rects[1].w, 60.0f, 0.001f));
    check(approx_eq(backend.rects[1].h, 30.0f, 0.001f));
    require_color(backend.rects[1].fill_color, 0x33 / 255.0f, 0x66 / 255.0f,
                  0x99 / 255.0f, 1.0f);
    check(backend.clear_blur_calls >= 1);
    check(backend.clear_shadow_calls >= 1);
  }
}

spec("RenderManager applies CSS filter drop-shadow without blur support") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.shadow = true;
    backend.capabilities_.blur = false;
    Box box(&backend);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        width: 60px;
        height: 30px;
        background-color: #336699;
        filter: drop-shadow(2px 4px 3px rgba(10, 20, 30, 0.5));
      }
    )");

    box.update();

    check(backend.blur_radii.empty());
    check(backend.shadows.size() == 1);
    if (backend.shadows.size() != 1 || backend.rects.size() < 2) {
      return;
    }
    check(approx_eq(backend.shadows[0].shadow.offset_x, 2.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.offset_y, 4.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.blur, 3.0f, 0.001f));
    require_color(backend.shadows[0].shadow.color, 10.0f / 255.0f,
                  20.0f / 255.0f, 30.0f / 255.0f, 0.5f);
    check(approx_eq(backend.rects[0].x, 2.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 4.0f, 0.001f));
    require_color(backend.rects[0].fill_color, 10.0f / 255.0f,
                  20.0f / 255.0f, 30.0f / 255.0f, 0.5f);
    require_color(backend.rects[1].fill_color, 0x33 / 255.0f,
                  0x66 / 255.0f, 0x99 / 255.0f, 1.0f);
    check(backend.clear_shadow_calls >= 1);
  }
}

spec("RenderManager multiplies CSS filter opacity into global alpha") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        width: 60px;
        height: 30px;
        opacity: 0.8;
        filter: opacity(50%);
      }
    )");

    box.update();

    check_false(backend.alphas.empty());
    check(approx_eq(backend.alphas.back(), 0.4f, 0.001f));
  }
}

spec("RenderManager applies CSS backdrop blur only around the background layer") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.blur = true;
    Box box(&backend);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    card->set_text("Foreground");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        width: 100px;
        height: 40px;
        background-color: rgba(51, 102, 153, 0.4);
        backdrop-filter: blur(8px);
      }
    )");

    box.update();

    check_false(backend.blur_radii.empty());
    check(approx_eq(backend.blur_radii.back(), 8.0f, 0.001f));
    check(backend.clear_blur_calls >= 1);
    check_false(backend.texts.empty());
    check(backend.texts.front().text == "Foreground");
  }
}

spec("RenderManager applies webkit-prefixed backdrop blur aliases") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.blur = true;
    Box box(&backend);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        width: 60px;
        height: 30px;
        background-color: #336699;
        -webkit-backdrop-filter: blur(10px);
      }
    )");

    box.update();

    check_false(backend.blur_radii.empty());
    check(approx_eq(backend.blur_radii.back(), 10.0f, 0.001f));
    check(backend.clear_blur_calls >= 1);
  }
}

spec("RenderManager uses active CSS opacity animations during rendering") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* button = box.create("button", "cta");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      @keyframes fade-in {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #cta {
        animation: fade-in 0.2s linear forwards;
        width: 100px;
        height: 40px;
        background-color: #336699;
      }
    )");

    box.update();
    check_false(backend.alphas.empty());
    check(approx_eq(backend.alphas.back(), 0.0f, 0.001f));

    box.update_time(100.0f);
    box.update();

    check_false(backend.alphas.empty());
    check(approx_eq(backend.alphas.back(), 0.5f, 0.02f));
  }
}

spec("RenderManager uses active CSS transform and background animations during rendering") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* child = box.create("div", "child");
    root->append(child);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      @keyframes pulse-slide {
        from {
          transform: translate(0px, 0px) rotate(0deg);
          background-color: #000000;
        }
        to {
          transform: translate(20px, 10px) rotate(90deg);
          background-color: #669933;
        }
      }
      #child {
        animation: pulse-slide 0.2s linear forwards;
        transform-origin: left top;
        width: 80px;
        height: 40px;
      }
    )");

    box.update();
    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.back().x, 0.0f, 0.001f));
    check(approx_eq(backend.rects.back().y, 0.0f, 0.001f));
    require_color(backend.rects.back().fill_color, 0.0f, 0.0f, 0.0f);

    box.update_time(100.0f);
    box.update();

    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.back().x, 10.0f, 0.02f));
    check(approx_eq(backend.rects.back().y, 5.0f, 0.02f));
    require_color(backend.rects.back().fill_color, 0.2f, 0.3f, 0.1f);
    check_false(backend.rotations.empty());
    if (!backend.rotations.empty()) {
      check(approx_eq(backend.rotations.back(), 45.0f, 0.02f));
    }
  }
}

spec("RenderManager keeps one-value CSS translate animations on the x axis") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* child = box.create("div", "child");
    root->append(child);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      @keyframes slide-x {
        from { transform: translate(0px); }
        to { transform: translate(20px); }
      }
      #child {
        animation: slide-x 0.2s linear forwards;
        width: 80px;
        height: 40px;
        background-color: #336699;
      }
    )");

    box.update();
    box.update_time(100.0f);
    box.update();

    check_false(backend.rects.empty());
    if (!backend.rects.empty()) {
      check(approx_eq(backend.rects.back().x, 10.0f, 0.02f));
      check(approx_eq(backend.rects.back().y, 0.0f, 0.02f));
    }
  }
}

spec("RenderManager animates 3d-compatible CSS transform functions in 2d") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* child = box.create("div", "child");
    root->append(child);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      @keyframes transform-compat {
        from {
          transform: translate3d(0px, 0px, 0) scale3d(1, 1, 1) rotateZ(0turn);
        }
        to {
          transform: translate3d(20px, 10px, 0) scale3d(2, 0.5, 1) rotateZ(0.25turn);
        }
      }
      #child {
        animation: transform-compat 0.2s linear forwards;
        transform-origin: left top;
        width: 80px;
        height: 40px;
        background-color: #336699;
      }
    )");

    box.update();
    box.update_time(100.0f);
    box.update();

    check_false(backend.rects.empty());
    if (!backend.rects.empty()) {
      check(approx_eq(backend.rects.back().x, 10.0f, 0.02f));
      check(approx_eq(backend.rects.back().y, 5.0f, 0.02f));
    }
    check_false(backend.scales.empty());
    if (!backend.scales.empty()) {
      check(approx_eq(backend.scales.back().first, 1.5f, 0.02f));
      check(approx_eq(backend.scales.back().second, 0.75f, 0.02f));
    }
    check_false(backend.rotations.empty());
    if (!backend.rotations.empty()) {
      check(approx_eq(backend.rotations.back(), 45.0f, 0.02f));
    }
  }
}

spec("RenderManager uses active border outline and ring transitions during rendering") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    Box box(nullptr);

    auto* root = box.create("div");
    auto* child = box.create("div", "child");
    root->append(child);

    root->set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    root->computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};

    child->set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    child->computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};
    child->computed_style->border_width[0] = 1.0f;
    child->computed_style->border_color = {0.6f, 0.2f, 0.4f, 0.5f};
    child->computed_style->outline_width = 2.0f;
    child->computed_style->outline_offset = 3.0f;
    child->computed_style->outline_color = {0.2f, 0.4f, 0.6f, 1.0f};
    child->computed_style->ring_width = 6.0f;
    child->computed_style->ring_offset = 2.0f;
    child->computed_style->ring_color = {0.5f, 0.6f, 0.7f, 0.8f};

    const auto element_id = reinterpret_cast<std::uintptr_t>(child);
    TransitionDef def;
    def.property = "all";
    def.duration_ms = 200.0f;
    def.delay_ms = 0.0f;
    def.easing = EasingType::Linear;

    box.transitions().start(element_id, "border-color-r", 0.2f, 0.6f, def, 0.0f);
    box.transitions().start(element_id, "border-color-g", 0.4f, 0.2f, def, 0.0f);
    box.transitions().start(element_id, "border-color-b", 0.6f, 0.4f, def, 0.0f);
    box.transitions().start(element_id, "border-color-a", 1.0f, 0.5f, def, 0.0f);
    box.transitions().start(element_id, "outline-width", 0.0f, 2.0f, def, 0.0f);
    box.transitions().start(element_id, "outline-offset", 0.0f, 3.0f, def, 0.0f);
    box.transitions().start(element_id, "outline-color-r", 0.0f, 0.2f, def, 0.0f);
    box.transitions().start(element_id, "outline-color-g", 0.0f, 0.4f, def, 0.0f);
    box.transitions().start(element_id, "outline-color-b", 0.0f, 0.6f, def, 0.0f);
    box.transitions().start(element_id, "outline-color-a", 0.0f, 1.0f, def, 0.0f);
    box.transitions().start(element_id, "ring-width", 2.0f, 6.0f, def, 0.0f);
    box.transitions().start(element_id, "ring-offset", 0.0f, 2.0f, def, 0.0f);
    box.transitions().start(element_id, "ring-color-r", 0.1f, 0.5f, def, 0.0f);
    box.transitions().start(element_id, "ring-color-g", 0.2f, 0.6f, def, 0.0f);
    box.transitions().start(element_id, "ring-color-b", 0.3f, 0.7f, def, 0.0f);
    box.transitions().start(element_id, "ring-color-a", 0.2f, 0.8f, def, 0.0f);

    box.update_time(100.0f);
    render_manager.render_tree(root);

    check(backend.rects.size() == 3);

    check(approx_eq(backend.rects[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[0].w, 80.0f, 0.001f));
    check(approx_eq(backend.rects[0].h, 1.0f, 0.001f));
    check(approx_eq(backend.rects[0].stroke_width, 0.0f, 0.001f));
    require_color(backend.rects[0].fill_color, 0.4f, 0.3f, 0.5f, 0.75f);

    check(approx_eq(backend.rects[1].x, 17.0f, 0.001f));
    check(approx_eq(backend.rects[1].y, 27.0f, 0.001f));
    check(approx_eq(backend.rects[1].w, 86.0f, 0.001f));
    check(approx_eq(backend.rects[1].h, 46.0f, 0.001f));
    check(approx_eq(backend.rects[1].stroke_width, 4.0f, 0.001f));
    require_color(backend.rects[1].stroke_color, 0.3f, 0.4f, 0.5f, 0.5f);

    check(approx_eq(backend.rects[2].x, 18.0f, 0.001f));
    check(approx_eq(backend.rects[2].y, 28.0f, 0.001f));
    check(approx_eq(backend.rects[2].w, 84.0f, 0.001f));
    check(approx_eq(backend.rects[2].h, 44.0f, 0.001f));
    check(approx_eq(backend.rects[2].stroke_width, 1.0f, 0.001f));
    require_color(backend.rects[2].stroke_color, 0.1f, 0.2f, 0.3f, 0.5f);
  }
}

spec("RenderManager uses active CSS box-shadow animations during rendering") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.shadow = true;
    Box box(&backend);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      @keyframes shadow-in {
        from { box-shadow: 0 1px 2px 0 rgba(10, 20, 30, 0.2); }
        to { box-shadow: 0 5px 10px 2px rgba(110, 120, 130, 0.8); }
      }
      #card {
        animation: shadow-in 0.2s linear forwards;
        width: 80px;
        height: 40px;
        box-shadow: 0 1px 2px 0 rgba(10, 20, 30, 0.2);
      }
    )");

    box.update();
    check(backend.shadows.size() == 1);
    if (backend.shadows.size() != 1) {
      return;
    }
    check(approx_eq(backend.shadows.back().shadow.offset_y, 1.0f, 0.001f));
    check(approx_eq(backend.shadows.back().shadow.blur, 2.0f, 0.001f));

    box.update_time(100.0f);
    box.update();

    check_false(backend.shadows.empty());
    check(approx_eq(backend.shadows.back().shadow.offset_y, 3.0f, 0.02f));
    check(approx_eq(backend.shadows.back().shadow.blur, 6.0f, 0.02f));
    check(approx_eq(backend.shadows.back().shadow.spread, 1.0f, 0.02f));
    check(approx_eq(backend.shadows.back().shadow.color.a, 0.5f, 0.02f));
  }
}

spec("RenderManager uses active box-shadow transitions during rendering") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.shadow = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);
    Box box(nullptr);

    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);

    root->set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    card->set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    card->computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};
    card->computed_style->has_shadow = true;
    card->computed_style->shadow.offset_x = 0.0f;
    card->computed_style->shadow.offset_y = 5.0f;
    card->computed_style->shadow.blur_radius = 10.0f;
    card->computed_style->shadow.spread_radius = 2.0f;
    card->computed_style->shadow.color = {110.0f / 255.0f, 120.0f / 255.0f,
                                          130.0f / 255.0f, 0.8f};

    const auto element_id = reinterpret_cast<std::uintptr_t>(card);
    TransitionDef def;
    def.property = "box-shadow";
    def.duration_ms = 200.0f;
    def.delay_ms = 0.0f;
    def.easing = EasingType::Linear;

    box.transitions().start(element_id, "box-shadow-offset-y", 1.0f, 5.0f, def, 0.0f);
    box.transitions().start(element_id, "box-shadow-blur", 2.0f, 10.0f, def, 0.0f);
    box.transitions().start(element_id, "box-shadow-spread", 0.0f, 2.0f, def, 0.0f);
    box.transitions().start(element_id, "box-shadow-color-r", 10.0f / 255.0f,
                            110.0f / 255.0f, def, 0.0f);
    box.transitions().start(element_id, "box-shadow-color-g", 20.0f / 255.0f,
                            120.0f / 255.0f, def, 0.0f);
    box.transitions().start(element_id, "box-shadow-color-b", 30.0f / 255.0f,
                            130.0f / 255.0f, def, 0.0f);
    box.transitions().start(element_id, "box-shadow-color-a", 0.2f, 0.8f, def, 0.0f);

    box.update_time(100.0f);
    render_manager.render_tree(root);

    check(backend.shadows.size() == 1);
    if (backend.shadows.size() != 1) {
      return;
    }
    check(approx_eq(backend.shadows[0].shadow.offset_y, 3.0f, 0.02f));
    check(approx_eq(backend.shadows[0].shadow.blur, 6.0f, 0.02f));
    check(approx_eq(backend.shadows[0].shadow.spread, 1.0f, 0.02f));
    check(approx_eq(backend.shadows[0].shadow.color.a, 0.5f, 0.02f));
  }
}

spec("RenderManager skips unsupported rotate and scale transforms") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.rotation = false;
    backend.capabilities_.scaling = false;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element root;
    root.set_layout_bounds(10.0f, 20.0f, 400.0f, 300.0f);

    Element child;
    child.set_layout_bounds(30.0f, 40.0f, 100.0f, 50.0f);
    child.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    child.computed_style->transform_scale = 1.5f;
    child.computed_style->transform_rotate = 0.5f;
    root.append(&child);

    render_manager.render_tree(&root);

    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.back().x, 40.0f, 0.001f));
    check(approx_eq(backend.rects.back().y, 60.0f, 0.001f));
    check(backend.rotations.empty());
    check(backend.scales.empty());
  }
}

spec("RenderManager skips shadow fallback when backend lacks shadow support") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.shadow = false;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->has_shadow = true;
    elem.computed_style->shadow.offset_x = 4.0f;
    elem.computed_style->shadow.offset_y = 6.0f;
    elem.computed_style->shadow.spread_radius = 2.0f;

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 1);
    check(approx_eq(backend.rects.front().x, 20.0f, 0.001f));
    check(approx_eq(backend.rects.front().y, 30.0f, 0.001f));
  }
}

spec("RenderManager draws CSS box-shadow layers with inset overlays in order") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.shadow = true;
    backend.capabilities_.blur = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->has_shadow = true;
    elem.computed_style->shadows = {
        BoxShadow{1.0f, 2.0f, 3.0f, 4.0f, {0.1f, 0.2f, 0.3f, 0.4f}, false},
        BoxShadow{0.0f, 1.0f, 2.0f, 0.0f, {0.5f, 0.4f, 0.3f, 0.2f}, true},
        BoxShadow{5.0f, 6.0f, 0.0f, 0.0f, {0.7f, 0.6f, 0.5f, 0.4f}, false},
    };
    elem.computed_style->shadow = elem.computed_style->shadows.front();

    render_manager.render_tree(&elem);

    check(backend.shadows.size() == 3);
    if (backend.shadows.size() != 3) {
      return;
    }
    check(approx_eq(backend.shadows[0].shadow.offset_x, 1.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.offset_y, 2.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.blur, 3.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.spread, 4.0f, 0.001f));
    check_false(backend.shadows[0].shadow.inset);
    require_color(backend.shadows[0].shadow.color, 0.1f, 0.2f, 0.3f, 0.4f);
    check(approx_eq(backend.shadows[1].shadow.offset_x, 5.0f, 0.001f));
    check(approx_eq(backend.shadows[1].shadow.offset_y, 6.0f, 0.001f));
    check(approx_eq(backend.shadows[1].shadow.blur, 0.0f, 0.001f));
    check(approx_eq(backend.shadows[1].shadow.spread, 0.0f, 0.001f));
    check_false(backend.shadows[1].shadow.inset);
    check(backend.shadows[2].shadow.inset);
    check(approx_eq(backend.shadows[2].shadow.offset_x, 0.0f, 0.001f));
    check(approx_eq(backend.shadows[2].shadow.offset_y, 1.0f, 0.001f));
    check(approx_eq(backend.shadows[2].shadow.blur, 2.0f, 0.001f));
    check(approx_eq(backend.shadows[2].shadow.spread, 0.0f, 0.001f));
    require_color(backend.shadows[2].shadow.color, 0.5f, 0.4f, 0.3f, 0.2f);
    check(backend.clear_shadow_calls == 3);
    check_false(backend.blur_radii.empty());
    check(approx_eq(backend.blur_radii.front(), 3.0f, 0.001f));
    check(backend.clips.size() == 1);
    if (backend.clips.size() != 1) {
      return;
    }
    check(approx_eq(backend.clips[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.clips[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.clips[0].w, 80.0f, 0.001f));
    check(approx_eq(backend.clips[0].h, 40.0f, 0.001f));

    check(backend.rects.size() == 4);
    if (backend.rects.size() != 4) {
      return;
    }
    check(approx_eq(backend.rects[0].x, 17.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 28.0f, 0.001f));
    check(approx_eq(backend.rects[0].w, 88.0f, 0.001f));
    check(approx_eq(backend.rects[0].h, 48.0f, 0.001f));
    require_color(backend.rects[0].fill_color, 0.1f, 0.2f, 0.3f, 0.4f);

    check(approx_eq(backend.rects[1].x, 25.0f, 0.001f));
    check(approx_eq(backend.rects[1].y, 36.0f, 0.001f));
    check(approx_eq(backend.rects[1].w, 80.0f, 0.001f));
    check(approx_eq(backend.rects[1].h, 40.0f, 0.001f));
    require_color(backend.rects[1].fill_color, 0.7f, 0.6f, 0.5f, 0.4f);

    check(approx_eq(backend.rects[2].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[2].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[2].w, 80.0f, 0.001f));
    check(approx_eq(backend.rects[2].h, 40.0f, 0.001f));
    require_color(backend.rects[2].fill_color, 0.2f, 0.4f, 0.6f, 1.0f);

    check(approx_eq(backend.rects[3].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[3].y, 31.0f, 0.001f));
    check(approx_eq(backend.rects[3].w, 80.0f, 0.001f));
    check(approx_eq(backend.rects[3].h, 40.0f, 0.001f));
    require_color(backend.rects[3].fill_color, 0.5f, 0.4f, 0.3f, 0.2f);
  }
}

spec("RenderManager draws parsed CSS box-shadow layers in paint order") {
  it("runs") {
    Box box(nullptr);
    Element* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #card {
        width: 80px;
        height: 40px;
        background-color: #336699;
        box-shadow: 1px 2px 3px 4px rgba(26, 51, 77, 0.4),
                    inset 0 1px 2px 0 rgba(128, 102, 77, 0.2),
                    5px 6px 0 0 rgba(179, 153, 128, 0.4);
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.capabilities_.shadow = true;
    backend.capabilities_.blur = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(card);

    check(backend.shadows.size() == 3);
    if (backend.shadows.size() != 3) {
      return;
    }

    check(approx_eq(backend.shadows[0].shadow.offset_x, 1.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.offset_y, 2.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.blur, 3.0f, 0.001f));
    check(approx_eq(backend.shadows[0].shadow.spread, 4.0f, 0.001f));
    check_false(backend.shadows[0].shadow.inset);
    require_color(backend.shadows[0].shadow.color, 26.0f / 255.0f,
                  51.0f / 255.0f, 77.0f / 255.0f, 0.4f);

    check(approx_eq(backend.shadows[1].shadow.offset_x, 5.0f, 0.001f));
    check(approx_eq(backend.shadows[1].shadow.offset_y, 6.0f, 0.001f));
    check(approx_eq(backend.shadows[1].shadow.blur, 0.0f, 0.001f));
    check(approx_eq(backend.shadows[1].shadow.spread, 0.0f, 0.001f));
    check_false(backend.shadows[1].shadow.inset);
    require_color(backend.shadows[1].shadow.color, 179.0f / 255.0f,
                  153.0f / 255.0f, 128.0f / 255.0f, 0.4f);

    check(backend.shadows[2].shadow.inset);
    check(approx_eq(backend.shadows[2].shadow.offset_x, 0.0f, 0.001f));
    check(approx_eq(backend.shadows[2].shadow.offset_y, 1.0f, 0.001f));
    check(approx_eq(backend.shadows[2].shadow.blur, 2.0f, 0.001f));
    check(approx_eq(backend.shadows[2].shadow.spread, 0.0f, 0.001f));
    require_color(backend.shadows[2].shadow.color, 128.0f / 255.0f,
                  102.0f / 255.0f, 77.0f / 255.0f, 0.2f);
    check(backend.clear_shadow_calls == 3);
  }
}

spec("RenderManager draws CSS border, ring, and outline outside local bounds") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.2f, 0.3f, 0.4f, 1.0f};
    elem.computed_style->border_width[0] = 1.0f;
    elem.computed_style->border_color = {0.5f, 0.6f, 0.7f, 1.0f};
    elem.computed_style->ring_width = 2.0f;
    elem.computed_style->ring_offset = 1.0f;
    elem.computed_style->ring_color = {0.1f, 0.2f, 0.3f, 0.8f};
    elem.computed_style->ring_offset_color = {0.9f, 0.8f, 0.7f, 1.0f};
    elem.computed_style->outline_width = 1.0f;
    elem.computed_style->outline_offset = 4.0f;
    elem.computed_style->outline_color = {0.8f, 0.4f, 0.2f, 1.0f};

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 5);
    check(approx_eq(backend.rects[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[0].stroke_width, 0.0f, 0.001f));

    check(approx_eq(backend.rects[1].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[1].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[1].w, 80.0f, 0.001f));
    check(approx_eq(backend.rects[1].h, 1.0f, 0.001f));
    check(approx_eq(backend.rects[1].stroke_width, 0.0f, 0.001f));

    check(approx_eq(backend.rects[2].x, 19.5f, 0.001f));
    check(approx_eq(backend.rects[2].y, 29.5f, 0.001f));
    check(approx_eq(backend.rects[2].w, 81.0f, 0.001f));
    check(approx_eq(backend.rects[2].h, 41.0f, 0.001f));
    check(approx_eq(backend.rects[2].stroke_width, 1.0f, 0.001f));
    require_color(backend.rects[2].stroke_color, 0.9f, 0.8f, 0.7f, 1.0f);

    check(approx_eq(backend.rects[3].x, 18.0f, 0.001f));
    check(approx_eq(backend.rects[3].y, 28.0f, 0.001f));
    check(approx_eq(backend.rects[3].w, 84.0f, 0.001f));
    check(approx_eq(backend.rects[3].h, 44.0f, 0.001f));
    check(approx_eq(backend.rects[3].stroke_width, 2.0f, 0.001f));
    require_color(backend.rects[3].stroke_color, 0.1f, 0.2f, 0.3f, 0.8f);

    check(approx_eq(backend.rects[4].x, 15.5f, 0.001f));
    check(approx_eq(backend.rects[4].y, 25.5f, 0.001f));
    check(approx_eq(backend.rects[4].w, 89.0f, 0.001f));
    check(approx_eq(backend.rects[4].h, 49.0f, 0.001f));
    check(approx_eq(backend.rects[4].stroke_width, 1.0f, 0.001f));
  }
}

spec("RenderManager draws double borders and outlines with two strokes") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element border_elem;
    border_elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    border_elem.computed_style->background_color = {0.2f, 0.3f, 0.4f, 1.0f};
    border_elem.computed_style->border_width[0] = 6.0f;
    border_elem.computed_style->border_style[0] = BorderStyle::Double;
    border_elem.computed_style->border_color = {0.5f, 0.6f, 0.7f, 1.0f};

    Element outline_elem;
    outline_elem.set_layout_bounds(120.0f, 30.0f, 80.0f, 40.0f);
    outline_elem.computed_style->background_color = {0.2f, 0.3f, 0.4f, 1.0f};
    outline_elem.computed_style->outline_width = 6.0f;
    outline_elem.computed_style->outline_style = BorderStyle::Double;
    outline_elem.computed_style->outline_color = {0.8f, 0.4f, 0.2f, 1.0f};

    render_manager.render_tree(&border_elem);
    const size_t border_line_count = backend.lines.size();
    render_manager.render_tree(&outline_elem);

    check(border_line_count == 2);
    if (border_line_count == 2) {
      check(approx_eq(backend.lines[0].width, 2.0f, 0.001f));
      check(approx_eq(backend.lines[1].width, 2.0f, 0.001f));
      check(approx_eq(std::fabs(backend.lines[0].y1 - backend.lines[1].y1),
                      4.0f, 0.001f));
    }
    check(backend.rects.size() >= 4);
    if (backend.rects.size() >= 4) {
      const auto& outer = backend.rects[backend.rects.size() - 2];
      const auto& inner = backend.rects[backend.rects.size() - 1];
      check(approx_eq(outer.stroke_width, 2.0f, 0.001f));
      check(approx_eq(inner.stroke_width, 2.0f, 0.001f));
      check(outer.x < inner.x);
      check(outer.y < inner.y);
      check(outer.w > inner.w);
      check(outer.h > inner.h);
    }
  }
}

spec("RenderManager uses path drawing for non-uniform corner radii") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.2f, 0.3f, 0.4f, 1.0f};
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->border_color = {0.5f, 0.6f, 0.7f, 1.0f};
    elem.computed_style->border_radius[0] = 4.0f;
    elem.computed_style->border_radius[1] = 8.0f;
    elem.computed_style->border_radius[2] = 12.0f;
    elem.computed_style->border_radius[3] = 16.0f;

    render_manager.render_tree(&elem);

    check(backend.rects.empty());
    check(backend.fill_paths.size() == 1);
    check(backend.stroke_paths.size() == 1);
    require_color(backend.fill_paths.front().color, 0.2f, 0.3f, 0.4f, 1.0f);
    require_color(backend.stroke_paths.front().color, 0.5f, 0.6f, 0.7f, 1.0f);
    check(approx_eq(backend.stroke_paths.front().width, 2.0f, 0.001f));
    check(backend.fill_paths.front().d.find("A 4 4 0 0 1") != std::string::npos);
    check(backend.fill_paths.front().d.find("A 8 8 0 0 1") != std::string::npos);
    check(backend.fill_paths.front().d.find("A 12 12 0 0 1") != std::string::npos);
    check(backend.fill_paths.front().d.find("A 16 16 0 0 1") != std::string::npos);
  }
}

spec("RenderManager clips non-uniform rounded backgrounds to content box") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 80.0f);
    elem.computed_style->background_color = {0.2f, 0.3f, 0.4f, 1.0f};
    elem.computed_style->background_clip = BackgroundClip::ContentBox;
    elem.computed_style->padding[0] = 5.0f;
    elem.computed_style->padding[1] = 7.0f;
    elem.computed_style->padding[2] = 5.0f;
    elem.computed_style->padding[3] = 7.0f;
    elem.computed_style->border_radius[0] = 20.0f;
    elem.computed_style->border_radius[1] = 16.0f;
    elem.computed_style->border_radius[2] = 12.0f;
    elem.computed_style->border_radius[3] = 8.0f;

    render_manager.render_tree(&elem);

    check(backend.rects.empty());
    check(backend.fill_paths.size() == 1);
    if (backend.fill_paths.empty()) {
      return;
    }
    require_color(backend.fill_paths.front().color, 0.2f, 0.3f, 0.4f, 1.0f);
    check(backend.fill_paths.front().d.find("M 20 5") != std::string::npos);
    check(backend.fill_paths.front().d.find("H 84") != std::string::npos);
    check(backend.fill_paths.front().d.find("A 13 13 0 0 1") !=
          std::string::npos);
    check(backend.fill_paths.front().d.find("A 9 9 0 0 1") !=
          std::string::npos);
    check(backend.fill_paths.front().d.find("A 5 5 0 0 1") !=
          std::string::npos);
    check(backend.fill_paths.front().d.find("A 1 1 0 0 1") !=
          std::string::npos);
  }
}

spec("RenderManager draws CSS non-uniform border radii as paths") {
  it("runs") {
    Box box(nullptr);
    auto* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #card {
        width: 80px;
        height: 40px;
        background-color: #112233;
        border: 2px solid #336699;
        border-top-left-radius: 4px;
        border-top-right-radius: 8px;
        border-bottom-right-radius: 12px;
        border-bottom-left-radius: 16px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(card);

    check(backend.rects.empty());
    check(backend.fill_paths.size() == 1);
    check(backend.stroke_paths.size() == 1);
    if (backend.fill_paths.empty() || backend.stroke_paths.empty()) {
      return;
    }
    require_color(backend.fill_paths.front().color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
    require_color(backend.stroke_paths.front().color, 0x33 / 255.0f,
                  0x66 / 255.0f, 0x99 / 255.0f);
  }
}

spec("RenderManager draws dashed outlines with line strokes outside local bounds") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.2f, 0.3f, 0.4f, 1.0f};
    elem.computed_style->outline_width = 2.0f;
    elem.computed_style->outline_offset = 3.0f;
    elem.computed_style->outline_color = {0.8f, 0.4f, 0.2f, 1.0f};
    elem.computed_style->outline_style = BorderStyle::Dashed;

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 1);
    check(backend.lines.size() >= 4);
    require_color(backend.lines.front().color, 0.8f, 0.4f, 0.2f, 1.0f);
    check(approx_eq(backend.lines.front().width, 2.0f, 0.001f));
    check(approx_eq(backend.lines.front().y1, 27.0f, 0.001f));
  }
}

spec("RenderManager draws CSS 3d outline styles with shaded sides") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->outline_width = 4.0f;
    elem.computed_style->outline_offset = 2.0f;
    elem.computed_style->outline_color = {0.4f, 0.4f, 0.4f, 1.0f};
    elem.computed_style->outline_style = BorderStyle::Outset;

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 4);
    if (backend.rects.size() != 4) {
      return;
    }
    check(backend.rects[0].fill_color.r > elem.computed_style->outline_color.r);
    check(backend.rects[1].fill_color.r < elem.computed_style->outline_color.r);
    check(backend.rects[2].fill_color.r < elem.computed_style->outline_color.r);
    check(backend.rects[3].fill_color.r > elem.computed_style->outline_color.r);
  }
}

spec("RenderManager draws CSS outline shorthand outside local bounds") {
  it("runs") {
    Box box(nullptr);
    auto* card = box.create("div", "card");
    box.set_root(card);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #card {
        width: 80px;
        height: 40px;
        outline: 2px dashed #cc6633;
        outline-offset: 3px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(card);

    check(backend.lines.size() >= 4);
    if (backend.lines.empty()) {
      return;
    }
    require_color(backend.lines.front().color, 0xcc / 255.0f,
                  0x66 / 255.0f, 0x33 / 255.0f);
    check(approx_eq(backend.lines.front().width, 2.0f, 0.001f));
    check(approx_eq(backend.lines.front().y1, -3.0f, 0.001f));
  }
}

spec("RenderManager draws side specific borders without full stroke") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};
    elem.computed_style->border_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->border_width[2] = 3.0f;

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 1);
    check(approx_eq(backend.rects.front().x, 20.0f, 0.001f));
    check(approx_eq(backend.rects.front().y, 67.0f, 0.001f));
    check(approx_eq(backend.rects.front().w, 80.0f, 0.001f));
    check(approx_eq(backend.rects.front().h, 3.0f, 0.001f));
    check(approx_eq(backend.rects.front().stroke_width, 0.0f, 0.001f));
    require_color(backend.rects.front().fill_color, 0.2f, 0.4f, 0.6f, 1.0f);
  }
}

spec("RenderManager draws CSS 3d border styles with shaded sides") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element inset;
    inset.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    inset.computed_style->border_width[0] = 4.0f;
    inset.computed_style->border_width[1] = 4.0f;
    inset.computed_style->border_width[2] = 4.0f;
    inset.computed_style->border_width[3] = 4.0f;
    inset.computed_style->border_color = {0.4f, 0.4f, 0.4f, 1.0f};
    inset.computed_style->border_colors[0] = inset.computed_style->border_color;
    inset.computed_style->border_colors[1] = inset.computed_style->border_color;
    inset.computed_style->border_colors[2] = inset.computed_style->border_color;
    inset.computed_style->border_colors[3] = inset.computed_style->border_color;
    inset.computed_style->border_style[0] = BorderStyle::Inset;
    inset.computed_style->border_style[1] = BorderStyle::Inset;
    inset.computed_style->border_style[2] = BorderStyle::Inset;
    inset.computed_style->border_style[3] = BorderStyle::Inset;

    render_manager.render_tree(&inset);

    check(backend.rects.size() == 4);
    if (backend.rects.size() != 4) {
      return;
    }
    check(backend.rects[0].fill_color.r < inset.computed_style->border_color.r);
    check(backend.rects[1].fill_color.r > inset.computed_style->border_color.r);
    check(backend.rects[2].fill_color.r > inset.computed_style->border_color.r);
    check(backend.rects[3].fill_color.r < inset.computed_style->border_color.r);

    const size_t groove_start = backend.rects.size();
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Element groove;
    groove.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    *groove.computed_style = *inset.computed_style;
    groove.computed_style->border_style[0] = BorderStyle::Groove;
    groove.computed_style->border_style[1] = BorderStyle::Groove;
    groove.computed_style->border_style[2] = BorderStyle::Groove;
    groove.computed_style->border_style[3] = BorderStyle::Groove;

    render_manager.render_tree(&groove);

    check(backend.rects.size() == groove_start + 8);
    if (backend.rects.size() != groove_start + 8) {
      return;
    }
    check(backend.rects[groove_start].fill_color.r <
          backend.rects[groove_start + 1].fill_color.r);
    check(backend.rects[groove_start + 2].fill_color.r >
          backend.rects[groove_start + 3].fill_color.r);
  }
}

spec("RenderManager draws per-side border colors independently") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};
    elem.computed_style->has_border_side_colors = true;
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 2.0f;
    elem.computed_style->border_width[2] = 2.0f;
    elem.computed_style->border_width[3] = 2.0f;
    elem.computed_style->border_colors[0] = {0.8f, 0.1f, 0.2f, 1.0f};
    elem.computed_style->border_colors[1] = {0.1f, 0.7f, 0.2f, 1.0f};
    elem.computed_style->border_colors[2] = {0.2f, 0.3f, 0.9f, 1.0f};
    elem.computed_style->border_colors[3] = {0.2f, 0.2f, 0.2f, 0.0f};

    render_manager.render_tree(&elem);

    check(backend.rects.size() == 3);
    if (backend.rects.size() != 3) {
      return;
    }

    check(approx_eq(backend.rects[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[0].w, 80.0f, 0.001f));
    check(approx_eq(backend.rects[0].h, 2.0f, 0.001f));
    require_color(backend.rects[0].fill_color, 0.8f, 0.1f, 0.2f, 1.0f);

    check(approx_eq(backend.rects[1].x, 98.0f, 0.001f));
    check(approx_eq(backend.rects[1].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[1].w, 2.0f, 0.001f));
    check(approx_eq(backend.rects[1].h, 40.0f, 0.001f));
    require_color(backend.rects[1].fill_color, 0.1f, 0.7f, 0.2f, 1.0f);

    check(approx_eq(backend.rects[2].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[2].y, 68.0f, 0.001f));
    check(approx_eq(backend.rects[2].w, 80.0f, 0.001f));
    check(approx_eq(backend.rects[2].h, 2.0f, 0.001f));
    require_color(backend.rects[2].fill_color, 0.2f, 0.3f, 0.9f, 1.0f);
  }
}

spec("RenderManager draws RTL logical CSS borders on physical sides") {
  it("runs") {
    Box box(nullptr);
    auto* panel = box.create("div", "panel");
    box.set_root(panel);
    box.set_viewport(240.0f, 120.0f);

    box.load_css(R"(
      #panel {
        direction: rtl;
        box-sizing: border-box;
        width: 80px;
        height: 40px;
        border-inline-start: 3px solid #112233;
        border-inline-end: 7px solid #445566;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(panel);

    const auto right_border = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 77.0f, 0.001f) &&
                 approx_eq(call.w, 3.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 0x11 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x22 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x33 / 255.0f, 0.001f);
        });
    check(right_border != backend.rects.end());

    const auto left_border = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.x, 0.0f, 0.001f) &&
                 approx_eq(call.w, 7.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 0x44 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x55 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x66 / 255.0f, 0.001f);
        });
    check(left_border != backend.rects.end());
  }
}

spec("RenderManager draws dashed and dotted borders with line strokes") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    Element elem;
    elem.set_layout_bounds(20.0f, 30.0f, 80.0f, 40.0f);
    elem.computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};
    elem.computed_style->border_color = {0.2f, 0.4f, 0.6f, 1.0f};
    elem.computed_style->border_width[0] = 2.0f;
    elem.computed_style->border_width[1] = 3.0f;
    elem.computed_style->border_width[2] = 0.0f;
    elem.computed_style->border_width[3] = 1.0f;
    elem.computed_style->border_style[0] = BorderStyle::Dashed;
    elem.computed_style->border_style[1] = BorderStyle::Dotted;
    elem.computed_style->border_style[2] = BorderStyle::None;
    elem.computed_style->border_style[3] = BorderStyle::Solid;

    render_manager.render_tree(&elem);

    check(backend.lines.size() >= 3);
    check(backend.rects.size() == 1);
    if (backend.lines.size() < 3 || backend.rects.size() != 1) {
      return;
    }

    check(approx_eq(backend.lines.front().y1, 31.0f, 0.001f));
    check(approx_eq(backend.lines.front().width, 2.0f, 0.001f));
    require_color(backend.lines.front().color, 0.2f, 0.4f, 0.6f, 1.0f);

    auto vertical = std::find_if(
        backend.lines.begin(), backend.lines.end(),
        [](const LineCall& line) { return std::fabs(line.x1 - line.x2) <= 0.001f; });
    check(vertical != backend.lines.end());
    if (vertical == backend.lines.end()) {
      return;
    }
    check(approx_eq(vertical->x1, 98.5f, 0.001f));
    check(approx_eq(vertical->width, 3.0f, 0.001f));

    check(approx_eq(backend.rects[0].x, 20.0f, 0.001f));
    check(approx_eq(backend.rects[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.rects[0].w, 1.0f, 0.001f));
    check(approx_eq(backend.rects[0].h, 40.0f, 0.001f));
  }
}

spec("ImageWidget draws placeholder rect when raster images are unsupported") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = false;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element image_elem;
    image_elem.set_layout_bounds(0.0f, 0.0f, 64.0f, 48.0f);
    image_elem.computed_style->background_color = {0.0f, 0.0f, 0.0f, 0.0f};
    image_elem.computed_style->border_color = {0.2f, 0.3f, 0.4f, 1.0f};
    ImageWidget image("demo.png");
    image_elem.widget = &image;

    render_widget(image, image_elem, renderer);

    check(backend.images.empty());
    check(backend.rects.size() == 1);
    check(approx_eq(backend.rects.front().x, 0.0f, 0.001f));
    check(approx_eq(backend.rects.front().y, 0.0f, 0.001f));
    check(approx_eq(backend.rects.front().w, 64.0f, 0.001f));
    check(approx_eq(backend.rects.front().h, 48.0f, 0.001f));
  }
}

spec("ImageWidget routes svg sources to draw_svg when raster images are unsupported") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = false;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element image_elem;
    image_elem.set_layout_bounds(0.0f, 0.0f, 72.0f, 36.0f);
    ImageWidget image("icon.svg");
    render_widget(image, image_elem, renderer);

    check(backend.images.empty());
    check(backend.rects.empty());
    check(backend.svgs.size() == 1);
    check(backend.svgs.front().src == "icon.svg");
    check(approx_eq(backend.svgs.front().w, 72.0f, 0.001f));
    check(approx_eq(backend.svgs.front().h, 36.0f, 0.001f));
  }
}

spec("ImageWidget prefers draw_svg for svg sources") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element image_elem;
    image_elem.set_layout_bounds(0.0f, 0.0f, 72.0f, 36.0f);
    ImageWidget image("icon.svg");
    render_widget(image, image_elem, renderer);

    check(backend.images.empty());
    check(backend.rects.empty());
    check(backend.svgs.size() == 1);
    check(backend.svgs.front().src == "icon.svg");
    check(approx_eq(backend.svgs.front().w, 72.0f, 0.001f));
    check(approx_eq(backend.svgs.front().h, 36.0f, 0.001f));
  }
}

spec("ImageWidget applies object-fit none and object-position variables") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element image_elem;
    image_elem.set_layout_bounds(0.0f, 0.0f, 100.0f, 80.0f);
    image_elem.computed_style->variables[Symbol("--object-fit")] = "none";
    image_elem.computed_style->variables[Symbol("--object-position")] =
        "right bottom";

    ImageWidget image("demo.png");
    image.set_natural_size(20.0f, 10.0f);
    render_widget(image, image_elem, renderer);

    check(backend.images.size() == 1);
    check(approx_eq(backend.images.front().x, 80.0f, 0.001f));
    check(approx_eq(backend.images.front().y, 70.0f, 0.001f));
    check(approx_eq(backend.images.front().w, 20.0f, 0.001f));
    check(approx_eq(backend.images.front().h, 10.0f, 0.001f));
  }
}

spec("ImageWidget applies object-position edge offsets") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element image_elem;
    image_elem.set_layout_bounds(0.0f, 0.0f, 100.0f, 80.0f);
    image_elem.computed_style->variables[Symbol("--object-fit")] = "none";
    image_elem.computed_style->variables[Symbol("--object-position")] =
        "right 10px bottom 5px";

    ImageWidget image("demo.png");
    image.set_natural_size(20.0f, 10.0f);
    render_widget(image, image_elem, renderer);

    check(backend.images.size() == 1);
    check(approx_eq(backend.images.front().x, 70.0f, 0.001f));
    check(approx_eq(backend.images.front().y, 65.0f, 0.001f));
    check(approx_eq(backend.images.front().w, 20.0f, 0.001f));
    check(approx_eq(backend.images.front().h, 10.0f, 0.001f));
  }
}

spec("ImageWidget applies object-fit contain and cover variables") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element contain_elem;
    contain_elem.set_layout_bounds(0.0f, 0.0f, 100.0f, 80.0f);
    contain_elem.computed_style->variables[Symbol("--object-fit")] = "contain";
    contain_elem.computed_style->variables[Symbol("--object-position")] =
        "center bottom";
    ImageWidget contain("contain.png");
    contain.set_natural_size(200.0f, 100.0f);
    render_widget(contain, contain_elem, renderer);

    check(backend.images.size() == 1);
    if (backend.images.size() != 1) {
      return;
    }
    check(approx_eq(backend.images[0].x, 0.0f, 0.001f));
    check(approx_eq(backend.images[0].y, 30.0f, 0.001f));
    check(approx_eq(backend.images[0].w, 100.0f, 0.001f));
    check(approx_eq(backend.images[0].h, 50.0f, 0.001f));

    const size_t contain_count = backend.images.size();
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Element cover_elem;
    cover_elem.set_layout_bounds(0.0f, 0.0f, 100.0f, 80.0f);
    cover_elem.computed_style->variables[Symbol("--object-fit")] = "cover";
    cover_elem.computed_style->variables[Symbol("--object-position")] =
        "right center";
    ImageWidget cover("cover.png");
    cover.set_natural_size(200.0f, 100.0f);
    render_widget(cover, cover_elem, renderer);

    check(backend.images.size() == contain_count + 1);
    if (backend.images.size() != contain_count + 1) {
      return;
    }
    const auto& cover_call = backend.images.back();
    check(approx_eq(cover_call.x, -60.0f, 0.001f));
    check(approx_eq(cover_call.y, 0.0f, 0.001f));
    check(approx_eq(cover_call.w, 160.0f, 0.001f));
    check(approx_eq(cover_call.h, 80.0f, 0.001f));
  }
}

spec("ImageWidget applies parsed CSS object fit and position") {
  it("runs") {
    Box box(nullptr);
    Element* image_elem = box.create_widget<ImageWidget>("img", "hero",
                                                         "demo.png");
    auto* image = static_cast<ImageWidget*>(image_elem->widget);
    image->set_natural_size(20.0f, 10.0f);
    box.set_root(image_elem);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #hero {
        width: 100px;
        height: 80px;
        object-fit: none;
        object-position: right bottom;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(image_elem);

    check(backend.images.size() == 1);
    if (backend.images.empty()) {
      return;
    }
    check(approx_eq(backend.images.front().x, 80.0f, 0.001f));
    check(approx_eq(backend.images.front().y, 70.0f, 0.001f));
    check(approx_eq(backend.images.front().w, 20.0f, 0.001f));
    check(approx_eq(backend.images.front().h, 10.0f, 0.001f));
  }
}

spec("ImageWidget applies parsed CSS object-position edge offsets") {
  it("runs") {
    Box box(nullptr);
    Element* image_elem = box.create_widget<ImageWidget>("img", "hero",
                                                         "demo.png");
    auto* image = static_cast<ImageWidget*>(image_elem->widget);
    image->set_natural_size(20.0f, 10.0f);
    box.set_root(image_elem);
    box.set_viewport(240.0f, 160.0f);

    box.load_css(R"(
      #hero {
        width: 100px;
        height: 80px;
        object-fit: none;
        object-position: right 10px bottom 5px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(image_elem);

    check(backend.images.size() == 1);
    if (backend.images.empty()) {
      return;
    }
    check(approx_eq(backend.images.front().x, 70.0f, 0.001f));
    check(approx_eq(backend.images.front().y, 65.0f, 0.001f));
    check(approx_eq(backend.images.front().w, 20.0f, 0.001f));
    check(approx_eq(backend.images.front().h, 10.0f, 0.001f));
  }
}

spec("ImageWidget applies parsed CSS object-fit contain and cover") {
  it("runs") {
    Box contain_box(nullptr);
    Element* contain_elem = contain_box.create_widget<ImageWidget>(
        "img", "contain", "contain.png");
    auto* contain = static_cast<ImageWidget*>(contain_elem->widget);
    contain->set_natural_size(200.0f, 100.0f);
    contain_box.set_root(contain_elem);
    contain_box.set_viewport(240.0f, 160.0f);

    contain_box.load_css(R"(
      #contain {
        width: 100px;
        height: 80px;
        object-fit: contain;
        object-position: center bottom;
      }
    )");
    contain_box.update();

    RecordingRenderer contain_backend;
    contain_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer contain_renderer(&contain_backend);
    RenderManager contain_render_manager(&contain_renderer);

    contain_render_manager.render_tree(contain_elem);

    check(contain_backend.images.size() == 1);
    if (contain_backend.images.empty()) {
      return;
    }
    check(approx_eq(contain_backend.images.front().x, 0.0f, 0.001f));
    check(approx_eq(contain_backend.images.front().y, 30.0f, 0.001f));
    check(approx_eq(contain_backend.images.front().w, 100.0f, 0.001f));
    check(approx_eq(contain_backend.images.front().h, 50.0f, 0.001f));

    Box cover_box(nullptr);
    Element* cover_elem =
        cover_box.create_widget<ImageWidget>("img", "cover", "cover.png");
    auto* cover = static_cast<ImageWidget*>(cover_elem->widget);
    cover->set_natural_size(200.0f, 100.0f);
    cover_box.set_root(cover_elem);
    cover_box.set_viewport(240.0f, 160.0f);

    cover_box.load_css(R"(
      #cover {
        width: 100px;
        height: 80px;
        object-fit: cover;
        object-position: right center;
      }
    )");
    cover_box.update();

    RecordingRenderer cover_backend;
    cover_backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer cover_renderer(&cover_backend);
    RenderManager cover_render_manager(&cover_renderer);

    cover_render_manager.render_tree(cover_elem);

    check(cover_backend.images.size() == 1);
    if (cover_backend.images.empty()) {
      return;
    }
    check(approx_eq(cover_backend.images.front().x, -60.0f, 0.001f));
    check(approx_eq(cover_backend.images.front().y, 0.0f, 0.001f));
    check(approx_eq(cover_backend.images.front().w, 160.0f, 0.001f));
    check(approx_eq(cover_backend.images.front().h, 80.0f, 0.001f));
  }
}

spec("ButtonWidget skips scale transform when backend lacks scaling support") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.scaling = false;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element button_elem;
    button_elem.set_layout_bounds(20.0f, 30.0f, 100.0f, 40.0f);
    button_elem.computed_style->background_color = {0.2f, 0.4f, 0.6f, 1.0f};
    button_elem.computed_style->text_color = {1.0f, 1.0f, 1.0f, 1.0f};
    ButtonWidget button("Click");

    check_false(button.handle_event(Event::mouse_down(30.0f, 40.0f), button_elem));
    button.update(16.0f, button_elem);
    render_widget(button, button_elem, renderer);

    check_false(backend.transforms.empty());
    const auto& first = backend.transforms.front();
    check(approx_eq(first.data[0], 1.0f, 0.001f));
    check(approx_eq(first.data[4], 1.0f, 0.001f));
    check(approx_eq(flex::tx(first), 20.0f, 0.001f));
    check(approx_eq(flex::ty(first), 30.0f, 0.001f));
  }
}

spec("ButtonWidget provides default variant tokens without CSS") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element button_elem;
    button_elem.set_layout_bounds(0.0f, 0.0f, 100.0f, 40.0f);
    ButtonWidget button("Click");

    render_widget(button, button_elem, renderer);

    check_false(backend.rects.empty());
    require_color(backend.rects.front().fill_color,
                  15.0f / 255.0f,
                  23.0f / 255.0f,
                  42.0f / 255.0f);
    check_false(backend.texts.empty());
    require_color(backend.texts.front().color,
                  248.0f / 255.0f,
                  250.0f / 255.0f,
                  252.0f / 255.0f);
  }
}

spec("ButtonWidget outline variant renders transparent fill and border") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element button_elem;
    button_elem.set_layout_bounds(0.0f, 0.0f, 100.0f, 40.0f);
    ButtonWidget button("Click");
    button.set_variant(ButtonWidget::Variant::Outline);

    render_widget(button, button_elem, renderer);

    check_false(backend.rects.empty());
    check(approx_eq(backend.rects.front().fill_color.a, 0.0f, 0.001f));
    require_color(backend.rects.front().stroke_color,
                  203.0f / 255.0f,
                  213.0f / 255.0f,
                  225.0f / 255.0f);
    check(approx_eq(backend.rects.front().stroke_width, 1.0f, 0.001f));
  }
}

spec("ButtonWidget size presets affect text metrics without CSS") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element small_elem;
    small_elem.set_layout_bounds(0.0f, 0.0f, 100.0f, 40.0f);
    ButtonWidget small("Click");
    small.set_size(ButtonWidget::Size::Small);
    render_widget(small, small_elem, renderer);

    Element large_elem;
    large_elem.set_layout_bounds(0.0f, 50.0f, 100.0f, 40.0f);
    ButtonWidget large("Click");
    large.set_size(ButtonWidget::Size::Large);
    render_widget(large, large_elem, renderer);

    check(backend.texts.size() >= 2);
    check(approx_eq(backend.texts[0].size, 13.0f, 0.001f));
    check(approx_eq(backend.texts[1].size, 16.0f, 0.001f));
  }
}

spec("ButtonWidget reuses shared text ellipsis and decoration semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element button_elem;
    button_elem.set_layout_bounds(0.0f, 0.0f, 80.0f, 40.0f);
    button_elem.computed_style->font_size = 10.0f;
    button_elem.computed_style->variables[Symbol("--white-space")] = "nowrap";
    button_elem.computed_style->variables[Symbol("--text-overflow")] = "ellipsis";
    button_elem.computed_style->variables[Symbol("--text-decoration")] = "underline";
    button_elem.computed_style->variables[Symbol("--text-underline-offset")] = "4px";
    ButtonWidget button("VeryLongButtonLabel");

    render_widget(button, button_elem, renderer);

    check_false(backend.texts.empty());
    check(backend.texts.front().text.find("...") != std::string::npos);
    check_false(backend.lines.empty());
    check(backend.lines.front().x2 > backend.lines.front().x1);
    if (!backend.texts.empty() && !backend.lines.empty()) {
      const float expected_y = backend.texts.front().y + 10.0f + 4.0f;
      check(approx_eq(backend.lines.front().y1, expected_y, 0.001f));
      check(approx_eq(backend.lines.front().y2, expected_y, 0.001f));
    }
  }
}

spec("InputWidget provides default tokens without CSS") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    InputWidget input("Email");

    render_widget(input, input_elem, renderer);

    check_false(backend.rects.empty());
    const auto* fill_rect = find_fill_rect(backend);
    const auto* stroke_rect = find_stroke_rect(backend);
    check(fill_rect != nullptr);
    check(stroke_rect != nullptr);
    require_color(fill_rect->fill_color,
                  1.0f, 1.0f, 1.0f);
    require_color(stroke_rect->stroke_color,
                  203.0f / 255.0f,
                  213.0f / 255.0f,
                  225.0f / 255.0f);
    check_false(backend.texts.empty());
    require_color(backend.texts.front().color,
                  100.0f / 255.0f,
                  116.0f / 255.0f,
                  139.0f / 255.0f);
  }
}

spec("InputWidget focus state promotes border token without CSS") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    input_elem.set_focus(true);
    InputWidget input("Email");

    render_widget(input, input_elem, renderer);

    check_false(backend.rects.empty());
    const auto* stroke_rect = find_stroke_rect(backend);
    check(stroke_rect != nullptr);
    require_color(stroke_rect->stroke_color,
                  59.0f / 255.0f,
                  130.0f / 255.0f,
                  246.0f / 255.0f);
    check(approx_eq(stroke_rect->stroke_width, 1.5f, 0.001f));
  }
}

spec("InputWidget size presets affect placeholder metrics without CSS") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element small_elem;
    small_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    InputWidget small("Email");
    small.set_size(InputWidget::Size::Small);
    render_widget(small, small_elem, renderer);

    Element large_elem;
    large_elem.set_layout_bounds(0.0f, 50.0f, 180.0f, 48.0f);
    InputWidget large("Email");
    large.set_size(InputWidget::Size::Large);
    render_widget(large, large_elem, renderer);

    check(backend.texts.size() >= 2);
    check(approx_eq(backend.texts[0].size, 13.0f, 0.001f));
    check(approx_eq(backend.texts[1].size, 16.0f, 0.001f));
  }
}

spec("InputWidget uses CSS placeholder color tokens from ::placeholder rules") {
  it("runs") {
    RecordingRenderer backend;
    Box box(&backend);
    auto* root = box.create("div");
    auto* input_elem = box.create_widget<InputWidget>("input", "search", "Search");
    input_elem->add_class("field");
    root->append(input_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .field {
        width: 180px;
        height: 40px;
      }
      .field::placeholder {
        color: #7c8da1;
      }
    )");
    box.update();

    check_false(backend.texts.empty());
    require_color(backend.texts.front().color,
                  0x7c / 255.0f,
                  0x8d / 255.0f,
                  0xa1 / 255.0f);
  }
}

spec("ButtonWidget shares line-through geometry with text layout semantics") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element button_elem;
    button_elem.set_layout_bounds(0.0f, 0.0f, 120.0f, 40.0f);
    button_elem.computed_style->font_size = 10.0f;
    button_elem.computed_style->variables[Symbol("--text-decoration")] =
        "line-through";
    ButtonWidget button("Hello");

    render_widget(button, button_elem, renderer);

    check(backend.texts.size() == 1);
    check(backend.lines.size() == 1);
    if (backend.texts.size() != 1 || backend.lines.size() != 1) {
      return;
    }
    const float expected_y =
        backend.texts.front().y + 10.0f * 0.5f - 10.0f * 0.3f;
    check(approx_eq(backend.lines.front().y1, expected_y, 0.001f));
    check(approx_eq(backend.lines.front().y2, expected_y, 0.001f));
  }
}

spec("InputWidget uses shared selection bridge colors and rtl caret geometry") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    input_elem.computed_style->font_size = 10.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    input_elem.computed_style->direction = Direction::Rtl;
    input_elem.computed_style->variables[Symbol("--selection-bg")] =
        "51, 68, 85, 255";
    input_elem.computed_style->variables[Symbol("--selection-color")] =
        "248, 250, 252, 255";
    InputWidget input("Email");
    input.set_text("abcd");
    render_widget(input, input_elem, renderer);
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    input.select_all();

    float caret_x = 0.0f;
    float caret_y = 0.0f;
    float caret_w = 0.0f;
    float caret_h = 0.0f;
    input.get_caret_rect(input_elem, caret_x, caret_y, caret_w, caret_h);

    render_widget(input, input_elem, renderer);

    check_false(backend.rects.empty());
    check_false(backend.texts.empty());
    const auto selection_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x44 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x55 / 255.0f, 0.001f);
        });
    check(selection_rect != backend.rects.end());
    if (selection_rect != backend.rects.end()) {
      check(caret_x >= selection_rect->x - 0.001f);
      check(caret_x <= selection_rect->x + selection_rect->w + 0.001f);
      check(approx_eq(caret_x, selection_rect->x, 0.001f) ||
            approx_eq(caret_x, selection_rect->x + selection_rect->w, 0.001f));
    }
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) {
                        return approx_eq(call.color.r, 0xf8 / 255.0f, 0.001f) &&
                               approx_eq(call.color.g, 0xfa / 255.0f, 0.001f) &&
                               approx_eq(call.color.b, 0xfc / 255.0f, 0.001f) &&
                               approx_eq(call.color.a, 1.0f, 0.001f);
                      }));
  }
}

spec("Input TextArea Checkbox and Switch accept generic caret and accent aliases") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    input_elem.add_state("focus");
    input_elem.computed_style->font_size = 10.0f;
    input_elem.computed_style->variables[Symbol("--caret-color")] =
        "18, 52, 86, 255";
    InputWidget input;
    input.set_text("A");
    input.set_cursor_pos(1);
    render_widget(input, input_elem, renderer);

    const auto input_caret = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 18.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 52.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 86.0f / 255.0f, 0.001f);
        });
    check(input_caret != backend.rects.end());

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Element textarea_elem;
    textarea_elem.set_layout_bounds(0.0f, 50.0f, 180.0f, 80.0f);
    textarea_elem.add_state("focus");
    textarea_elem.computed_style->font_size = 10.0f;
    textarea_elem.computed_style->variables[Symbol("--caret-color")] =
        "18, 52, 86, 255";
    TextAreaWidget textarea("A");
    textarea.set_cursor_position(1);
    render_widget(textarea, textarea_elem, renderer);

    const auto textarea_caret = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 2.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 18.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 52.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 86.0f / 255.0f, 0.001f);
        });
    check(textarea_caret != backend.rects.end());

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Element checkbox_elem;
    checkbox_elem.set_layout_bounds(0.0f, 0.0f, 120.0f, 24.0f);
    checkbox_elem.computed_style->variables[Symbol("--accent-color")] =
        "255, 51, 0, 255";
    CheckboxWidget checkbox("Label", true);
    render_widget(checkbox, checkbox_elem, renderer);

    const auto checkbox_fill = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 51.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.0f, 0.001f);
        });
    check(checkbox_fill != backend.rects.end());

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Element switch_elem;
    switch_elem.set_layout_bounds(0.0f, 0.0f, 120.0f, 28.0f);
    switch_elem.computed_style->variables[Symbol("--accent-color")] =
        "255, 51, 0, 255";
    SwitchWidget toggle("", true);
    render_widget(toggle, switch_elem, renderer);

    const auto switch_track = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 51.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.0f, 0.001f) &&
                 call.w >= 50.0f;
        });
    check(switch_track != backend.rects.end());
  }
}

spec("Input TextArea Checkbox Radio and Switch apply parsed CSS caret and accent colors") {
  it("runs") {
    Box box(nullptr);
    Element* root = box.create("div", "root");
    Element* input_elem = box.create_widget<InputWidget>("input", "input");
    Element* textarea_elem =
        box.create_widget<TextAreaWidget>("textarea", "textarea", "A");
    Element* checkbox_elem =
        box.create_widget<CheckboxWidget>("button", "checkbox", "Label", true);
    Element* radio_elem =
        box.create_widget<RadioWidget>("button", "radio", "Choice", "choice",
                                       "group", true);
    Element* switch_elem =
        box.create_widget<SwitchWidget>("button", "switch", "", true);
    Element* progress_elem =
        box.create_widget<ProgressBarWidget>("div", "progress", 50.0f, false);
    Element* slider_elem =
        box.create_widget<SliderWidget>("div", "slider", 0.0f, 100.0f, 50.0f,
                                        1.0f);
    auto* input = static_cast<InputWidget*>(input_elem->widget);
    auto* textarea = static_cast<TextAreaWidget*>(textarea_elem->widget);
    input->set_text("A");
    input->set_cursor_pos(1);
    textarea->set_cursor_position(1);
    input_elem->add_state("focus");
    textarea_elem->add_state("focus");
    root->append(input_elem);
    root->append(textarea_elem);
    root->append(checkbox_elem);
    root->append(radio_elem);
    root->append(switch_elem);
    root->append(progress_elem);
    root->append(slider_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 280.0f);

    box.load_css(R"(
      #root {
        width: 240px;
        height: 260px;
        display: flex;
        flex-direction: column;
      }
      #input {
        width: 180px;
        height: 40px;
        font-size: 10px;
        caret-color: #123456;
      }
      #textarea {
        width: 180px;
        height: 80px;
        font-size: 10px;
        caret-color: #123456;
      }
      #checkbox {
        width: 120px;
        height: 24px;
        accent-color: #ff3300;
      }
      #radio {
        width: 120px;
        height: 24px;
        accent-color: #ff3300;
      }
      #switch {
        width: 120px;
        height: 28px;
        accent-color: #ff3300;
      }
      #progress {
        width: 120px;
        height: 24px;
        accent-color: #ff3300;
      }
      #slider {
        width: 120px;
        height: 28px;
        accent-color: #ff3300;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto input_caret = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 18.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 52.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 86.0f / 255.0f, 0.001f);
        });
    check(input_caret != backend.rects.end());

    const auto textarea_caret = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 2.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 18.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 52.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 86.0f / 255.0f, 0.001f);
        });
    check(textarea_caret != backend.rects.end());

    const auto accent_fill = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 51.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.0f, 0.001f);
        });
    check(accent_fill != backend.rects.end());

    const auto radio_fill = std::find_if(
        backend.circles.begin(), backend.circles.end(),
        [](const DrawCircleCall& call) {
          return approx_eq(call.fill_color.r, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 51.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.0f, 0.001f);
        });
    check(radio_fill != backend.circles.end());

    const auto progress_fill = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 60.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 51.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.0f, 0.001f);
        });
    check(progress_fill != backend.rects.end());

    const auto slider_fill = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.h, 4.0f, 0.001f) &&
                 approx_eq(call.w, 60.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 51.0f / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.0f, 0.001f);
        });
    check(slider_fill != backend.rects.end());
  }
}

spec("Input and TextArea apply parsed CSS font-family to editable text") {
  it("runs") {
    Box box(nullptr);
    Element* root = box.create("div", "root");
    Element* input_elem = box.create_widget<InputWidget>("input", "input");
    Element* textarea_elem =
        box.create_widget<TextAreaWidget>("textarea", "textarea", "Area");
    auto* input = static_cast<InputWidget*>(input_elem->widget);
    input->set_text("Input");
    root->append(input_elem);
    root->append(textarea_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 180.0f);

    box.load_css(R"(
      #root {
        width: 240px;
        height: 140px;
        display: flex;
        flex-direction: column;
      }
      #input,
      #textarea {
        width: 180px;
        height: 40px;
        font-family: Consolas;
        font-size: 10px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto input_text =
        std::find_if(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "Input"; });
    check(input_text != backend.texts.end());
    if (input_text != backend.texts.end()) {
      check(input_text->font == "Consolas");
    }

    const auto textarea_text =
        std::find_if(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "Area"; });
    check(textarea_text != backend.texts.end());
    if (textarea_text != backend.texts.end()) {
      check(textarea_text->font == "Consolas");
    }
  }
}

spec("Host-box widgets apply parsed CSS font-family to text shapes") {
  it("runs") {
    Box box(nullptr);
    Element* root = box.create("div", "root");
    Element* button_elem =
        box.create_widget<ButtonWidget>("button", "button", "Press");
    Element* checkbox_elem =
        box.create_widget<CheckboxWidget>("button", "checkbox", "Agree", true);
    root->append(button_elem);
    root->append(checkbox_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 180.0f);

    box.load_css(R"(
      #root {
        width: 240px;
        height: 120px;
        display: flex;
        flex-direction: column;
      }
      #button,
      #checkbox {
        width: 140px;
        height: 32px;
        font-family: Consolas;
        font-size: 13px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager render_manager(&renderer);

    render_manager.render_tree(root);

    const auto button_text =
        std::find_if(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "Press"; });
    check(button_text != backend.texts.end());
    if (button_text != backend.texts.end()) {
      check(button_text->font == "Consolas");
    }

    const auto checkbox_text =
        std::find_if(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "Agree"; });
    check(checkbox_text != backend.texts.end());
    if (checkbox_text != backend.texts.end()) {
      check(checkbox_text->font == "Consolas");
    }
  }
}

spec("Semantic control parts drive delegated atomic rendering") {
  it("uses child color and background styles for label text caret and selection") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button_elem =
        box.create_widget<ButtonWidget>("button", "part-button", "PartLabel");
    auto* input_elem =
        box.create_widget<InputWidget>("input", "part-input", "Hint");
    auto* textarea_elem = box.create_widget<TextAreaWidget>(
        "textarea", "part-textarea", "AB", "Hint");
    auto* input = static_cast<InputWidget*>(input_elem->widget);
    auto* textarea = static_cast<TextAreaWidget*>(textarea_elem->widget);
    input->set_text("InputPart");
    input->set_cursor_pos(input->text().size());
    textarea->set_selection(0, 1);
    input_elem->add_state("focus");
    root->append(button_elem);
    root->append(input_elem);
    root->append(textarea_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 180.0f);
    box.load_css(R"(
      #root { display: flex; flex-direction: column; width: 240px; }
      #part-button { width: 180px; height: 36px; }
      #part-button > label { color: #123456; }
      #part-input { width: 180px; height: 36px; font-size: 12px; }
      #part-input > text { color: #345678; }
      #part-input > caret { background-color: #abcdef; }
      #part-textarea { width: 180px; height: 72px; font-size: 12px; }
      #part-textarea > selection-layer { background-color: #334455; }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(320.0f, 180.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager manager(&renderer);
    manager.render_tree(root);

    const auto label = std::find_if(
        backend.texts.begin(), backend.texts.end(), [](const TextCall& call) {
          return call.text == "PartLabel" &&
                 approx_eq(call.color.r, 0x12 / 255.0f, 0.001f) &&
                 approx_eq(call.color.g, 0x34 / 255.0f, 0.001f) &&
                 approx_eq(call.color.b, 0x56 / 255.0f, 0.001f);
        });
    check(label != backend.texts.end());
    const auto input_text = std::find_if(
        backend.texts.begin(), backend.texts.end(), [](const TextCall& call) {
          return call.text == "InputPart" &&
                 approx_eq(call.color.r, 0x34 / 255.0f, 0.001f) &&
                 approx_eq(call.color.g, 0x56 / 255.0f, 0.001f) &&
                 approx_eq(call.color.b, 0x78 / 255.0f, 0.001f);
        });
    check(input_text != backend.texts.end());
    const auto caret = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.w, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 0xab / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0xcd / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0xef / 255.0f, 0.001f);
        });
    check(caret != backend.rects.end());
    const auto selection = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return call.w > 0.0f &&
                 approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x44 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x55 / 255.0f, 0.001f);
        });
    check(selection != backend.rects.end());
  }
}

spec("Composite control semantic parts render host CSS and vector indicators") {
  it("renders checkbox switch and textarea surfaces through their stable tree") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* checkbox_elem = box.create_widget<CheckboxWidget>(
        "checkbox", "render-checkbox", "Status", true);
    auto* switch_elem = box.create_widget<SwitchWidget>(
        "switch", "render-switch", "Power", true);
    auto* textarea_elem = box.create_widget<TextAreaWidget>(
        "textarea", "render-textarea", "", "Write here");
    root->append(checkbox_elem);
    root->append(switch_elem);
    root->append(textarea_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 180.0f);
    box.load_css(R"(
      #root { display: flex; flex-direction: column; width: 300px; }
      #root { --bg: #334155; }
      #render-checkbox {
        height: 20px;
        background-color: #2563eb;
        border: 1px solid #1d4ed8;
      }
      #render-checkbox > indicator { color: #f8fafc; }
      #render-switch {
        height: 24px;
        --switch-width: 44px;
        --switch-height: 24px;
        background-color: #22c55e;
      }
      #render-switch > thumb { background-color: #ffffff; }
      #render-textarea {
        width: 180px;
        height: 72px;
        background-color: #f1f5f9;
        border: 2px solid #64748b;
        border-radius: 6px;
      }
    )");
    box.update();

    RecordingRenderer backend;
    backend.begin_frame(320.0f, 180.0f, 1.0f);
    Renderer renderer(&backend);
    RenderManager manager(&renderer);
    manager.render_tree(root);

    check(backend.lines.size() >= 2);
    check(std::all_of(backend.lines.begin(), backend.lines.end(),
                      [](const LineCall& line) {
                        return approx_eq(line.color.r, 0xf8 / 255.0f, 0.001f) &&
                               approx_eq(line.color.g, 0xfa / 255.0f, 0.001f) &&
                               approx_eq(line.color.b, 0xfc / 255.0f, 0.001f);
                      }));
    const auto checkbox_surface = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x25 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x63 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0xeb / 255.0f, 0.001f);
        });
    check(checkbox_surface != backend.rects.end());
    const auto switch_surface = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x22 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0xc5 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x5e / 255.0f, 0.001f);
        });
    check(switch_surface != backend.rects.end());
    const auto switch_thumb = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 1.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 1.0f, 0.001f) &&
                 call.w > 0.0f && call.w < 44.0f;
        });
    check(switch_thumb != backend.rects.end());
    const auto textarea_surface = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0xf1 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0xf5 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0xf9 / 255.0f, 0.001f);
        });
    check(textarea_surface != backend.rects.end());
    const auto inherited_bg_overlay = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.w, 180.0f, 0.001f) &&
                 approx_eq(call.h, 72.0f, 0.001f) &&
                 approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x41 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x55 / 255.0f, 0.001f);
        });
    check(inherited_bg_overlay == backend.rects.end());
  }
}

spec("InputWidget renders composition text through shared segmented text drawing") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    input_elem.computed_style->font_size = 16.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    InputWidget input("");

    check(input.handle_event(Event::composition_start(), input_elem));
    check(input.handle_event(Event::composition_update("A🙂B"), input_elem));
    render_widget(input, input_elem, renderer);

    check(backend.texts.size() >= 3);
    if (backend.texts.size() >= 3) {
      check(backend.texts[0].text == "A");
      check(backend.texts[1].text == "🙂");
      check(backend.texts[2].text == "B");
      check(backend.texts[1].x > backend.texts[0].x);
      check(backend.texts[2].x > backend.texts[1].x);
    }
  }
}

spec("InputWidget applies letter spacing across segmented emoji text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    input_elem.computed_style->font_size = 16.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    InputWidget input("");
    input.set_text("A🙂B");

    render_widget(input, input_elem, renderer);
    check(backend.texts.size() >= 3);
    if (backend.texts.size() < 3) {
      return;
    }
    const auto base_a = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "A"; });
    const auto base_emoji = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto base_b = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(base_a != backend.texts.end());
    check(base_emoji != backend.texts.end());
    check(base_b != backend.texts.end());
    if (base_a == backend.texts.end() || base_emoji == backend.texts.end() ||
        base_b == backend.texts.end()) {
      return;
    }
    const float base_emoji_x = base_emoji->x;
    const float base_b_x = base_b->x;
    const size_t base_count = backend.texts.size();

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    input_elem.computed_style->letter_spacing = 4.0f;
    render_widget(input, input_elem, renderer);
    check(backend.texts.size() >= base_count + 3);
    if (backend.texts.size() < base_count + 3) {
      return;
    }
    const auto spaced_emoji = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(base_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto spaced_b = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(base_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(spaced_emoji != backend.texts.end());
    check(spaced_b != backend.texts.end());
    if (spaced_emoji == backend.texts.end() || spaced_b == backend.texts.end()) {
      return;
    }
    check(spaced_emoji->x > base_emoji_x + 3.5f);
    check(spaced_b->x > base_b_x + 7.5f);
  }
}

spec("InputWidget applies word spacing to editable text rendering") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    input_elem.computed_style->font_size = 10.0f;
    input_elem.computed_style->word_spacing = 8.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    InputWidget input("");
    input.set_text("Hello world");

    render_widget(input, input_elem, renderer);

    const auto hello = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Hello"; });
    const auto world = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "world"; });
    check(hello != backend.texts.end());
    check(world != backend.texts.end());
    if (hello == backend.texts.end() || world == backend.texts.end()) {
      return;
    }

    ComputedStyle measure_style = *input_elem.computed_style;
    measure_style.word_spacing = 0.0f;
    const float expected_delta =
        approximate_text_width(&measure_style, "Hello ") + 8.0f;
    check(approx_eq(world->x - hello->x, expected_delta, 0.001f));
  }
}

spec("InputWidget renders transformed display text without mutating value") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    input_elem.computed_style->font_size = 14.0f;
    input_elem.computed_style->text_transform = TextTransform::Capitalize;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    InputWidget input("");
    input.set_text("hello-world_test");

    render_widget(input, input_elem, renderer);

    check(input.text() == "hello-world_test");
    check(std::any_of(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Hello-World_Test"; }));
  }
}

spec("InputWidget forwards CSS font-weight to editable text drawing") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    input_elem.computed_style->font_size = 14.0f;
    input_elem.computed_style->font_weight = FontWeight::Bold;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    InputWidget input("");
    input.set_text("Bold");

    render_widget(input, input_elem, renderer);

    const auto drawn = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Bold"; });
    check(drawn != backend.texts.end());
    if (drawn != backend.texts.end()) {
      check(drawn->bold);
    }
  }
}

spec("InputWidget draws CSS text shadows before editable text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    input_elem.computed_style->font_size = 14.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    input_elem.computed_style->has_text_shadow = true;
    input_elem.computed_style->text_shadows = {
        TextShadow{2.0f, 3.0f, 0.0f, Color{1.0f, 0.0f, 0.0f, 1.0f}}};
    InputWidget input("");
    input.set_text("Shadow");

    render_widget(input, input_elem, renderer);

    check(backend.texts.size() >= 2);
    if (backend.texts.size() < 2) {
      return;
    }
    check(backend.texts[0].text == "Shadow");
    check(backend.texts[1].text == "Shadow");
    check(approx_eq(backend.texts[0].x, backend.texts[1].x + 2.0f, 0.001f));
    check(approx_eq(backend.texts[0].y, backend.texts[1].y + 3.0f, 0.001f));
  }
}

spec("InputWidget preserves letter spacing across segmented selection boundaries") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    input_elem.computed_style->font_size = 16.0f;
    input_elem.computed_style->letter_spacing = 4.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    input_elem.computed_style->variables[Symbol("--selection-bg")] =
        "51, 68, 85, 255";
    input_elem.computed_style->variables[Symbol("--selection-color")] =
        "248, 250, 252, 255";

    InputWidget input("");
    input.set_text("A🙂B");
    render_widget(input, input_elem, renderer);

    check(backend.texts.size() >= 3);
    if (backend.texts.size() < 3) {
      return;
    }

    const auto base_a = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "A"; });
    const auto base_emoji = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto base_b = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(base_a != backend.texts.end());
    check(base_emoji != backend.texts.end());
    check(base_b != backend.texts.end());
    if (base_a == backend.texts.end() || base_emoji == backend.texts.end() ||
        base_b == backend.texts.end()) {
      return;
    }
    const float base_a_x = base_a->x;
    const float base_emoji_x = base_emoji->x;
    const float base_b_x = base_b->x;

    float caret0_x = 0.0f;
    float caret1_x = 0.0f;
    float caret_y = 0.0f;
    float caret_w = 0.0f;
    float caret_h = 0.0f;
    input.set_cursor_pos(0);
    input.get_caret_rect(input_elem, caret0_x, caret_y, caret_w, caret_h);
    input.set_cursor_pos(1);
    input.get_caret_rect(input_elem, caret1_x, caret_y, caret_w, caret_h);

    const float drag_y = input_elem.height() * 0.5f;
    check(input.handle_event(Event::mouse_down(caret0_x + 0.1f, drag_y), input_elem));
    check(input.handle_event(Event::mouse_move(caret1_x + 0.1f, drag_y), input_elem));
    check(input.handle_event(Event::mouse_up(caret1_x + 0.1f, drag_y), input_elem));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    render_widget(input, input_elem, renderer);

    check(backend.texts.size() >= 3);
    if (backend.texts.size() < 3) {
      return;
    }

    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) {
                        return approx_eq(call.color.r, 0xf8 / 255.0f, 0.001f) &&
                               approx_eq(call.color.g, 0xfa / 255.0f, 0.001f) &&
                               approx_eq(call.color.b, 0xfc / 255.0f, 0.001f) &&
                               approx_eq(call.color.a, 1.0f, 0.001f);
                      }));

    const auto selected_a = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "A"; });
    const auto selected_emoji = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto selected_b = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(selected_a != backend.texts.end());
    check(selected_emoji != backend.texts.end());
    check(selected_b != backend.texts.end());
    if (selected_a != backend.texts.end() && selected_emoji != backend.texts.end() &&
        selected_b != backend.texts.end()) {
      check(approx_eq(selected_a->x, base_a_x, 0.001f));
      check(approx_eq(selected_emoji->x, base_emoji_x, 0.001f));
      check(approx_eq(selected_b->x, base_b_x, 0.001f));
    }

    const auto selection_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x44 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x55 / 255.0f, 0.001f);
        });
    check(selection_rect != backend.rects.end());
    if (selection_rect != backend.rects.end()) {
      check(approx_eq(selection_rect->x, caret0_x, 0.001f));
      check(approx_eq(selection_rect->w, caret1_x - caret0_x, 0.001f));
    }
  }
}

spec("InputWidget keeps letter spacing geometry consistent for segmented emoji text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    input_elem.computed_style->font_size = 16.0f;
    input_elem.computed_style->letter_spacing = 4.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    input_elem.computed_style->variables[Symbol("--selection-bg")] =
        "51, 68, 85, 255";
    InputWidget input("");
    input.set_text("A🙂B");
    render_widget(input, input_elem, renderer);

    float caret0_x = 0.0f;
    float caret1_x = 0.0f;
    float caret2_x = 0.0f;
    float caret3_x = 0.0f;
    float caret_y = 0.0f;
    float caret_w = 0.0f;
    float caret_h = 0.0f;
    input.set_cursor_pos(0);
    input.get_caret_rect(input_elem, caret0_x, caret_y, caret_w, caret_h);
    input.set_cursor_pos(1);
    input.get_caret_rect(input_elem, caret1_x, caret_y, caret_w, caret_h);
    input.set_cursor_pos(2);
    input.get_caret_rect(input_elem, caret2_x, caret_y, caret_w, caret_h);
    input.set_cursor_pos(3);
    input.get_caret_rect(input_elem, caret3_x, caret_y, caret_w, caret_h);

    check(caret1_x > caret0_x);
    check(caret2_x > caret1_x);
    check(caret3_x > caret2_x);
    check((caret2_x - caret1_x) > (caret1_x - caret0_x) + 3.0f);

    const float y = input_elem.height() * 0.5f;
    const float left_probe = caret1_x + (caret2_x - caret1_x) * 0.25f;
    const float right_probe = caret1_x + (caret2_x - caret1_x) * 0.75f;
    check(input.handle_event(Event::mouse_down(left_probe, y), input_elem));
    float left_hit_x = 0.0f;
    input.get_caret_rect(input_elem, left_hit_x, caret_y, caret_w, caret_h);
    check(approx_eq(left_hit_x, caret1_x, 0.001f));
    check(input.handle_event(Event::mouse_up(left_probe, y), input_elem));

    check(input.handle_event(Event::mouse_down(right_probe, y), input_elem));
    float right_hit_x = 0.0f;
    input.get_caret_rect(input_elem, right_hit_x, caret_y, caret_w, caret_h);
    check(approx_eq(right_hit_x, caret2_x, 0.001f));
    check(input.handle_event(Event::mouse_up(right_probe, y), input_elem));

    input.select_all();
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    render_widget(input, input_elem, renderer);
    const auto selection_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0x44 / 255.0f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0x55 / 255.0f, 0.001f);
        });
    check(selection_rect != backend.rects.end());
    if (selection_rect != backend.rects.end()) {
      check(approx_eq(selection_rect->x, caret0_x, 0.001f));
      check(approx_eq(selection_rect->w, caret3_x - caret0_x, 0.001f));
    }

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Element composition_elem;
    composition_elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    composition_elem.computed_style->font_size = 16.0f;
    composition_elem.computed_style->letter_spacing = 4.0f;
    composition_elem.computed_style->padding[1] = 8.0f;
    composition_elem.computed_style->padding[3] = 8.0f;
    InputWidget composition_input("");
    check(composition_input.handle_event(Event::composition_start(), composition_elem));
    check(
        composition_input.handle_event(Event::composition_update("A🙂B"), composition_elem));
    render_widget(composition_input, composition_elem, renderer);

    check(backend.texts.size() >= 3);
    const auto underline_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(), [](const DrawRectCall& call) {
          return approx_eq(call.h, 1.0f, 0.001f);
        });
    check(underline_rect != backend.rects.end());
    if (backend.texts.size() >= 3 && underline_rect != backend.rects.end()) {
      check(approx_eq(underline_rect->x, caret0_x, 0.001f));
      check(approx_eq(underline_rect->w, caret3_x - caret0_x, 0.5f));
    }
  }
}

spec("InputWidget applies tabular numeric semantics to caret geometry") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element input_elem;
    input_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    input_elem.computed_style->font_size = 20.0f;
    input_elem.computed_style->padding[1] = 8.0f;
    input_elem.computed_style->padding[3] = 8.0f;
    InputWidget input("Code");

    input.set_text("11");
    render_widget(input, input_elem, renderer);
    input.set_cursor_pos(2);
    float default_11_x = 0.0f;
    float caret_y = 0.0f;
    float caret_w = 0.0f;
    float caret_h = 0.0f;
    input.get_caret_rect(input_elem, default_11_x, caret_y, caret_w, caret_h);

    input.set_text("88");
    render_widget(input, input_elem, renderer);
    input.set_cursor_pos(2);
    float default_88_x = 0.0f;
    input.get_caret_rect(input_elem, default_88_x, caret_y, caret_w, caret_h);
    check(default_11_x < default_88_x);

    input_elem.computed_style->variables[Symbol("--font-variant-numeric")] =
        "tabular-nums";
    input.set_text("11");
    render_widget(input, input_elem, renderer);
    input.set_cursor_pos(2);
    float tabular_11_x = 0.0f;
    input.get_caret_rect(input_elem, tabular_11_x, caret_y, caret_w, caret_h);

    input.set_text("88");
    render_widget(input, input_elem, renderer);
    input.set_cursor_pos(2);
    float tabular_88_x = 0.0f;
    input.get_caret_rect(input_elem, tabular_88_x, caret_y, caret_w, caret_h);
    check(approx_eq(tabular_11_x, tabular_88_x, 0.001f));
  }
}

spec("SearchBoxWidget uses shared segmented text width for caret and backspace") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 240.0f, 36.0f);
    elem.computed_style->font_size = 13.0f;
    elem.computed_style->letter_spacing = 4.0f;

    SearchBoxWidget widget("Find 🙂");
    widget.add_suggestion("1", "Alpha 🙂", "Desc 😀");

    check(widget.handle_event(Event::mouse_down(10.0f, 10.0f), elem));
    check(widget.handle_event(Event::text_input("a"), elem));
    check(widget.handle_event(Event::text_input("🙂"), elem));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    render_widget(widget, elem, renderer);
    render_widget_overlay(widget, elem, renderer);

    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));

    ComputedStyle measure_style = *elem.computed_style;
    measure_style.font_size = 13.0f;
    measure_style.font_weight = FontWeight::Normal;
    const float expected_cursor_x =
        32.0f + approximate_segmented_text_width(&measure_style, "a🙂");
    const auto caret_rect = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 1.0f, 0.001f) &&
                 approx_eq(call.h, 20.0f, 0.001f);
        });
    check(caret_rect != backend.rects.end());
    if (caret_rect != backend.rects.end()) {
      check(approx_eq(caret_rect->x, expected_cursor_x, 0.001f));
    }

    check(widget.handle_event(Event::key_down(KeyCode::Backspace), elem));

    const size_t text_count_before_backspace_render = backend.texts.size();
    const size_t rect_count_before_backspace_render = backend.rects.size();
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    render_widget(widget, elem, renderer);
    check(std::none_of(backend.texts.begin() +
                           static_cast<std::ptrdiff_t>(text_count_before_backspace_render),
                       backend.texts.end(),
                       [](const TextCall& call) { return call.text == "🙂"; }));

    const auto caret_after_backspace = std::find_if(
        backend.rects.begin() +
            static_cast<std::ptrdiff_t>(rect_count_before_backspace_render),
        backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.w, 1.0f, 0.001f) &&
                 approx_eq(call.h, 20.0f, 0.001f);
        });
    check(caret_after_backspace != backend.rects.end());
    if (caret_after_backspace != backend.rects.end()) {
      check(approx_eq(caret_after_backspace->x,
                      32.0f + approximate_segmented_text_width(&measure_style, "a"),
                      0.001f));
    }
  }
}

spec("MenuWidget measures segmented labels and shortcuts through shared text width") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 200.0f, 40.0f);
    elem.computed_style->font_size = 13.0f;
    elem.computed_style->letter_spacing = 3.0f;

    MenuWidget widget;
    widget.add_item("open", "Open 🙂", nullptr, "Ctrl+🙂");
    widget.show(20.0f, 24.0f);
    render_widget_overlay(widget, elem, renderer);

    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));

    ComputedStyle label_style = *elem.computed_style;
    label_style.font_size = 13.0f;
    label_style.font_weight = FontWeight::Normal;
    ComputedStyle shortcut_style = label_style;
    shortcut_style.font_size = 12.0f;

    const float label_width =
        approximate_segmented_text_width(&label_style, "Open 🙂");
    const float shortcut_width =
        approximate_segmented_text_width(&shortcut_style, "Ctrl+🙂");
    const float expected_menu_width =
        std::max(120.0f, label_width + 40.0f + shortcut_width + 20.0f);
    const float expected_shortcut_x = 20.0f + expected_menu_width - 12.0f - shortcut_width;

    const auto shortcut_call = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Ctrl+"; });
    check(shortcut_call != backend.texts.end());
    if (shortcut_call != backend.texts.end()) {
      check(approx_eq(shortcut_call->x, expected_shortcut_x, 0.001f));
    }
  }
}

spec("DialogWidget and PopoverWidget use shared segmented overlay text width") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(40.0f, 48.0f, 180.0f, 36.0f);
    elem.computed_style->font_size = 13.0f;
    elem.computed_style->letter_spacing = 3.0f;

    DialogWidget dialog("Hello 🙂", DialogWidget::Type::Confirm);
    dialog.set_message("Body 😀");
    dialog.set_buttons({{"ok", "OK 🙂", true}});
    dialog.show();
    render_widget_overlay(dialog, elem, renderer);

    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "😀"; }));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    PopoverWidget popover("Details 🙂");
    popover.set_title("Info 😀");
    popover.show();
    render_widget_overlay(popover, elem, renderer);

    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "😀"; }));

    ComputedStyle content_style = *elem.computed_style;
    content_style.font_size = 12.0f;
    const float expected_popover_width =
        std::max(120.0f,
                 approximate_segmented_text_width(&content_style, "Details 🙂") + 24.0f);
    const auto popover_bg = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [&](const DrawRectCall& call) {
          return approx_eq(call.stroke_width, 1.0f, 0.001f) &&
                 approx_eq(call.w, expected_popover_width, 0.001f);
        });
    check(popover_bg != backend.rects.end());
  }
}

spec("Notification Sidebar ListView and Toolbar use shared segmented text helpers") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(40.0f, 48.0f, 220.0f, 180.0f);
    elem.computed_style->font_size = 13.0f;
    elem.computed_style->letter_spacing = 3.0f;

    NotificationWidget notifications;
    notifications.notify("Done 🙂", "Saved 😀");
    notifications.update(250.0f, elem);
    render_widget_overlay(notifications, elem, renderer);
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "😀"; }));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    SidebarWidget sidebar;
    sidebar.add_section("Main 😀");
    sidebar.add_item("home", "🏠", "Home 🙂", "1");
    render_widget(sidebar, elem, renderer);
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "😀"; }));
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    ListViewWidget list;
    list.add_item("a", "Alpha 🙂", "Beta 😀");
    render_widget(list, elem, renderer);
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "😀"; }));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    ToolbarWidget toolbar;
    toolbar.add_dropdown("more", "⚙", {{"wide", "Very Wide 🙂 Entry"}});
    Element toolbar_elem;
    toolbar_elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 40.0f);
    toolbar_elem.computed_style->font_size = 13.0f;
    toolbar_elem.computed_style->letter_spacing = 3.0f;
    render_widget(toolbar, toolbar_elem, renderer);
    check(toolbar.handle_event(Event::mouse_down(10.0f, 10.0f), toolbar_elem));
    render_widget_overlay(toolbar, toolbar_elem, renderer);

    ComputedStyle dropdown_style = *toolbar_elem.computed_style;
    dropdown_style.font_size = 12.0f;
    const float expected_dropdown_width =
        std::max(120.0f,
                 approximate_segmented_text_width(&dropdown_style, "Very Wide 🙂 Entry") +
                     24.0f);
    const auto dropdown_bg = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [&](const DrawRectCall& call) {
          return approx_eq(call.stroke_width, 1.0f, 0.001f) &&
                 approx_eq(call.w, expected_dropdown_width, 0.001f);
        });
    check(dropdown_bg != backend.rects.end());
  }
}

spec("DatePicker TimePicker and Panel use shared segmented text helpers") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 40.0f);
    elem.computed_style->font_size = 13.0f;
    elem.computed_style->letter_spacing = 3.0f;

    PanelWidget panel("Panel 🙂");
    render_widget(panel, elem, renderer);
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🙂"; }));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    DatePickerWidget datepicker({2024, 4, 22});
    render_widget(datepicker, elem, renderer);
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "📅"; }));

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    TimePickerWidget timepicker({8, 9, 0}, true);
    render_widget(timepicker, elem, renderer);
    check(timepicker.handle_event(Event::mouse_down(10.0f, 10.0f), elem));
    render_widget_overlay(timepicker, elem, renderer);
    check(std::any_of(backend.texts.begin(), backend.texts.end(),
                      [](const TextCall& call) { return call.text == "🕐"; }));

    ComputedStyle spinner_style = *elem.computed_style;
    spinner_style.font_size = 20.0f;
    spinner_style.font_weight = FontWeight::Bold;
    const float expected_center_x =
        8.0f + (60.0f - 8.0f) * 0.5f -
        approximate_segmented_text_width(&spinner_style, "08") * 0.5f;
    const auto spinner_value = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "08"; });
    check(spinner_value != backend.texts.end());
    if (spinner_value != backend.texts.end()) {
      check(approx_eq(spinner_value->x, expected_center_x, 0.001f));
    }
  }
}

spec("SidebarWidget renders selected navigation and badges with stable colors") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 240.0f, 180.0f);
    elem.computed_style->font_size = 13.0f;

    SidebarWidget sidebar;
    sidebar.add_section("Main");
    sidebar.add_item("dashboard", "H", "Dashboard", "3");
    sidebar.add_item("settings", "S", "Settings");
    sidebar.select("dashboard");

    render_widget(sidebar, elem, renderer);

    const auto sidebar_bg = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0.12f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0.14f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.18f, 0.001f) &&
                 approx_eq(call.w, 240.0f, 0.001f) &&
                 approx_eq(call.h, 180.0f, 0.001f);
        });
    check(sidebar_bg != backend.rects.end());

    const auto selected_bg = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0.25f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0.45f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.85f, 0.001f) &&
                 call.w >= 232.0f &&
                 call.h >= 40.0f;
        });
    check(selected_bg != backend.rects.end());

    const auto badge_bg = std::find_if(
        backend.rects.begin(), backend.rects.end(),
        [](const DrawRectCall& call) {
          return approx_eq(call.fill_color.r, 0.9f, 0.001f) &&
                 approx_eq(call.fill_color.g, 0.3f, 0.001f) &&
                 approx_eq(call.fill_color.b, 0.3f, 0.001f) &&
                 approx_eq(call.w, 20.0f, 0.001f) &&
                 approx_eq(call.h, 20.0f, 0.001f);
        });
    check(badge_bg != backend.rects.end());

    const auto dashboard_text = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Dashboard"; });
    check(dashboard_text != backend.texts.end());
    if (dashboard_text != backend.texts.end()) {
      require_color(dashboard_text->color, 1.0f, 1.0f, 1.0f);
    }

    const auto settings_text = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Settings"; });
    check(settings_text != backend.texts.end());
    if (settings_text != backend.texts.end()) {
      require_color(settings_text->color, 0.85f, 0.85f, 0.88f);
    }
  }
}

spec("TextAreaWidget uses shared placeholder metrics and color tokens") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 80.0f);
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    elem.computed_style->variables[Symbol("--line-height")] = "2";
    elem.computed_style->variables[Symbol("--textarea-placeholder")] =
        "124, 141, 161, 255";
    TextAreaWidget textarea("", "Placeholder");

    render_widget(textarea, elem, renderer);

    check(backend.texts.size() == 1);
    check(approx_eq(backend.texts.front().x, 6.0f, 0.001f));
    check(approx_eq(backend.texts.front().y, 9.0f, 0.001f));
    require_color(backend.texts.front().color,
                  0x7c / 255.0f,
                  0x8d / 255.0f,
                  0xa1 / 255.0f);
  }
}

spec("TextAreaWidget uses shared selection bridge colors and rtl alignment") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 80.0f);
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[1] = 6.0f;
    elem.computed_style->padding[3] = 6.0f;
    elem.computed_style->direction = Direction::Rtl;
    elem.computed_style->variables[Symbol("--selection-bg")] =
        "51, 68, 85, 255";
    elem.computed_style->variables[Symbol("--selection-color")] =
        "248, 250, 252, 255";
    TextAreaWidget textarea("AB");
    check(textarea.handle_event(ctrl_key(KeyCode::A), elem));

    float caret_x = 0.0f;
    float caret_y = 0.0f;
    float caret_w = 0.0f;
    float caret_h = 0.0f;
    textarea.get_caret_rect(elem, caret_x, caret_y, caret_w, caret_h);

    render_widget(textarea, elem, renderer);

    check_false(backend.rects.empty());
    check_false(backend.texts.empty());
    check(std::any_of(backend.rects.begin(), backend.rects.end(),
                      [](const DrawRectCall& call) {
                        return approx_eq(call.fill_color.r, 0x33 / 255.0f, 0.001f) &&
                               approx_eq(call.fill_color.g, 0x44 / 255.0f, 0.001f) &&
                               approx_eq(call.fill_color.b, 0x55 / 255.0f, 0.001f);
                      }));
    require_color(backend.texts.front().color,
                  0xf8 / 255.0f,
                  0xfa / 255.0f,
                  0xfc / 255.0f);
    check(backend.texts.front().x > elem.width() * 0.6f);
  }
}

spec("TextAreaWidget applies letter spacing across segmented emoji text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 80.0f);
    elem.computed_style->font_size = 16.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    TextAreaWidget textarea("A🙂B");

    render_widget(textarea, elem, renderer);
    check(backend.texts.size() >= 3);
    if (backend.texts.size() < 3) {
      return;
    }
    const auto base_emoji = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto base_b = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(base_emoji != backend.texts.end());
    check(base_b != backend.texts.end());
    if (base_emoji == backend.texts.end() || base_b == backend.texts.end()) {
      return;
    }
    const float base_emoji_x = base_emoji->x;
    const float base_b_x = base_b->x;
    const size_t base_count = backend.texts.size();

    backend.begin_frame(800.0f, 600.0f, 1.0f);
    elem.computed_style->letter_spacing = 4.0f;
    render_widget(textarea, elem, renderer);
    check(backend.texts.size() >= base_count + 3);
    if (backend.texts.size() < base_count + 3) {
      return;
    }
    const auto spaced_emoji = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(base_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; });
    const auto spaced_b = std::find_if(
        backend.texts.begin() + static_cast<std::ptrdiff_t>(base_count), backend.texts.end(),
        [](const TextCall& call) { return call.text == "B"; });
    check(spaced_emoji != backend.texts.end());
    check(spaced_b != backend.texts.end());
    if (spaced_emoji == backend.texts.end() || spaced_b == backend.texts.end()) {
      return;
    }
    check(spaced_emoji->x > base_emoji_x + 3.5f);
    check(spaced_b->x > base_b_x + 7.5f);
  }
}

spec("TextAreaWidget applies word spacing to editable text rendering") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 80.0f);
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->word_spacing = 8.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    TextAreaWidget textarea("Hello world");

    render_widget(textarea, elem, renderer);

    const auto hello = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Hello"; });
    const auto world = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "world"; });
    check(hello != backend.texts.end());
    check(world != backend.texts.end());
    if (hello == backend.texts.end() || world == backend.texts.end()) {
      return;
    }

    ComputedStyle measure_style = *elem.computed_style;
    measure_style.word_spacing = 0.0f;
    const float expected_delta =
        approximate_text_width(&measure_style, "Hello ") + 8.0f;
    check(approx_eq(world->x - hello->x, expected_delta, 0.001f));
  }
}

spec("TextAreaWidget renders transformed display text without mutating value") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 80.0f);
    elem.computed_style->font_size = 14.0f;
    elem.computed_style->text_transform = TextTransform::Lowercase;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    TextAreaWidget textarea("Mixed CASE");

    render_widget(textarea, elem, renderer);

    check(textarea.text() == "Mixed CASE");
    check(std::any_of(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "mixed case"; }));
  }
}

spec("TextAreaWidget forwards CSS font-weight to editable text drawing") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 80.0f);
    elem.computed_style->font_size = 14.0f;
    elem.computed_style->font_weight = FontWeight::Bold;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    TextAreaWidget textarea("Bold");

    render_widget(textarea, elem, renderer);

    const auto drawn = std::find_if(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "Bold"; });
    check(drawn != backend.texts.end());
    if (drawn != backend.texts.end()) {
      check(drawn->bold);
    }
  }
}

spec("TextAreaWidget draws CSS text shadows before editable text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 220.0f, 80.0f);
    elem.computed_style->font_size = 14.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    elem.computed_style->has_text_shadow = true;
    elem.computed_style->text_shadows = {
        TextShadow{2.0f, 3.0f, 0.0f, Color{1.0f, 0.0f, 0.0f, 1.0f}}};
    TextAreaWidget textarea("Shadow");

    render_widget(textarea, elem, renderer);

    check(backend.texts.size() >= 2);
    if (backend.texts.size() < 2) {
      return;
    }
    check(backend.texts[0].text == "Shadow");
    check(backend.texts[1].text == "Shadow");
    check(approx_eq(backend.texts[0].x, backend.texts[1].x + 2.0f, 0.001f));
    check(approx_eq(backend.texts[0].y, backend.texts[1].y + 3.0f, 0.001f));
  }
}

spec("TextAreaWidget applies tabular numeric semantics to caret geometry") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 80.0f);
    elem.computed_style->font_size = 20.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    TextAreaWidget textarea("11");

    render_widget(textarea, elem, renderer);
    textarea.set_cursor_position(2);
    float default_11_x = 0.0f;
    float caret_y = 0.0f;
    float caret_w = 0.0f;
    float caret_h = 0.0f;
    textarea.get_caret_rect(elem, default_11_x, caret_y, caret_w, caret_h);

    textarea.set_text("88");
    render_widget(textarea, elem, renderer);
    textarea.set_cursor_position(2);
    float default_88_x = 0.0f;
    textarea.get_caret_rect(elem, default_88_x, caret_y, caret_w, caret_h);
    check(default_11_x < default_88_x);

    elem.computed_style->variables[Symbol("--font-variant-numeric")] =
        "tabular-nums";
    textarea.set_text("11");
    render_widget(textarea, elem, renderer);
    textarea.set_cursor_position(2);
    float tabular_11_x = 0.0f;
    textarea.get_caret_rect(elem, tabular_11_x, caret_y, caret_w, caret_h);

    textarea.set_text("88");
    render_widget(textarea, elem, renderer);
    textarea.set_cursor_position(2);
    float tabular_88_x = 0.0f;
    textarea.get_caret_rect(elem, tabular_88_x, caret_y, caret_w, caret_h);
    check(approx_eq(tabular_11_x, tabular_88_x, 0.001f));
  }
}

spec("TextAreaWidget segments emoji runs through shared text utilities") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 80.0f);
    elem.computed_style->font_size = 16.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    TextAreaWidget textarea("A🙂B");

    render_widget(textarea, elem, renderer);

    check(backend.texts.size() >= 3);
    if (backend.texts.size() >= 3) {
      check(backend.texts[0].text == "A");
      check(backend.texts[1].text == "🙂");
      check(backend.texts[2].text == "B");
      check(backend.texts[1].x > backend.texts[0].x);
      check(backend.texts[2].x > backend.texts[1].x);
    }
  }
}

spec("TextAreaWidget uses shared line-height metrics for multiline text") {
  it("runs") {
    RecordingRenderer backend;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element elem;
    elem.set_layout_bounds(0.0f, 0.0f, 180.0f, 80.0f);
    elem.computed_style->font_size = 10.0f;
    elem.computed_style->padding[0] = 4.0f;
    elem.computed_style->padding[3] = 6.0f;
    elem.computed_style->variables[Symbol("--line-height")] = "2";
    TextAreaWidget textarea("One\nTwo");

    render_widget(textarea, elem, renderer);

    check(backend.texts.size() == 2);
    check(approx_eq(backend.texts[0].x, 6.0f, 0.001f));
    check(approx_eq(backend.texts[1].x, 6.0f, 0.001f));
    check(approx_eq(backend.texts[1].y - backend.texts[0].y, 20.0f, 0.001f));
  }
}

spec("AvatarWidget falls back to initials when image backends are unavailable") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = false;
    backend.capabilities_.svg_images = false;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element avatar_elem;
    avatar_elem.set_layout_bounds(0.0f, 0.0f, 64.0f, 64.0f);
    AvatarWidget avatar("Alice Smith");
    render_widget(avatar, avatar_elem, renderer);

    check(backend.images.empty());
    check(backend.svgs.empty());
    check_false(backend.texts.empty());
    check(backend.texts.front().text == "AS");
  }
}

spec("AvatarWidget draws image and suppresses initials when raster images are supported") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element avatar_elem;
    avatar_elem.set_layout_bounds(10.0f, 20.0f, 64.0f, 64.0f);
    AvatarWidget avatar("Alice Smith", "alice.png");
    render_widget(avatar, avatar_elem, renderer);

    check(backend.texts.empty());
    check(backend.images.size() == 1);
    check(backend.images.front().src == "alice.png");
    check(approx_eq(backend.images.front().x, 10.0f, 0.001f));
    check(approx_eq(backend.images.front().y, 20.0f, 0.001f));
    check(approx_eq(backend.images.front().w, 64.0f, 0.001f));
    check(approx_eq(backend.images.front().h, 64.0f, 0.001f));
  }
}

spec("AvatarWidget routes svg images when raster images are unavailable") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = false;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element avatar_elem;
    avatar_elem.set_layout_bounds(4.0f, 6.0f, 48.0f, 48.0f);
    AvatarWidget avatar("Alice Smith", "avatar.svg");
    render_widget(avatar, avatar_elem, renderer);

    check(backend.images.empty());
    check(backend.texts.empty());
    check(backend.svgs.size() == 1);
    check(backend.svgs.front().src == "avatar.svg");
    check(approx_eq(backend.svgs.front().x, 4.0f, 0.001f));
    check(approx_eq(backend.svgs.front().y, 6.0f, 0.001f));
  }
}

spec("AvatarWidget prefers svg rendering for svg image urls") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.capabilities_.svg_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element avatar_elem;
    avatar_elem.set_layout_bounds(4.0f, 6.0f, 48.0f, 48.0f);
    AvatarWidget avatar("Alice Smith", "avatar.svg");
    render_widget(avatar, avatar_elem, renderer);

    check(backend.images.empty());
    check(backend.texts.empty());
    check(backend.svgs.size() == 1);
    check(backend.svgs.front().src == "avatar.svg");
    check(approx_eq(backend.svgs.front().x, 4.0f, 0.001f));
    check(approx_eq(backend.svgs.front().y, 6.0f, 0.001f));
  }
}

spec("AvatarWidget keeps initials when image url is empty") {
  it("runs") {
    RecordingRenderer backend;
    backend.capabilities_.raster_images = true;
    backend.begin_frame(800.0f, 600.0f, 1.0f);
    Renderer renderer(&backend);

    Element avatar_elem;
    avatar_elem.set_layout_bounds(0.0f, 0.0f, 48.0f, 48.0f);
    AvatarWidget avatar("Bob Jones");
    render_widget(avatar, avatar_elem, renderer);

    check(backend.images.empty());
    check(backend.svgs.empty());
    check_false(backend.texts.empty());
    check(backend.texts.front().text == "BJ");
  }
}
