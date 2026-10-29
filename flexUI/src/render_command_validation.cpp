#include <flexUI/render_command.h>
#include <flexUI/renderer.h>
#include <flex/core/render_value_validation.h>

#include <cmath>
#include <initializer_list>

namespace flexUI {
namespace {

template <typename... T> struct Overloaded : T... { using T::operator()...; };
template <typename... T> Overloaded(T...) -> Overloaded<T...>;

bool finite(std::initializer_list<float> values) noexcept {
  for (float value : values) if (!std::isfinite(value)) return false;
  return true;
}

bool finite_color(const Color& color) noexcept {
  return finite({color.r, color.g, color.b, color.a});
}

bool visible(const Paint& paint) noexcept {
  return paint.type != Paint::Type::None &&
         (paint.type != Paint::Type::Solid || paint.color.a > 0.0f);
}

bool visible_stroke(const Paint& paint, float width) noexcept {
  return width > 0.0f && visible(paint);
}

bool nonnegative(std::initializer_list<float> values) noexcept {
  for (float value : values)
    if (!std::isfinite(value) || value < 0.0f) return false;
  return true;
}

bool valid_limits(const RenderValidationLimits& limits) noexcept {
  return limits.max_commands && limits.max_payload_bytes && limits.max_string_bytes &&
         limits.max_gradient_stops && limits.max_state_depth;
}

// A single pass over caller-owned immutable commands. Payload accounting includes
// inactive Paint storage too; an unused vector must not bypass the input bound.
class CommandCheck {
public:
  CommandCheck(const flex::RendererCapabilities& caps, const RenderValidationLimits& limits)
      : caps_(caps), limits_(limits), remaining_(limits.max_payload_bytes) {}

  RenderValidationResult result() const noexcept { return result_; }
  void index(std::size_t value) noexcept { index_ = value; }

  bool error(RenderValidationError code, const char* message) noexcept {
    if (result_) result_ = {code, index_, message};
    return false;
  }

  bool bytes(std::size_t count) noexcept {
    if (count > remaining_)
      return error(RenderValidationError::ResourceLimit, "render payload byte limit exceeded");
    remaining_ -= count;
    return true;
  }

  bool string(const std::string& value) noexcept {
    if (value.size() > limits_.max_string_bytes)
      return error(RenderValidationError::ResourceLimit, "render string byte limit exceeded");
    return bytes(value.size());
  }

  bool stops_storage(const std::vector<flex::ColorStop>& stops) noexcept {
    if (stops.size() > limits_.max_gradient_stops ||
        stops.size() > remaining_ / sizeof(flex::ColorStop))
      return error(RenderValidationError::ResourceLimit, "render gradient storage limit exceeded");
    return bytes(stops.size() * sizeof(flex::ColorStop));
  }

  bool stops_values(const std::vector<flex::ColorStop>& stops) noexcept {
    if (stops.empty())
      return error(RenderValidationError::InvalidValue, "active gradient requires a color stop");
    for (const auto& stop : stops)
      if (!std::isfinite(stop.offset) || !finite_color(stop.color))
        return error(RenderValidationError::InvalidValue, "gradient stop values must be finite");
    return true;
  }

  bool paint(const Paint& paint) noexcept {
    if (!stops_storage(paint.linear.stops) || !stops_storage(paint.radial.stops)) return false;
    switch (paint.type) {
    case Paint::Type::None: return true;
    case Paint::Type::Solid:
      return finite_color(paint.color) ||
             error(RenderValidationError::InvalidValue, "solid paint color must be finite");
    case Paint::Type::Linear:
      return (finite({paint.linear.x1, paint.linear.y1, paint.linear.x2, paint.linear.y2}) ||
              error(RenderValidationError::InvalidValue, "linear gradient coordinates must be finite")) &&
             stops_values(paint.linear.stops);
    case Paint::Type::Radial:
      return ((finite({paint.radial.cx, paint.radial.cy, paint.radial.fx, paint.radial.fy}) &&
               nonnegative({paint.radial.radius})) ||
              error(RenderValidationError::InvalidValue, "radial gradient values require a finite nonnegative radius")) &&
             stops_values(paint.radial.stops);
    default: return error(RenderValidationError::InvalidValue, "unknown paint kind");
    }
  }

  bool rect(float x, float y, float width, float height) noexcept {
    return (finite({x, y}) && nonnegative({width, height}) && finite({x + width, y + height})) ||
           error(RenderValidationError::InvalidValue, "rectangle values must be finite with nonnegative dimensions");
  }

  bool transform(const Transform& transform) noexcept {
    for (float value : transform.data)
      if (!std::isfinite(value))
        return error(RenderValidationError::InvalidValue, "transform values must be finite");
    if (transform.data[6] != 0.0f || transform.data[7] != 0.0f || transform.data[8] != 1.0f)
      return error(RenderValidationError::InvalidValue, "render commands require an affine 2D transform");
    if (!caps_.rotation && !caps_.scaling &&
        (transform.data[0] != 1.0f || transform.data[1] != 0.0f ||
         transform.data[3] != 0.0f || transform.data[4] != 1.0f))
      return error(RenderValidationError::UnsupportedOperation, "renderer supports only translation in this transform domain");
    return true;
  }

  bool command(const RenderCommand& command) noexcept {
    if (command.valueless_by_exception())
      return error(RenderValidationError::InvalidValue, "render command has no active value");
    return std::visit(Overloaded{
      [&](const BeginFrameCommand& c) {
        return (finite({c.viewport.width, c.viewport.height, c.viewport.pixel_ratio}) &&
                c.viewport.width > 0.0f && c.viewport.height > 0.0f && c.viewport.pixel_ratio > 0.0f) ||
               error(RenderValidationError::InvalidValue, "frame dimensions and pixel ratio must be finite and positive");
      },
      [&](const EndFrameCommand&) { return true; },
      [&](const ClearCommand& c) {
        return finite_color(c.color) || error(RenderValidationError::InvalidValue, "clear color must be finite");
      },
      [&](const SaveCommand&) { return true; },
      [&](const RestoreCommand&) { return true; },
      [&](const SetTransformCommand& c) { return transform(c.transform); },
      [&](const TranslateCommand& c) {
        return finite({c.x, c.y}) || error(RenderValidationError::InvalidValue, "translation must be finite");
      },
      [&](const RotateCommand& c) {
        if (!std::isfinite(c.degrees))
          return error(RenderValidationError::InvalidValue, "rotation must be finite");
        return c.degrees == 0.0f || caps_.rotation ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support rotation");
      },
      [&](const ScaleCommand& c) {
        if (!finite({c.x, c.y}))
          return error(RenderValidationError::InvalidValue, "scale must be finite");
        return (c.x == 1.0f && c.y == 1.0f) || caps_.scaling ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support scaling");
      },
      [&](const SetGlobalAlphaCommand& c) {
        return std::isfinite(c.alpha) || error(RenderValidationError::InvalidValue, "global alpha must be finite");
      },
      [&](const SetShadowCommand& c) {
        if (!flex::render_values::valid_shadow(c.shadow))
          return error(RenderValidationError::InvalidValue, "shadow values must be finite with nonnegative blur");
        return c.shadow.is_none() || c.shadow.color.a <= 0.0f || caps_.shadow ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support visible shadows");
      },
      [&](const ClearShadowCommand&) { return true; },
      [&](const SetBlurCommand& c) {
        if (!flex::render_values::valid_blur(c.blur))
          return error(RenderValidationError::InvalidValue, "blur radius must be finite and nonnegative");
        return c.blur.is_none() || caps_.blur ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support blur");
      },
      [&](const ClearBlurCommand&) { return true; },
      [&](const ClipRectCommand& c) { return rect(c.x, c.y, c.width, c.height); },
      [&](const ResetClipCommand&) { return true; },
      [&](const FillPathCommand& c) {
        if (!string(c.d) || !paint(c.paint)) return false;
        return c.d.empty() || !visible(c.paint) || caps_.path_drawing ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support path drawing");
      },
      [&](const StrokePathCommand& c) {
        if (!string(c.d) || !paint(c.paint)) return false;
        if (!nonnegative({c.width}))
          return error(RenderValidationError::InvalidValue, "path stroke width must be finite and nonnegative");
        return c.d.empty() || !visible_stroke(c.paint, c.width) || caps_.path_drawing ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support path drawing");
      },
      [&](const DrawRectCommand& c) {
        return rect(c.x, c.y, c.width, c.height) && paint(c.fill) && paint(c.stroke) &&
               (nonnegative({c.radius, c.stroke_width}) ||
                error(RenderValidationError::InvalidValue, "rectangle radius and stroke width must be finite and nonnegative"));
      },
      [&](const DrawLineCommand& c) {
        return paint(c.paint) &&
               ((finite({c.x1, c.y1, c.x2, c.y2}) && nonnegative({c.width})) ||
                error(RenderValidationError::InvalidValue, "line values must be finite with nonnegative width"));
      },
      [&](const DrawCircleCommand& c) {
        return paint(c.fill) && paint(c.stroke) &&
               ((finite({c.cx, c.cy, c.cx - c.radius, c.cx + c.radius,
                         c.cy - c.radius, c.cy + c.radius}) &&
                 nonnegative({c.radius, c.stroke_width})) ||
                error(RenderValidationError::InvalidValue, "circle values must be finite with nonnegative radius and stroke"));
      },
      [&](const DrawEllipseCommand& c) {
        return paint(c.fill) && paint(c.stroke) &&
               ((finite({c.cx, c.cy, c.cx - c.radius_x, c.cx + c.radius_x,
                         c.cy - c.radius_y, c.cy + c.radius_y}) &&
                 nonnegative({c.radius_x, c.radius_y, c.stroke_width})) ||
                error(RenderValidationError::InvalidValue, "ellipse values must be finite with nonnegative radii and stroke"));
      },
      [&](const DrawTextCommand& c) {
        return string(c.text) && string(c.font) &&
               ((finite({c.x, c.y}) && nonnegative({c.size}) && finite_color(c.color)) ||
                error(RenderValidationError::InvalidValue, "text values must be finite with nonnegative size"));
      },
      [&](const DrawImageCommand& c) {
        if (!string(c.src) || !rect(c.x, c.y, c.width, c.height)) return false;
        return c.src.empty() || c.width == 0.0f || c.height == 0.0f || caps_.raster_images ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support raster images");
      },
      [&](const DrawSvgCommand& c) {
        if (!string(c.src) || !rect(c.x, c.y, c.width, c.height)) return false;
        return c.src.empty() || c.width == 0.0f || c.height == 0.0f || caps_.svg_images ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support SVG images");
      },
      [&](const DrawSvgDataCommand& c) {
        if (!string(c.data) || !rect(c.x, c.y, c.width, c.height)) return false;
        return c.data.empty() || c.width == 0.0f || c.height == 0.0f || caps_.svg_images ||
               error(RenderValidationError::UnsupportedOperation, "renderer does not support SVG data");
      },
    }, command);
  }

private:
  const flex::RendererCapabilities& caps_;
  const RenderValidationLimits& limits_;
  std::size_t remaining_;
  std::size_t index_ = 0;
  RenderValidationResult result_;
};

} // namespace

RenderValidationResult RenderCommandList::validate_replay(
    const Renderer& renderer, RenderValidationScope scope,
    const RenderValidationLimits& limits) const {
  if (!valid_limits(limits))
    return {RenderValidationError::InvalidLimits, 0, "all render validation limits must be nonzero"};
  if (scope != RenderValidationScope::Commands && scope != RenderValidationScope::CompleteFrame)
    return {RenderValidationError::InvalidValue, 0, "unknown render validation scope"};
  if (commands_.size() > limits.max_commands)
    return {RenderValidationError::ResourceLimit, limits.max_commands, "render command count limit exceeded"};
  const bool complete = scope == RenderValidationScope::CompleteFrame;
  if (complete && (commands_.size() < 2 ||
      !std::holds_alternative<BeginFrameCommand>(commands_.front()) ||
      !std::holds_alternative<EndFrameCommand>(commands_.back())))
    return {RenderValidationError::InvalidSequence, 0, "complete frame requires BeginFrame first and EndFrame last"};

  const auto capabilities = renderer.capabilities();
  CommandCheck check(capabilities, limits);
  std::size_t local_depth = 0;
  for (std::size_t i = 0; i < commands_.size(); ++i) {
    const auto& command = commands_[i];
    check.index(i);
    if (complete && ((i != 0 && std::holds_alternative<BeginFrameCommand>(command)) ||
                     (i + 1 != commands_.size() && std::holds_alternative<EndFrameCommand>(command))))
      return {RenderValidationError::InvalidSequence, i, "complete frame contains an extra BeginFrame or EndFrame"};
    if (std::holds_alternative<SaveCommand>(command)) {
      if (local_depth == limits.max_state_depth)
        return {RenderValidationError::ResourceLimit, i, "render save depth limit exceeded"};
      ++local_depth;
    } else if (std::holds_alternative<RestoreCommand>(command)) {
      if (local_depth != 0) --local_depth;
      else if (complete)
        return {RenderValidationError::InvalidSequence, i, "complete frame restores an empty state stack"};
      // A fragment may restore state owned by the calling frame. No adapter
      // state is guessed or read back to infer that incoming save depth.
    }
    if (!check.command(command)) return check.result();
  }
  if (complete && local_depth != 0)
    return {RenderValidationError::InvalidSequence, commands_.size() - 1,
            "complete frame ends with an unbalanced state stack"};
  return {};
}

void RenderCommandList::require_valid_replay(
    const Renderer& renderer, RenderValidationScope scope,
    const RenderValidationLimits& limits) const {
  const auto result = validate_replay(renderer, scope, limits);
  if (!result) throw RenderValidationException(result);
}

} // namespace flexUI
