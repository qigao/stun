/*
 * flexUI - RenderCommand
 *
 * Backend-neutral drawing commands emitted after style and layout.
 */

#ifndef FLEXUI_RENDER_COMMAND_H
#define FLEXUI_RENDER_COMMAND_H

#include "render_frame.h"
#include <flex/runtime/renderer.h>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace flexUI {

class Renderer;
class RenderCommandCache;
using flex::Paint;
using flex::PaintHandle;
using flex::Transform;

struct BeginFrameCommand {
  RenderViewport viewport;
};

struct EndFrameCommand {};

struct ClearCommand {
  Color color;
};

struct SaveCommand {};
struct RestoreCommand {};

struct SetTransformCommand {
  Transform transform;
};

struct TranslateCommand {
  float x = 0.0f;
  float y = 0.0f;
};

struct RotateCommand {
  float degrees = 0.0f;
};

struct ScaleCommand {
  float x = 1.0f;
  float y = 1.0f;
};

struct SetGlobalAlphaCommand {
  float alpha = 1.0f;
};

struct SetShadowCommand {
  flex::Shadow shadow;
};

struct ClearShadowCommand {};

struct SetBlurCommand {
  flex::BlurFilter blur;
};

struct ClearBlurCommand {};

struct ClipRectCommand {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

struct ResetClipCommand {};

struct FillPathCommand {
  std::string d;
  Paint paint = Paint::none();
};

struct StrokePathCommand {
  std::string d;
  Paint paint = Paint::none();
  float width = 0.0f;
};

struct DrawRectCommand {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  float radius = 0.0f;
  Paint fill = Paint::none();
  Paint stroke = Paint::none();
  float stroke_width = 0.0f;
};

struct DrawLineCommand {
  float x1 = 0.0f;
  float y1 = 0.0f;
  float x2 = 0.0f;
  float y2 = 0.0f;
  Paint paint = Paint::none();
  float width = 0.0f;
};

struct DrawCircleCommand {
  float cx = 0.0f;
  float cy = 0.0f;
  float radius = 0.0f;
  Paint fill = Paint::none();
  Paint stroke = Paint::none();
  float stroke_width = 0.0f;
};

struct DrawEllipseCommand {
  float cx = 0.0f;
  float cy = 0.0f;
  float radius_x = 0.0f;
  float radius_y = 0.0f;
  Paint fill = Paint::none();
  Paint stroke = Paint::none();
  float stroke_width = 0.0f;
};

struct DrawTextCommand {
  std::string text;
  float x = 0.0f;
  float y = 0.0f;
  std::string font;
  float size = 0.0f;
  bool bold = false;
  Color color{};
};

struct DrawImageCommand {
  std::string src;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

struct DrawSvgCommand {
  std::string src;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

struct DrawSvgDataCommand {
  std::string data;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

using RenderCommand =
    std::variant<BeginFrameCommand, EndFrameCommand, ClearCommand, SaveCommand,
                 RestoreCommand, SetTransformCommand, TranslateCommand, RotateCommand, ScaleCommand,
                 SetGlobalAlphaCommand, SetShadowCommand, ClearShadowCommand,
                 SetBlurCommand, ClearBlurCommand, ClipRectCommand,
                 ResetClipCommand, FillPathCommand, StrokePathCommand,
                 DrawRectCommand, DrawLineCommand, DrawCircleCommand, DrawEllipseCommand, DrawTextCommand,
                 DrawImageCommand, DrawSvgCommand, DrawSvgDataCommand>;

// Checks the recorded command snapshot. This is not qualified shader admission:
// old capability flags can reject operations, but cannot prove native readiness.
enum class RenderValidationScope : std::uint8_t { Commands, CompleteFrame };
enum class RenderValidationError : std::uint8_t {
  None, InvalidLimits, ResourceLimit, InvalidValue, UnsupportedOperation,
  InvalidSequence,
};

struct RenderValidationLimits {
  std::size_t max_commands = 65536;
  std::size_t max_payload_bytes = 8 * 1024 * 1024;
  std::size_t max_string_bytes = 1024 * 1024;
  std::size_t max_gradient_stops = 256;
  std::size_t max_state_depth = 256;
};

struct RenderValidationResult {
  RenderValidationError code = RenderValidationError::None;
  std::size_t command_index = 0;
  // Points to a static diagnostic literal; no input string is borrowed.
  const char* message = "";
  explicit operator bool() const noexcept { return code == RenderValidationError::None; }
};

class RenderValidationException final : public std::logic_error {
public:
  explicit RenderValidationException(RenderValidationResult diagnostic)
      : std::logic_error(diagnostic.message ? diagnostic.message :
                         "render command validation failed"), diagnostic_(diagnostic) {}
  const RenderValidationResult& diagnostic() const noexcept { return diagnostic_; }
private:
  RenderValidationResult diagnostic_;
};

class RenderCommandList {
public:
  RenderCommandList() = delete;
  explicit RenderCommandList(const flex::RendererCapabilities& capabilities);
  RenderCommandList(const flex::RendererCapabilities& capabilities,
                    const Transform& transform_prefix);

  void begin_frame(const RenderViewport& viewport);
  void end_frame();
  void clear(const Color& color);
  void save();
  void restore();
  void set_transform(const Transform& transform);
  void translate(float x, float y);
  void rotate(float degrees);
  void scale(float x, float y);
  void set_global_alpha(float alpha);
  void set_shadow(const flex::Shadow& shadow);
  void clear_shadow();
  void set_blur(const flex::BlurFilter& blur);
  void clear_blur();
  void clip_rect(float x, float y, float width, float height);
  void reset_clip();
  void push_transform_prefix(const Transform& transform);
  void pop_transform_prefix();
  void fill_path(std::string d, const Paint& paint);
  void stroke_path(std::string d, const Paint& paint, float width);
  void draw_rect(float x, float y, float width, float height, float radius,
                 const Paint& fill, const Paint& stroke, float stroke_width);
  void draw_line(float x1, float y1, float x2, float y2, const Paint& paint,
                 float width);
  void draw_circle(float cx, float cy, float radius, const Paint& fill,
                   const Paint& stroke, float stroke_width);
  void draw_ellipse(float cx, float cy, float radius_x, float radius_y,
                    const Paint& fill, const Paint& stroke, float stroke_width);
  void draw_text(std::string text, float x, float y, std::string font,
                 float size, bool bold, const Color& color);
  void draw_image(std::string src, float x, float y, float width,
                  float height);
  void draw_svg(std::string src, float x, float y, float width, float height);
  void draw_svg_data(std::string data, float x, float y, float width,
                     float height);

  // Uses the executing renderer's capabilities, not the recording-time copy.
  // CompleteFrame additionally requires one Begin/End pair and balanced saves.
  // Commands preserves existing fragment replay, including inherited state.
  // The owner must not mutate this list or its renderer concurrently with calls.
  // O(commands + retained gradient stops); O(1) auxiliary space, no allocation.
  RenderValidationResult validate_replay(
      const Renderer& renderer,
      RenderValidationScope scope = RenderValidationScope::Commands,
      const RenderValidationLimits& limits = {}) const;
  void require_valid_replay(
      const Renderer& renderer,
      RenderValidationScope scope = RenderValidationScope::Commands,
      const RenderValidationLimits& limits = {}) const;

  void replay(Renderer& renderer) const;
  void replay(Renderer& renderer, const RenderValidationLimits& limits) const;
  void replay_retained(Renderer& renderer, RenderCommandCache& cache) const;
  void replay_retained(Renderer& renderer, RenderCommandCache& cache,
                       const RenderValidationLimits& limits) const;
  uint64_t signature() const;
  void reset();
  void reserve(size_t capacity);
  void append(const RenderCommandList& other);
  void append(const std::vector<RenderCommand>& commands);
  void append_with_transform_prefix(const std::vector<RenderCommand>& commands);

  const flex::RendererCapabilities& capabilities() const { return capabilities_; }
  const std::vector<RenderCommand>& commands() const { return commands_; }

private:
  void add(RenderCommand command);

  flex::RendererCapabilities capabilities_{};
  Transform base_transform_prefix_{};
  Transform transform_prefix_{};
  std::vector<Transform> transform_prefix_stack_;
  std::vector<RenderCommand> commands_;
};

class RenderCommandCache {
public:
  ~RenderCommandCache() = default;
  void clear(Renderer& renderer);
  bool empty() const { return entries_.empty(); }

private:
  friend class RenderCommandList;

  struct Entry {
    PaintHandle handle = nullptr;
    uint64_t content_signature = 0;
    Transform transform{};
    bool retained = false;
  };

  std::vector<Entry> entries_;
  std::vector<Entry> pending_entries_;
};

} // namespace flexUI

#endif // FLEXUI_RENDER_COMMAND_H
