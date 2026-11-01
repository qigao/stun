#include "native_timeline_widget.h"

#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/native_canvas_widget.h>
#include <flexUI/render_command.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace flexui_demo {
namespace {

constexpr float kOriginX = 20.0F;
constexpr float kPixelsPerFrame = 12.0F;
constexpr float kClipY = 74.0F;
constexpr float kClipHeight = 44.0F;
constexpr float kEdgeHitPixels = 8.0F;
constexpr float kSnapPixels = 14.0F;
constexpr int kMaxFrames = 64;

enum class DragMode { None, Move, TrimStart, TrimEnd };

struct Gesture {
  DragMode mode = DragMode::None;
  int clip_index = -1;
  float grab_offset_frames = 0.0F;
};

float pixel_at_frame(int frame) {
  return kOriginX + static_cast<float>(frame) * kPixelsPerFrame;
}

int quantized_frame(float local_x) {
  return static_cast<int>(std::lround((local_x - kOriginX) / kPixelsPerFrame));
}

int snap_edge(int frame, const TimelineClip &other) {
  int result = frame;
  float best_distance = kSnapPixels + 1.0F;
  for (const int target : {other.start_frame, other.start_frame + other.length_frames}) {
    const float distance = static_cast<float>(std::abs(target - frame)) * kPixelsPerFrame;
    if (distance <= kSnapPixels && distance < best_distance) {
      best_distance = distance;
      result = target;
    }
  }
  return result;
}

int snap_moving_start(int start, int length, const TimelineClip &other) {
  int result = start;
  float best_distance = kSnapPixels + 1.0F;
  for (const int edge : {start, start + length}) {
    for (const int target : {other.start_frame, other.start_frame + other.length_frames}) {
      const float distance = static_cast<float>(std::abs(target - edge)) * kPixelsPerFrame;
      if (distance <= kSnapPixels && distance < best_distance) {
        best_distance = distance;
        result = start + target - edge;
      }
    }
  }
  return result;
}

void draw_timeline(const TimelineState &model, const flexUI::Element &element,
                   flexUI::RenderCommandList &commands) {
  using flexUI::Color;
  using flexUI::Paint;

  const float width = element.width();
  const auto guide = Paint::solid(Color{0.30F, 0.39F, 0.52F, 0.65F});
  const auto title = Color{0.85F, 0.90F, 0.99F, 1.0F};
  const auto muted = Color{0.65F, 0.72F, 0.84F, 1.0F};

  commands.draw_text("TIMELINE  |  linked native widget", 14.0F, 15.0F,
                     "Arial", 12.0F, true, title);
  for (int frame = 0; frame <= kMaxFrames; frame += 4) {
    const float x = pixel_at_frame(frame);
    if (x >= width - 10.0F) break;
    commands.draw_line(x, 24.0F, x, frame % 8 == 0 ? 43.0F : 35.0F, guide, 1.0F);
    if (frame % 8 == 0) {
      commands.draw_text(std::to_string(frame), x + 3.0F, 45.0F,
                         "Arial", 10.0F, false, muted);
    }
  }
  commands.draw_rect(12.0F, 66.0F, std::max(0.0F, width - 24.0F), 60.0F,
                     4.0F, Paint::solid(Color{0.10F, 0.14F, 0.22F, 1.0F}),
                     Paint::none(), 0.0F);

  for (std::size_t i = 0; i < model.clips.size(); ++i) {
    const TimelineClip &clip = model.clips[i];
    const float x = pixel_at_frame(clip.start_frame);
    const float w = static_cast<float>(clip.length_frames) * kPixelsPerFrame;
    const Color fill = i == 0 ? Color{0.22F, 0.48F, 0.87F, 1.0F}
                              : Color{0.66F, 0.40F, 0.77F, 1.0F};
    commands.draw_rect(x, kClipY, w, kClipHeight, 4.0F,
                       Paint::solid(fill),
                       Paint::solid(Color{0.80F, 0.85F, 0.96F, 0.80F}), 1.0F);
    commands.draw_line(x + kEdgeHitPixels, kClipY + 4.0F,
                       x + kEdgeHitPixels, kClipY + kClipHeight - 4.0F,
                       Paint::solid(Color{1.0F, 1.0F, 1.0F, 0.45F}), 1.0F);
    commands.draw_line(x + w - kEdgeHitPixels, kClipY + 4.0F,
                       x + w - kEdgeHitPixels, kClipY + kClipHeight - 4.0F,
                       Paint::solid(Color{1.0F, 1.0F, 1.0F, 0.45F}), 1.0F);
    commands.draw_text(i == 0 ? "Clip A" : "Clip B", x + 12.0F,
                       kClipY + 26.0F, "Arial", 12.0F, true, title);
  }
  commands.draw_text("Drag clips / trim edges / magnetic snap", 14.0F,
                     148.0F, "Arial", 11.0F, false, muted);
}

bool handle_timeline_input(flexUI::NativeCanvasWidget &widget,
                           const flexUI::Event &event, flexUI::Element &element,
                           TimelineState &model, Gesture &gesture) {
  using flexUI::EventType;
  using flexUI::MouseButton;

  if (event.type == EventType::FocusOut) {
    gesture.mode = DragMode::None;
    gesture.clip_index = -1;
    widget.release_pointer_capture();
    return false;
  }
  if (event.type == EventType::MouseUp && event.button == MouseButton::Left) {
    if (gesture.mode == DragMode::None) return false;
    gesture.mode = DragMode::None;
    gesture.clip_index = -1;
    widget.release_pointer_capture();
    return true;
  }
  if (event.type != EventType::MouseDown && event.type != EventType::MouseMove) {
    return false;
  }
  if (!std::isfinite(event.x) || !std::isfinite(event.y)) return false;

  const float x = event.x - element.absolute_x();
  const float y = event.y - element.absolute_y();

  if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
    if (y < kClipY || y > kClipY + kClipHeight) return false;
    for (std::size_t i = 0; i < model.clips.size(); ++i) {
      const auto &clip = model.clips[i];
      const float begin = pixel_at_frame(clip.start_frame);
      const float end = pixel_at_frame(clip.start_frame + clip.length_frames);
      if (x < begin || x > end) continue;
      gesture.clip_index = static_cast<int>(i);
      gesture.grab_offset_frames =
          (x - kOriginX) / kPixelsPerFrame - static_cast<float>(clip.start_frame);
      gesture.mode = x - begin <= kEdgeHitPixels ? DragMode::TrimStart
                   : end - x <= kEdgeHitPixels ? DragMode::TrimEnd
                                              : DragMode::Move;
      widget.request_pointer_capture();
      return true;
    }
    return false;
  }

  if (event.type != EventType::MouseMove || gesture.mode == DragMode::None) {
    return false;
  }
  if (gesture.clip_index < 0 ||
      static_cast<std::size_t>(gesture.clip_index) >= model.clips.size()) {
    widget.release_pointer_capture();
    gesture.mode = DragMode::None;
    return false;
  }

  auto &clip = model.clips[static_cast<std::size_t>(gesture.clip_index)];
  const auto &other = model.clips[static_cast<std::size_t>(1 - gesture.clip_index)];
  const int current_end = clip.start_frame + clip.length_frames;
  if (gesture.mode == DragMode::Move) {
    const float offset = (x - kOriginX) / kPixelsPerFrame - gesture.grab_offset_frames;
    int start = static_cast<int>(std::lround(offset));
    start = snap_moving_start(start, clip.length_frames, other);
    clip.start_frame = std::clamp(start, 0, kMaxFrames - clip.length_frames);
  } else if (gesture.mode == DragMode::TrimStart) {
    const int start = std::clamp(snap_edge(quantized_frame(x), other),
                                 0, current_end - 1);
    clip.start_frame = start;
    clip.length_frames = current_end - start;
  } else if (gesture.mode == DragMode::TrimEnd) {
    const int end = std::clamp(snap_edge(quantized_frame(x), other),
                               clip.start_frame + 1, kMaxFrames);
    clip.length_frames = end - clip.start_frame;
  }
  element.mark_paint_dirty();
  return true;
}

} // namespace

flexUI::WidgetRegistryError register_timeline_widget(
    flexUI::WidgetRegistry &registry, std::shared_ptr<TimelineState> model) {
  if (!model) {
    return {flexUI::WidgetRegistryErrorCode::InvalidFactory,
            "Native Timeline requires an application-owned model"};
  }
  return registry.register_widget(
      "native-timeline",
      [model = std::move(model)](const flexUI::UiNodeDefinition &)
          -> std::unique_ptr<flexUI::Widget> {
        auto gesture = std::make_shared<Gesture>();
        flexUI::NativeCanvasWidget::Callbacks callbacks;
        callbacks.paint = [model](const flexUI::Element &element,
                                  flexUI::RenderCommandList &commands) {
          draw_timeline(*model, element, commands);
        };
        callbacks.input = [model, gesture](flexUI::NativeCanvasWidget &widget,
                                           const flexUI::Event &event,
                                           flexUI::Element &element) {
          return handle_timeline_input(widget, event, element, *model, *gesture);
        };
        return std::make_unique<flexUI::NativeCanvasWidget>(std::move(callbacks));
      },
      flexUI::UiContentModel::Empty);
}

} // namespace flexui_demo
