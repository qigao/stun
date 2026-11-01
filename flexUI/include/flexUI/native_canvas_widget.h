#pragma once

#include <flexUI/event.h>
#include <flexUI/widget.h>

#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace flexUI {

/// Lightweight adapter for linked, trusted C++ canvas-like UI extensions.
///
/// This is NOT the dynamic PluginHost Widget ABI and does not accept external
/// GPU textures. All drawing goes through the host RenderCommandList and all
/// input goes through the one Box EventDispatcher on the UI owner thread.
/// Plugin code must not retain Element or RenderCommandList references beyond
/// the callback; widget ownership remains with Box.
class NativeCanvasWidget final : public Widget {
public:
  struct Callbacks {
    using Paint = std::function<void(const Element &, RenderCommandList &)>;
    using Input = std::function<bool(NativeCanvasWidget &, const Event &, Element &)>;
    using Frame = std::function<void(float, Element &)>;
    using Measure = std::function<bool(const Element &, float, float, float &, float &)>;
    // Linked-only semantic parts remain in the one Box-owned Element tree.
    using Mount = std::function<void(NativeCanvasWidget &, Element &)>;
    using Sync = std::function<void(NativeCanvasWidget &, Element &)>;
    using PartPaint = std::function<bool(std::string_view)>;

    Paint paint;
    Input input;
    Frame frame;
    Measure measure;
    Mount mount;
    Sync sync_semantics;
    PartPaint owns_part_paint;
  };

  explicit NativeCanvasWidget(Callbacks callbacks) : callbacks_(std::move(callbacks)) {
    if (!callbacks_.paint) {
      throw std::invalid_argument("NativeCanvasWidget requires a paint callback");
    }
  }

  const char *type_name() const override { return "NativeCanvasWidget"; }

  void emit_render_commands(const Element &elem, RenderCommandList &commands) override {
    callbacks_.paint(elem, commands);
    dirty_ = false;
  }

  bool handle_event(const Event &event, Element &elem) override {
    const bool consumed = callbacks_.input ? callbacks_.input(*this, event, elem) : false;
    if (callbacks_.sync_semantics) callbacks_.sync_semantics(*this, elem);
    // Captured pointers must never remain captured after release or focus loss.
    if (event.type == EventType::MouseUp || event.type == EventType::FocusOut) {
      pointer_capture_ = false;
    }
    return consumed;
  }

  bool wants_mouse_capture() const override { return pointer_capture_; }
  bool paints_part_box(std::string_view part_name) const override {
    return callbacks_.owns_part_paint && callbacks_.owns_part_paint(part_name);
  }

  /// Create one stable, Box-owned semantic part during the mount callback.
  Element *declare_semantic_part(const char *tag, const char *name) {
    return create_part(tag, name);
  }

  /// Call during an input callback; EventDispatcher reconciles capture after
  /// the callback. The host, not the extension, owns event routing.
  void request_pointer_capture() noexcept { pointer_capture_ = true; }
  void release_pointer_capture() noexcept { pointer_capture_ = false; }

  bool needs_frame_update(const Element &) const override {
    return static_cast<bool>(callbacks_.frame);
  }

  void update(float delta_ms, Element &elem) override {
    if (callbacks_.frame) callbacks_.frame(delta_ms, elem);
  }

  bool measure_intrinsic_size(const Element &elem, float available_width,
                              float available_height, float &out_width,
                              float &out_height) const override {
    return callbacks_.measure && callbacks_.measure(
        elem, available_width, available_height, out_width, out_height);
  }

protected:
  void build_semantic_tree() override {
    if (callbacks_.mount && host_element()) callbacks_.mount(*this, *host_element());
  }
  void sync_host_semantics() override {
    if (callbacks_.sync_semantics && host_element()) {
      callbacks_.sync_semantics(*this, *host_element());
    }
  }

public:
  void sync_host_semantics_for_layout(Element &element) override {
    if (callbacks_.sync_semantics) callbacks_.sync_semantics(*this, element);
  }

private:
  Callbacks callbacks_;
  bool pointer_capture_ = false;
};

} // namespace flexUI
