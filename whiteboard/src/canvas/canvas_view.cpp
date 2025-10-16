/**
 * \file canvas_view.cpp
 * \brief Implementation of CanvasView class.
 */

#include "whiteboard/canvas/canvas_view.h"
#include "whiteboard/canvas/canvas_controller.h"
#include <nanovg.h>

namespace whiteboard {

CanvasView::CanvasView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Canvas(parent, 1, false, false, false), m_document(document), m_controller(nullptr),
      m_has_current_stroke(false), m_has_marquee(false) {

  set_draw_border(false);

  // Register as observer
  if (m_document) {
    m_document->add_observer(this);
  }
}

CanvasView::~CanvasView() {
  // Unregister from document
  if (m_document) {
    m_document->remove_observer(this);
  }
}

// === Temporary State ===

void CanvasView::set_current_stroke(const Stroke &stroke) {
  m_current_stroke = stroke;
  m_has_current_stroke = true;
}

void CanvasView::clear_current_stroke() { m_has_current_stroke = false; }

void CanvasView::set_selection_marquee(const nanogui::Vector2f &start,
                                        const nanogui::Vector2f &end) {
  m_marquee_start = start;
  m_marquee_end = end;
  m_has_marquee = true;
}

void CanvasView::clear_selection_marquee() { m_has_marquee = false; }

// === Observer Notifications ===

void CanvasView::on_strokes_changed() {
  // Trigger redraw
  // Note: In NanoGUI, we typically don't have an explicit redraw() method
  // The framework will redraw on the next frame
}

void CanvasView::on_selection_changed() {
  // Trigger redraw
}

void CanvasView::on_view_changed() {
  // Viewport changed (zoom, pan) - trigger redraw
}

// === Coordinate Transforms ===

nanogui::Vector2f CanvasView::local_to_canvas(const nanogui::Vector2i &local) const {
  // Convert to float
  nanogui::Vector2f adjusted_local = to_vec(local);

  // Account for ruler offset if rulers are shown
  if (m_document->get_guides_visible()) {
    adjusted_local.x() -= RULER_SIZE;
    adjusted_local.y() -= RULER_SIZE;
  }

  // Apply inverse transform: (local - pan) / zoom
  return (adjusted_local - m_document->get_pan_offset()) / m_document->get_zoom();
}

nanogui::Vector2f CanvasView::canvas_to_local(const nanogui::Vector2f &canvas) const {
  // Apply transform: canvas * zoom + pan
  nanogui::Vector2f local = canvas * m_document->get_zoom() + m_document->get_pan_offset();

  // Account for ruler offset if rulers are shown
  if (m_document->get_guides_visible()) {
    local.x() += RULER_SIZE;
    local.y() += RULER_SIZE;
  }

  return local;
}

nanogui::Vector2f CanvasView::canvas_to_global(const nanogui::Vector2f &canvas) const {
  // Convert canvas to local, then local to global
  nanogui::Vector2f local = canvas_to_local(canvas);
  nanogui::Vector2i abs_pos = absolute_position();
  return nanogui::Vector2f(local.x() + abs_pos.x(), local.y() + abs_pos.y());
}

// === Input Event Delegation ===

bool CanvasView::mouse_button_event(const nanogui::Vector2i &p, int button, bool down,
                                     int modifiers) {
  if (!m_controller) {
    return nanogui::Canvas::mouse_button_event(p, button, down, modifiers);
  }

  // Convert to canvas coordinates
  nanogui::Vector2i local_p = p - absolute_position();
  nanogui::Vector2f canvas_pos = local_to_canvas(local_p);

  // Delegate to controller
  if (down) {
    return m_controller->handle_mouse_down(canvas_pos, modifiers);
  } else {
    return m_controller->handle_mouse_up(canvas_pos, modifiers);
  }
}

bool CanvasView::mouse_drag_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel,
                                   int button, int modifiers) {
  if (!m_controller) {
    return nanogui::Canvas::mouse_drag_event(p, rel, button, modifiers);
  }

  // Convert to canvas coordinates
  nanogui::Vector2i local_p = p - absolute_position();
  nanogui::Vector2f canvas_pos = local_to_canvas(local_p);

  // Delegate to controller
  return m_controller->handle_mouse_drag(canvas_pos, modifiers);
}

bool CanvasView::scroll_event(const nanogui::Vector2i &p, const nanogui::Vector2f &rel) {
  if (!m_controller) {
    return nanogui::Canvas::scroll_event(p, rel);
  }

  // Convert to canvas coordinates
  nanogui::Vector2i local_p = p - absolute_position();
  nanogui::Vector2f canvas_pos = local_to_canvas(local_p);

  // Delegate to controller
  return m_controller->handle_scroll(canvas_pos, rel.y());
}

bool CanvasView::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (!m_controller) {
    return nanogui::Canvas::keyboard_event(key, scancode, action, modifiers);
  }

  // Delegate to controller
  if (action == 1) { // Press
    return m_controller->handle_key_press(key, modifiers);
  } else if (action == 0) { // Release
    return m_controller->handle_key_release(key, modifiers);
  }

  return false;
}

// === Main Draw Method ===

void CanvasView::draw(NVGcontext *ctx) {
  nanogui::Canvas::draw(ctx);

  // Draw in layers from back to front
  draw_grid(ctx);
  draw_guides(ctx);
  draw_strokes(ctx);
  draw_current_stroke(ctx);
  draw_marquee(ctx);
  draw_selection(ctx);
  draw_rulers(ctx);
  draw_minimap(ctx);
  draw_coordinate_display(ctx);
  draw_snap_feedback(ctx);
}

// === Rendering Helpers ===

void CanvasView::draw_grid(NVGcontext *ctx) {
  if (!m_document->get_grid_visible()) {
    return;
  }

  float grid_size = GRID_SIZE;
  float zoom = m_document->get_zoom();
  nanogui::Vector2f pan = m_document->get_pan_offset();

  // Calculate visible canvas area
  nanogui::Vector2f top_left_canvas = local_to_canvas(nanogui::Vector2i(0, 0));
  nanogui::Vector2f bottom_right_canvas = local_to_canvas(m_size);

  float start_x = std::floor(top_left_canvas.x() / grid_size) * grid_size;
  float end_x = std::ceil(bottom_right_canvas.x() / grid_size) * grid_size;
  float start_y = std::floor(top_left_canvas.y() / grid_size) * grid_size;
  float end_y = std::ceil(bottom_right_canvas.y() / grid_size) * grid_size;

  nvgSave(ctx);
  nvgBeginPath(ctx);

  // Draw vertical lines
  for (float x = start_x; x <= end_x; x += grid_size) {
    nanogui::Vector2f top = canvas_to_global(nanogui::Vector2f(x, top_left_canvas.y()));
    nanogui::Vector2f bottom = canvas_to_global(nanogui::Vector2f(x, bottom_right_canvas.y()));
    nvgMoveTo(ctx, top.x(), top.y());
    nvgLineTo(ctx, bottom.x(), bottom.y());
  }

  // Draw horizontal lines
  for (float y = start_y; y <= end_y; y += grid_size) {
    nanogui::Vector2f left = canvas_to_global(nanogui::Vector2f(top_left_canvas.x(), y));
    nanogui::Vector2f right = canvas_to_global(nanogui::Vector2f(bottom_right_canvas.x(), y));
    nvgMoveTo(ctx, left.x(), left.y());
    nvgLineTo(ctx, right.x(), right.y());
  }

  nvgStrokeColor(ctx, nvgRGBA(220, 220, 220, 100));
  nvgStrokeWidth(ctx, 1.0f);
  nvgStroke(ctx);
  nvgRestore(ctx);
}

void CanvasView::draw_guides(NVGcontext *ctx) {
  if (!m_document->get_guides_visible()) {
    return;
  }

  const auto &guides = m_document->get_guides();
  nanogui::Vector2i abs_pos = absolute_position();

  nvgSave(ctx);
  for (const auto &guide : guides) {
    if (!guide.visible) {
      continue;
    }

    nvgBeginPath(ctx);
    if (guide.type == Guide::Horizontal) {
      nanogui::Vector2f screen_pos = canvas_to_global(nanogui::Vector2f(0, guide.position));
      float y = screen_pos.y();
      nvgMoveTo(ctx, abs_pos.x(), y);
      nvgLineTo(ctx, abs_pos.x() + m_size.x(), y);
    } else {
      nanogui::Vector2f screen_pos = canvas_to_global(nanogui::Vector2f(guide.position, 0));
      float x = screen_pos.x();
      nvgMoveTo(ctx, x, abs_pos.y());
      nvgLineTo(ctx, x, abs_pos.y() + m_size.y());
    }

    nvgStrokeColor(ctx, nvgRGBA(guide.color.r() * 255, guide.color.g() * 255,
                                guide.color.b() * 255, guide.color.w() * 255));
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);
  }
  nvgRestore(ctx);
}

void CanvasView::draw_strokes(NVGcontext *ctx) {
  const auto &strokes = m_document->get_strokes();
  for (const auto &stroke : strokes) {
    if (stroke.visible) {
      draw_stroke(ctx, stroke);
    }
  }
}

void CanvasView::draw_stroke(NVGcontext *ctx, const Stroke &stroke) {
  if (stroke.points.empty()) {
    return;
  }

  float zoom = m_document->get_zoom();

  nvgSave(ctx);

  // Draw based on tool type
  switch (stroke.tool) {
  case Tool::Rectangle: {
    if (stroke.points.size() < 2)
      break;
    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float x = std::min(x1, x2), y = std::min(y1, y2);
    float w = std::abs(x2 - x1), h = std::abs(y2 - y1);

    nanogui::Vector2f top_left = canvas_to_global(nanogui::Vector2f(x, y));
    nvgBeginPath(ctx);
    nvgRect(ctx, top_left.x(), top_left.y(), w * zoom, h * zoom);

    if (stroke.fill_style != FillStyle::None) {
      nvgFillColor(ctx, nvgRGBA(stroke.fill_color.r() * 255, stroke.fill_color.g() * 255,
                                stroke.fill_color.b() * 255, stroke.fill_color.w() * 255));
      nvgFill(ctx);
    }

    nvgStrokeColor(ctx, nvgRGBA(stroke.color.r() * 255, stroke.color.g() * 255,
                                stroke.color.b() * 255, stroke.color.w() * 255));
    nvgStrokeWidth(ctx, stroke.width * zoom);
    nvgStroke(ctx);
    break;
  }

  case Tool::Circle: {
    if (stroke.points.size() < 2)
      break;
    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float dx = x2 - x1, dy = y2 - y1;
    float radius = std::sqrt(dx * dx + dy * dy);

    nanogui::Vector2f center = canvas_to_global(nanogui::Vector2f(x1, y1));
    nvgBeginPath(ctx);
    nvgCircle(ctx, center.x(), center.y(), radius * zoom);

    if (stroke.fill_style != FillStyle::None) {
      nvgFillColor(ctx, nvgRGBA(stroke.fill_color.r() * 255, stroke.fill_color.g() * 255,
                                stroke.fill_color.b() * 255, stroke.fill_color.w() * 255));
      nvgFill(ctx);
    }

    nvgStrokeColor(ctx, nvgRGBA(stroke.color.r() * 255, stroke.color.g() * 255,
                                stroke.color.b() * 255, stroke.color.w() * 255));
    nvgStrokeWidth(ctx, stroke.width * zoom);
    nvgStroke(ctx);
    break;
  }

  case Tool::Line:
  case Tool::Arrow: {
    if (stroke.points.size() < 2)
      break;
    nanogui::Vector2f p0 = canvas_to_global(to_vec(stroke.points[0]));
    nanogui::Vector2f p1 = canvas_to_global(to_vec(stroke.points[1]));

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, p0.x(), p0.y());
    nvgLineTo(ctx, p1.x(), p1.y());
    nvgStrokeColor(ctx, nvgRGBA(stroke.color.r() * 255, stroke.color.g() * 255,
                                stroke.color.b() * 255, stroke.color.w() * 255));
    nvgStrokeWidth(ctx, stroke.width * zoom);
    nvgStroke(ctx);

    // Draw arrowhead for Arrow tool
    if (stroke.tool == Tool::Arrow) {
      float dx = p1.x() - p0.x();
      float dy = p1.y() - p0.y();
      float angle = std::atan2(dy, dx);
      float arrow_size = 10.0f * zoom;

      nvgBeginPath(ctx);
      nvgMoveTo(ctx, p1.x(), p1.y());
      nvgLineTo(ctx, p1.x() - arrow_size * std::cos(angle - 0.5f),
                p1.y() - arrow_size * std::sin(angle - 0.5f));
      nvgLineTo(ctx, p1.x() - arrow_size * std::cos(angle + 0.5f),
                p1.y() - arrow_size * std::sin(angle + 0.5f));
      nvgClosePath(ctx);
      nvgFillColor(ctx, nvgRGBA(stroke.color.r() * 255, stroke.color.g() * 255,
                                stroke.color.b() * 255, stroke.color.w() * 255));
      nvgFill(ctx);
    }
    break;
  }

  case Tool::Pen:
  default: {
    // Draw freehand path
    nvgBeginPath(ctx);
    for (size_t i = 0; i < stroke.points.size(); ++i) {
      nanogui::Vector2f screen_pos = canvas_to_global(to_vec(stroke.points[i]));
      if (i == 0) {
        nvgMoveTo(ctx, screen_pos.x(), screen_pos.y());
      } else {
        nvgLineTo(ctx, screen_pos.x(), screen_pos.y());
      }
    }
    nvgStrokeColor(ctx, nvgRGBA(stroke.color.r() * 255, stroke.color.g() * 255,
                                stroke.color.b() * 255, stroke.color.w() * 255));
    nvgStrokeWidth(ctx, stroke.width * zoom);
    nvgStroke(ctx);
    break;
  }
  }

  nvgRestore(ctx);
}

void CanvasView::draw_selection(NVGcontext *ctx) {
  const auto &selected = m_document->get_selected_indices();
  if (selected.empty()) {
    return;
  }

  const auto &strokes = m_document->get_strokes();

  // Calculate bounding box of all selected strokes
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();

  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float s_min_x, s_min_y, s_max_x, s_max_y;
      strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
      min_x = std::min(min_x, s_min_x);
      min_y = std::min(min_y, s_min_y);
      max_x = std::max(max_x, s_max_x);
      max_y = std::max(max_y, s_max_y);
    }
  }

  if (min_x > max_x) {
    return;
  }

  // Draw selection box
  nanogui::Vector2f top_left = canvas_to_global(nanogui::Vector2f(min_x, min_y));
  nanogui::Vector2f bottom_right = canvas_to_global(nanogui::Vector2f(max_x, max_y));

  nvgSave(ctx);
  nvgBeginPath(ctx);
  nvgRect(ctx, top_left.x(), top_left.y(), bottom_right.x() - top_left.x(),
          bottom_right.y() - top_left.y());
  nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255));
  nvgStrokeWidth(ctx, 2.0f);
  nvgStroke(ctx);
  nvgRestore(ctx);
}

void CanvasView::draw_current_stroke(NVGcontext *ctx) {
  if (!m_has_current_stroke || m_current_stroke.points.empty()) {
    return;
  }

  draw_stroke(ctx, m_current_stroke);
}

void CanvasView::draw_marquee(NVGcontext *ctx) {
  if (!m_has_marquee) {
    return;
  }

  nanogui::Vector2f start = canvas_to_global(m_marquee_start);
  nanogui::Vector2f end = canvas_to_global(m_marquee_end);

  nvgSave(ctx);
  nvgBeginPath(ctx);
  nvgRect(ctx, start.x(), start.y(), end.x() - start.x(), end.y() - start.y());
  nvgFillColor(ctx, nvgRGBA(0, 120, 215, 30));
  nvgFill(ctx);
  nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 200));
  nvgStrokeWidth(ctx, 1.0f);
  nvgStroke(ctx);
  nvgRestore(ctx);
}

void CanvasView::draw_rulers(NVGcontext *ctx) {
  if (!m_document->get_guides_visible()) {
    return;
  }

  // TODO: Implement ruler rendering
  // This requires drawing tick marks and numbers along the edges
}

void CanvasView::draw_minimap(NVGcontext *ctx) {
  // TODO: Implement minimap rendering
  // This requires calculating content bounds and drawing a scaled-down version
}

void CanvasView::draw_coordinate_display(NVGcontext *ctx) {
  // TODO: Implement coordinate display
  // This shows zoom level, cursor position, and pan offset
}

void CanvasView::draw_snap_feedback(NVGcontext *ctx) {
  // TODO: Implement snap feedback visualization
  // This shows a visual indicator when snapping occurs
}

} // namespace whiteboard
