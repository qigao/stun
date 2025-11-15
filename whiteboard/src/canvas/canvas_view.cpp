/**
 * \file canvas_view.cpp
 * \brief Implementation of CanvasView class.
 */

#include "whiteboard/canvas/canvas_view.h"
#include "whiteboard/canvas/canvas_controller.h"
#include "whiteboard/ddf/ddf_document.h"
#include "whiteboard/ddf/event_layer.h"
#include "whiteboard/ddf/rendering_pipeline.h"
#include "whiteboard/ddf/shape_layer.h"
#include "whiteboard/ddf/style_layer.h"
#include "whiteboard/svg/svg_renderer.h"
#include "whiteboard/svg/svg_shape_library.h"
#include <algorithm>
#include <cmath>
#include <fmtlog.h>
#include <fstream>
#include <limits>
#include <lunasvg.h>
#include <nanogui/screen.h>
#include <nanovg.h>

namespace whiteboard {

// Constants
static constexpr float GRID_SIZE = 20.0f;  // Grid spacing in pixels
static constexpr float RULER_SIZE = 30.0f; // Ruler area size in pixels

CanvasView::CanvasView(nanogui::Widget *parent, WhiteboardDocument *document)
    : nanogui::Canvas(parent, 1, false, false, false), m_document(document), m_controller(nullptr),
      m_svg_renderer(nullptr), m_has_current_stroke(false), m_has_marquee(false),
      m_has_snap_point(false), m_has_temp_guide(false), m_has_cursor_pos(false),
      m_nvg_context(nullptr) {

  set_draw_border(false);

  // Create SVG renderer
  m_svg_renderer = new SVGRenderer();

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

  // Clean up SVG renderer
  delete m_svg_renderer;
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

void CanvasView::set_snap_indicator(const nanogui::Vector2f &point) {
  m_snap_point = point;
  m_has_snap_point = true;
}

void CanvasView::clear_snap_indicator() { m_has_snap_point = false; }

void CanvasView::set_temp_guide(Guide::Type type, float position) {
  m_temp_guide_type = type;
  m_temp_guide_position = position;
  m_has_temp_guide = true;
}

void CanvasView::clear_temp_guide() { m_has_temp_guide = false; }

bool CanvasView::is_in_top_ruler(const nanogui::Vector2i &p) const {
  if (!m_document->get_guides_visible()) {
    return false;
  }
  return p.y() >= 0 && p.y() < RULER_SIZE && p.x() >= RULER_SIZE;
}

bool CanvasView::is_in_left_ruler(const nanogui::Vector2i &p) const {
  if (!m_document->get_guides_visible()) {
    return false;
  }
  return p.x() >= 0 && p.x() < RULER_SIZE && p.y() >= RULER_SIZE;
}

// === Observer Notifications ===

void CanvasView::on_strokes_changed() {
  // Invalidate text bounds cache since strokes have changed
  m_text_bounds_cache.clear();
  m_cached_svg_data.clear();

  // Trigger redraw by requesting screen refresh
  logi("CanvasView::on_strokes_changed() - requesting redraw");
  if (screen()) {
    screen()->redraw();
  } else {
    logw("CanvasView::on_strokes_changed() - screen() is null!");
  }
}

void CanvasView::on_selection_changed() {
  // Trigger redraw
}

void CanvasView::on_view_changed() {
  // Update canvas background based on dark mode
  if (m_document) {
    set_background_color(m_document->get_canvas_background());
  }
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
  // Check if mouse is over any sibling widget (panels, toolbars)
  // Canvas covers full screen, so we need to let siblings handle events first
  // Check in REVERSE order (last created = on top = checked first)
  if (parent()) {
    const auto &children = parent()->children();

    // Iterate in reverse order (top to bottom in z-order)
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      auto child = *it;
      if (child == this || !child->visible())
        continue;

      // Check if point is inside this sibling's bounds
      auto pos = child->position();
      auto size = child->size();
      if (p.x() >= pos.x() && p.x() < pos.x() + size.x() && p.y() >= pos.y() &&
          p.y() < pos.y() + size.y()) {
        // Let the sibling handle it
        if (child->mouse_button_event(p, button, down, modifiers)) {
          return true;
        }
        // Continue checking other siblings - don't stop here!
        // Multiple widgets might overlap, and we want the topmost one that handles it
      }
    }
  }

  if (!m_controller) {
    return nanogui::Canvas::mouse_button_event(p, button, down, modifiers);
  }

  // Convert to local coordinates
  nanogui::Vector2i local_p = p - absolute_position();

  // DON'T request focus on click - let widgets handle their own focus
  // This prevents stealing focus from the inline text editor
  // The canvas controller will handle focus when needed

  // Check if clicking in rulers to create guides
  if (down && button == 0) { // Left button
    if (is_in_top_ruler(local_p)) {
      // Start creating vertical guide
      nanogui::Vector2f canvas_pos = local_to_canvas(local_p);
      m_controller->start_guide_creation(Guide::Vertical, canvas_pos);
      return true;
    } else if (is_in_left_ruler(local_p)) {
      // Start creating horizontal guide
      nanogui::Vector2f canvas_pos = local_to_canvas(local_p);
      m_controller->start_guide_creation(Guide::Horizontal, canvas_pos);
      return true;
    }
  }

  // Handle right-click on SVG shapes for add/remove line menu
  if (button == 1 && down && m_right_click_callback) { // Right button
    nanogui::Vector2f canvas_pos = local_to_canvas(local_p);
    int clicked_stroke = m_document->find_stroke_at_point(canvas_pos.x(), canvas_pos.y());

    if (clicked_stroke >= 0) {
      const auto &strokes = m_document->get_strokes();
      if (clicked_stroke < static_cast<int>(strokes.size())) {
        const Stroke &stroke = strokes[clicked_stroke];
        if (stroke.tool == Tool::SVGShape) {
          // Right-clicked on SVG shape - show add/remove line menu
          m_right_click_callback(p); // Pass global position
          return true;
        }
      }
    }
  }

  // Convert to canvas coordinates
  nanogui::Vector2f canvas_pos = local_to_canvas(local_p);

  // Handle DDF element clicks
  if (down && button == 0) { // Left button
    if (handle_ddf_mouse_event(canvas_pos, "click")) {
      return true; // DDF element handled the click
    }
  }

  // Delegate to controller
  if (down) {
    return m_controller->handle_mouse_down(canvas_pos, modifiers);
  } else {
    return m_controller->handle_mouse_up(canvas_pos, modifiers);
  }
}

bool CanvasView::mouse_drag_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel,
                                  int button, int modifiers) {
  // Check if mouse is over any sibling widget (panels, toolbars)
  // Check in REVERSE order (last created = on top = checked first)
  if (parent()) {
    const auto &children = parent()->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      auto child = *it;
      if (child == this || !child->visible())
        continue;

      // Check if point is inside this sibling's bounds
      auto pos = child->position();
      auto size = child->size();
      if (p.x() >= pos.x() && p.x() < pos.x() + size.x() && p.y() >= pos.y() &&
          p.y() < pos.y() + size.y()) {
        // Let the sibling handle it
        if (child->mouse_drag_event(p, rel, button, modifiers)) {
          return true;
        }
      }
    }
  }

  // Track cursor position for pending shape preview
  nanogui::Vector2i local_p = p - absolute_position();
  m_last_cursor_pos = local_to_canvas(local_p);
  m_has_cursor_pos = true;

  if (!m_controller) {
    return nanogui::Canvas::mouse_drag_event(p, rel, button, modifiers);
  }

  // Delegate to controller
  return m_controller->handle_mouse_drag(m_last_cursor_pos, modifiers);
}

bool CanvasView::mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel,
                                    int button, int modifiers) {
  // Track cursor position for pending shape preview
  nanogui::Vector2i local_p = p - absolute_position();
  m_last_cursor_pos = local_to_canvas(local_p);
  m_has_cursor_pos = true;

  // Update DDF pseudo-states (hover)
  update_ddf_pseudo_states(m_last_cursor_pos);

  // Debug log for pending shape
  if (m_document && !m_document->get_pending_svg_shape().empty()) {
    logi("Mouse motion: canvas pos ({:.1f}, {:.1f}), pending shape: '{}'", m_last_cursor_pos.x(),
         m_last_cursor_pos.y(), m_document->get_pending_svg_shape());
  }

  return nanogui::Canvas::mouse_motion_event(p, rel, button, modifiers);
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
  // Debug log
  logd("CanvasView: keyboard_event - key={}, action={}, modifiers={}", key, action, modifiers);
  // This allows the inline text editor to receive keyboard input
  // Call parent's keyboard_event which will route to focused children
  bool handled_by_child = nanogui::Canvas::keyboard_event(key, scancode, action, modifiers);
  if (handled_by_child) {
    logd("CanvasView: keyboard event handled by child widget");
    return true;
  }

  if (!m_controller) {
    return false;
  }

  // Only delegate to controller if no child handled it
  if (action == 1) { // Press
    bool handled = m_controller->handle_key_press(key, modifiers);
    logd("CanvasView: key press handled by controller={}", handled);
    return handled;
  } else if (action == 0) { // Release
    return m_controller->handle_key_release(key, modifiers);
  }

  return false;
}

// === Main Draw Method ===

void CanvasView::draw(NVGcontext *ctx) {
  // Cache the NVGcontext for use in other methods
  m_nvg_context = ctx;

  // Note: We don't clear m_frame_pictures here because canvas might still have references
  // Pictures are released (not deleted) and will be cleaned up by ThorVG::term()

  nanogui::Canvas::draw(ctx);

  // Draw in layers from back to front
  draw_grid(ctx);
  draw_guides(ctx);
  draw_strokes(ctx);
  draw_ddf_elements(ctx); // Draw DDF elements after regular strokes
  draw_current_stroke(ctx);
  draw_pending_shape_preview(ctx);
  draw_marquee(ctx);
  draw_selection(ctx);
  draw_snap_indicator(ctx);
  draw_rulers(ctx);
  draw_minimap(ctx);
  draw_coordinate_display(ctx);
  draw_snap_feedback(ctx);
}

// === Rendering Helpers ===

void CanvasView::draw_grid(NVGcontext *ctx) {
  if (!m_document || !m_document->get_grid_visible()) {
    return;
  }

  float grid_size = GRID_SIZE;
  if (grid_size <= 0.0f) {
    return; // Safety check
  }

  // Calculate visible canvas area
  nanogui::Vector2f top_left_canvas = local_to_canvas(nanogui::Vector2i(0, 0));
  nanogui::Vector2f bottom_right_canvas = local_to_canvas(m_size);

  float start_x = std::floor(top_left_canvas.x() / grid_size) * grid_size;
  float end_x = std::ceil(bottom_right_canvas.x() / grid_size) * grid_size;
  float start_y = std::floor(top_left_canvas.y() / grid_size) * grid_size;
  float end_y = std::ceil(bottom_right_canvas.y() / grid_size) * grid_size;

  // Limit grid lines to prevent infinite loops or excessive drawing
  const int MAX_LINES = 1000;
  int line_count = 0;

  nvgSave(ctx);
  nvgBeginPath(ctx);

  // Draw vertical lines
  for (float x = start_x; x <= end_x && line_count < MAX_LINES; x += grid_size) {
    nanogui::Vector2f top = canvas_to_global(nanogui::Vector2f(x, top_left_canvas.y()));
    nanogui::Vector2f bottom = canvas_to_global(nanogui::Vector2f(x, bottom_right_canvas.y()));
    nvgMoveTo(ctx, top.x(), top.y());
    nvgLineTo(ctx, bottom.x(), bottom.y());
    line_count++;
  }

  // Draw horizontal lines
  for (float y = start_y; y <= end_y && line_count < MAX_LINES; y += grid_size) {
    nanogui::Vector2f left = canvas_to_global(nanogui::Vector2f(top_left_canvas.x(), y));
    nanogui::Vector2f right = canvas_to_global(nanogui::Vector2f(bottom_right_canvas.x(), y));
    nvgMoveTo(ctx, left.x(), left.y());
    nvgLineTo(ctx, right.x(), right.y());
    line_count++;
  }

  // Use different grid color based on dark mode
  bool dark_mode = m_document->get_dark_mode();
  if (dark_mode) {
    nvgStrokeColor(ctx, nvgRGBA(80, 80, 80, 100)); // Light lines for dark mode
  } else {
    nvgStrokeColor(ctx, nvgRGBA(220, 220, 220, 100)); // Dark lines for light mode
  }
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

  // Draw permanent guides
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

    nvgStrokeColor(ctx, nvgRGBA(guide.color.r() * 255, guide.color.g() * 255, guide.color.b() * 255,
                                guide.color.w() * 255));
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);
  }

  // Draw temporary guide being created
  if (m_has_temp_guide) {
    nvgBeginPath(ctx);
    if (m_temp_guide_type == Guide::Horizontal) {
      nanogui::Vector2f screen_pos = canvas_to_global(nanogui::Vector2f(0, m_temp_guide_position));
      float y = screen_pos.y();
      nvgMoveTo(ctx, abs_pos.x(), y);
      nvgLineTo(ctx, abs_pos.x() + m_size.x(), y);
    } else {
      nanogui::Vector2f screen_pos = canvas_to_global(nanogui::Vector2f(m_temp_guide_position, 0));
      float x = screen_pos.x();
      nvgMoveTo(ctx, x, abs_pos.y());
      nvgLineTo(ctx, x, abs_pos.y() + m_size.y());
    }

    // Draw with brighter color and dashed pattern for temporary guide
    nvgStrokeColor(ctx, nvgRGBA(0, 150, 255, 200));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);
  }

  nvgRestore(ctx);
}

void CanvasView::draw_strokes(NVGcontext *ctx) {
  const auto &strokes = m_document->get_strokes();

  static int last_stroke_count = 0;
  if (strokes.size() != last_stroke_count) {
    logi("CanvasView::draw_strokes - Rendering {} strokes (changed from {})", strokes.size(),
         last_stroke_count);
    last_stroke_count = strokes.size();
  }

  logd("CanvasView: Drawing {} strokes", strokes.size());

  // Calculate visible viewport in canvas coordinates for culling
  nanogui::Vector2f viewport_min = local_to_canvas(nanogui::Vector2i(0, 0));
  nanogui::Vector2f viewport_max = local_to_canvas(m_size);

  // Add margin to account for stroke width and transforms
  float cull_margin = 100.0f / m_document->get_zoom();
  viewport_min.x() -= cull_margin;
  viewport_min.y() -= cull_margin;
  viewport_max.x() += cull_margin;
  viewport_max.y() += cull_margin;

  int stroke_index = 0;
  for (const auto &stroke : strokes) {
    if (!stroke.visible) {
      stroke_index++;
      continue;
    }

    // Viewport culling: skip strokes that are completely outside the viewport
    float stroke_min_x, stroke_min_y, stroke_max_x, stroke_max_y;
    stroke.get_bounds(stroke_min_x, stroke_min_y, stroke_max_x, stroke_max_y);

    // Check if stroke bounds intersect with viewport
    bool is_visible = !(stroke_max_x < viewport_min.x() || stroke_min_x > viewport_max.x() ||
                        stroke_max_y < viewport_min.y() || stroke_min_y > viewport_max.y());

    if (is_visible) {
      draw_stroke(ctx, stroke, stroke_index);
    }

    stroke_index++;
  }

}

void CanvasView::draw_stroke(NVGcontext *ctx, const Stroke &stroke, int stroke_index) {
  if (stroke.points.empty()) {
    logw("CanvasView: Stroke '{}' has no points, skipping", stroke.name);
    return;
  }

  float zoom = m_document->get_zoom();

  nvgSave(ctx);

  // Apply opacity (Task 12)
  nvgGlobalAlpha(ctx, stroke.opacity);

  // TODO: Implement dashed/dotted line rendering (Task 12)
  // NanoVG doesn't have built-in dash support, would need manual segment drawing
  // For now, stroke_style is stored but not rendered

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

    // Use rounded rectangle if corner_radius > 0 (Task 12)
    if (stroke.corner_radius > 0.0f) {
      nvgRoundedRect(ctx, top_left.x(), top_left.y(), w * zoom, h * zoom,
                     stroke.corner_radius * zoom);
    } else {
      nvgRect(ctx, top_left.x(), top_left.y(), w * zoom, h * zoom);
    }

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

  case Tool::SVGShape: {
    try {
      // Delegate to SVG rendering
      draw_svg_stroke(ctx, stroke, stroke_index);
    } catch (const std::exception &e) {
      loge("CanvasView: Exception drawing SVG '{}': {}", stroke.name, e.what());
      break;
    } catch (...) {
      loge("CanvasView: Unknown exception drawing SVG '{}'", stroke.name);
      break;
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
  // Reset alpha (Task 12)
  nvgGlobalAlpha(ctx, 1.0f);

  nvgRestore(ctx);
}

void CanvasView::draw_svg_stroke(NVGcontext *ctx, const Stroke &stroke, int stroke_index) {
  if (!m_svg_renderer) {
    logw("CanvasView: SVG renderer is null");
    return;
  }

  if (stroke.svg_data.empty()) {
    logw("CanvasView: SVG data is empty for stroke '{}'", stroke.name);
    return;
  }

  if (stroke.points.empty()) {
    logw("CanvasView: No position points for SVG stroke '{}'", stroke.name);
    return;
  }

  logd("CanvasView: Rendering SVG stroke '{}' ({} bytes) at ({}, {})", stroke.name,
       stroke.svg_data.size(), stroke.points[0].x, stroke.points[0].y);
  logd("CanvasView: Loading SVG document for stroke '{}'...", stroke.name);

  auto document = m_svg_renderer->load_svg(stroke.svg_data);

  if (!document) {
    loge("CanvasView: Failed to load SVG document for stroke '{}'", stroke.name);
    // Failed to load SVG - draw placeholder
    float min_x, min_y, max_x, max_y;
    stroke.get_bounds(min_x, min_y, max_x, max_y);
    nanogui::Vector2f top_left = canvas_to_global(nanogui::Vector2f(min_x, min_y));
    nanogui::Vector2f bottom_right = canvas_to_global(nanogui::Vector2f(max_x, max_y));

    nvgSave(ctx);
    nvgBeginPath(ctx);
    nvgRect(ctx, top_left.x(), top_left.y(), bottom_right.x() - top_left.x(),
            bottom_right.y() - top_left.y());
    nvgStrokeColor(ctx, nvgRGBA(255, 0, 0, 255));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);
    nvgRestore(ctx);
    return;
  }

  // Get position from first point
  nanogui::Vector2f canvas_pos = to_vec(stroke.points[0]);
  nanogui::Vector2f pos = canvas_to_local(canvas_pos); // Use local coords, not global!

  // Apply zoom to scale factors
  float zoom = m_document->get_zoom();
  float scale_x = stroke.svg_scale_x * zoom;
  float scale_y = stroke.svg_scale_y * zoom;

  // Debug logging (commented out for performance)
  // logd("CanvasView: Rendering SVG '{}' at ({}, {})", stroke.name, canvas_pos.x(),
  // canvas_pos.y());

  // Prepare color tint (use stroke color for tinting)
  NVGcolor tint = nvgRGBAf(stroke.color.r(), stroke.color.g(), stroke.color.b(), stroke.color.w());

  // Render the SVG with color tint (LunaSVG handles all memory automatically!)
  try {
    m_svg_renderer->render(ctx, document.get(), pos, scale_x, scale_y, stroke.rotation, &tint);
  } catch (const std::exception &e) {
    loge("CanvasView: Exception from SVG renderer: {}", e.what());
    return;
  } catch (...) {
    loge("CanvasView: Unknown exception from SVG renderer!");
    return;
  }

  // Manually render text parameters (LunaSVG doesn't render text well)
  // Use NanoGUI's UTF-8/CJK text rendering approach
  if (!stroke.svg_parameters.empty()) {
    nvgSave(ctx);

    // Apply same transform as SVG rendering
    nvgTranslate(ctx, pos.x(), pos.y());
    nvgScale(ctx, scale_x, scale_y);
    if (stroke.rotation != 0.0f) {
      nvgRotate(ctx, stroke.rotation);
    }

    // Get SVG dimensions (unscaled)
    double svg_width = document->width();
    double svg_height = document->height();

    // Parse SVG data to find text element positions
    // This is a simple approach - for production, consider using XML parser
    std::map<std::string, std::pair<float, float>> text_positions;

    // Structure to hold text rendering info
    struct TextInfo {
      float x, y;
      int align;
      float font_size;
      std::string font_face;
    };
    std::map<std::string, TextInfo> text_info;

    // Try to extract text positions and attributes from SVG
    // Format: <text id="paramName" x="150" y="50" text-anchor="middle" font-size="16" ...>

    // First, log the SVG data for debugging
    logd("SVG data for parsing ({} bytes): {}", stroke.svg_data.length(),
         stroke.svg_data.substr(0, std::min(size_t(500), stroke.svg_data.length())));

    size_t pos_search = 0;
    int text_count = 0;
    while ((pos_search = stroke.svg_data.find("<text", pos_search)) != std::string::npos) {
      text_count++;
      size_t end_tag = stroke.svg_data.find(">", pos_search);
      if (end_tag == std::string::npos)
        break;

      std::string text_tag = stroke.svg_data.substr(pos_search, end_tag - pos_search);
      std::string id;

      logd("Found <text> tag #{}: {}", text_count, text_tag);

      // Try to extract id attribute
      size_t id_pos = text_tag.find("id=\"");
      if (id_pos != std::string::npos) {
        id_pos += 4;
        size_t id_end = text_tag.find("\"", id_pos);
        id = text_tag.substr(id_pos, id_end - id_pos);
        logd("  Extracted id from attribute: '{}'", id);
      } else {
        logd("  No id attribute found, checking content...");
        // No id attribute - check if we can match by parameter name
        // Since placeholders are already replaced, we need to match against known parameters
        for (const auto &param : stroke.svg_parameters) {
          // Check if this text element might be for this parameter
          // We'll use position-based heuristics or just assign in order
          if (text_info.find(param.first) == text_info.end()) {
            id = param.first;
            logd("  Assigned id by parameter order: '{}'", id);
            break;
          }
        }
      }

      if (!id.empty()) {

        TextInfo info;
        info.align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP; // Default
        info.font_size = 14.0f;                      // Default
        info.font_face = "sans";                     // Default

        // Extract x and y positions
        size_t x_pos = text_tag.find("x=\"");
        if (x_pos != std::string::npos) {
          x_pos += 3;
          size_t x_end = text_tag.find("\"", x_pos);
          info.x = std::stof(text_tag.substr(x_pos, x_end - x_pos));
        }

        size_t y_pos = text_tag.find("y=\"");
        if (y_pos != std::string::npos) {
          y_pos += 3;
          size_t y_end = text_tag.find("\"", y_pos);
          info.y = std::stof(text_tag.substr(y_pos, y_end - y_pos));
        }

        // Extract text-anchor (alignment)
        size_t anchor_pos = text_tag.find("text-anchor=\"");
        if (anchor_pos != std::string::npos) {
          anchor_pos += 13;
          size_t anchor_end = text_tag.find("\"", anchor_pos);
          std::string anchor = text_tag.substr(anchor_pos, anchor_end - anchor_pos);

          if (anchor == "middle") {
            info.align = NVG_ALIGN_CENTER | NVG_ALIGN_TOP;
          } else if (anchor == "end") {
            info.align = NVG_ALIGN_RIGHT | NVG_ALIGN_TOP;
          } else {
            info.align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP;
          }
        }

        // Extract font-size
        size_t size_pos = text_tag.find("font-size=\"");
        if (size_pos != std::string::npos) {
          size_pos += 11;
          size_t size_end = text_tag.find("\"", size_pos);
          info.font_size = std::stof(text_tag.substr(size_pos, size_end - size_pos));
        }

        // Extract font-weight (for bold)
        size_t weight_pos = text_tag.find("font-weight=\"bold\"");
        if (weight_pos != std::string::npos) {
          info.font_face = "sans-bold";
        }

        // Extract font-family
        size_t family_pos = text_tag.find("font-family=\"");
        if (family_pos != std::string::npos) {
          family_pos += 13;
          size_t family_end = text_tag.find("\"", family_pos);
          std::string family = text_tag.substr(family_pos, family_end - family_pos);

          if (family.find("monospace") != std::string::npos) {
            info.font_face = "mono";
          } else if (family.find("serif") != std::string::npos) {
            info.font_face = "serif";
          }
          // Keep bold if already set
          if (weight_pos != std::string::npos) {
            info.font_face = "sans-bold";
          }
        }

        text_info[id] = info;
        logd("Extracted text info for '{}': x={}, y={}, size={}, face={}", id, info.x, info.y,
             info.font_size, info.font_face);
      }

      pos_search = end_tag;
    }

    // Render each text parameter using info from SVG template
    for (const auto &param : stroke.svg_parameters) {
      if (param.second.empty())
        continue;

      float text_x, text_y;
      int align;
      float font_size;
      std::string font_face;

      // Check if we have info from SVG template
      auto info_it = text_info.find(param.first);
      if (info_it != text_info.end()) {
        // Use exact position and attributes from SVG template
        const TextInfo &info = info_it->second;

        text_x = info.x;
        text_y = info.y;
        align = info.align;
        font_size = info.font_size;
        font_face = info.font_face;

        logd("Rendering '{}' from SVG template at ({}, {})", param.first, text_x, text_y);
      } else {
        // Fallback: Use smart defaults based on parameter name
        logw("No SVG template info for '{}', using defaults", param.first);

        if (param.first == "interfaceName" || param.first == "className") {
          text_x = svg_width / 2;
          text_y = 35.0f;
          align = NVG_ALIGN_CENTER | NVG_ALIGN_TOP;
          font_size = 16.0f;
          font_face = "sans-bold";
        } else if (param.first == "methods" || param.first == "attributes") {
          text_x = 10.0f;
          text_y = 65.0f;
          align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP;
          font_size = 12.0f;
          font_face = "mono";
        } else {
          // Generic fallback
          text_x = svg_width / 2;
          text_y = svg_height / 2;
          align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;
          font_size = 14.0f;
          font_face = "sans";
        }
      }

      // Render text with UTF-8/CJK support (in SVG coordinate space, not screen space)
      nvgFontSize(ctx, font_size);
      nvgFontFace(ctx, font_face.c_str());
      nvgFillColor(ctx, nvgRGB(0, 0, 0));
      nvgTextAlign(ctx, align);

      // Use nvgTextBox for multi-line support
      if (param.first == "methods" || param.first == "attributes") {
        // Multi-line text with wrapping
        float margin = 20.0f;
        float max_width = svg_width - 2 * margin;
        nvgTextBox(ctx, text_x, text_y, max_width, param.second.c_str(), nullptr);
      } else {
        // Single line text
        nvgText(ctx, text_x, text_y, param.second.c_str(), nullptr);
      }
    }

    nvgRestore(ctx);
  }
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

  // Check if selection contains grouped shapes
  bool has_grouped = false;
  int common_group_id = -1;
  bool all_same_group = true;

  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      int gid = strokes[idx].group_id;
      if (gid >= 0) {
        has_grouped = true;
        if (common_group_id == -1) {
          common_group_id = gid;
        } else if (common_group_id != gid) {
          all_same_group = false;
        }
      } else {
        all_same_group = false;
      }
    }
  }

  // Draw selection box
  nanogui::Vector2f top_left = canvas_to_global(nanogui::Vector2f(min_x, min_y));
  nanogui::Vector2f bottom_right = canvas_to_global(nanogui::Vector2f(max_x, max_y));

  nvgSave(ctx);

  // Use dashed line if all selected shapes are in the same group
  if (has_grouped && all_same_group && common_group_id >= 0) {
    // Draw dashed rectangle manually
    float dash_length = 8.0f;
    float gap_length = 4.0f;
    (void)dash_length; // Suppress unused warning - for future use
    (void)gap_length;  // Suppress unused warning - for future use

    nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);

    // Draw dashed lines for each side
    auto draw_dashed_line = [&](float x1, float y1, float x2, float y2) {
      float dx = x2 - x1;
      float dy = y2 - y1;
      float length = std::sqrt(dx * dx + dy * dy);
      float ux = dx / length;
      float uy = dy / length;

      float pos = 0.0f;
      while (pos < length) {
        float dash_end = std::min(pos + dash_length, length);
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, x1 + ux * pos, y1 + uy * pos);
        nvgLineTo(ctx, x1 + ux * dash_end, y1 + uy * dash_end);
        nvgStroke(ctx);
        pos += dash_length + gap_length;
      }
    };

    // Top, right, bottom, left
    draw_dashed_line(top_left.x(), top_left.y(), bottom_right.x(), top_left.y());
    draw_dashed_line(bottom_right.x(), top_left.y(), bottom_right.x(), bottom_right.y());
    draw_dashed_line(bottom_right.x(), bottom_right.y(), top_left.x(), bottom_right.y());
    draw_dashed_line(top_left.x(), bottom_right.y(), top_left.x(), top_left.y());
  } else {
    // Draw solid rectangle
    nvgBeginPath(ctx);
    nvgRect(ctx, top_left.x(), top_left.y(), bottom_right.x() - top_left.x(),
            bottom_right.y() - top_left.y());
    nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);
  }

  // Draw corner resize handles (8 handles: 4 corners + 4 edges)
  const float handle_size = 8.0f;

  // Corner handles
  nanogui::Vector2f corners[4] = {
      canvas_to_global(nanogui::Vector2f(min_x, min_y)), // Top-left
      canvas_to_global(nanogui::Vector2f(max_x, min_y)), // Top-right
      canvas_to_global(nanogui::Vector2f(max_x, max_y)), // Bottom-right
      canvas_to_global(nanogui::Vector2f(min_x, max_y))  // Bottom-left
  };

  // Edge handles
  nanogui::Vector2f edges[4] = {
      canvas_to_global(nanogui::Vector2f((min_x + max_x) / 2, min_y)), // Top
      canvas_to_global(nanogui::Vector2f(max_x, (min_y + max_y) / 2)), // Right
      canvas_to_global(nanogui::Vector2f((min_x + max_x) / 2, max_y)), // Bottom
      canvas_to_global(nanogui::Vector2f(min_x, (min_y + max_y) / 2))  // Left
  };

  // Draw all handles
  for (int i = 0; i < 4; i++) {
    // Corner handles
    nvgBeginPath(ctx);
    nvgRect(ctx, corners[i].x() - handle_size / 2, corners[i].y() - handle_size / 2, handle_size,
            handle_size);
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgFill(ctx);
    nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);

    // Edge handles
    nvgBeginPath(ctx);
    nvgRect(ctx, edges[i].x() - handle_size / 2, edges[i].y() - handle_size / 2, handle_size,
            handle_size);
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgFill(ctx);
    nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);
  }

  // Draw rotation handle (30px above selection box)
  float center_x = (min_x + max_x) / 2.0f;
  float handle_y = min_y - 30.0f;
  nanogui::Vector2f handle_pos = canvas_to_global(nanogui::Vector2f(center_x, handle_y));
  nanogui::Vector2f top_center = canvas_to_global(nanogui::Vector2f(center_x, min_y));

  // Draw line from top of selection to handle
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, top_center.x(), top_center.y());
  nvgLineTo(ctx, handle_pos.x(), handle_pos.y());
  nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);

  // Draw circular rotation handle
  nvgBeginPath(ctx);
  nvgCircle(ctx, handle_pos.x(), handle_pos.y(), 6.0f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  nvgFill(ctx);
  nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255));
  nvgStrokeWidth(ctx, 2.0f);
  nvgStroke(ctx);

  nvgRestore(ctx);
}

void CanvasView::draw_current_stroke(NVGcontext *ctx) {
  if (!m_has_current_stroke || m_current_stroke.points.empty()) {
    return;
  }

  // Use -1 as stroke index for current stroke (not yet in document)
  draw_stroke(ctx, m_current_stroke, -1);
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

bool CanvasView::is_clicking_rotation_handle(const nanogui::Vector2i &p) const {
  const auto &selected = m_document->get_selected_indices();
  if (selected.empty()) {
    return false;
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
    return false;
  }

  // Calculate rotation handle position
  float center_x = (min_x + max_x) / 2.0f;
  float handle_y = min_y - 30.0f;
  nanogui::Vector2f handle_pos = canvas_to_global(nanogui::Vector2f(center_x, handle_y));

  // Check if mouse is within handle circle (radius 6px)
  float dx = p.x() - handle_pos.x();
  float dy = p.y() - handle_pos.y();
  float distance = std::sqrt(dx * dx + dy * dy);

  return distance <= 6.0f;
}

void CanvasView::draw_snap_indicator(NVGcontext *ctx) {
  if (!m_has_snap_point || !m_document->get_snap_enabled()) {
    return;
  }

  nanogui::Vector2f global_pos = canvas_to_global(m_snap_point);

  nvgSave(ctx);

  // Draw a bright circle at the snap point
  nvgBeginPath(ctx);
  nvgCircle(ctx, global_pos.x(), global_pos.y(), 4.0f);
  nvgFillColor(ctx, nvgRGBA(255, 100, 0, 200)); // Bright orange
  nvgFill(ctx);

  // Draw outer ring
  nvgBeginPath(ctx);
  nvgCircle(ctx, global_pos.x(), global_pos.y(), 6.0f);
  nvgStrokeColor(ctx, nvgRGBA(255, 255, 255, 255)); // White outline
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);

  nvgRestore(ctx);
}

// === Image Import ===

bool CanvasView::is_image_file(const std::string &file_path) {
  // Convert to lowercase for case-insensitive comparison
  std::string lower_path = file_path;
  std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);

  // Helper lambda to check if string ends with suffix
  auto ends_with = [](const std::string &str, const std::string &suffix) {
    if (suffix.length() > str.length())
      return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
  };

  return ends_with(lower_path, ".png") || ends_with(lower_path, ".jpg") ||
         ends_with(lower_path, ".jpeg") || ends_with(lower_path, ".bmp") ||
         ends_with(lower_path, ".gif");
}

bool CanvasView::is_svg_file(const std::string &file_path) {
  std::string lower_path = file_path;
  std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);

  auto ends_with = [](const std::string &str, const std::string &suffix) {
    if (suffix.length() > str.length())
      return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
  };

  return ends_with(lower_path, ".svg");
}

bool CanvasView::import_image(const std::string &file_path, const nanogui::Vector2f &pos) {
  // Use cached NVGcontext from draw calls
  if (!m_nvg_context) {
    logw("CanvasView: Cannot import image - NVGcontext not available yet");
    return false;
  }

  NVGcontext *ctx = m_nvg_context;

  // Load image using NanoVG
  int image_handle = nvgCreateImage(ctx, file_path.c_str(), 0);

  if (image_handle == 0) {
    logw("CanvasView: Failed to load image: {}", file_path);
    return false;
  }

  // Get image dimensions
  int img_width, img_height;
  nvgImageSize(ctx, image_handle, &img_width, &img_height);

  // Scale to fit within 800x600 while maintaining aspect ratio
  float max_width = 800.0f;
  float max_height = 600.0f;
  float scale = 1.0f;

  if (img_width > max_width || img_height > max_height) {
    float scale_x = max_width / img_width;
    float scale_y = max_height / img_height;
    scale = std::min(scale_x, scale_y);
  }

  float scaled_width = img_width * scale;
  float scaled_height = img_height * scale;

  // Create stroke for image
  Stroke image_stroke;
  image_stroke.tool = Tool::Image;
  image_stroke.nvg_image_handle = image_handle;
  image_stroke.file_path = file_path;
  image_stroke.image_width = scaled_width;
  image_stroke.image_height = scaled_height;

  // Set position (top-left corner)
  image_stroke.points.push_back(Point(pos.x(), pos.y()));
  image_stroke.points.push_back(Point(pos.x() + scaled_width, pos.y() + scaled_height));

  // Add to document
  m_document->add_stroke(image_stroke);

  // Select the new image
  int new_index = static_cast<int>(m_document->get_strokes().size()) - 1;
  m_document->set_selection({new_index});

  logi("CanvasView: Imported image: {} ({}x{} scaled to {}x{})", file_path, img_width, img_height,
       scaled_width, scaled_height);

  return true;
}

bool CanvasView::import_svg_shape(const std::string &file_path, const nanogui::Vector2f &pos) {
  // Read SVG file
  std::ifstream file(file_path);
  if (!file.is_open()) {
    logw("CanvasView: Failed to open SVG file: {}", file_path);
    return false;
  }

  std::string svg_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();

  if (svg_data.empty()) {
    logw("CanvasView: SVG file is empty: {}", file_path);
    return false;
  }

  // Create stroke for SVG shape
  Stroke svg_stroke;
  svg_stroke.tool = Tool::SVGShape;
  svg_stroke.svg_data = svg_data;
  svg_stroke.svg_shape_id = "imported";
  svg_stroke.svg_scale_x = 1.0f;
  svg_stroke.svg_scale_y = 1.0f;
  svg_stroke.color = m_document->get_stroke_color();
  svg_stroke.width = m_document->get_stroke_width();

  // Set position
  svg_stroke.points.push_back(Point(pos.x(), pos.y()));

  // Add to document
  m_document->add_stroke(svg_stroke);

  // Select the new shape
  int new_index = static_cast<int>(m_document->get_strokes().size()) - 1;
  m_document->set_selection({new_index});

  logi("CanvasView: Imported SVG shape: {}", file_path);

  return true;
}

void CanvasView::draw_pending_shape_preview(NVGcontext *ctx) {
  // Check if there's a pending SVG shape to preview
  if (!m_document || m_document->get_pending_svg_shape().empty() || !m_has_cursor_pos) {
    return;
  }

  // Get the shape library from controller
  if (!m_controller) {
    return;
  }

  SVGShapeLibrary *library = m_controller->get_shape_library();
  if (!library || !m_svg_renderer) {
    return;
  }

  std::string shape_id = m_document->get_pending_svg_shape();

  // Create a temporary preview stroke at cursor position
  Point cursor_point(m_last_cursor_pos.x(), m_last_cursor_pos.y());
  Stroke preview_stroke = library->create_shape(shape_id, cursor_point);

  if (preview_stroke.svg_data.empty()) {
    // Fallback to circle if shape creation failed
    nanogui::Vector2f screen_pos = canvas_to_global(m_last_cursor_pos);
    nvgSave(ctx);
    nvgBeginPath(ctx);
    nvgCircle(ctx, screen_pos.x(), screen_pos.y(), 50.0f);
    nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 128));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);
    nvgRestore(ctx);
    return;
  }

  // Scale down the preview (50% of original size)
  float preview_scale = 0.5f;
  preview_stroke.svg_scale_x *= preview_scale;
  preview_stroke.svg_scale_y *= preview_scale;

  // Make it semi-transparent
  preview_stroke.color = nanogui::Color(0.0f, 0.47f, 0.84f, 0.6f); // Blue with 60% opacity

  // Draw the actual SVG shape preview
  nvgSave(ctx);

  // Apply canvas transformations
  float zoom = m_document->get_zoom();
  nanogui::Vector2f pan = m_document->get_pan_offset();

  nvgTranslate(ctx, pan.x(), pan.y());
  nvgScale(ctx, zoom, zoom);

  // Draw the preview stroke
  draw_svg_stroke(ctx, preview_stroke, -1);

  nvgRestore(ctx);
}

bool CanvasView::update_text_bounds_cache(int stroke_index) const {
  const auto &strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(strokes.size())) {
    return false;
  }

  const Stroke &stroke = strokes[stroke_index];
  if (stroke.tool != Tool::SVGShape || stroke.svg_data.empty()) {
    return false;
  }

  // Check if cache is valid (SVG data hasn't changed)
  auto cached_data_it = m_cached_svg_data.find(stroke_index);
  if (cached_data_it != m_cached_svg_data.end() && cached_data_it->second == stroke.svg_data) {
    // Cache is still valid
    return true;
  }

  // Cache is invalid or doesn't exist, rebuild it
  logd("CanvasView: Rebuilding text bounds cache for stroke {}", stroke_index);
  if (stroke.points.empty()) {
    return false;
  }

  nanogui::Vector2f shape_pos(stroke.points[0].x, stroke.points[0].y);
  // Store bounds in canvas coordinates (without zoom) for zoom-independent caching
  float scale_x = stroke.svg_scale_x;
  float scale_y = stroke.svg_scale_y;

  // Parse SVG to get DOM tree
  auto document = m_svg_renderer->load_svg(stroke.svg_data);
  if (!document) {
    return false;
  }

  // Query all text elements in the SVG
  auto text_elements = document->querySelectorAll("text");

  logd("  Found {} text elements", text_elements.size());
  std::vector<TextBoundsInfo> bounds_list;

  for (const auto &element : text_elements) {
    // Get the element's ID (which should match the parameter name)
    std::string element_id;
    if (element.hasAttribute("id")) {
      element_id = element.getAttribute("id");
    }

    // Skip if this element doesn't correspond to a known parameter
    if (element_id.empty() ||
        stroke.svg_parameters.find(element_id) == stroke.svg_parameters.end()) {
      continue;
    }

    // Extract text-anchor attribute (Task 21.1, 21.2, 21.3)
    std::string text_anchor = "start"; // Default value
    if (element.hasAttribute("text-anchor")) {
      text_anchor = element.getAttribute("text-anchor");
    }

    // Get the bounding box of the text element (Task 20.1, 20.2)
    // getGlobalBoundingBox() already accounts for all transforms in the element hierarchy
    // including rotate, scale, and translate transforms within the SVG
    lunasvg::Box bbox = element.getGlobalBoundingBox();

    logd("    Text element '{}' bbox from SVG: x={}, y={}, w={}, h={}", element_id, bbox.x, bbox.y,
         bbox.w, bbox.h);
    // This handles all transform types: translate, scale, and rotate
    //
    // Step 1: SVG-internal transforms (already applied by getGlobalBoundingBox)
    //   - Text element's own transform attribute (rotate, scale, translate)
    //   - Parent group transforms
    //   - Any other SVG transforms in the hierarchy
    //
    // Step 2: Apply stroke's scale factors
    float scaled_x = bbox.x * scale_x;
    float scaled_y = bbox.y * scale_y;
    float scaled_w = bbox.w * scale_x;
    float scaled_h = bbox.h * scale_y;

    // Step 3: Apply stroke's rotation around the shape center
    // If the stroke has rotation, we need to rotate the text bounds
    if (std::abs(stroke.rotation) > 0.001f) {
      // Calculate the center of the SVG shape
      float shape_center_x = stroke.svg_width * scale_x / 2.0f;
      float shape_center_y = stroke.svg_height * scale_y / 2.0f;

      // Get the four corners of the text bounding box (relative to shape origin)
      float corners_x[4] = {scaled_x, scaled_x + scaled_w, scaled_x + scaled_w, scaled_x};
      float corners_y[4] = {scaled_y, scaled_y, scaled_y + scaled_h, scaled_y + scaled_h};

      // Rotate each corner around the shape center
      float cos_r = std::cos(stroke.rotation);
      float sin_r = std::sin(stroke.rotation);

      float min_x = std::numeric_limits<float>::max();
      float min_y = std::numeric_limits<float>::max();
      float max_x = std::numeric_limits<float>::lowest();
      float max_y = std::numeric_limits<float>::lowest();

      for (int i = 0; i < 4; i++) {
        // Translate to origin (shape center is pivot)
        float tx = corners_x[i] - shape_center_x;
        float ty = corners_y[i] - shape_center_y;

        // Rotate
        float rx = tx * cos_r - ty * sin_r;
        float ry = tx * sin_r + ty * cos_r;

        // Translate back
        float final_x = rx + shape_center_x;
        float final_y = ry + shape_center_y;

        // Update bounds
        min_x = std::min(min_x, final_x);
        min_y = std::min(min_y, final_y);
        max_x = std::max(max_x, final_x);
        max_y = std::max(max_y, final_y);
      }

      // Use the rotated bounding box
      scaled_x = min_x;
      scaled_y = min_y;
      scaled_w = max_x - min_x;
      scaled_h = max_y - min_y;

      logd("    Applied rotation {} rad: bbox ({}, {}, {}, {})", stroke.rotation, scaled_x,
           scaled_y, scaled_w, scaled_h);
    }

    // Step 4: Translate to canvas position
    TextBoundsInfo info;
    info.parameter_name = element_id;
    info.x = shape_pos.x() + scaled_x;
    info.y = shape_pos.y() + scaled_y;
    info.width = scaled_w;
    info.height = scaled_h;
    info.text_anchor = text_anchor;

    // Note: The bounding box from LunaSVG already represents the actual rendered bounds
    // of the text, which already accounts for text-anchor positioning.
    // We store the anchor for reference, but the bounds are already correct.
    // (Task 21.1, 21.2, 21.3, 21.4)

    bounds_list.push_back(info);

    logd("    Cached '{}': x={}, y={}, w={}, h={}, anchor={}", info.parameter_name, info.x, info.y,
         info.width, info.height, info.text_anchor);
  }

  // Store in cache
  m_text_bounds_cache[stroke_index] = bounds_list;
  m_cached_svg_data[stroke_index] = stroke.svg_data;

  logd("  Cache updated with {} text elements", bounds_list.size());
  return true;
}

std::string CanvasView::get_text_parameter_at_position(int stroke_index,
                                                       const nanogui::Vector2f &canvas_pos) {
  const auto &strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(strokes.size())) {
    return "";
  }

  const Stroke &stroke = strokes[stroke_index];
  if (stroke.tool != Tool::SVGShape || stroke.svg_data.empty()) {
    return "";
  }

  // Update cache if needed
  if (!update_text_bounds_cache(stroke_index)) {
    return "";
  }

  // Add 5px tolerance for hit detection (Requirement 6.6)
  const float HIT_TOLERANCE = 5.0f;

  // Use cached bounds for hit detection
  auto cache_it = m_text_bounds_cache.find(stroke_index);
  if (cache_it == m_text_bounds_cache.end()) {
    return "";
  }

  const auto &bounds_list = cache_it->second;

  logd("CanvasView: Checking {} cached text bounds for stroke {}", bounds_list.size(),
       stroke_index);
  logd("  Click position: ({}, {})", canvas_pos.x(), canvas_pos.y());
  for (const auto &info : bounds_list) {
    logd("  Checking '{}': x={}, y={}, w={}, h={}", info.parameter_name, info.x, info.y, info.width,
         info.height);
    if (canvas_pos.x() >= info.x - HIT_TOLERANCE &&
        canvas_pos.x() <= info.x + info.width + HIT_TOLERANCE &&
        canvas_pos.y() >= info.y - HIT_TOLERANCE &&
        canvas_pos.y() <= info.y + info.height + HIT_TOLERANCE) {
      logi("  Click is within '{}' text area (cached DOM-based detection)", info.parameter_name);
      return info.parameter_name;
    }
  }

  logd("  No text element matched click position");
  if (!stroke.svg_parameters.empty()) {
    const auto &param = stroke.svg_parameters.begin()->first;
    logd("  Fallback to first parameter: '{}'", param);
    return param;
  }

  return "";
}

// === DDF Integration ===

void CanvasView::set_ddf_document(std::shared_ptr<ddf::DDFDocument> document) {
  m_ddf_document = document;

  // Pass shape library to DDF document
  if (m_ddf_document && m_svg_shape_library) {
    m_ddf_document->set_svg_shape_library(m_svg_shape_library);
  }

  // Trigger redraw
  if (screen()) {
    screen()->redraw();
  }
}

void CanvasView::set_svg_shape_library(SVGShapeLibrary *library) {
  m_svg_shape_library = library;

  // If DDF document already set, pass library to it
  if (m_ddf_document) {
    m_ddf_document->set_svg_shape_library(library);
  }
}

void CanvasView::draw_ddf_elements(NVGcontext *ctx) {
  if (!m_ddf_document || !m_document) {
    return;
  }

  nvgSave(ctx);

  // Apply canvas transformations (zoom and pan)
  float zoom = m_document->get_zoom();
  nanogui::Vector2f pan = m_document->get_pan_offset();

  nvgTranslate(ctx, pan.x(), pan.y());
  nvgScale(ctx, zoom, zoom);

  // Calculate viewport for culling
  nanogui::Vector2f top_left_canvas = local_to_canvas(nanogui::Vector2i(0, 0));
  nanogui::Vector2f bottom_right_canvas = local_to_canvas(m_size);

  float viewport_width = bottom_right_canvas.x() - top_left_canvas.x();
  float viewport_height = bottom_right_canvas.y() - top_left_canvas.y();

  // Render DDF document
  m_ddf_document->render(ctx, top_left_canvas.x(), top_left_canvas.y(), viewport_width,
                         viewport_height);

  nvgRestore(ctx);
}

Stroke CanvasView::convert_ddf_shape_to_stroke(const ddf::Shape *shape) {
  Stroke stroke;

  if (!shape)
    return stroke;

  // Convert based on shape type
  if (shape->type == "rect") {
    stroke.tool = Tool::Rectangle;
    if (shape->geometry.count("x") && shape->geometry.count("y")) {
      float x = shape->geometry.at("x");
      float y = shape->geometry.at("y");
      float w = shape->geometry.count("width") ? shape->geometry.at("width") : 100.0f;
      float h = shape->geometry.count("height") ? shape->geometry.at("height") : 50.0f;

      stroke.points.push_back(Point(x, y));
      stroke.points.push_back(Point(x + w, y + h));

      if (shape->geometry.count("rx")) {
        stroke.corner_radius = shape->geometry.at("rx");
      }
    }
  } else if (shape->type == "ellipse" || shape->type == "circle") {
    stroke.tool = Tool::Circle;
    if (shape->geometry.count("cx") && shape->geometry.count("cy")) {
      float cx = shape->geometry.at("cx");
      float cy = shape->geometry.at("cy");
      float rx = shape->geometry.count("rx")  ? shape->geometry.at("rx")
                 : shape->geometry.count("r") ? shape->geometry.at("r")
                                              : 50.0f;

      stroke.points.push_back(Point(cx, cy));
      stroke.points.push_back(Point(cx + rx, cy));
    }
  } else if (shape->type == "text") {
    stroke.tool = Tool::Text;
    if (shape->geometry.count("x") && shape->geometry.count("y")) {
      stroke.points.push_back(Point(shape->geometry.at("x"), shape->geometry.at("y")));
    }
  } else if (shape->type == "svg") {
    // Convert SVG shape to SVGShape tool with SVG data
    stroke.tool = Tool::SVGShape;
    logi("Converting DDF SVG shape '{}' to editable stroke", shape->svg_shape_id);

    if (shape->geometry.count("x") && shape->geometry.count("y")) {
      float x = shape->geometry.at("x");
      float y = shape->geometry.at("y");
      float w = shape->geometry.count("width") ? shape->geometry.at("width") : 80.0f;
      float h = shape->geometry.count("height") ? shape->geometry.at("height") : 80.0f;

      // Position is just the top-left point
      stroke.points.push_back(Point(x, y));

      // Store SVG shape ID and parameters
      stroke.svg_shape_id = shape->svg_shape_id;
      stroke.svg_parameters = shape->svg_parameters;

      // Generate SVG data from library or use inline data
      if (!shape->svg_data.empty()) {
        stroke.svg_data = shape->svg_data;
        logi("Using inline SVG data ({} bytes)", stroke.svg_data.size());
      } else if (!shape->svg_shape_id.empty() && m_svg_shape_library) {
        stroke.svg_data =
            m_svg_shape_library->generate_svg(shape->svg_shape_id, shape->svg_parameters);
        logi("Generated SVG from library: {} ({} bytes)", shape->svg_shape_id,
             stroke.svg_data.size());
      }

      // Parse SVG to get actual dimensions
      if (!stroke.svg_data.empty()) {
        auto svg_doc = lunasvg::Document::loadFromData(stroke.svg_data);
        if (svg_doc) {
          stroke.svg_width = svg_doc->width();
          stroke.svg_height = svg_doc->height();
          // Calculate scale factors based on desired vs actual size
          stroke.svg_scale_x = w / stroke.svg_width;
          stroke.svg_scale_y = h / stroke.svg_height;
          logi("SVG dimensions: {}x{}, scale: {}x{}", stroke.svg_width, stroke.svg_height,
               stroke.svg_scale_x, stroke.svg_scale_y);
        } else {
          logw("Failed to parse SVG for dimensions");
          stroke.svg_width = w;
          stroke.svg_height = h;
          stroke.svg_scale_x = 1.0f;
          stroke.svg_scale_y = 1.0f;
        }
      } else {
        logw("No SVG data available for shape '{}'", shape->svg_shape_id);
      }
    }
  }

  // Copy styling
  if (shape->inline_style.count("fill")) {
    auto fill_str = shape->inline_style.at("fill");
    if (fill_str != "none") {
      stroke.fill_color = hex_to_color(fill_str);
      stroke.fill_style = FillStyle::Solid;
    } else {
      stroke.fill_style = FillStyle::None;
    }
  }

  if (shape->inline_style.count("stroke")) {
    stroke.color = hex_to_color(shape->inline_style.at("stroke"));
  }

  if (shape->inline_style.count("stroke-width")) {
    stroke.width = std::stof(shape->inline_style.at("stroke-width"));
  }

  // Copy text
  stroke.name = shape->text;

  return stroke;
}

nanogui::Color CanvasView::hex_to_color(const std::string &hex) {
  if (hex.empty() || hex[0] != '#' || hex.size() < 7) {
    return nanogui::Color(0, 0, 0, 255);
  }

  int r = std::stoi(hex.substr(1, 2), nullptr, 16);
  int g = std::stoi(hex.substr(3, 2), nullptr, 16);
  int b = std::stoi(hex.substr(5, 2), nullptr, 16);

  return nanogui::Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

bool CanvasView::handle_ddf_mouse_event(const nanogui::Vector2f &canvas_pos,
                                        const std::string &event_type) {
  if (!m_ddf_document) {
    return false;
  }

  // Find which DDF shape is at the mouse position
  auto &shape_layer = m_ddf_document->shape_layer();
  auto shapes = shape_layer.get_all_shapes();

  for (auto *shape : shapes) {
    if (!shape) {
      continue;
    }

    bool hit = false;

    // Hit test based on shape type
    if (shape->type == "rect") {
      // Rectangle hit test
      if (shape->geometry.count("x") && shape->geometry.count("y") &&
          shape->geometry.count("width") && shape->geometry.count("height")) {
        float x = shape->geometry.at("x");
        float y = shape->geometry.at("y");
        float w = shape->geometry.at("width");
        float h = shape->geometry.at("height");

        hit = (canvas_pos.x() >= x && canvas_pos.x() <= x + w && canvas_pos.y() >= y &&
               canvas_pos.y() <= y + h);
      }
    } else if (shape->type == "ellipse" || shape->type == "circle") {
      // Ellipse hit test
      if (shape->geometry.count("cx") && shape->geometry.count("cy")) {
        float cx = shape->geometry.at("cx");
        float cy = shape->geometry.at("cy");
        float rx = shape->geometry.count("rx")  ? shape->geometry.at("rx")
                   : shape->geometry.count("r") ? shape->geometry.at("r")
                                                : 50.0f;
        float ry = shape->geometry.count("ry") ? shape->geometry.at("ry") : rx;

        // Ellipse equation: (x-cx)²/rx² + (y-cy)²/ry² <= 1
        float dx = canvas_pos.x() - cx;
        float dy = canvas_pos.y() - cy;
        hit = ((dx * dx) / (rx * rx) + (dy * dy) / (ry * ry)) <= 1.0f;
      }
    } else if (shape->type == "path") {
      // For paths, use bounding box approximation
      // TODO: Implement proper path hit testing
      if (shape->inline_style.count("d")) {
        // Parse path to get bounds - for now, use a simple heuristic
        // The diamond paths in flowchart are roughly 120x80 centered at 200,260
        // This is a hack - proper implementation would parse the path
        float cx = 200.0f; // Approximate center
        float cy = 260.0f;
        float w = 120.0f;
        float h = 80.0f;

        hit = (canvas_pos.x() >= cx - w / 2 && canvas_pos.x() <= cx + w / 2 &&
               canvas_pos.y() >= cy - h / 2 && canvas_pos.y() <= cy + h / 2);
      }
    } else if (shape->type == "text") {
      // Text hit test - use approximate bounds
      if (shape->geometry.count("x") && shape->geometry.count("y")) {
        float x = shape->geometry.at("x");
        float y = shape->geometry.at("y");
        float w = 100.0f; // Approximate text width
        float h = 20.0f;  // Approximate text height

        hit = (canvas_pos.x() >= x - w / 2 && canvas_pos.x() <= x + w / 2 &&
               canvas_pos.y() >= y - h / 2 && canvas_pos.y() <= y + h / 2);
      }
    } else if (shape->type == "svg") {
      // SVG shape hit test - use bounding box
      if (shape->geometry.count("x") && shape->geometry.count("y")) {
        float x = shape->geometry.at("x");
        float y = shape->geometry.at("y");
        float w = shape->geometry.count("width") ? shape->geometry.at("width") : 80.0f;
        float h = shape->geometry.count("height") ? shape->geometry.at("height") : 80.0f;

        hit = (canvas_pos.x() >= x && canvas_pos.x() <= x + w && canvas_pos.y() >= y &&
               canvas_pos.y() <= y + h);
      }
    }

    if (hit) {
      // Found a shape at this position
      logi("DDF: Mouse {} on shape {}", event_type, shape->id);

      // Trigger DDF events for this shape
      auto &event_layer = m_ddf_document->event_layer();

      // Convert event type string to EventTrigger enum
      ddf::EventTrigger trigger;
      if (event_type == "click") {
        trigger = ddf::EventTrigger::Click;
      } else if (event_type == "hover") {
        trigger = ddf::EventTrigger::Hover;
      } else if (event_type == "hover_end") {
        trigger = ddf::EventTrigger::HoverEnd;
      } else {
        continue;
      }

      // Handle the event
      event_layer.handle_event(shape->id, trigger);

      // On click, convert DDF shape to editable stroke
      if (event_type == "click" && m_document) {
        logi("DDF: Converting shape {} to editable stroke", shape->id);
        Stroke stroke = convert_ddf_shape_to_stroke(shape);
        m_document->add_stroke(stroke);

        // Select the new stroke
        int stroke_index = static_cast<int>(m_document->get_strokes().size()) - 1;
        m_document->set_selection({stroke_index});

        // Remove the clicked shape from DDF document so it doesn't render twice
        m_ddf_document->shape_layer().remove_shape(shape->id);

        logi("DDF: Shape converted to stroke index {}, removed from DDF", stroke_index);
      }

      return true; // Event was handled
    }
  }

  return false; // No DDF element at this position
}

void CanvasView::update_ddf_pseudo_states(const nanogui::Vector2f &canvas_pos) {
  if (!m_ddf_document) {
    return;
  }

  auto &shape_layer = m_ddf_document->shape_layer();
  auto &style_layer = m_ddf_document->style_layer();
  auto shapes = shape_layer.get_all_shapes();

  // Track which shape is currently hovered
  std::string hovered_shape_id;

  for (auto *shape : shapes) {
    if (!shape) {
      continue;
    }

    bool is_hovered = false;

    // Simple bounding box hit test
    if (shape->geometry.count("x") && shape->geometry.count("y") &&
        shape->geometry.count("width") && shape->geometry.count("height")) {
      float x = shape->geometry["x"];
      float y = shape->geometry["y"];
      float w = shape->geometry["width"];
      float h = shape->geometry["height"];

      if (canvas_pos.x() >= x && canvas_pos.x() <= x + w && canvas_pos.y() >= y &&
          canvas_pos.y() <= y + h) {
        is_hovered = true;
        hovered_shape_id = shape->id;
      }
    }

    // Update hover pseudo-state
    bool was_hovered = shape->pseudo_states.count("hover") > 0;
    if (is_hovered && !was_hovered) {
      // Mouse entered
      style_layer.update_pseudo_state(shape->id, "hover", true);
    } else if (!is_hovered && was_hovered) {
      // Mouse left
      style_layer.update_pseudo_state(shape->id, "hover", false);
    }
  }

  // Update selected pseudo-state based on document selection
  if (m_document) {
    const auto &selected_indices = m_document->get_selected_indices();
    const auto &strokes = m_document->get_strokes();

    for (auto *shape : shapes) {
      if (!shape) {
        continue;
      }

      // Check if any selected stroke is linked to this DDF shape
      bool is_selected = false;
      for (int idx : selected_indices) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          const auto &stroke = strokes[idx];
          if (stroke.ddf_shape_id == shape->id) {
            is_selected = true;
            break;
          }
        }
      }

      // Update selected pseudo-state
      bool was_selected = shape->pseudo_states.count("selected") > 0;
      if (is_selected != was_selected) {
        style_layer.update_pseudo_state(shape->id, "selected", is_selected);
        logi("DDF: Shape {} selected state: {}", shape->id, is_selected);
      }
    }
  }
}

} // namespace whiteboard
