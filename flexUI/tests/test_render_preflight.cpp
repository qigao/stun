#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/render_manager.h>
#include <flexUI/renderer.h>
#include <flexUI/widget.h>

#include <tinytest.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

class PreflightRenderer final : public flex::Renderer {
 public:
  flex::RendererCapabilities caps;
  int operations = 0;
  int begins = 0;
  int ends = 0;
  int clears = 0;
  int rectangles = 0;
  int retained_pushes = 0;
  int removals = 0;
  mutable int surface_queries = 0;
  bool recreate_surface = false;
  bool fail_draw = false;
  std::function<void()> on_draw;

  void begin_frame(float, float, float) override { ++operations; ++begins; }
  void end_frame() override { ++operations; ++ends; }
  bool requires_surface_recreation() const override {
    ++surface_queries;
    return recreate_surface;
  }
  void set_retained_mode(bool enabled) override {
    ++operations;
    retained_mode_ = enabled;
  }
  void save() override { ++operations; }
  void restore() override { ++operations; }
  void reset() override { ++operations; }
  void set_transform(const flex::Transform&) override { ++operations; }
  void translate(float, float) override { ++operations; }
  void rotate(float) override { ++operations; }
  void scale(float, float) override { ++operations; }
  void clip_rect(float, float, float, float) override { ++operations; }
  void reset_clip() override { ++operations; }
  void set_global_alpha(float) override { ++operations; }
  void set_shadow(const flex::Shadow&) override { ++operations; }
  void clear_shadow() override { ++operations; }
  void set_blur(const flex::BlurFilter&) override { ++operations; }
  void clear_blur() override { ++operations; }
  void fill_path(const std::string&, const flex::Paint&) override { ++operations; }
  void stroke_path(const std::string&, const flex::Paint&, float) override {
    ++operations;
  }
  void draw_line(float, float, float, float, const flex::Paint&, float) override {
    ++operations;
  }
  void draw_rect(float, float, float, float, float, const flex::Paint&,
                 const flex::Paint&, float) override {
    ++operations;
    ++rectangles;
    if (on_draw) on_draw();
    if (fail_draw) throw std::runtime_error("test backend draw failure");
  }
  void draw_circle(float, float, float, const flex::Paint&, const flex::Paint&,
                   float) override { ++operations; }
  void draw_ellipse(float, float, float, float, const flex::Paint&,
                    const flex::Paint&, float) override { ++operations; }
  void draw_text(const std::string&, float, float, const std::string&, float,
                 bool, const flex::Color&) override { ++operations; }
  void draw_image(const std::string&, float, float, float, float) override {
    ++operations;
  }
  void draw_svg(const std::string&, float, float, float, float) override {
    ++operations;
  }
  void draw_svg_data(const std::string&, float, float, float, float) override {
    ++operations;
  }
  void clear(const flex::Color&) override { ++operations; ++clears; }
  flex::Bounds viewport() const override { return {0, 0, 100, 80}; }
  flex::RendererCapabilities capabilities() const override { return caps; }
  bool supports_retained_mode() const override { return caps.retained_mode; }
  void remove_cached(flex::PaintHandle) override { ++operations; ++removals; }
  void set_retained_insertion_anchor(flex::PaintHandle) override { ++operations; }
  flex::PaintHandle push_rect(float, float, float, float, float,
                              const flex::Paint&, const flex::Paint&, float,
                              const flex::Transform&, float) override {
    ++operations;
    ++retained_pushes;
    return this;
  }
  flex::PaintHandle push_circle(float, float, float, const flex::Paint&,
                                const flex::Paint&, float,
                                const flex::Transform&, float) override {
    ++operations;
    return this;
  }
  flex::PaintHandle push_ellipse(float, float, float, float, const flex::Paint&,
                                 const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    ++operations;
    return this;
  }
  flex::PaintHandle push_polygon(int, float, const flex::Paint&,
                                 const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    ++operations;
    return this;
  }
  flex::PaintHandle push_star(int, float, float, const flex::Paint&,
                              const flex::Paint&, float,
                              const flex::Transform&, float) override {
    ++operations;
    return this;
  }
  flex::PaintHandle push_path(const std::string&, const flex::Paint&,
                              const flex::Paint&, float,
                              const flex::Transform&, float) override {
    ++operations;
    return this;
  }
  void update_transform(flex::PaintHandle, const flex::Transform&) override {
    ++operations;
  }
};

class PreflightWidget final : public flexUI::Widget {
 public:
  bool append_unsupported_blur = false;

  const char* type_name() const override { return "PreflightWidget"; }
  bool paints_host_box() const override { return true; }
  void emit_render_commands(const flexUI::Element&,
                            flexUI::RenderCommandList& commands) override {
    commands.draw_rect(0, 0, 20, 16, 0,
                       flex::Paint::solid(flex::Color{0.2f, 0.4f, 0.6f, 1.0f}),
                       flex::Paint::none(), 0);
    if (append_unsupported_blur) {
      // Bypass helper filtering just as a custom command producer can.
      commands.append(std::vector<flexUI::RenderCommand>{
          flexUI::SetBlurCommand{flex::BlurFilter{2.0f}}});
    }
  }
};

class ThrowingDirtyParent final : public flexUI::Element {
 public:
  bool reject_layout = false;
  int rejected_propagations = 0;

  void mark_dirty(flex::DirtyFlags flags) override {
    if (reject_layout && flex::has_flag(flags, flex::DirtyFlags::Layout)) {
      ++rejected_propagations;
      throw std::runtime_error("test parent dirty propagation failure");
    }
    flex::Group::mark_dirty(flags);
  }
};

void configure_root(flexUI::Element& root, PreflightWidget& widget) {
  root.set_layout_bounds(0, 0, 20, 16);
  root.widget = &widget;
  root.clear_dirty();
  root.mark_dirty(flex::DirtyFlags::Visual);
}

flexUI::RenderFrame make_frame(flexUI::Element& root) {
  flexUI::RenderFrame frame;
  frame.root = &root;
  frame.viewport = {100, 80, 1};
  return frame;
}

} // namespace

suite("RenderManager complete-frame preflight") {
  it("rejects a late widget command before frame start and restores paint dirtiness") {
    PreflightWidget widget;
    flexUI::Element root;
    configure_root(root, widget);
    widget.append_unsupported_blur = true;
    PreflightRenderer backend;
    flexUI::Renderer renderer(&backend);
    flexUI::RenderManager manager(&renderer);
    const auto frame = make_frame(root);

    bool rejected = false;
    try {
      manager.render_frame(frame);
    } catch (const flexUI::RenderValidationException& error) {
      rejected = true;
      check(error.diagnostic().code ==
            flexUI::RenderValidationError::UnsupportedOperation);
    }
    check(rejected);
    check_equal(backend.operations, 0);
    check_equal(backend.surface_queries, 0);
    check(root.is_dirty(flex::DirtyFlags::Visual));

    widget.append_unsupported_blur = false;
    manager.render_frame(frame);
    check_equal(backend.begins, 1);
    check_equal(backend.clears, 1);
    check_equal(backend.rectangles, 1);
    check_equal(backend.ends, 1);
    check(!root.is_dirty(flex::DirtyFlags::Visual));
  }

  it("does not consume paint dirtiness when only building frame commands") {
    PreflightWidget widget;
    flexUI::Element root;
    configure_root(root, widget);
    PreflightRenderer backend;
    flexUI::Renderer renderer(&backend);
    flexUI::RenderManager manager(&renderer);

    const auto commands =
        manager.build_frame_commands(make_frame(root), backend.capabilities());
    check(!commands.commands().empty());
    check(root.is_dirty(flex::DirtyFlags::Visual));
    check_equal(backend.operations, 0);
  }

  it("preserves a new paint request raised during successful replay") {
    PreflightWidget widget;
    flexUI::Element root;
    configure_root(root, widget);
    PreflightRenderer backend;
    backend.on_draw = [&root] { root.mark_dirty(flex::DirtyFlags::Visual); };
    flexUI::Renderer renderer(&backend);
    flexUI::RenderManager manager(&renderer);

    manager.render_frame(make_frame(root));
    check_equal(backend.rectangles, 1);
    check_equal(backend.ends, 1);
    check(root.is_dirty(flex::DirtyFlags::Visual));
  }

  it("restores every flag and exposes only the appropriate propagation error") {
    PreflightWidget widget;
    ThrowingDirtyParent root;
    flexUI::Element first_child;
    flexUI::Element second_child;
    configure_root(root, widget);
    configure_root(first_child, widget);
    configure_root(second_child, widget);
    root.append(&first_child);
    root.append(&second_child);
    first_child.mark_dirty(flex::DirtyFlags::Layout | flex::DirtyFlags::Visual);
    second_child.mark_dirty(flex::DirtyFlags::Layout | flex::DirtyFlags::Visual);
    PreflightRenderer backend;
    flexUI::Renderer renderer(&backend);
    flexUI::RenderManager manager(&renderer);
    const auto frame = make_frame(root);

    root.reject_layout = true;
    bool parent_error = false;
    try {
      (void)manager.build_frame_commands(frame, backend.capabilities());
    } catch (const std::runtime_error& error) {
      parent_error = std::string(error.what()) ==
                     "test parent dirty propagation failure";
    }
    root.reject_layout = false;
    check(parent_error);
    check_equal(root.rejected_propagations, 2);
    check(root.is_dirty(flex::DirtyFlags::Visual));
    check(first_child.is_dirty(flex::DirtyFlags::Visual));
    check(second_child.is_dirty(flex::DirtyFlags::Visual));

    // While a validation error is already unwinding, parent propagation must
    // not mask that error, and the other nodes still need their flags back.
    widget.append_unsupported_blur = true;
    root.reject_layout = true;
    bool validation_error = false;
    try {
      manager.render_frame(frame);
    } catch (const flexUI::RenderValidationException& error) {
      validation_error = error.diagnostic().code ==
                         flexUI::RenderValidationError::UnsupportedOperation;
    }
    root.reject_layout = false;
    check(validation_error);
    check_equal(root.rejected_propagations, 4);
    check(root.is_dirty(flex::DirtyFlags::Visual));
    check(first_child.is_dirty(flex::DirtyFlags::Visual));
    check(second_child.is_dirty(flex::DirtyFlags::Visual));
    check_equal(backend.operations, 0);
  }

  it("restores paint dirtiness when the backend fails after accepted submission") {
    PreflightWidget widget;
    flexUI::Element root;
    configure_root(root, widget);
    PreflightRenderer backend;
    backend.fail_draw = true;
    flexUI::Renderer renderer(&backend);
    flexUI::RenderManager manager(&renderer);

    bool failed = false;
    try {
      manager.render_frame(make_frame(root));
    } catch (const std::runtime_error&) {
      failed = true;
    }
    check(failed);
    check_equal(backend.begins, 1);
    check_equal(backend.clears, 1);
    check_equal(backend.rectangles, 1);
    check_equal(backend.ends, 0);
    check(root.is_dirty(flex::DirtyFlags::Visual));
  }

  it("skips expired elements while restoring flags after a backend exception") {
    PreflightWidget widget;
    auto root = std::make_unique<flexUI::Element>();
    configure_root(*root, widget);
    const auto lifetime = root->lifetime_token();
    const auto frame = make_frame(*root);
    PreflightRenderer backend;
    backend.on_draw = [&root] { root.reset(); };
    backend.fail_draw = true;
    flexUI::Renderer renderer(&backend);
    flexUI::RenderManager manager(&renderer);

    bool failed = false;
    try {
      manager.render_frame(frame);
    } catch (const std::runtime_error&) {
      failed = true;
    }
    check(failed);
    check(lifetime.expired());
    check_equal(backend.rectangles, 1);
  }

  it("rejects before clearing an existing retained cache for surface recreation") {
    PreflightWidget widget;
    flexUI::Element root;
    configure_root(root, widget);
    PreflightRenderer backend;
    backend.caps.retained_mode = true;
    flexUI::Renderer renderer(&backend);
    flexUI::RenderManager manager(&renderer);
    const auto frame = make_frame(root);

    manager.render_frame(frame);
    check_equal(backend.retained_pushes, 1);
    const int operations_before = backend.operations;
    const int queries_before = backend.surface_queries;
    const int removals_before = backend.removals;
    root.mark_dirty(flex::DirtyFlags::Visual);
    widget.append_unsupported_blur = true;
    backend.recreate_surface = true;
    bool rejected = false;
    try {
      manager.render_frame(frame);
    } catch (const flexUI::RenderValidationException&) {
      rejected = true;
    }
    check(rejected);
    check_equal(backend.operations, operations_before);
    check_equal(backend.surface_queries, queries_before);
    check_equal(backend.removals, removals_before);
    check(root.is_dirty(flex::DirtyFlags::Visual));

    widget.append_unsupported_blur = false;
    manager.render_frame(frame);
    check_equal(backend.removals, removals_before + 1);
    check_equal(backend.retained_pushes, 2);
    check(!root.is_dirty(flex::DirtyFlags::Visual));
  }
}
