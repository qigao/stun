#include <flexUI/render_command.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace flexUI {

using flex::operator*;

namespace {

template <typename... T>
struct Overloaded : T... {
  using T::operator()...;
};

template <typename... T>
Overloaded(T...) -> Overloaded<T...>;

bool has_visible_solid_alpha(const Paint& paint) {
  return paint.type != Paint::Type::Solid || paint.color.a > 0.0f;
}

bool is_visible_paint(const Paint& paint) {
  return paint.type != Paint::Type::None && has_visible_solid_alpha(paint);
}

bool is_visible_stroke(const Paint& paint, float width) {
  return width > 0.0f && is_visible_paint(paint);
}

bool nearly_zero(float value) {
  return std::fabs(value) <= 0.001f;
}

bool nearly_equal(float lhs, float rhs) {
  return std::fabs(lhs - rhs) <= 0.001f;
}

bool transforms_equal(const Transform& lhs, const Transform& rhs) {
  for (int i = 0; i < 9; ++i) {
    if (lhs.data[i] != rhs.data[i]) {
      return false;
    }
  }
  return true;
}

void hash_bytes(uint64_t& hash, const void* data, size_t size) {
  const auto* bytes = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < size; ++i) {
    hash ^= static_cast<uint64_t>(bytes[i]);
    hash *= 1099511628211ull;
  }
}

void hash_u64(uint64_t& hash, uint64_t value) {
  hash_bytes(hash, &value, sizeof(value));
}

void hash_bool(uint64_t& hash, bool value) {
  hash_u64(hash, value ? 1ull : 0ull);
}

void hash_float(uint64_t& hash, float value) {
  if (value == 0.0f) {
    value = 0.0f;
  }
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  hash_u64(hash, bits);
}

void hash_string(uint64_t& hash, const std::string& value) {
  hash_u64(hash, static_cast<uint64_t>(value.size()));
  if (!value.empty()) {
    hash_bytes(hash, value.data(), value.size());
  }
}

void hash_color(uint64_t& hash, const Color& color) {
  hash_float(hash, color.r);
  hash_float(hash, color.g);
  hash_float(hash, color.b);
  hash_float(hash, color.a);
}

void hash_transform(uint64_t& hash, const Transform& transform) {
  for (float value : transform.data) {
    hash_float(hash, value);
  }
}

void hash_color_stop(uint64_t& hash, const flex::ColorStop& stop) {
  hash_float(hash, stop.offset);
  hash_color(hash, stop.color);
}

void hash_linear_gradient(uint64_t& hash, const flex::LinearGradient& gradient) {
  hash_float(hash, gradient.x1);
  hash_float(hash, gradient.y1);
  hash_float(hash, gradient.x2);
  hash_float(hash, gradient.y2);
  hash_u64(hash, static_cast<uint64_t>(gradient.stops.size()));
  for (const auto& stop : gradient.stops) {
    hash_color_stop(hash, stop);
  }
}

void hash_radial_gradient(uint64_t& hash, const flex::RadialGradient& gradient) {
  hash_float(hash, gradient.cx);
  hash_float(hash, gradient.cy);
  hash_float(hash, gradient.radius);
  hash_float(hash, gradient.fx);
  hash_float(hash, gradient.fy);
  hash_u64(hash, static_cast<uint64_t>(gradient.stops.size()));
  for (const auto& stop : gradient.stops) {
    hash_color_stop(hash, stop);
  }
}

void hash_paint(uint64_t& hash, const Paint& paint) {
  hash_u64(hash, static_cast<uint64_t>(paint.type));
  hash_color(hash, paint.color);
  hash_linear_gradient(hash, paint.linear);
  hash_radial_gradient(hash, paint.radial);
}

void hash_shadow(uint64_t& hash, const flex::Shadow& shadow) {
  hash_float(hash, shadow.offset_x);
  hash_float(hash, shadow.offset_y);
  hash_float(hash, shadow.blur);
  hash_float(hash, shadow.spread);
  hash_color(hash, shadow.color);
  hash_bool(hash, shadow.inset);
}

void hash_blur(uint64_t& hash, const flex::BlurFilter& blur) {
  hash_float(hash, blur.radius);
}

Transform transform_with_offset(const Transform& transform, float x, float y) {
  Transform out = transform;
  if (flex::is_translation_only(transform)) {
    out.data[2] = flex::tx(transform) + x;
    out.data[5] = flex::ty(transform) + y;
    return out;
  }
  const flex::Vec2 world_pos = transform * flex::Vec2{x, y};
  out.data[2] = world_pos.x;
  out.data[5] = world_pos.y;
  return out;
}

void apply_translate(Transform& transform, float x, float y) {
  transform.data[2] += x;
  transform.data[5] += y;
}

void apply_rotate(Transform& transform, float degrees) {
  if (degrees == 0.0f) {
    return;
  }
  const float radians = degrees * (3.14159265f / 180.0f);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  const float m00 = transform.data[0];
  const float m01 = transform.data[1];
  const float m10 = transform.data[3];
  const float m11 = transform.data[4];
  transform.data[0] = m00 * c + m01 * s;
  transform.data[1] = -m00 * s + m01 * c;
  transform.data[3] = m10 * c + m11 * s;
  transform.data[4] = -m10 * s + m11 * c;
}

void apply_scale(Transform& transform, float x, float y) {
  transform.data[0] *= x;
  transform.data[1] *= y;
  transform.data[3] *= x;
  transform.data[4] *= y;
}

std::string line_path(float x1, float y1, float x2, float y2) {
  return "M " + std::to_string(x1) + " " + std::to_string(y1) + " L " +
         std::to_string(x2) + " " + std::to_string(y2);
}

struct RetainedReplayState {
  struct SavedState {
    Transform transform{};
    float alpha = 1.0f;
    uint64_t state_signature = 0;
  };

  Transform transform{};
  float alpha = 1.0f;
  uint64_t state_signature = 0;
  std::vector<SavedState> stack;
};

struct RetainedPlanEntry {
  uint64_t content_signature = 0;
  Transform transform{};
  bool cacheable = false;
};

void hash_state_signature(uint64_t& hash,
                          const RetainedReplayState& state) {
  hash_u64(hash, state.state_signature);
  hash_float(hash, state.alpha);
}

uint64_t retained_content_seed(size_t command_index,
                               const RetainedReplayState& state) {
  uint64_t hash = 14695981039346656037ull;
  hash_u64(hash, static_cast<uint64_t>(command_index));
  hash_state_signature(hash, state);
  return hash;
}

void update_state_signature(uint64_t& state_signature, uint64_t value) {
  hash_u64(state_signature, value);
}

void apply_state_for_plan(const RenderCommand& command,
                          RetainedReplayState& state) {
  std::visit(
      Overloaded{
          [&](const BeginFrameCommand&) {
            state.transform = Transform{};
            state.alpha = 1.0f;
            state.state_signature = 0;
            state.stack.clear();
          },
          [&](const EndFrameCommand&) {},
          [&](const ClearCommand&) {},
          [&](const SaveCommand&) {
            state.stack.push_back(
                {state.transform, state.alpha, state.state_signature});
          },
          [&](const RestoreCommand&) {
            if (state.stack.empty()) {
              return;
            }
            const auto saved = state.stack.back();
            state.stack.pop_back();
            state.transform = saved.transform;
            state.alpha = saved.alpha;
            state.state_signature = saved.state_signature;
          },
          [&](const SetTransformCommand& cmd) { state.transform = cmd.transform; },
          [&](const TranslateCommand& cmd) {
            apply_translate(state.transform, cmd.x, cmd.y);
          },
          [&](const RotateCommand& cmd) {
            apply_rotate(state.transform, cmd.degrees);
          },
          [&](const ScaleCommand& cmd) {
            apply_scale(state.transform, cmd.x, cmd.y);
          },
          [&](const SetGlobalAlphaCommand& cmd) {
            const float base_alpha =
                state.stack.empty() ? 1.0f : state.stack.back().alpha;
            state.alpha = base_alpha * cmd.alpha;
          },
          [&](const SetShadowCommand& cmd) {
            update_state_signature(state.state_signature, 1);
            hash_shadow(state.state_signature, cmd.shadow);
          },
          [&](const ClearShadowCommand&) {
            update_state_signature(state.state_signature, 2);
          },
          [&](const SetBlurCommand& cmd) {
            update_state_signature(state.state_signature, 3);
            hash_blur(state.state_signature, cmd.blur);
          },
          [&](const ClearBlurCommand&) {
            update_state_signature(state.state_signature, 4);
          },
          [&](const ClipRectCommand& cmd) {
            update_state_signature(state.state_signature, 5);
            hash_float(state.state_signature, cmd.x);
            hash_float(state.state_signature, cmd.y);
            hash_float(state.state_signature, cmd.width);
            hash_float(state.state_signature, cmd.height);
            hash_transform(state.state_signature, state.transform);
          },
          [&](const ResetClipCommand&) {
            update_state_signature(state.state_signature, 6);
          },
          [&](const auto&) {},
      },
      command);
}

std::vector<RetainedPlanEntry>
build_retained_plan(const std::vector<RenderCommand>& commands) {
  std::vector<RetainedPlanEntry> plan(commands.size());
  RetainedReplayState state;
  for (size_t i = 0; i < commands.size(); ++i) {
    const auto& command = commands[i];
    std::visit(
        Overloaded{
            [&](const FillPathCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_string(hash, cmd.d);
              hash_paint(hash, cmd.paint);
              plan[i] = {hash, state.transform, true};
            },
            [&](const StrokePathCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_string(hash, cmd.d);
              hash_paint(hash, cmd.paint);
              hash_float(hash, cmd.width);
              plan[i] = {hash, state.transform, true};
            },
            [&](const DrawRectCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
              hash_float(hash, cmd.radius);
              hash_paint(hash, cmd.fill);
              hash_paint(hash, cmd.stroke);
              hash_float(hash, cmd.stroke_width);
              plan[i] = {hash, state.transform, true};
            },
            [&](const DrawLineCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_float(hash, cmd.x1);
              hash_float(hash, cmd.y1);
              hash_float(hash, cmd.x2);
              hash_float(hash, cmd.y2);
              hash_paint(hash, cmd.paint);
              hash_float(hash, cmd.width);
              plan[i] = {hash, state.transform, true};
            },
            [&](const DrawCircleCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_float(hash, cmd.cx);
              hash_float(hash, cmd.cy);
              hash_float(hash, cmd.radius);
              hash_paint(hash, cmd.fill);
              hash_paint(hash, cmd.stroke);
              hash_float(hash, cmd.stroke_width);
              plan[i] = {hash, state.transform, true};
            },
            [&](const DrawEllipseCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_float(hash, cmd.cx);
              hash_float(hash, cmd.cy);
              hash_float(hash, cmd.radius_x);
              hash_float(hash, cmd.radius_y);
              hash_paint(hash, cmd.fill);
              hash_paint(hash, cmd.stroke);
              hash_float(hash, cmd.stroke_width);
              plan[i] = {hash, state.transform, true};
            },
            [&](const DrawTextCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_string(hash, cmd.text);
              hash_string(hash, cmd.font);
              hash_float(hash, cmd.size);
              hash_bool(hash, cmd.bold);
              hash_color(hash, cmd.color);
              plan[i] = {hash, transform_with_offset(state.transform, cmd.x, cmd.y), true};
            },
            [&](const DrawImageCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_string(hash, cmd.src);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
              plan[i] = {hash, transform_with_offset(state.transform, cmd.x, cmd.y), true};
            },
            [&](const DrawSvgCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_string(hash, cmd.src);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
              plan[i] = {hash, transform_with_offset(state.transform, cmd.x, cmd.y), true};
            },
            [&](const DrawSvgDataCommand& cmd) {
              auto hash = retained_content_seed(i, state);
              hash_string(hash, cmd.data);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
              plan[i] = {hash, transform_with_offset(state.transform, cmd.x, cmd.y), true};
            },
            [&](const auto&) {},
        },
        command);
    apply_state_for_plan(command, state);
  }
  return plan;
}

} // namespace

RenderCommandList::RenderCommandList(
    const flex::RendererCapabilities& capabilities)
    : capabilities_(capabilities) {}

RenderCommandList::RenderCommandList(
    const flex::RendererCapabilities& capabilities,
    const Transform& transform_prefix)
    : capabilities_(capabilities),
      base_transform_prefix_(transform_prefix),
      transform_prefix_(transform_prefix) {}

void RenderCommandList::add(RenderCommand command) {
  commands_.push_back(std::move(command));
}

void RenderCommandList::begin_frame(const RenderViewport& viewport) {
  add(BeginFrameCommand{viewport});
}

void RenderCommandList::end_frame() {
  add(EndFrameCommand{});
}

void RenderCommandList::clear(const Color& color) {
  add(ClearCommand{color});
}

void RenderCommandList::save() {
  add(SaveCommand{});
}

void RenderCommandList::restore() {
  add(RestoreCommand{});
}

void RenderCommandList::set_transform(const Transform& transform) {
  add(SetTransformCommand{transform_prefix_ * transform});
}

void RenderCommandList::translate(float x, float y) {
  if (nearly_zero(x) && nearly_zero(y)) {
    return;
  }
  add(TranslateCommand{x, y});
}

void RenderCommandList::rotate(float degrees) {
  if (nearly_zero(degrees)) {
    return;
  }
  add(RotateCommand{degrees});
}

void RenderCommandList::scale(float x, float y) {
  if (nearly_equal(x, 1.0f) && nearly_equal(y, 1.0f)) {
    return;
  }
  add(ScaleCommand{x, y});
}

void RenderCommandList::set_global_alpha(float alpha) {
  add(SetGlobalAlphaCommand{alpha});
}

void RenderCommandList::set_shadow(const flex::Shadow& shadow) {
  add(SetShadowCommand{shadow});
}

void RenderCommandList::clear_shadow() {
  add(ClearShadowCommand{});
}

void RenderCommandList::set_blur(const flex::BlurFilter& blur) {
  add(SetBlurCommand{blur});
}

void RenderCommandList::clear_blur() {
  add(ClearBlurCommand{});
}

void RenderCommandList::clip_rect(float x, float y, float width, float height) {
  add(ClipRectCommand{x, y, width, height});
}

void RenderCommandList::reset_clip() {
  add(ResetClipCommand{});
}

void RenderCommandList::push_transform_prefix(const Transform& transform) {
  transform_prefix_stack_.push_back(transform_prefix_);
  transform_prefix_ = transform_prefix_ * transform;
}

void RenderCommandList::pop_transform_prefix() {
  transform_prefix_ = transform_prefix_stack_.back();
  transform_prefix_stack_.pop_back();
}

void RenderCommandList::fill_path(std::string d, const Paint& paint) {
  if (d.empty() || !is_visible_paint(paint)) {
    return;
  }
  add(FillPathCommand{std::move(d), paint});
}

void RenderCommandList::stroke_path(std::string d, const Paint& paint,
                                    float width) {
  if (d.empty() || !is_visible_stroke(paint, width)) {
    return;
  }
  add(StrokePathCommand{std::move(d), paint, width});
}

void RenderCommandList::draw_rect(float x, float y, float width, float height,
                                  float radius, const Paint& fill,
                                  const Paint& stroke, float stroke_width) {
  if (width <= 0.0f || height <= 0.0f ||
      (!is_visible_paint(fill) && !is_visible_stroke(stroke, stroke_width))) {
    return;
  }
  add(DrawRectCommand{x, y, width, height, radius, fill, stroke, stroke_width});
}

void RenderCommandList::draw_line(float x1, float y1, float x2, float y2,
                                  const Paint& paint, float width) {
  if (!is_visible_stroke(paint, width) ||
      (std::fabs(x2 - x1) <= 0.001f && std::fabs(y2 - y1) <= 0.001f)) {
    return;
  }
  add(DrawLineCommand{x1, y1, x2, y2, paint, width});
}

void RenderCommandList::draw_circle(float cx, float cy, float radius,
                                    const Paint& fill, const Paint& stroke,
                                    float stroke_width) {
  if (radius <= 0.0f ||
      (!is_visible_paint(fill) && !is_visible_stroke(stroke, stroke_width))) {
    return;
  }
  add(DrawCircleCommand{cx, cy, radius, fill, stroke, stroke_width});
}

void RenderCommandList::draw_ellipse(float cx, float cy, float radius_x,
                                     float radius_y, const Paint& fill,
                                     const Paint& stroke,
                                     float stroke_width) {
  if (radius_x <= 0.0f || radius_y <= 0.0f ||
      (!is_visible_paint(fill) && !is_visible_stroke(stroke, stroke_width))) {
    return;
  }
  add(DrawEllipseCommand{cx, cy, radius_x, radius_y, fill, stroke,
                         stroke_width});
}

void RenderCommandList::draw_text(std::string text, float x, float y,
                                  std::string font, float size, bool bold,
                                  const Color& color) {
  if (text.empty() || size <= 0.0f || color.a <= 0.0f) {
    return;
  }
  add(DrawTextCommand{std::move(text), x, y, std::move(font), size, bold,
                      color});
}

void RenderCommandList::draw_image(std::string src, float x, float y,
                                   float width, float height) {
  if (src.empty() || width <= 0.0f || height <= 0.0f) {
    return;
  }
  add(DrawImageCommand{std::move(src), x, y, width, height});
}

void RenderCommandList::draw_svg(std::string src, float x, float y, float width,
                                 float height) {
  if (src.empty() || width <= 0.0f || height <= 0.0f) {
    return;
  }
  add(DrawSvgCommand{std::move(src), x, y, width, height});
}

void RenderCommandList::draw_svg_data(std::string data, float x, float y,
                                      float width, float height) {
  if (data.empty() || width <= 0.0f || height <= 0.0f) {
    return;
  }
  add(DrawSvgDataCommand{std::move(data), x, y, width, height});
}

void RenderCommandList::replay(Renderer& renderer) const {
  for (const auto& command : commands_) {
    std::visit(
        Overloaded{
            [&](const BeginFrameCommand& cmd) {
              renderer.begin_frame(cmd.viewport.width, cmd.viewport.height,
                                   cmd.viewport.pixel_ratio);
            },
            [&](const EndFrameCommand&) { renderer.end_frame(); },
            [&](const ClearCommand& cmd) { renderer.clear(cmd.color); },
            [&](const SaveCommand&) { renderer.save(); },
            [&](const RestoreCommand&) { renderer.restore(); },
            [&](const SetTransformCommand& cmd) {
              renderer.set_transform(cmd.transform);
            },
            [&](const TranslateCommand& cmd) {
              renderer.translate(cmd.x, cmd.y);
            },
            [&](const RotateCommand& cmd) { renderer.rotate(cmd.degrees); },
            [&](const ScaleCommand& cmd) { renderer.scale(cmd.x, cmd.y); },
            [&](const SetGlobalAlphaCommand& cmd) {
              renderer.set_global_alpha(cmd.alpha);
            },
            [&](const SetShadowCommand& cmd) { renderer.set_shadow(cmd.shadow); },
            [&](const ClearShadowCommand&) { renderer.clear_shadow(); },
            [&](const SetBlurCommand& cmd) { renderer.set_blur(cmd.blur); },
            [&](const ClearBlurCommand&) { renderer.clear_blur(); },
            [&](const ClipRectCommand& cmd) {
              renderer.clip_rect(cmd.x, cmd.y, cmd.width, cmd.height);
            },
            [&](const ResetClipCommand&) { renderer.reset_clip(); },
            [&](const FillPathCommand& cmd) {
              renderer.fill_path(cmd.d, cmd.paint);
            },
            [&](const StrokePathCommand& cmd) {
              renderer.stroke_path(cmd.d, cmd.paint, cmd.width);
            },
            [&](const DrawRectCommand& cmd) {
              renderer.draw_rect(cmd.x, cmd.y, cmd.width, cmd.height,
                                 cmd.radius, cmd.fill, cmd.stroke,
                                 cmd.stroke_width);
            },
            [&](const DrawLineCommand& cmd) {
              renderer.draw_line(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.paint,
                                 cmd.width);
            },
            [&](const DrawCircleCommand& cmd) {
              renderer.draw_circle(cmd.cx, cmd.cy, cmd.radius, cmd.fill,
                                   cmd.stroke, cmd.stroke_width);
            },
            [&](const DrawEllipseCommand& cmd) {
              renderer.draw_ellipse(cmd.cx, cmd.cy, cmd.radius_x, cmd.radius_y,
                                    cmd.fill, cmd.stroke, cmd.stroke_width);
            },
            [&](const DrawTextCommand& cmd) {
              renderer.draw_text(cmd.text, cmd.x, cmd.y, cmd.font, cmd.size,
                                 cmd.bold, cmd.color);
            },
            [&](const DrawImageCommand& cmd) {
              renderer.draw_image(cmd.src, cmd.x, cmd.y, cmd.width, cmd.height);
            },
            [&](const DrawSvgCommand& cmd) {
              renderer.draw_svg(cmd.src, cmd.x, cmd.y, cmd.width, cmd.height);
            },
            [&](const DrawSvgDataCommand& cmd) {
              renderer.draw_svg_data(cmd.data, cmd.x, cmd.y, cmd.width,
                                     cmd.height);
            },
        },
        command);
  }
}

void RenderCommandCache::clear(Renderer& renderer) {
  for (const auto& entry : entries_) {
    if (entry.retained && entry.handle) {
      renderer.remove_cached(entry.handle);
    }
  }
  entries_.clear();
  pending_entries_.clear();
}

void RenderCommandList::replay_retained(Renderer& renderer,
                                        RenderCommandCache& cache) const {
  renderer.set_retained_mode(true);
  if (!renderer.supports_retained_mode()) {
    renderer.set_retained_mode(false);
    replay(renderer);
    return;
  }

  const auto plan = build_retained_plan(commands_);
  const size_t previous_size = cache.entries_.size();
  const size_t common_size = std::min(previous_size, plan.size());
  const bool stable_slots = previous_size == plan.size();
  std::vector<bool> rebuild_slots(plan.size(), false);
  size_t rebuild_from = plan.size();

  if (stable_slots) {
    for (size_t i = 0; i < plan.size(); ++i) {
      const auto& planned = plan[i];
      const auto& cached = cache.entries_[i];
      const bool same_content =
          planned.cacheable == cached.retained &&
          (!planned.cacheable ||
           planned.content_signature == cached.content_signature);
      rebuild_slots[i] = planned.cacheable && (!same_content || !cached.handle);
      if (!planned.cacheable && cached.retained && cached.handle) {
        renderer.remove_cached(cached.handle);
      }
    }
  } else {
    for (size_t i = 0; i < common_size; ++i) {
      const auto& planned = plan[i];
      const auto& cached = cache.entries_[i];
      if (planned.cacheable != cached.retained) {
        rebuild_from = i;
        break;
      }
      if (planned.cacheable &&
          planned.content_signature != cached.content_signature) {
        rebuild_from = i;
        break;
      }
    }
    if (rebuild_from == plan.size() && previous_size != plan.size()) {
      rebuild_from = common_size;
    }
    for (size_t i = rebuild_from; i < previous_size; ++i) {
      const auto& entry = cache.entries_[i];
      if (entry.retained && entry.handle) {
        renderer.remove_cached(entry.handle);
      }
    }
  }
  cache.pending_entries_.assign(plan.size(), RenderCommandCache::Entry{});
  const auto can_reuse = [&](size_t index) {
    if (stable_slots) {
      return index < cache.entries_.size() && !rebuild_slots[index] &&
             cache.entries_[index].handle;
    }
    return index < rebuild_from && index < cache.entries_.size() &&
           cache.entries_[index].handle;
  };
  const auto prepare_rebuild = [&](size_t index) {
    if (!stable_slots || index >= cache.entries_.size()) {
      return;
    }
    const auto& cached = cache.entries_[index];
    if (cached.retained && cached.handle) {
      renderer.remove_cached(cached.handle);
    }
    flex::PaintHandle anchor = nullptr;
    for (size_t next = index + 1; next < cache.entries_.size(); ++next) {
      const auto& entry = cache.entries_[next];
      if (entry.retained && entry.handle) {
        anchor = entry.handle;
        break;
      }
    }
    renderer.set_retained_insertion_anchor(anchor);
  };

  RetainedReplayState state;
  for (size_t i = 0; i < commands_.size(); ++i) {
    const auto& command = commands_[i];
    const auto& planned = plan[i];

    std::visit(
        Overloaded{
            [&](const BeginFrameCommand& cmd) {
              renderer.begin_frame(cmd.viewport.width, cmd.viewport.height,
                                   cmd.viewport.pixel_ratio);
              state.transform = Transform{};
              state.alpha = 1.0f;
              state.state_signature = 0;
              state.stack.clear();
            },
            [&](const EndFrameCommand&) { renderer.end_frame(); },
            [&](const ClearCommand& cmd) { renderer.clear(cmd.color); },
            [&](const SaveCommand&) {
              renderer.save();
              state.stack.push_back(
                  {state.transform, state.alpha, state.state_signature});
            },
            [&](const RestoreCommand&) {
              renderer.restore();
              if (state.stack.empty()) {
                return;
              }
              const auto saved = state.stack.back();
              state.stack.pop_back();
              state.transform = saved.transform;
              state.alpha = saved.alpha;
              state.state_signature = saved.state_signature;
            },
            [&](const SetTransformCommand& cmd) {
              renderer.set_transform(cmd.transform);
              state.transform = cmd.transform;
            },
            [&](const TranslateCommand& cmd) {
              renderer.translate(cmd.x, cmd.y);
              apply_translate(state.transform, cmd.x, cmd.y);
            },
            [&](const RotateCommand& cmd) {
              renderer.rotate(cmd.degrees);
              apply_rotate(state.transform, cmd.degrees);
            },
            [&](const ScaleCommand& cmd) {
              renderer.scale(cmd.x, cmd.y);
              apply_scale(state.transform, cmd.x, cmd.y);
            },
            [&](const SetGlobalAlphaCommand& cmd) {
              renderer.set_global_alpha(cmd.alpha);
              const float base_alpha =
                  state.stack.empty() ? 1.0f : state.stack.back().alpha;
              state.alpha = base_alpha * cmd.alpha;
            },
            [&](const SetShadowCommand& cmd) {
              renderer.set_shadow(cmd.shadow);
              update_state_signature(state.state_signature, 1);
              hash_shadow(state.state_signature, cmd.shadow);
            },
            [&](const ClearShadowCommand&) {
              renderer.clear_shadow();
              update_state_signature(state.state_signature, 2);
            },
            [&](const SetBlurCommand& cmd) {
              renderer.set_blur(cmd.blur);
              update_state_signature(state.state_signature, 3);
              hash_blur(state.state_signature, cmd.blur);
            },
            [&](const ClearBlurCommand&) {
              renderer.clear_blur();
              update_state_signature(state.state_signature, 4);
            },
            [&](const ClipRectCommand& cmd) {
              renderer.clip_rect(cmd.x, cmd.y, cmd.width, cmd.height);
              update_state_signature(state.state_signature, 5);
              hash_float(state.state_signature, cmd.x);
              hash_float(state.state_signature, cmd.y);
              hash_float(state.state_signature, cmd.width);
              hash_float(state.state_signature, cmd.height);
              hash_transform(state.state_signature, state.transform);
            },
            [&](const ResetClipCommand&) {
              renderer.reset_clip();
              update_state_signature(state.state_signature, 6);
            },
            [&](const FillPathCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_path(cmd.d, cmd.paint,
                                                  Paint::none(), 0.0f,
                                                  planned.transform,
                                                  state.alpha);
              }
            },
            [&](const StrokePathCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_path(cmd.d, Paint::none(),
                                                  cmd.paint, cmd.width,
                                                  planned.transform,
                                                  state.alpha);
              }
            },
            [&](const DrawRectCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_rect(
                    cmd.x, cmd.y, cmd.width, cmd.height, cmd.radius, cmd.fill,
                    cmd.stroke, cmd.stroke_width, planned.transform,
                    state.alpha);
              }
            },
            [&](const DrawLineCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_path(
                    line_path(cmd.x1, cmd.y1, cmd.x2, cmd.y2), Paint::none(),
                    cmd.paint, cmd.width, planned.transform, state.alpha);
              }
            },
            [&](const DrawCircleCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_circle(
                    cmd.cx, cmd.cy, cmd.radius, cmd.fill, cmd.stroke,
                    cmd.stroke_width, planned.transform, state.alpha);
              }
            },
            [&](const DrawEllipseCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_ellipse(
                    cmd.cx, cmd.cy, cmd.radius_x, cmd.radius_y, cmd.fill,
                    cmd.stroke, cmd.stroke_width, planned.transform,
                    state.alpha);
              }
            },
            [&](const DrawTextCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_text(
                    cmd.text, cmd.font, cmd.size, cmd.bold, cmd.color,
                    planned.transform, state.alpha);
              }
            },
            [&](const DrawImageCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_image(
                    cmd.src, cmd.width, cmd.height, planned.transform,
                    state.alpha);
              }
            },
            [&](const DrawSvgCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_svg(
                    cmd.src, cmd.width, cmd.height, planned.transform,
                    state.alpha);
              }
            },
            [&](const DrawSvgDataCommand& cmd) {
              auto& entry = cache.pending_entries_[i];
              entry.content_signature = planned.content_signature;
              entry.transform = planned.transform;
              entry.retained = true;
              if (can_reuse(i)) {
                entry.handle = cache.entries_[i].handle;
                if (!transforms_equal(cache.entries_[i].transform, planned.transform)) {
                  renderer.update_transform(entry.handle, planned.transform);
                }
              } else {
                prepare_rebuild(i);
                entry.handle = renderer.push_svg_data(
                    cmd.data, cmd.width, cmd.height, planned.transform,
                    state.alpha);
              }
            },
        },
        command);
  }

  cache.entries_.swap(cache.pending_entries_);
  cache.pending_entries_.clear();
  renderer.set_retained_mode(false);
}

uint64_t RenderCommandList::signature() const {
  uint64_t hash = 14695981039346656037ull;
  hash_u64(hash, static_cast<uint64_t>(commands_.size()));
  for (const auto& command : commands_) {
    hash_u64(hash, static_cast<uint64_t>(command.index()));
    std::visit(
        Overloaded{
            [&](const BeginFrameCommand& cmd) {
              hash_float(hash, cmd.viewport.width);
              hash_float(hash, cmd.viewport.height);
              hash_float(hash, cmd.viewport.pixel_ratio);
            },
            [&](const EndFrameCommand&) {},
            [&](const ClearCommand& cmd) { hash_color(hash, cmd.color); },
            [&](const SaveCommand&) {},
            [&](const RestoreCommand&) {},
            [&](const SetTransformCommand& cmd) {
              hash_transform(hash, cmd.transform);
            },
            [&](const TranslateCommand& cmd) {
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
            },
            [&](const RotateCommand& cmd) { hash_float(hash, cmd.degrees); },
            [&](const ScaleCommand& cmd) {
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
            },
            [&](const SetGlobalAlphaCommand& cmd) {
              hash_float(hash, cmd.alpha);
            },
            [&](const SetShadowCommand& cmd) { hash_shadow(hash, cmd.shadow); },
            [&](const ClearShadowCommand&) {},
            [&](const SetBlurCommand& cmd) { hash_blur(hash, cmd.blur); },
            [&](const ClearBlurCommand&) {},
            [&](const ClipRectCommand& cmd) {
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
            },
            [&](const ResetClipCommand&) {},
            [&](const FillPathCommand& cmd) {
              hash_string(hash, cmd.d);
              hash_paint(hash, cmd.paint);
            },
            [&](const StrokePathCommand& cmd) {
              hash_string(hash, cmd.d);
              hash_paint(hash, cmd.paint);
              hash_float(hash, cmd.width);
            },
            [&](const DrawRectCommand& cmd) {
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
              hash_float(hash, cmd.radius);
              hash_paint(hash, cmd.fill);
              hash_paint(hash, cmd.stroke);
              hash_float(hash, cmd.stroke_width);
            },
            [&](const DrawLineCommand& cmd) {
              hash_float(hash, cmd.x1);
              hash_float(hash, cmd.y1);
              hash_float(hash, cmd.x2);
              hash_float(hash, cmd.y2);
              hash_paint(hash, cmd.paint);
              hash_float(hash, cmd.width);
            },
            [&](const DrawCircleCommand& cmd) {
              hash_float(hash, cmd.cx);
              hash_float(hash, cmd.cy);
              hash_float(hash, cmd.radius);
              hash_paint(hash, cmd.fill);
              hash_paint(hash, cmd.stroke);
              hash_float(hash, cmd.stroke_width);
            },
            [&](const DrawEllipseCommand& cmd) {
              hash_float(hash, cmd.cx);
              hash_float(hash, cmd.cy);
              hash_float(hash, cmd.radius_x);
              hash_float(hash, cmd.radius_y);
              hash_paint(hash, cmd.fill);
              hash_paint(hash, cmd.stroke);
              hash_float(hash, cmd.stroke_width);
            },
            [&](const DrawTextCommand& cmd) {
              hash_string(hash, cmd.text);
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
              hash_string(hash, cmd.font);
              hash_float(hash, cmd.size);
              hash_bool(hash, cmd.bold);
              hash_color(hash, cmd.color);
            },
            [&](const DrawImageCommand& cmd) {
              hash_string(hash, cmd.src);
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
            },
            [&](const DrawSvgCommand& cmd) {
              hash_string(hash, cmd.src);
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
            },
            [&](const DrawSvgDataCommand& cmd) {
              hash_string(hash, cmd.data);
              hash_float(hash, cmd.x);
              hash_float(hash, cmd.y);
              hash_float(hash, cmd.width);
              hash_float(hash, cmd.height);
            },
        },
        command);
  }
  return hash;
}

void RenderCommandList::reset() {
  commands_.clear();
  transform_prefix_stack_.clear();
  transform_prefix_ = base_transform_prefix_;
}

void RenderCommandList::reserve(size_t capacity) {
  commands_.reserve(capacity);
}

void RenderCommandList::append(const RenderCommandList& other) {
  append(other.commands());
}

void RenderCommandList::append(const std::vector<RenderCommand>& commands) {
  commands_.reserve(commands_.size() + commands.size());
  commands_.insert(commands_.end(), commands.begin(), commands.end());
}

void RenderCommandList::append_with_transform_prefix(
    const std::vector<RenderCommand>& commands) {
  commands_.reserve(commands_.size() + commands.size());
  for (const auto& command : commands) {
    if (const auto* transform = std::get_if<SetTransformCommand>(&command)) {
      set_transform(transform->transform);
    } else {
      add(command);
    }
  }
}

} // namespace flexUI
