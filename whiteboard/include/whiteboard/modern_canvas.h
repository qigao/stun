/**
 * \file modern_canvas.h
 * \brief Custom canvas widget that powers the modern whiteboard drawing surface.
 */

#pragma once

#include "whiteboard/common.h"

namespace whiteboard {


/**
 * \class ModernCanvas
 * \brief Interactive canvas supporting multi-tool drawing, guides, and snapping.
 *
 * The canvas extends NanoGUI's `Canvas` to add whiteboard-specific behaviors,
 * including infinite grid rendering, selection handles, shape snapping, and
 * gesture-based panning/zooming. It exposes callbacks so higher-level UI
 * elements can react to stroke or selection changes.
 */
class ModernCanvas : public Canvas {
public:
  ModernCanvas(Widget *parent) : Canvas(parent, 1, false, false, false) {
    set_draw_border(false);
    m_current_color = Color(255, 100, 100, 255);
    m_current_width = 3.0f;
    m_current_tool = Tool::Pen;
    m_fill_style = FillStyle::Solid;
    m_fill_color = Color(255, 182, 193, 255);
    m_zoom = 1.0f;
    m_pan_offset = Vector2f(0.f, 0.f);
    m_is_panning = false;
    m_selection_mode = false;
    m_is_moving_shape = false;
    m_selection_changed_callback = nullptr;
  }

  /**
   * \brief Registers a callback fired whenever the active selection changes.
   */
  void set_selection_changed_callback(std::function<void()> callback) {
    m_selection_changed_callback = callback;
  }

  /**
   * \brief Registers a callback invoked after stroke additions or edits.
   */
  void set_strokes_changed_callback(std::function<void()> callback) {
    m_strokes_changed_callback = callback;
  }

  /**
   * \brief Renders the canvas surface, strokes, guides, and selection affordances.
   */
  virtual void draw(NVGcontext *ctx) override {
    Canvas::draw(ctx);

    // Update snap feedback timer
    if (m_snap_feedback_timer > 0.0f) {
      m_snap_feedback_timer -= 0.016f; // Approximate frame time
      if (m_snap_feedback_timer <= 0.0f) {
        m_show_snap_feedback = false;
      }
    }

    // Draw infinite grid
    if (m_show_grid) {
      draw_infinite_grid(ctx);
    }

    // Draw guides (before strokes so they're behind)
    draw_guides(ctx);

    // Draw all strokes (skip invisible ones)
    for (const auto &stroke : m_strokes) {
      if (stroke.visible) {
        draw_stroke(ctx, stroke);
      }
    }

    // Draw current stroke
    if (m_is_drawing && !m_current_stroke.points.empty()) {
      draw_stroke(ctx, m_current_stroke);
    }

    // Draw selection marquee while selecting
    if (m_is_selecting_area) {
      draw_selection_marquee(ctx);
    }

    // Draw search highlights
    if (!m_search_highlighted_indices.empty()) {
      draw_search_highlights(ctx);
    }

    // Draw single bounding box around all selected shapes
    if (!m_selected_indices.empty() && !m_is_selecting_area) {
      draw_group_selection_box(ctx);
      // Draw rotation handle when in selection mode
      if (m_selection_mode) {
        draw_rotation_handle(ctx);
      }
    }

    // Draw rotation angle indicator during rotation
    if (m_is_rotating) {
      draw_rotation_angle_indicator(ctx);
    }

    // Draw snap feedback
    if (m_show_snap_feedback && m_snap_feedback_timer > 0.0f) {
      draw_snap_feedback(ctx);
    }

    // Draw coordinate display
    draw_coordinate_display(ctx);

    // Draw minimap
    draw_minimap(ctx);

    // Draw rulers (on top of everything)
    draw_rulers(ctx);
  }

  virtual bool mouse_button_event(const Vector2i &p, int button, bool down,
                                  int modifiers) override {
    Vector2i local_p = p - absolute_position();

    // Check for ruler interactions (creating guides)
    if (button == NANOGUI_MOUSE_BUTTON_LEFT && down && m_show_rulers) {
      // Check if clicking on top ruler
      if (local_p.x() >= m_ruler_size && local_p.y() >= 0 && local_p.y() <= m_ruler_size) {
        m_is_creating_guide = true;
        m_creating_guide_type = Guide::Horizontal;
        Vector2f canvas_pos = local_to_canvas(local_p);
        m_guides.push_back(Guide(Guide::Horizontal, canvas_pos.y()));
        m_dragging_guide_index = m_guides.size() - 1;
        m_is_dragging_guide = true;
        return true;
      }

      // Check if clicking on left ruler
      if (local_p.y() >= m_ruler_size && local_p.x() >= 0 && local_p.x() <= m_ruler_size) {
        m_is_creating_guide = true;
        m_creating_guide_type = Guide::Vertical;
        Vector2f canvas_pos = local_to_canvas(local_p);
        m_guides.push_back(Guide(Guide::Vertical, canvas_pos.x()));
        m_dragging_guide_index = m_guides.size() - 1;
        m_is_dragging_guide = true;
        return true;
      }

      // Check if clicking on existing guide
      if (!m_is_dragging_guide) {
        Vector2f canvas_pos = local_to_canvas(local_p);
        for (int i = 0; i < (int)m_guides.size(); i++) {
          const auto &guide = m_guides[i];
          if (!guide.visible)
            continue;

          if (guide.type == Guide::Horizontal) {
            Vector2f screen_pos = canvas_to_global(Vector2f(0, guide.position));
            if (std::abs(local_p.y() - (screen_pos.y() - absolute_position().y())) < 5.0f) {
              m_is_dragging_guide = true;
              m_dragging_guide_index = i;
              return true;
            }
          } else {
            Vector2f screen_pos = canvas_to_global(Vector2f(guide.position, 0));
            if (std::abs(local_p.x() - (screen_pos.x() - absolute_position().x())) < 5.0f) {
              m_is_dragging_guide = true;
              m_dragging_guide_index = i;
              return true;
            }
          }
        }
      }
    }

    // Middle mouse button or Pan tool for panning
    if (button == NANOGUI_MOUSE_BUTTON_MIDDLE ||
        (button == NANOGUI_MOUSE_BUTTON_LEFT && m_current_tool == Tool::Pan)) {
      if (down) {
        m_is_panning = true;
        m_pan_start = local_p;
        m_pan_origin = m_pan_offset;
        return true;
      }
      m_is_panning = false;
      return true;
    }

    Vector2f canvas_p = local_to_canvas(local_p);

    // Handle right-click for context menu
    if (button == NANOGUI_MOUSE_BUTTON_RIGHT && down) {
      if (m_selection_mode && !m_selected_indices.empty()) {
        show_context_menu(p);
        return true;
      }
      return false;
    }

    if (button != NANOGUI_MOUSE_BUTTON_LEFT)
      return Canvas::mouse_button_event(p, button, down, modifiers);

    if (down) {
      request_focus();

      // Pan tool - already handled above
      if (m_current_tool == Tool::Pan) {
        return true;
      }

      if (m_selection_mode) {
        // Check if clicking on resize handle (for images)
        int resize_handle = get_resize_handle_at_point(canvas_p.x(), canvas_p.y());
        if (resize_handle >= 0) {
          m_is_resizing = true;
          m_resize_handle_index = resize_handle;
          m_resize_start_point = Point(canvas_p.x(), canvas_p.y());

          // Store original image bounds
          int idx = m_selected_indices[0];
          if (idx >= 0 && idx < (int)m_strokes.size()) {
            m_resize_original_p1 = m_strokes[idx].points[0];
            m_resize_original_p2 = m_strokes[idx].points[1];
          }
          return true;
        }

        // Check if clicking on rotation handle
        if (!m_selected_indices.empty() &&
            is_point_in_rotation_handle(canvas_p.x(), canvas_p.y())) {
          m_is_rotating = true;
          m_rotation_center = Vector2f(get_selection_center().x, get_selection_center().y);

          // Calculate initial angle from center to mouse
          float dx = canvas_p.x() - m_rotation_center.x();
          float dy = canvas_p.y() - m_rotation_center.y();
          m_rotation_start_angle = std::atan2(dy, dx);
          m_rotation_current_angle = 0.0f;
          return true;
        }

        // Check if clicking inside the bounding box of selected shapes
        if (!m_selected_indices.empty() &&
            is_point_in_selection_bounds(canvas_p.x(), canvas_p.y())) {
          // Clicking inside bounding box - start moving all selected shapes
          m_is_moving_shape = true;
          m_move_start_canvas = canvas_p;
          return true;
        }

        int clicked = find_stroke_at_point(canvas_p.x(), canvas_p.y());
        if (clicked >= 0) {
          // Check if shift is held for multi-select
          bool is_shift = (modifiers & MOD_SHIFT) != 0;

          if (!is_shift) {
            clear_selection();
          }

          // Toggle selection if shift-clicking already selected item
          if (is_shift && is_selected(clicked)) {
            remove_from_selection(clicked);
          } else {
            add_to_selection(clicked);
          }

          m_is_moving_shape = true;
          m_move_start_canvas = canvas_p;
          return true;
        }
        // Start area selection
        clear_selection();
        m_is_selecting_area = true;
        m_selection_start = canvas_p;
        m_selection_end = canvas_p;
        return true;
      }

      // Handle Text tool specially
      if (m_current_tool == Tool::Text) {
        start_text_input(canvas_p);
        return true;
      }

      // Start drawing
      m_is_drawing = true;
      m_current_stroke.points.clear();
      m_current_stroke.color = m_current_color;
      m_current_stroke.width = m_current_width;
      m_current_stroke.tool = m_current_tool;
      m_current_stroke.fill_style = m_fill_style;
      m_current_stroke.fill_color = m_fill_color;
      m_current_stroke.selected = false;

      // Apply snap to grid for starting point (check Alt key to disable)
      bool alt_pressed = (modifiers & MOD_ALT) != 0;
      Point snapped_start = snap_to_grid(Point(canvas_p.x(), canvas_p.y()), alt_pressed);

      // Show snap feedback if snapping occurred
      if (m_snap_to_grid_enabled && !alt_pressed &&
          (std::abs(snapped_start.x - canvas_p.x()) > 0.1f ||
           std::abs(snapped_start.y - canvas_p.y()) > 0.1f)) {
        m_show_snap_feedback = true;
        m_snap_feedback_timer = 0.3f; // Show for 0.3 seconds
        m_last_snap_pos = Vector2f(snapped_start.x, snapped_start.y);
      }

      m_current_stroke.points.push_back(snapped_start);
      return true;
    }

    // Mouse released
    if (m_is_dragging_guide) {
      // Check if guide was dragged off canvas (delete it)
      if (m_dragging_guide_index >= 0 && m_dragging_guide_index < (int)m_guides.size()) {
        const auto &guide = m_guides[m_dragging_guide_index];
        Vector2i local = p - absolute_position();

        bool off_canvas = false;
        if (guide.type == Guide::Horizontal) {
          // Check if dragged above or below canvas
          if (local.y() < 0 || local.y() > m_size.y()) {
            off_canvas = true;
          }
        } else {
          // Check if dragged left or right of canvas
          if (local.x() < 0 || local.x() > m_size.x()) {
            off_canvas = true;
          }
        }

        if (off_canvas) {
          m_guides.erase(m_guides.begin() + m_dragging_guide_index);
        }
      }

      m_is_dragging_guide = false;
      m_is_creating_guide = false;
      m_dragging_guide_index = -1;
      return true;
    }

    if (m_is_panning) {
      m_is_panning = false;
      return true;
    }

    if (m_is_selecting_area) {
      m_is_selecting_area = false;
      // Select all shapes within the rectangle
      select_shapes_in_area();
      return true;
    }

    if (m_is_resizing) {
      m_is_resizing = false;
      m_resize_handle_index = -1;
      m_undo_stack.push(m_strokes);
      m_redo_stack = std::stack<std::vector<Stroke>>();
      return true;
    }

    if (m_is_rotating) {
      m_is_rotating = false;
      m_undo_stack.push(m_strokes);
      m_redo_stack = std::stack<std::vector<Stroke>>();
      return true;
    }

    if (m_is_moving_shape) {
      m_is_moving_shape = false;

      // Apply snap to grid when movement ends (check Alt key to disable)
      bool alt_pressed = (modifiers & MOD_ALT) != 0;
      if (m_snap_to_grid_enabled && !alt_pressed && !m_selected_indices.empty()) {
        // For each selected shape, snap its first point and move the entire shape by that offset
        for (int idx : m_selected_indices) {
          if (idx >= 0 && idx < (int)m_strokes.size()) {
            auto &stroke = m_strokes[idx];
            if (!stroke.points.empty()) {
              // Get the first point and calculate snap offset
              Point original = stroke.points[0];
              Point snapped = snap_to_grid(original, alt_pressed);
              float offset_x = snapped.x - original.x;
              float offset_y = snapped.y - original.y;

              // Apply the same offset to all points to maintain shape integrity
              if (std::abs(offset_x) > 0.1f || std::abs(offset_y) > 0.1f) {
                for (auto &point : stroke.points) {
                  point.x += offset_x;
                  point.y += offset_y;
                }
                m_show_snap_feedback = true;
                m_snap_feedback_timer = 0.5f;
                m_last_snap_pos = Vector2f(snapped.x, snapped.y);
              }
            }
          }
        }
      }

      m_undo_stack.push(m_strokes);
      m_redo_stack = std::stack<std::vector<Stroke>>();
      return true;
    }

    if (m_is_drawing) {
      m_is_drawing = false;
      if (!m_current_stroke.points.empty()) {
        m_strokes.push_back(m_current_stroke);
        m_undo_stack.push(m_strokes);
        m_redo_stack = std::stack<std::vector<Stroke>>();

        // Notify layers panel
        if (m_strokes_changed_callback) {
          m_strokes_changed_callback();
        }
      }
      m_current_stroke.points.clear();
      return true;
    }

    return Canvas::mouse_button_event(p, button, down, modifiers);
  }

  virtual bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                int modifiers) override {
    Vector2i local_p = p - absolute_position();
    Vector2f canvas_p = local_to_canvas(local_p);
    m_last_mouse_pos = canvas_p;

    // Handle guide dragging
    if (m_is_dragging_guide && m_dragging_guide_index >= 0 &&
        m_dragging_guide_index < (int)m_guides.size()) {
      auto &guide = m_guides[m_dragging_guide_index];
      if (guide.type == Guide::Horizontal) {
        guide.position = canvas_p.y();
      } else {
        guide.position = canvas_p.x();
      }
      return true;
    }

    if (m_is_panning) {
      Vector2f delta(static_cast<float>(local_p.x() - m_pan_start.x()),
                     static_cast<float>(local_p.y() - m_pan_start.y()));
      m_pan_offset = m_pan_origin + delta;
      return true;
    }

    if (m_is_selecting_area) {
      m_selection_end = canvas_p;
      return true;
    }

    if (m_is_resizing && m_selected_indices.size() == 1) {
      int idx = m_selected_indices[0];
      if (idx >= 0 && idx < (int)m_strokes.size() && m_strokes[idx].tool == Tool::Image) {
        // Calculate new bounds based on which handle is being dragged
        Point p1 = m_resize_original_p1;
        Point p2 = m_resize_original_p2;

        float dx = canvas_p.x() - m_resize_start_point.x;
        float dy = canvas_p.y() - m_resize_start_point.y;

        // Proportional scaling with Shift key
        bool proportional = (modifiers & MOD_SHIFT) != 0;

        if (proportional) {
          // Calculate aspect ratio
          float orig_width = std::abs(p2.x - p1.x);
          float orig_height = std::abs(p2.y - p1.y);
          float aspect_ratio = orig_width / orig_height;

          // Use the larger delta to maintain aspect ratio
          float scale_factor = 1.0f;

          switch (m_resize_handle_index) {
          case 0: // Top-left
            scale_factor = std::max(std::abs(dx / orig_width), std::abs(dy / orig_height));
            p1.x -= scale_factor * orig_width * (dx < 0 ? -1 : 1);
            p1.y -= scale_factor * orig_height * (dy < 0 ? -1 : 1);
            break;
          case 1: // Top-right
            scale_factor = std::max(std::abs(dx / orig_width), std::abs(dy / orig_height));
            p2.x += scale_factor * orig_width * (dx > 0 ? 1 : -1);
            p1.y -= scale_factor * orig_height * (dy < 0 ? -1 : 1);
            break;
          case 2: // Bottom-right
            scale_factor = std::max(std::abs(dx / orig_width), std::abs(dy / orig_height));
            p2.x += scale_factor * orig_width * (dx > 0 ? 1 : -1);
            p2.y += scale_factor * orig_height * (dy > 0 ? 1 : -1);
            break;
          case 3: // Bottom-left
            scale_factor = std::max(std::abs(dx / orig_width), std::abs(dy / orig_height));
            p1.x -= scale_factor * orig_width * (dx < 0 ? -1 : 1);
            p2.y += scale_factor * orig_height * (dy > 0 ? 1 : -1);
            break;
          }
        } else {
          // Free-form scaling
          switch (m_resize_handle_index) {
          case 0: // Top-left
            p1.x += dx;
            p1.y += dy;
            break;
          case 1: // Top-right
            p2.x += dx;
            p1.y += dy;
            break;
          case 2: // Bottom-right
            p2.x += dx;
            p2.y += dy;
            break;
          case 3: // Bottom-left
            p1.x += dx;
            p2.y += dy;
            break;
          }
        }

        // Update the image stroke bounds
        m_strokes[idx].points[0] = p1;
        m_strokes[idx].points[1] = p2;
      }
      return true;
    }

    if (m_is_rotating && !m_selected_indices.empty()) {
      // Calculate current angle from center to mouse
      float dx = canvas_p.x() - m_rotation_center.x();
      float dy = canvas_p.y() - m_rotation_center.y();
      float current_angle = std::atan2(dy, dx);

      // Calculate angle delta
      float angle_delta = current_angle - m_rotation_start_angle;

      // Apply shift key snapping to 15-degree increments
      if (modifiers & MOD_SHIFT) {
        float snap_increment = 15.0f * M_PI / 180.0f; // 15 degrees in radians
        angle_delta = std::round(angle_delta / snap_increment) * snap_increment;
      }

      // Calculate the incremental rotation since last frame
      float incremental_rotation = angle_delta - m_rotation_current_angle;

      // Rotate shapes by the incremental amount
      if (std::abs(incremental_rotation) > 0.0001f) {
        Point center(m_rotation_center.x(), m_rotation_center.y());
        rotate_shapes(incremental_rotation, center);
        m_rotation_current_angle = angle_delta;
      }

      return true;
    }

    if (m_selection_mode && m_is_moving_shape && !m_selected_indices.empty()) {
      // Calculate delta from unsnapped positions for smooth movement
      Vector2f delta = canvas_p - m_move_start_canvas;

      // Move shapes by the delta
      for (int idx : m_selected_indices) {
        if (idx >= 0 && idx < (int)m_strokes.size()) {
          m_strokes[idx].move(delta.x(), delta.y());
        }
      }

      // Update start position for next frame
      m_move_start_canvas = canvas_p;

      // Check for snap to guide
      bool snapped_to_guide = false;
      if (m_show_guides && !m_selected_indices.empty()) {
        if (m_selected_indices[0] >= 0 && m_selected_indices[0] < (int)m_strokes.size()) {
          const auto &stroke = m_strokes[m_selected_indices[0]];
          if (!stroke.points.empty()) {
            Point snapped = snap_to_guide(stroke.points[0], snapped_to_guide);
            if (snapped_to_guide) {
              // Apply snap offset to all selected shapes
              float offset_x = snapped.x - stroke.points[0].x;
              float offset_y = snapped.y - stroke.points[0].y;

              for (int idx : m_selected_indices) {
                if (idx >= 0 && idx < (int)m_strokes.size()) {
                  for (auto &point : m_strokes[idx].points) {
                    point.x += offset_x;
                    point.y += offset_y;
                  }
                }
              }

              m_show_snap_feedback = true;
              m_snap_feedback_timer = 0.3f;
              m_last_snap_pos = Vector2f(snapped.x, snapped.y);
            }
          }
        }
      }

      // Show snap feedback if snap is enabled (visual only during drag)
      if (!snapped_to_guide) {
        bool alt_pressed = (modifiers & MOD_ALT) != 0;
        if (m_snap_to_grid_enabled && !alt_pressed && !m_selected_indices.empty()) {
          // Get the first selected shape's position to show snap preview
          if (m_selected_indices[0] >= 0 && m_selected_indices[0] < (int)m_strokes.size()) {
            const auto &stroke = m_strokes[m_selected_indices[0]];
            if (!stroke.points.empty()) {
              Point snapped = snap_to_grid(stroke.points[0], alt_pressed);
              if (std::abs(snapped.x - stroke.points[0].x) > 0.1f ||
                  std::abs(snapped.y - stroke.points[0].y) > 0.1f) {
                m_show_snap_feedback = true;
                m_snap_feedback_timer = 0.3f;
                m_last_snap_pos = Vector2f(snapped.x, snapped.y);
              }
            }
          }
        }
      }

      return true;
    }

    if (m_is_drawing) {
      // Apply snap to grid for drawing (check Alt key to disable)
      bool alt_pressed = (modifiers & MOD_ALT) != 0;
      Point snapped_point = snap_to_grid(Point(canvas_p.x(), canvas_p.y()), alt_pressed);

      // Show snap feedback if snapping occurred
      if (m_snap_to_grid_enabled && !alt_pressed &&
          (std::abs(snapped_point.x - canvas_p.x()) > 0.1f ||
           std::abs(snapped_point.y - canvas_p.y()) > 0.1f)) {
        m_show_snap_feedback = true;
        m_snap_feedback_timer = 0.3f;
        m_last_snap_pos = Vector2f(snapped_point.x, snapped_point.y);
      }

      if (m_current_tool == Tool::Pen) {
        m_current_stroke.points.push_back(snapped_point);
      } else {
        if (m_current_stroke.points.size() > 1)
          m_current_stroke.points[1] = snapped_point;
        else
          m_current_stroke.points.push_back(snapped_point);
      }
      return true;
    }

    return false;
  }

  bool scroll_event(const Vector2i &p, const Vector2f &rel) override {
    Vector2i local = p - absolute_position();
    Vector2f before = local_to_canvas(local);
    m_last_mouse_pos = before;

    float scroll_delta = rel.y();
    if (std::abs(scroll_delta) < 1e-6f)
      return false;

    float zoom_factor = scroll_delta > 0 ? 1.1f : 0.9f;
    float new_zoom = nanogui::clip(m_zoom * zoom_factor, 0.25f, 6.0f);
    if (std::abs(new_zoom - m_zoom) < 1e-6f)
      return false;

    m_zoom = new_zoom;
    Vector2f after_local = canvas_to_local(before);
    Vector2f cursor_local = to_vec(local);
    m_pan_offset += cursor_local - after_local;
    return true;
  }

  void set_color(const Color &color) { m_current_color = color; }
  void set_tool(Tool tool) {
    m_current_tool = tool;
    // Pan tool doesn't need selection mode
    if (tool == Tool::Pan) {
      m_selection_mode = false;
      clear_selection();
      set_cursor(Cursor::Hand);
    } else {
      set_cursor(Cursor::Arrow);
    }
  }
  void set_selection_mode(bool mode) {
    m_selection_mode = mode;
    if (!mode)
      clear_selection();
  }
  void set_fill_color(const Color &color) { m_fill_color = color; }

  Tool get_tool() const { return m_current_tool; }
  Color get_color() const { return m_current_color; }
  Color get_fill_color() const { return m_fill_color; }

  float get_stroke_width() const { return m_current_width; }
  void set_stroke_width(float width) { m_current_width = width; }

  void enable_space_pan() {
    if (!m_space_pressed) {
      m_tool_before_space = m_current_tool;
      m_current_tool = Tool::Pan;
      m_space_pressed = true;
      set_cursor(Cursor::Hand);
    }
  }

  void disable_space_pan() {
    if (m_space_pressed) {
      m_current_tool = m_tool_before_space;
      m_space_pressed = false;
      set_cursor(Cursor::Arrow);
    }
  }

  void toggle_grid() { m_show_grid = !m_show_grid; }
  void toggle_minimap() { m_show_minimap = !m_show_minimap; }
  void toggle_coordinates() { m_show_coordinates = !m_show_coordinates; }
  void toggle_guides() {
    m_show_guides = !m_show_guides;
    m_show_rulers = m_show_guides; // Toggle rulers with guides
  }

  bool get_guides_visible() const { return m_show_guides; }
  void set_guides_visible(bool visible) {
    m_show_guides = visible;
    m_show_rulers = visible; // Keep rulers in sync with guides
  }

  void zoom_in() {
    float new_zoom = nanogui::clip(m_zoom * 1.2f, 0.25f, 6.0f);
    if (std::abs(new_zoom - m_zoom) > 1e-6f) {
      m_zoom = new_zoom;
    }
  }

  void zoom_out() {
    float new_zoom = nanogui::clip(m_zoom / 1.2f, 0.25f, 6.0f);
    if (std::abs(new_zoom - m_zoom) > 1e-6f) {
      m_zoom = new_zoom;
    }
  }

  void reset_zoom() {
    m_zoom = 1.0f;
    m_pan_offset = Vector2f(0.f, 0.f);
  }

  float get_zoom() const { return m_zoom; }
  float get_pan_x() const { return m_pan_offset.x(); }
  float get_pan_y() const { return m_pan_offset.y(); }

  void set_view(float zoom, float pan_x, float pan_y) {
    m_zoom = nanogui::clip(zoom, 0.25f, 6.0f);
    m_pan_offset = Vector2f(pan_x, pan_y);
  }

  // Snap to grid methods
  void toggle_snap_to_grid() { m_snap_to_grid_enabled = !m_snap_to_grid_enabled; }

  bool get_snap_to_grid_enabled() const { return m_snap_to_grid_enabled; }

  void set_snap_to_grid_enabled(bool enabled) { m_snap_to_grid_enabled = enabled; }

  // Get all strokes for saving page state
  std::vector<Stroke> get_strokes() const { return m_strokes; }

  // Load strokes from a page
  void set_strokes(const std::vector<Stroke> &strokes) {
    m_strokes = strokes;
    clear_selection();
  }

  // Get undo/redo stacks for saving page state
  std::stack<std::vector<Stroke>> get_undo_stack() const { return m_undo_stack; }
  std::stack<std::vector<Stroke>> get_redo_stack() const { return m_redo_stack; }

  // Load undo/redo stacks from a page
  void set_undo_stack(const std::stack<std::vector<Stroke>> &stack) { m_undo_stack = stack; }
  void set_redo_stack(const std::stack<std::vector<Stroke>> &stack) { m_redo_stack = stack; }

  // Get guides for saving page state
  std::vector<Guide> get_guides() const { return m_guides; }

  // Load guides from a page
  void set_guides(const std::vector<Guide> &guides) { m_guides = guides; }

  // Clear canvas
  void clear_canvas() {
    m_strokes.clear();
    clear_selection();
  }

  void show_context_menu(const Vector2i &pos) {
    // Remove existing popup if any
    if (m_context_popup) {
      m_context_popup->set_visible(false);
      if (parent())
        parent()->remove_child(m_context_popup);
      m_context_popup = nullptr;
    }

    if (!parent())
      return;

    // Create popup menu (pass nullptr for parent_window since we don't have one)
    m_context_popup = new Popup(parent(), nullptr);
    m_context_popup->set_anchor_pos(pos);
    m_context_popup->set_fixed_size(Vector2i(180, 0));

    auto *popup_content = new Widget(m_context_popup);
    popup_content->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));

    // Bring to Front
    auto *front_btn = new Button(popup_content, "Bring to Front", FA_ARROW_UP);
    front_btn->set_callback([this]() {
      bring_to_front();
      m_context_popup->set_visible(false);
    });

    // Send to Back
    auto *back_btn = new Button(popup_content, "Send to Back", FA_ARROW_DOWN);
    back_btn->set_callback([this]() {
      send_to_back();
      m_context_popup->set_visible(false);
    });

    // Separator
    auto *sep1 = new Widget(popup_content);
    sep1->set_fixed_height(1);

    // Group (if multiple selected)
    if (m_selected_indices.size() > 1) {
      auto *group_btn = new Button(popup_content, "Group", FA_OBJECT_GROUP);
      group_btn->set_callback([this]() {
        group_selected();
        m_context_popup->set_visible(false);
      });
    }

    // Duplicate
    auto *duplicate_btn = new Button(popup_content, "Duplicate", FA_COPY);
    duplicate_btn->set_callback([this]() {
      duplicate_selected();
      m_context_popup->set_visible(false);
    });

    // Separator
    auto *sep2 = new Widget(popup_content);
    sep2->set_fixed_height(1);

    // Delete
    auto *delete_btn = new Button(popup_content, "Delete", FA_TRASH);
    delete_btn->set_background_color(Color(220, 50, 50, 255));
    delete_btn->set_callback([this]() {
      delete_selected();
      m_context_popup->set_visible(false);
    });

    m_context_popup->set_visible(true);
    if (parent())
      parent()->perform_layout(screen()->nvg_context());
  }

  void bring_to_front() {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    // Sort indices in ascending order
    std::vector<int> sorted = m_selected_indices;
    std::sort(sorted.begin(), sorted.end());

    // Move selected strokes to end (front)
    std::vector<Stroke> selected_strokes;
    for (int i = sorted.size() - 1; i >= 0; i--) {
      int idx = sorted[i];
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        selected_strokes.insert(selected_strokes.begin(), m_strokes[idx]);
        m_strokes.erase(m_strokes.begin() + idx);
      }
    }

    // Add them to the end
    m_strokes.insert(m_strokes.end(), selected_strokes.begin(), selected_strokes.end());

    // Update selection indices
    clear_selection();
    int start_idx = m_strokes.size() - selected_strokes.size();
    for (int i = 0; i < (int)selected_strokes.size(); i++) {
      add_to_selection(start_idx + i);
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void send_to_back() {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    // Sort indices in descending order
    std::vector<int> sorted = m_selected_indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());

    // Move selected strokes to beginning (back)
    std::vector<Stroke> selected_strokes;
    for (int idx : sorted) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        selected_strokes.push_back(m_strokes[idx]);
        m_strokes.erase(m_strokes.begin() + idx);
      }
    }

    // Add them to the beginning
    m_strokes.insert(m_strokes.begin(), selected_strokes.begin(), selected_strokes.end());

    // Update selection indices
    clear_selection();
    for (int i = 0; i < (int)selected_strokes.size(); i++) {
      add_to_selection(i);
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void group_selected() {
    // For now, just show a notification
    // Full grouping would require a group structure
    std::cout << "Group feature - would group " << m_selected_indices.size() << " shapes"
              << std::endl;
  }

  void duplicate_selected() {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    std::vector<Stroke> duplicates;
    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        Stroke duplicate = m_strokes[idx];
        // Offset the duplicate slightly
        duplicate.move(20.0f, 20.0f);
        duplicate.selected = false;
        duplicates.push_back(duplicate);
      }
    }

    // Add duplicates
    clear_selection();
    int start_idx = m_strokes.size();
    m_strokes.insert(m_strokes.end(), duplicates.begin(), duplicates.end());

    // Select the duplicates
    for (int i = 0; i < (int)duplicates.size(); i++) {
      add_to_selection(start_idx + i);
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void add_sticky_note(const Vector2i &pos, const Color &color) {
    Stroke sticky;
    sticky.tool = Tool::Sticky;
    sticky.color = color;
    sticky.fill_style = FillStyle::Solid;
    sticky.fill_color = color;
    sticky.width = 2.0f;

    Vector2f canvas_pos = screen_to_canvas(pos);
    float size = 100.0f;
    sticky.points.push_back(Point(canvas_pos.x() - size / 2, canvas_pos.y() - size / 2));
    sticky.points.push_back(Point(canvas_pos.x() + size / 2, canvas_pos.y() + size / 2));

    m_strokes.push_back(sticky);
    m_undo_stack.push(m_strokes);
    m_redo_stack = std::stack<std::vector<Stroke>>();

    // Notify layers panel
    if (m_strokes_changed_callback) {
      m_strokes_changed_callback();
    }
  }

  void undo() {
    if (!m_strokes.empty()) {
      m_redo_stack.push(m_strokes);
      if (!m_undo_stack.empty()) {
        m_strokes = m_undo_stack.top();
        m_undo_stack.pop();
      } else {
        m_strokes.clear();
      }
      clear_selection();

      // Notify layers panel
      if (m_strokes_changed_callback) {
        m_strokes_changed_callback();
      }
    }
  }

  void redo() {
    if (!m_redo_stack.empty()) {
      m_undo_stack.push(m_strokes);
      m_strokes = m_redo_stack.top();
      m_redo_stack.pop();
      clear_selection();

      // Notify layers panel
      if (m_strokes_changed_callback) {
        m_strokes_changed_callback();
      }
    }
  }

  void delete_selected() {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    // Sort in descending order to delete from back to front
    std::vector<int> sorted_indices = m_selected_indices;
    std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<int>());

    for (int idx : sorted_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        // Free NanoVG image handle if this is an image stroke
        if (m_strokes[idx].tool == Tool::Image && m_strokes[idx].nvg_image_handle >= 0) {
          nvgDeleteImage(screen()->nvg_context(), m_strokes[idx].nvg_image_handle);
        }
        m_strokes.erase(m_strokes.begin() + idx);
      }
    }

    clear_selection();
    m_redo_stack = std::stack<std::vector<Stroke>>();

    // Notify layers panel
    if (m_strokes_changed_callback) {
      m_strokes_changed_callback();
    }
  }

  void clear() {
    m_undo_stack.push(m_strokes);
    m_strokes.clear();
    clear_selection();
    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void copy_selected() {
    if (m_selected_indices.empty())
      return;

    ClipboardManager::copy(m_strokes, m_selected_indices);
  }

  void cut_selected() {
    if (m_selected_indices.empty())
      return;

    ClipboardManager::cut(m_strokes, m_selected_indices);

    // Delete the selected strokes
    m_undo_stack.push(m_strokes);

    // Sort in descending order to delete from back to front
    std::vector<int> sorted_indices = m_selected_indices;
    std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<int>());

    for (int idx : sorted_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        m_strokes.erase(m_strokes.begin() + idx);
      }
    }

    clear_selection();
    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void paste_clipboard() {
    if (!ClipboardManager::has_content())
      return;

    m_undo_stack.push(m_strokes);

    std::vector<Stroke> pasted = ClipboardManager::paste();

    // Clear current selection
    clear_selection();

    // Add pasted strokes and select them
    int start_idx = m_strokes.size();
    m_strokes.insert(m_strokes.end(), pasted.begin(), pasted.end());

    // Auto-select pasted shapes
    for (int i = 0; i < (int)pasted.size(); i++) {
      add_to_selection(start_idx + i);
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();

    // Notify layers panel
    if (m_strokes_changed_callback) {
      m_strokes_changed_callback();
    }
  }

  void duplicate_selected_shortcut() {
    if (m_selected_indices.empty())
      return;

    // Copy to clipboard
    ClipboardManager::copy(m_strokes, m_selected_indices);

    // Paste immediately
    paste_clipboard();
  }

  // Get selected indices for properties panel
  std::vector<int> get_selected_indices() const { return m_selected_indices; }

  // Layer management methods
  void select_layer(int index) {
    if (index >= 0 && index < (int)m_strokes.size()) {
      // Don't select locked strokes
      if (m_strokes[index].locked) {
        return;
      }

      clear_selection();
      add_to_selection(index);
    }
  }

  void toggle_layer_visibility(int index) {
    if (index >= 0 && index < (int)m_strokes.size()) {
      m_strokes[index].visible = !m_strokes[index].visible;
    }
  }

  void toggle_layer_lock(int index) {
    if (index >= 0 && index < (int)m_strokes.size()) {
      m_strokes[index].locked = !m_strokes[index].locked;

      // If locking a selected stroke, deselect it
      if (m_strokes[index].locked && m_strokes[index].selected) {
        remove_from_selection(index);
      }
    }
  }

  // Apply property changes to selected shapes
  void apply_property_to_selected(const std::string &property, const Color &color) {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        if (property == "stroke_color") {
          m_strokes[idx].color = color;
        } else if (property == "fill_color") {
          m_strokes[idx].fill_color = color;
          if (m_strokes[idx].fill_style == FillStyle::None) {
            m_strokes[idx].fill_style = FillStyle::Solid;
          }
        }
      }
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void apply_property_to_selected(const std::string &property, float value) {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        if (property == "stroke_width") {
          m_strokes[idx].width = value;
        } else if (property == "opacity") {
          // Update alpha channel of stroke color
          Color c = m_strokes[idx].color;
          m_strokes[idx].color = Color(c.r(), c.g(), c.b(), value);
        } else if (property == "rotation") {
          // Set absolute rotation
          float old_rotation = m_strokes[idx].rotation;
          float rotation_delta = value - old_rotation;

          // Rotate the shape around its center
          float min_x, min_y, max_x, max_y;
          m_strokes[idx].get_bounds(min_x, min_y, max_x, max_y);
          Point center((min_x + max_x) / 2.0f, (min_y + max_y) / 2.0f);

          for (auto &point : m_strokes[idx].points) {
            float dx = point.x - center.x;
            float dy = point.y - center.y;
            float cos_a = std::cos(rotation_delta);
            float sin_a = std::sin(rotation_delta);
            point.x = center.x + dx * cos_a - dy * sin_a;
            point.y = center.y + dx * sin_a + dy * cos_a;
          }

          m_strokes[idx].rotation = value;
        }
      }
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void apply_property_to_selected(const std::string &property, bool value) {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        if (property == "fill_none") {
          m_strokes[idx].fill_style = value ? FillStyle::None : FillStyle::Solid;
        }
      }
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void apply_property_to_selected(const std::string &property, float x, float y) {
    if (m_selected_indices.empty())
      return;

    m_undo_stack.push(m_strokes);

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        if (property == "position" && !m_strokes[idx].points.empty()) {
          // Move shape to new position
          float current_x = m_strokes[idx].points[0].x;
          float current_y = m_strokes[idx].points[0].y;

          float dx = (x >= 0) ? (x - current_x) : 0;
          float dy = (y >= 0) ? (y - current_y) : 0;

          m_strokes[idx].move(dx, dy);
        } else if (property == "size" && m_strokes[idx].points.size() >= 2) {
          // Resize shape (for rectangles)
          if (m_strokes[idx].tool == Tool::Rectangle) {
            float min_x = std::min(m_strokes[idx].points[0].x, m_strokes[idx].points[1].x);
            float min_y = std::min(m_strokes[idx].points[0].y, m_strokes[idx].points[1].y);

            if (x >= 0) {
              m_strokes[idx].points[1].x = min_x + x;
            }
            if (y >= 0) {
              m_strokes[idx].points[1].y = min_y + y;
            }
          }
        }
      }
    }

    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  // Search-related methods
  void set_search_highlights(const std::vector<int> &indices, int current_index) {
    m_search_highlighted_indices = indices;
    m_search_current_index = current_index;
  }

  void clear_search_highlights() {
    m_search_highlighted_indices.clear();
    m_search_current_index = -1;
  }

  void pan_to_stroke(int stroke_index) {
    if (stroke_index < 0 || stroke_index >= (int)m_strokes.size()) {
      return;
    }

    const auto &stroke = m_strokes[stroke_index];
    if (stroke.points.empty()) {
      return;
    }

    // Calculate center of stroke
    float min_x, min_y, max_x, max_y;
    stroke.get_bounds(min_x, min_y, max_x, max_y);
    float center_x = (min_x + max_x) / 2.0f;
    float center_y = (min_y + max_y) / 2.0f;

    // Pan to center the stroke in the viewport
    Vector2f canvas_center = Vector2f(center_x, center_y);
    Vector2f viewport_center = Vector2f(m_size.x() / 2.0f, m_size.y() / 2.0f);

    // Calculate required pan offset
    m_pan_offset = viewport_center - canvas_center * m_zoom;
  }

private:
  std::vector<Stroke> m_strokes;
  std::stack<std::vector<Stroke>> m_undo_stack;
  std::stack<std::vector<Stroke>> m_redo_stack;
  Stroke m_current_stroke;
  bool m_is_drawing = false;
  Color m_current_color;
  float m_current_width;
  Tool m_current_tool;
  FillStyle m_fill_style;
  Color m_fill_color;
  Vector2f m_pan_offset;
  Vector2f m_pan_origin;
  float m_zoom;
  bool m_is_panning;
  Vector2i m_pan_start;
  bool m_selection_mode;
  bool m_is_moving_shape;
  Vector2f m_move_start_canvas;
  std::vector<int> m_selected_indices;
  bool m_is_selecting_area = false;
  Vector2f m_selection_start;
  Vector2f m_selection_end;
  Popup *m_context_popup = nullptr;
  bool m_show_grid = true;
  bool m_show_minimap = true;
  bool m_show_coordinates = true; // Show by default
  Vector2f m_last_mouse_pos = Vector2f(0.f, 0.f);
  Tool m_tool_before_space = Tool::Select; // Track tool before space bar pressed
  bool m_space_pressed = false;

  // Text input state
  TextBox *m_text_input = nullptr;
  Vector2f m_text_input_pos;
  bool m_is_editing_text = false;

  // Rotation state
  bool m_is_rotating = false;
  Vector2f m_rotation_center;
  float m_rotation_start_angle = 0.0f;

  // Resize state (for images)
  bool m_is_resizing = false;
  int m_resize_handle_index = -1; // 0=TL, 1=TR, 2=BR, 3=BL
  Point m_resize_start_point;
  Point m_resize_original_p1;
  Point m_resize_original_p2;
  float m_rotation_current_angle = 0.0f;

  // Snap to grid state
  bool m_snap_to_grid_enabled = false;
  float m_grid_size = 50.0f;
  Vector2f m_last_snap_pos = Vector2f(0.f, 0.f);
  bool m_show_snap_feedback = false;
  float m_snap_feedback_timer = 0.0f;

  // Ruler and guide state
  std::vector<Guide> m_guides;
  bool m_show_rulers = true;
  bool m_show_guides = true;
  bool m_is_dragging_guide = false;
  int m_dragging_guide_index = -1;
  bool m_is_creating_guide = false;
  Guide::Type m_creating_guide_type = Guide::Horizontal;
  float m_ruler_size = 20.0f;

  // Selection changed callback
  std::function<void()> m_selection_changed_callback;

  // Strokes changed callback (for layers panel)
  std::function<void()> m_strokes_changed_callback;

  // Search state
  std::vector<int> m_search_highlighted_indices;
  int m_search_current_index = -1;

  Vector2f to_vec(const Vector2i &value) const {
    return Vector2f(static_cast<float>(value.x()), static_cast<float>(value.y()));
  }

  // Snap to grid function
  Point snap_to_grid(const Point &point, bool alt_pressed) const {
    if (!m_snap_to_grid_enabled || alt_pressed) {
      return point;
    }
    Point snapped = Point(std::round(point.x / m_grid_size) * m_grid_size,
                          std::round(point.y / m_grid_size) * m_grid_size);
    std::cout << "Snapping point (" << point.x << ", " << point.y << ") to (" << snapped.x << ", "
              << snapped.y << ")" << std::endl;
    return snapped;
  }

  // Snap to guide with 5px threshold
  Point snap_to_guide(const Point &point, bool &snapped_to_guide) const {
    if (!m_show_guides || m_guides.empty()) {
      snapped_to_guide = false;
      return point;
    }

    Point result = point;
    snapped_to_guide = false;
    float snap_threshold = 5.0f / m_zoom; // Adjust threshold based on zoom

    // Check horizontal guides (snap Y coordinate)
    for (const auto &guide : m_guides) {
      if (!guide.visible)
        continue;

      if (guide.type == Guide::Horizontal) {
        if (std::abs(point.y - guide.position) < snap_threshold) {
          result.y = guide.position;
          snapped_to_guide = true;
        }
      } else {
        // Vertical guide (snap X coordinate)
        if (std::abs(point.x - guide.position) < snap_threshold) {
          result.x = guide.position;
          snapped_to_guide = true;
        }
      }
    }

    return result;
  }

  Vector2f snap_to_grid_vec(const Vector2f &point, bool alt_pressed) const {
    if (!m_snap_to_grid_enabled || alt_pressed) {
      return point;
    }
    return Vector2f(std::round(point.x() / m_grid_size) * m_grid_size,
                    std::round(point.y() / m_grid_size) * m_grid_size);
  }

  Vector2f canvas_to_local(const Vector2f &pt) const { return pt * m_zoom + m_pan_offset; }

  Vector2f canvas_to_global(const Vector2f &pt) const {
    Vector2f local = canvas_to_local(pt);
    Vector2i abs_pos = absolute_position();
    return local + Vector2f(abs_pos.x(), abs_pos.y());
  }

  Vector2f point_to_global(const Point &pt) const { return canvas_to_global(Vector2f(pt.x, pt.y)); }

  Vector2f local_to_canvas(const Vector2i &local) const {
    return (to_vec(local) - m_pan_offset) / m_zoom;
  }

  Vector2f screen_to_canvas(const Vector2i &screen) const {
    Vector2i abs_pos = absolute_position();
    Vector2i local = screen - abs_pos;
    return local_to_canvas(local);
  }

  int find_stroke_at_point(float x, float y) {
    for (int i = m_strokes.size() - 1; i >= 0; i--) {
      // Skip locked and invisible strokes
      if (m_strokes[i].locked || !m_strokes[i].visible) {
        continue;
      }
      if (m_strokes[i].contains_point(x, y)) {
        return i;
      }
    }
    return -1;
  }

  void start_text_input(const Vector2f &canvas_pos) {
    m_text_input_pos = canvas_pos;
    m_is_editing_text = true;

    // Create text input widget if it doesn't exist
    if (!m_text_input && parent()) {
      m_text_input = new TextBox(parent(), "");
      m_text_input->set_editable(true);
      m_text_input->set_alignment(TextBox::Alignment::Left);
    }

    if (m_text_input) {
      // Position the text box at the click location
      Vector2f screen_pos = canvas_to_global(canvas_pos);
      m_text_input->set_position(Vector2i((int)screen_pos.x(), (int)screen_pos.y()));
      m_text_input->set_fixed_size(Vector2i(200, 30));
      m_text_input->set_value("");
      m_text_input->set_visible(true);
      m_text_input->request_focus();

      // Set callback for when user finishes editing
      m_text_input->set_callback([this](const std::string &value) {
        finalize_text(value);
        return true;
      });
    }
  }

  void finalize_text(const std::string &text) {
    if (!text.empty() && m_is_editing_text) {
      Stroke text_stroke;
      text_stroke.tool = Tool::Text;
      text_stroke.color = m_current_color;
      text_stroke.text = text;
      text_stroke.font_face = "sans";
      text_stroke.font_size = 16.0f;
      text_stroke.text_align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP;
      text_stroke.points.push_back(Point(m_text_input_pos.x(), m_text_input_pos.y()));

      m_strokes.push_back(text_stroke);
      m_undo_stack.push(m_strokes);
      m_redo_stack = std::stack<std::vector<Stroke>>();

      // Notify layers panel
      if (m_strokes_changed_callback) {
        m_strokes_changed_callback();
      }
    }

    // Hide and cleanup text input
    if (m_text_input) {
      m_text_input->set_visible(false);
    }
    m_is_editing_text = false;
  }

  void clear_selection() {
    for (auto &stroke : m_strokes) {
      stroke.selected = false;
    }
    m_selected_indices.clear();
    if (m_selection_changed_callback) {
      m_selection_changed_callback();
    }
  }

  bool is_selected(int index) const {
    return std::find(m_selected_indices.begin(), m_selected_indices.end(), index) !=
           m_selected_indices.end();
  }

  void add_to_selection(int index) {
    if (index >= 0 && index < (int)m_strokes.size() && !is_selected(index)) {
      m_selected_indices.push_back(index);
      m_strokes[index].selected = true;
      if (m_selection_changed_callback) {
        m_selection_changed_callback();
      }
    }
  }

  void remove_from_selection(int index) {
    auto it = std::find(m_selected_indices.begin(), m_selected_indices.end(), index);
    if (it != m_selected_indices.end()) {
      m_selected_indices.erase(it);
      if (index >= 0 && index < (int)m_strokes.size()) {
        m_strokes[index].selected = false;
      }
      if (m_selection_changed_callback) {
        m_selection_changed_callback();
      }
    }
  }

  void select_shapes_in_area() {
    float min_x = std::min(m_selection_start.x(), m_selection_end.x());
    float min_y = std::min(m_selection_start.y(), m_selection_end.y());
    float max_x = std::max(m_selection_start.x(), m_selection_end.x());
    float max_y = std::max(m_selection_start.y(), m_selection_end.y());

    for (int i = 0; i < (int)m_strokes.size(); i++) {
      // Skip locked and invisible strokes
      if (m_strokes[i].locked || !m_strokes[i].visible) {
        continue;
      }

      float s_min_x, s_min_y, s_max_x, s_max_y;
      m_strokes[i].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);

      // Check if shape is completely within selection area
      if (s_min_x >= min_x && s_max_x <= max_x && s_min_y >= min_y && s_max_y <= max_y) {
        add_to_selection(i);
      }
    }
  }

  // Helper method to rotate a point around a center
  Point rotate_point(const Point &pt, const Point &center, float angle) const {
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float dx = pt.x - center.x;
    float dy = pt.y - center.y;
    return Point(center.x + dx * cos_a - dy * sin_a, center.y + dx * sin_a + dy * cos_a);
  }

  // Get center point of selected shapes
  Point get_selection_center() const {
    if (m_selected_indices.empty())
      return Point(0, 0);

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        float s_min_x, s_min_y, s_max_x, s_max_y;
        m_strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
        min_x = std::min(min_x, s_min_x);
        min_y = std::min(min_y, s_min_y);
        max_x = std::max(max_x, s_max_x);
        max_y = std::max(max_y, s_max_y);
      }
    }

    return Point((min_x + max_x) / 2.0f, (min_y + max_y) / 2.0f);
  }

  // Get rotation handle position in canvas coordinates
  Point get_rotation_handle_pos() const {
    if (m_selected_indices.empty())
      return Point(0, 0);

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        float s_min_x, s_min_y, s_max_x, s_max_y;
        m_strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
        min_x = std::min(min_x, s_min_x);
        min_y = std::min(min_y, s_min_y);
        max_x = std::max(max_x, s_max_x);
        max_y = std::max(max_y, s_max_y);
      }
    }

    float center_x = (min_x + max_x) / 2.0f;
    return Point(center_x, min_y - 30.0f);
  }

  // Check if point is in rotation handle
  bool is_point_in_rotation_handle(float x, float y) const {
    if (m_selected_indices.empty())
      return false;

    Point handle_pos = get_rotation_handle_pos();
    float dx = x - handle_pos.x;
    float dy = y - handle_pos.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    return dist <= 10.0f; // 10px radius for handle
  }

  // Check if point is in a resize handle (for images)
  int get_resize_handle_at_point(float x, float y) const {
    if (m_selected_indices.size() != 1)
      return -1;

    int idx = m_selected_indices[0];
    if (idx < 0 || idx >= (int)m_strokes.size() || m_strokes[idx].tool != Tool::Image)
      return -1;

    // Get bounds of selected image
    float min_x, min_y, max_x, max_y;
    m_strokes[idx].get_bounds(min_x, min_y, max_x, max_y);

    float padding = 8.0f;
    min_x -= padding;
    min_y -= padding;
    max_x += padding;
    max_y += padding;

    float handle_size = 12.0f; // Slightly larger hit area

    // Check each corner handle (0=TL, 1=TR, 2=BR, 3=BL)
    if (std::abs(x - min_x) < handle_size && std::abs(y - min_y) < handle_size)
      return 0; // Top-left
    if (std::abs(x - max_x) < handle_size && std::abs(y - min_y) < handle_size)
      return 1; // Top-right
    if (std::abs(x - max_x) < handle_size && std::abs(y - max_y) < handle_size)
      return 2; // Bottom-right
    if (std::abs(x - min_x) < handle_size && std::abs(y - max_y) < handle_size)
      return 3; // Bottom-left

    return -1;
  }

  // Rotate selected shapes around a center point
  void rotate_shapes(float angle, const Point &center) {
    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        Stroke &stroke = m_strokes[idx];

        // Just update the stroke's rotation angle
        // The actual rotation will be applied during rendering
        stroke.rotation += angle;
      }
    }
  }

  // Draw rotation handle above selection box
  void draw_rotation_handle(NVGcontext *ctx) {
    if (m_selected_indices.empty())
      return;

    Point handle_pos = get_rotation_handle_pos();
    Vector2f screen_pos = canvas_to_global(Vector2f(handle_pos.x, handle_pos.y));

    // Draw line from selection box to handle
    Point center = get_selection_center();
    Vector2f center_screen = canvas_to_global(Vector2f(center.x, center.y));

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        float s_min_x, s_min_y, s_max_x, s_max_y;
        m_strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
        min_x = std::min(min_x, s_min_x);
        min_y = std::min(min_y, s_min_y);
        max_x = std::max(max_x, s_max_x);
        max_y = std::max(max_y, s_max_y);
      }
    }

    Vector2f top_center = canvas_to_global(Vector2f((min_x + max_x) / 2.0f, min_y));

    // Draw connecting line
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, top_center.x(), top_center.y());
    nvgLineTo(ctx, screen_pos.x(), screen_pos.y());
    nvgStrokeColor(ctx, Color(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);

    // Draw rotation handle circle
    nvgBeginPath(ctx);
    nvgCircle(ctx, screen_pos.x(), screen_pos.y(), 8.0f);
    nvgFillColor(ctx, Color(255, 255, 255, 255));
    nvgFill(ctx);
    nvgStrokeColor(ctx, Color(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);

    // Draw rotation icon (circular arrow)
    nvgBeginPath(ctx);
    nvgArc(ctx, screen_pos.x(), screen_pos.y(), 4.0f, 0.5f, 5.5f, NVG_CW);
    nvgStrokeColor(ctx, Color(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);
  }

  // Draw rotation angle indicator during rotation
  void draw_rotation_angle_indicator(NVGcontext *ctx) {
    if (!m_is_rotating)
      return;

    Vector2f center_screen = canvas_to_global(m_rotation_center);
    Vector2f mouse_screen = canvas_to_global(m_last_mouse_pos);

    // Convert angle to degrees
    float degrees = m_rotation_current_angle * 180.0f / M_PI;
    // Normalize to 0-360
    while (degrees < 0)
      degrees += 360.0f;
    while (degrees >= 360.0f)
      degrees -= 360.0f;

    // Draw angle text near cursor
    char angle_text[32];
    snprintf(angle_text, sizeof(angle_text), "%.1f°", degrees);

    nvgSave(ctx);
    nvgFontSize(ctx, 14.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    // Draw background box
    float bounds[4];
    nvgTextBounds(ctx, mouse_screen.x(), mouse_screen.y() - 25, angle_text, nullptr, bounds);
    float padding = 6.0f;
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, bounds[0] - padding, bounds[1] - padding,
                   bounds[2] - bounds[0] + padding * 2, bounds[3] - bounds[1] + padding * 2, 4.0f);
    nvgFillColor(ctx, Color(0, 0, 0, 200));
    nvgFill(ctx);

    // Draw text
    nvgFillColor(ctx, Color(255, 255, 255, 255));
    nvgText(ctx, mouse_screen.x(), mouse_screen.y() - 25, angle_text, nullptr);
    nvgRestore(ctx);
  }

  // Draw snap to grid feedback
  void draw_snap_feedback(NVGcontext *ctx) {
    if (!m_show_snap_feedback || m_snap_feedback_timer <= 0.0f)
      return;

    Vector2f snap_screen = canvas_to_global(m_last_snap_pos);

    nvgSave(ctx);

    // Draw a brief highlight circle at snap position
    float alpha = std::min(1.0f, m_snap_feedback_timer / 0.3f) * 180.0f;
    nvgBeginPath(ctx);
    nvgCircle(ctx, snap_screen.x(), snap_screen.y(), 8.0f * m_zoom);
    nvgStrokeColor(ctx, Color(0, 120, 215, (int)alpha));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);

    // Draw crosshair
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, snap_screen.x() - 6.0f * m_zoom, snap_screen.y());
    nvgLineTo(ctx, snap_screen.x() + 6.0f * m_zoom, snap_screen.y());
    nvgMoveTo(ctx, snap_screen.x(), snap_screen.y() - 6.0f * m_zoom);
    nvgLineTo(ctx, snap_screen.x(), snap_screen.y() + 6.0f * m_zoom);
    nvgStrokeColor(ctx, Color(0, 120, 215, (int)alpha));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);

    nvgRestore(ctx);
  }

  // Draw search highlights
  void draw_search_highlights(NVGcontext *ctx) {
    if (m_search_highlighted_indices.empty())
      return;

    nvgSave(ctx);

    for (int i = 0; i < (int)m_search_highlighted_indices.size(); i++) {
      int idx = m_search_highlighted_indices[i];
      if (idx < 0 || idx >= (int)m_strokes.size())
        continue;

      const auto &stroke = m_strokes[idx];
      if (!stroke.visible || stroke.points.empty())
        continue;

      // Get bounds of the stroke
      float min_x, min_y, max_x, max_y;
      stroke.get_bounds(min_x, min_y, max_x, max_y);

      // Convert to screen coordinates
      Vector2f top_left = canvas_to_global(Vector2f(min_x, min_y));
      Vector2f bottom_right = canvas_to_global(Vector2f(max_x, max_y));

      // Add padding
      float padding = 8.0f;
      float x = top_left.x() - padding;
      float y = top_left.y() - padding;
      float w = bottom_right.x() - top_left.x() + padding * 2;
      float h = bottom_right.y() - top_left.y() + padding * 2;

      // Draw highlight box
      nvgBeginPath(ctx);
      nvgRoundedRect(ctx, x, y, w, h, 4.0f);

      // Use different color for current result vs other results
      if (i == m_search_current_index) {
        // Current result - bright orange
        nvgStrokeColor(ctx, Color(255, 140, 0, 255));
        nvgStrokeWidth(ctx, 3.0f);
        nvgFillColor(ctx, Color(255, 140, 0, 30));
        nvgFill(ctx);
      } else {
        // Other results - yellow
        nvgStrokeColor(ctx, Color(255, 215, 0, 200));
        nvgStrokeWidth(ctx, 2.0f);
        nvgFillColor(ctx, Color(255, 215, 0, 20));
        nvgFill(ctx);
      }
      nvgStroke(ctx);
    }

    nvgRestore(ctx);
  }

  // Draw rulers at top and left edges
  void draw_rulers(NVGcontext *ctx) {
    if (!m_show_rulers)
      return;

    nvgSave(ctx);

    Vector2i abs_pos = absolute_position();
    float ruler_bg_color_r = 245, ruler_bg_color_g = 245, ruler_bg_color_b = 245;

    // Draw top ruler background
    nvgBeginPath(ctx);
    nvgRect(ctx, abs_pos.x(), abs_pos.y(), m_size.x(), m_ruler_size);
    nvgFillColor(ctx,
                 Color((int)ruler_bg_color_r, (int)ruler_bg_color_g, (int)ruler_bg_color_b, 255));
    nvgFill(ctx);

    // Draw left ruler background
    nvgBeginPath(ctx);
    nvgRect(ctx, abs_pos.x(), abs_pos.y(), m_ruler_size, m_size.y());
    nvgFillColor(ctx,
                 Color((int)ruler_bg_color_r, (int)ruler_bg_color_g, (int)ruler_bg_color_b, 255));
    nvgFill(ctx);

    // Draw corner square
    nvgBeginPath(ctx);
    nvgRect(ctx, abs_pos.x(), abs_pos.y(), m_ruler_size, m_ruler_size);
    nvgFillColor(ctx, Color((int)ruler_bg_color_r - 10, (int)ruler_bg_color_g - 10,
                            (int)ruler_bg_color_b - 10, 255));
    nvgFill(ctx);

    // Draw tick marks and numbers on top ruler
    nvgFontSize(ctx, 9.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, Color(100, 100, 100, 255));

    // Calculate visible canvas range
    Vector2f canvas_start = local_to_canvas(Vector2i(0, 0));
    Vector2f canvas_end = local_to_canvas(Vector2i(m_size.x(), m_size.y()));

    // Top ruler (horizontal)
    float start_x = std::floor(canvas_start.x() / m_grid_size) * m_grid_size;
    float end_x = std::ceil(canvas_end.x() / m_grid_size) * m_grid_size;

    for (float canvas_x = start_x; canvas_x <= end_x; canvas_x += m_grid_size) {
      Vector2f screen_pos = canvas_to_global(Vector2f(canvas_x, 0));
      float screen_x = screen_pos.x();

      if (screen_x >= abs_pos.x() + m_ruler_size && screen_x <= abs_pos.x() + m_size.x()) {
        // Draw tick mark
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, screen_x, abs_pos.y() + m_ruler_size - 5);
        nvgLineTo(ctx, screen_x, abs_pos.y() + m_ruler_size);
        nvgStrokeColor(ctx, Color(150, 150, 150, 255));
        nvgStrokeWidth(ctx, 1.0f);
        nvgStroke(ctx);

        // Draw position number
        char label[16];
        snprintf(label, sizeof(label), "%.0f", canvas_x);
        nvgText(ctx, screen_x, abs_pos.y() + m_ruler_size / 2, label, nullptr);
      }
    }

    // Left ruler (vertical)
    float start_y = std::floor(canvas_start.y() / m_grid_size) * m_grid_size;
    float end_y = std::ceil(canvas_end.y() / m_grid_size) * m_grid_size;

    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    for (float canvas_y = start_y; canvas_y <= end_y; canvas_y += m_grid_size) {
      Vector2f screen_pos = canvas_to_global(Vector2f(0, canvas_y));
      float screen_y = screen_pos.y();

      if (screen_y >= abs_pos.y() + m_ruler_size && screen_y <= abs_pos.y() + m_size.y()) {
        // Draw tick mark
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, abs_pos.x() + m_ruler_size - 5, screen_y);
        nvgLineTo(ctx, abs_pos.x() + m_ruler_size, screen_y);
        nvgStrokeColor(ctx, Color(150, 150, 150, 255));
        nvgStrokeWidth(ctx, 1.0f);
        nvgStroke(ctx);

        // Draw position number (rotated)
        char label[16];
        snprintf(label, sizeof(label), "%.0f", canvas_y);
        nvgSave(ctx);
        nvgTranslate(ctx, abs_pos.x() + m_ruler_size / 2, screen_y);
        nvgRotate(ctx, -M_PI / 2);
        nvgText(ctx, 0, 0, label, nullptr);
        nvgRestore(ctx);
      }
    }

    nvgRestore(ctx);
  }

  // Draw guide lines
  void draw_guides(NVGcontext *ctx) {
    if (!m_show_guides || m_guides.empty())
      return;

    nvgSave(ctx);

    Vector2i abs_pos = absolute_position();

    for (const auto &guide : m_guides) {
      if (!guide.visible)
        continue;

      if (guide.type == Guide::Horizontal) {
        // Horizontal guide
        Vector2f screen_pos = canvas_to_global(Vector2f(0, guide.position));
        float screen_y = screen_pos.y();

        nvgBeginPath(ctx);
        nvgMoveTo(ctx, abs_pos.x() + m_ruler_size, screen_y);
        nvgLineTo(ctx, abs_pos.x() + m_size.x(), screen_y);
        nvgStrokeColor(ctx, guide.color);
        nvgStrokeWidth(ctx, 1.5f);
        nvgStroke(ctx);
      } else {
        // Vertical guide
        Vector2f screen_pos = canvas_to_global(Vector2f(guide.position, 0));
        float screen_x = screen_pos.x();

        nvgBeginPath(ctx);
        nvgMoveTo(ctx, screen_x, abs_pos.y() + m_ruler_size);
        nvgLineTo(ctx, screen_x, abs_pos.y() + m_size.y());
        nvgStrokeColor(ctx, guide.color);
        nvgStrokeWidth(ctx, 1.5f);
        nvgStroke(ctx);
      }
    }

    nvgRestore(ctx);
  }

  bool is_point_in_selection_bounds(float x, float y) const {
    if (m_selected_indices.empty())
      return false;

    // Calculate bounding box of all selected shapes
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        float s_min_x, s_min_y, s_max_x, s_max_y;
        m_strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
        min_x = std::min(min_x, s_min_x);
        min_y = std::min(min_y, s_min_y);
        max_x = std::max(max_x, s_max_x);
        max_y = std::max(max_y, s_max_y);
      }
    }

    if (min_x > max_x || min_y > max_y)
      return false;

    float padding = 8.0f;
    min_x -= padding;
    min_y -= padding;
    max_x += padding;
    max_y += padding;

    return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
  }

  void draw_selection_box(NVGcontext *ctx, const Stroke &stroke) {
    float min_x, min_y, max_x, max_y;
    stroke.get_bounds(min_x, min_y, max_x, max_y);

    float padding = 4.0f;
    min_x -= padding;
    min_y -= padding;
    max_x += padding;
    max_y += padding;

    Vector2f top_left = canvas_to_global(Vector2f(min_x, min_y));
    Vector2f size = Vector2f(max_x - min_x, max_y - min_y) * m_zoom;

    // Draw dotted/dashed rectangle
    nvgSave(ctx);

    // Set up dash pattern: 5px dash, 5px gap
    nvgLineCap(ctx, NVG_BUTT);
    nvgLineJoin(ctx, NVG_MITER);

    // Draw the rectangle with dashed lines
    float x = top_left.x();
    float y = top_left.y();
    float w = size.x();
    float h = size.y();

    float dash_length = 8.0f;
    float gap_length = 4.0f;

    nvgStrokeColor(ctx, Color(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);

    // Draw dashed lines manually
    draw_dashed_line(ctx, x, y, x + w, y, dash_length, gap_length);         // Top
    draw_dashed_line(ctx, x + w, y, x + w, y + h, dash_length, gap_length); // Right
    draw_dashed_line(ctx, x + w, y + h, x, y + h, dash_length, gap_length); // Bottom
    draw_dashed_line(ctx, x, y + h, x, y, dash_length, gap_length);         // Left

    nvgRestore(ctx);
  }

  void draw_group_selection_box(NVGcontext *ctx) {
    if (m_selected_indices.empty())
      return;

    // Calculate bounding box of all selected shapes
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();

    for (int idx : m_selected_indices) {
      if (idx >= 0 && idx < (int)m_strokes.size()) {
        float s_min_x, s_min_y, s_max_x, s_max_y;
        m_strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
        min_x = std::min(min_x, s_min_x);
        min_y = std::min(min_y, s_min_y);
        max_x = std::max(max_x, s_max_x);
        max_y = std::max(max_y, s_max_y);
      }
    }

    if (min_x > max_x || min_y > max_y)
      return;

    float padding = 8.0f;
    min_x -= padding;
    min_y -= padding;
    max_x += padding;
    max_y += padding;

    Vector2f top_left = canvas_to_global(Vector2f(min_x, min_y));
    Vector2f size = Vector2f(max_x - min_x, max_y - min_y) * m_zoom;

    // Draw large dashed rectangle around all selected shapes
    nvgSave(ctx);

    float x = top_left.x();
    float y = top_left.y();
    float w = size.x();
    float h = size.y();

    float dash_length = 10.0f;
    float gap_length = 5.0f;

    nvgStrokeColor(ctx, Color(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.5f);

    // Draw dashed lines
    draw_dashed_line(ctx, x, y, x + w, y, dash_length, gap_length);         // Top
    draw_dashed_line(ctx, x + w, y, x + w, y + h, dash_length, gap_length); // Right
    draw_dashed_line(ctx, x + w, y + h, x, y + h, dash_length, gap_length); // Bottom
    draw_dashed_line(ctx, x, y + h, x, y, dash_length, gap_length);         // Left

    // Draw resize handles for images (corner handles only)
    if (m_selected_indices.size() == 1) {
      int idx = m_selected_indices[0];
      if (idx >= 0 && idx < (int)m_strokes.size() && m_strokes[idx].tool == Tool::Image) {
        float handle_size = 8.0f;

        // Draw corner handles
        draw_resize_handle(ctx, x, y, handle_size);         // Top-left
        draw_resize_handle(ctx, x + w, y, handle_size);     // Top-right
        draw_resize_handle(ctx, x + w, y + h, handle_size); // Bottom-right
        draw_resize_handle(ctx, x, y + h, handle_size);     // Bottom-left
      }
    }

    nvgRestore(ctx);
  }

  void draw_resize_handle(NVGcontext *ctx, float x, float y, float size) {
    nvgBeginPath(ctx);
    nvgRect(ctx, x - size / 2, y - size / 2, size, size);
    nvgFillColor(ctx, Color(255, 255, 255, 255));
    nvgFill(ctx);
    nvgStrokeColor(ctx, Color(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);
  }

  void draw_selection_marquee(NVGcontext *ctx) {
    float min_x = std::min(m_selection_start.x(), m_selection_end.x());
    float min_y = std::min(m_selection_start.y(), m_selection_end.y());
    float max_x = std::max(m_selection_start.x(), m_selection_end.x());
    float max_y = std::max(m_selection_start.y(), m_selection_end.y());

    Vector2f top_left = canvas_to_global(Vector2f(min_x, min_y));
    Vector2f size = Vector2f(max_x - min_x, max_y - min_y) * m_zoom;

    float x = top_left.x();
    float y = top_left.y();
    float w = size.x();
    float h = size.y();

    // Draw dotted selection rectangle
    nvgSave(ctx);

    float dash_length = 6.0f;
    float gap_length = 3.0f;

    nvgStrokeColor(ctx, Color(0, 120, 215, 200));
    nvgStrokeWidth(ctx, 1.5f);

    // Draw dashed lines
    draw_dashed_line(ctx, x, y, x + w, y, dash_length, gap_length);         // Top
    draw_dashed_line(ctx, x + w, y, x + w, y + h, dash_length, gap_length); // Right
    draw_dashed_line(ctx, x + w, y + h, x, y + h, dash_length, gap_length); // Bottom
    draw_dashed_line(ctx, x, y + h, x, y, dash_length, gap_length);         // Left

    // Draw semi-transparent fill
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, Color(0, 120, 215, 30));
    nvgFill(ctx);

    nvgRestore(ctx);
  }

  void draw_dashed_line(NVGcontext *ctx, float x1, float y1, float x2, float y2, float dash_length,
                        float gap_length) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);

    if (length < 0.001f)
      return;

    float ux = dx / length;
    float uy = dy / length;

    float pattern_length = dash_length + gap_length;
    float current = 0.0f;

    while (current < length) {
      float dash_end = std::min(current + dash_length, length);

      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x1 + ux * current, y1 + uy * current);
      nvgLineTo(ctx, x1 + ux * dash_end, y1 + uy * dash_end);
      nvgStroke(ctx);

      current += pattern_length;
    }
  }

  void draw_stroke(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.empty())
      return;

    switch (stroke.tool) {
    case Tool::Pen:
      draw_pen(ctx, stroke);
      break;
    case Tool::Text:
      draw_text(ctx, stroke);
      break;
    case Tool::Sticky:
      draw_sticky(ctx, stroke);
      break;
    case Tool::Rectangle:
      draw_rectangle(ctx, stroke);
      break;
    case Tool::Circle:
      draw_circle(ctx, stroke);
      break;
    case Tool::Line:
      draw_line(ctx, stroke);
      break;
    case Tool::Arrow:
      draw_arrow(ctx, stroke);
      break;
    case Tool::Image:
      draw_image(ctx, stroke);
      break;
    default:
      break;
    }
  }

  void draw_infinite_grid(NVGcontext *ctx) {
    float grid_size = 50.0f; // Grid cell size in canvas coordinates

    // Calculate visible canvas area
    Vector2f top_left_canvas = local_to_canvas(Vector2i(0, 0));
    Vector2f bottom_right_canvas = local_to_canvas(Vector2i(m_size.x(), m_size.y()));

    float start_x = std::floor(top_left_canvas.x() / grid_size) * grid_size;
    float end_x = std::ceil(bottom_right_canvas.x() / grid_size) * grid_size;
    float start_y = std::floor(top_left_canvas.y() / grid_size) * grid_size;
    float end_y = std::ceil(bottom_right_canvas.y() / grid_size) * grid_size;

    nvgSave(ctx);
    nvgBeginPath(ctx);

    // Draw vertical lines
    for (float x = start_x; x <= end_x; x += grid_size) {
      Vector2f top = canvas_to_global(Vector2f(x, top_left_canvas.y()));
      Vector2f bottom = canvas_to_global(Vector2f(x, bottom_right_canvas.y()));
      nvgMoveTo(ctx, top.x(), top.y());
      nvgLineTo(ctx, bottom.x(), bottom.y());
    }

    // Draw horizontal lines
    for (float y = start_y; y <= end_y; y += grid_size) {
      Vector2f left = canvas_to_global(Vector2f(top_left_canvas.x(), y));
      Vector2f right = canvas_to_global(Vector2f(bottom_right_canvas.x(), y));
      nvgMoveTo(ctx, left.x(), left.y());
      nvgLineTo(ctx, right.x(), right.y());
    }

    nvgStrokeColor(ctx, Color(220, 220, 220, 100));
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);
    nvgRestore(ctx);
  }

  void draw_coordinate_display(NVGcontext *ctx) {
    if (!m_show_coordinates)
      return;

    Vector2i abs_pos = absolute_position();
    float box_width = 320.0f;
    float box_height = 25.0f;
    float x = abs_pos.x() + m_size.x() - box_width - 10;  // Right side
    float y = abs_pos.y() + m_size.y() - box_height - 10; // Bottom

    // Background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, box_width, box_height, 4);
    nvgFillColor(ctx, Color(0, 0, 0, 180));
    nvgFill(ctx);

    // Text - now includes mouse position
    char text[150];
    snprintf(text, sizeof(text), "Zoom: %.0f%% | Cursor: (%.0f, %.0f) | Pan: (%.0f, %.0f)",
             m_zoom * 100, m_last_mouse_pos.x(), m_last_mouse_pos.y(), -m_pan_offset.x(),
             -m_pan_offset.y());

    nvgFontSize(ctx, 12.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, Color(255, 255, 255, 255));
    nvgText(ctx, x + 8, y + 12.5f, text, nullptr);
  }

  void draw_minimap(NVGcontext *ctx) {
    if (!m_show_minimap)
      return;

    Vector2i abs_pos = absolute_position();
    float minimap_size = 150.0f;
    float margin = 10.0f;
    float x = abs_pos.x() + m_size.x() - minimap_size - margin;
    float y = abs_pos.y() + margin;

    // Background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, minimap_size, minimap_size, 4);
    nvgFillColor(ctx, Color(255, 255, 255, 200));
    nvgFill(ctx);
    nvgStrokeColor(ctx, Color(0, 0, 0, 100));
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);

    if (m_strokes.empty())
      return;

    // Calculate bounds of all content
    float content_min_x = std::numeric_limits<float>::max();
    float content_min_y = std::numeric_limits<float>::max();
    float content_max_x = std::numeric_limits<float>::lowest();
    float content_max_y = std::numeric_limits<float>::lowest();

    for (const auto &stroke : m_strokes) {
      float s_min_x, s_min_y, s_max_x, s_max_y;
      stroke.get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
      content_min_x = std::min(content_min_x, s_min_x);
      content_min_y = std::min(content_min_y, s_min_y);
      content_max_x = std::max(content_max_x, s_max_x);
      content_max_y = std::max(content_max_y, s_max_y);
    }

    if (content_min_x > content_max_x)
      return;

    float content_width = content_max_x - content_min_x;
    float content_height = content_max_y - content_min_y;
    float padding = 50.0f;
    content_width += padding * 2;
    content_height += padding * 2;
    content_min_x -= padding;
    content_min_y -= padding;

    // Calculate scale to fit content in minimap
    float scale =
        std::min((minimap_size - 20) / content_width, (minimap_size - 20) / content_height);

    nvgSave(ctx);
    nvgScissor(ctx, x + 2, y + 2, minimap_size - 4, minimap_size - 4);

    // Draw content
    for (const auto &stroke : m_strokes) {
      if (stroke.points.empty())
        continue;

      nvgBeginPath(ctx);
      for (size_t i = 0; i < stroke.points.size(); ++i) {
        float px = x + 10 + (stroke.points[i].x - content_min_x) * scale;
        float py = y + 10 + (stroke.points[i].y - content_min_y) * scale;
        if (i == 0)
          nvgMoveTo(ctx, px, py);
        else
          nvgLineTo(ctx, px, py);
      }
      nvgStrokeColor(ctx, Color(100, 100, 100, 150));
      nvgStrokeWidth(ctx, 1.0f);
      nvgStroke(ctx);
    }

    // Draw viewport rectangle
    Vector2f view_tl = local_to_canvas(Vector2i(0, 0));
    Vector2f view_br = local_to_canvas(Vector2i(m_size.x(), m_size.y()));

    float vx1 = x + 10 + (view_tl.x() - content_min_x) * scale;
    float vy1 = y + 10 + (view_tl.y() - content_min_y) * scale;
    float vx2 = x + 10 + (view_br.x() - content_min_x) * scale;
    float vy2 = y + 10 + (view_br.y() - content_min_y) * scale;

    nvgBeginPath(ctx);
    nvgRect(ctx, vx1, vy1, vx2 - vx1, vy2 - vy1);
    nvgStrokeColor(ctx, Color(0, 120, 215, 255));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);
    nvgFillColor(ctx, Color(0, 120, 215, 30));
    nvgFill(ctx);

    nvgResetScissor(ctx);
    nvgRestore(ctx);
  }

  void draw_pen(NVGcontext *ctx, const Stroke &stroke) {
    nvgSave(ctx);

    // Apply rotation if needed
    if (std::abs(stroke.rotation) > 0.0001f) {
      // Get center of stroke for rotation
      float min_x, min_y, max_x, max_y;
      stroke.get_bounds(min_x, min_y, max_x, max_y);
      float center_x = (min_x + max_x) / 2.0f;
      float center_y = (min_y + max_y) / 2.0f;
      Vector2f center_screen = canvas_to_global(Vector2f(center_x, center_y));

      nvgTranslate(ctx, center_screen.x(), center_screen.y());
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -center_screen.x(), -center_screen.y());
    }

    if (stroke.points.size() == 1) {
      Vector2f center = point_to_global(stroke.points[0]);
      nvgBeginPath(ctx);
      nvgCircle(ctx, center.x(), center.y(), (stroke.width * m_zoom) / 2.f);
      nvgFillColor(ctx, stroke.color);
      nvgFill(ctx);
      nvgRestore(ctx);
      return;
    }

    nvgBeginPath(ctx);
    Vector2f start = point_to_global(stroke.points.front());
    nvgMoveTo(ctx, start.x(), start.y());
    for (size_t i = 1; i < stroke.points.size(); ++i) {
      Vector2f pt = point_to_global(stroke.points[i]);
      nvgLineTo(ctx, pt.x(), pt.y());
    }
    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width * m_zoom);
    nvgLineCap(ctx, NVG_ROUND);
    nvgLineJoin(ctx, NVG_ROUND);
    nvgStroke(ctx);

    nvgRestore(ctx);
  }

  void draw_sticky(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    nvgSave(ctx);

    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float x = std::min(x1, x2), y = std::min(y1, y2);
    float w = std::abs(x2 - x1), h = std::abs(y2 - y1);

    // Apply rotation if needed
    if (std::abs(stroke.rotation) > 0.0001f) {
      float center_x = (x1 + x2) / 2.0f;
      float center_y = (y1 + y2) / 2.0f;
      Vector2f center_screen = canvas_to_global(Vector2f(center_x, center_y));

      nvgTranslate(ctx, center_screen.x(), center_screen.y());
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -center_screen.x(), -center_screen.y());
    }

    Vector2f top_left = canvas_to_global(Vector2f(x, y));
    nvgBeginPath(ctx);
    nvgRect(ctx, top_left.x(), top_left.y(), w * m_zoom, h * m_zoom);
    nvgFillColor(ctx, stroke.fill_color);
    nvgFill(ctx);
    nvgStrokeColor(ctx, Color(200, 200, 100, 255));
    nvgStrokeWidth(ctx, 2.0f * m_zoom);
    nvgStroke(ctx);

    nvgRestore(ctx);
  }

  void draw_rectangle(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    nvgSave(ctx);

    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float x = std::min(x1, x2), y = std::min(y1, y2);
    float w = std::abs(x2 - x1), h = std::abs(y2 - y1);

    // Apply rotation if needed
    if (std::abs(stroke.rotation) > 0.0001f) {
      float center_x = (x1 + x2) / 2.0f;
      float center_y = (y1 + y2) / 2.0f;
      Vector2f center_screen = canvas_to_global(Vector2f(center_x, center_y));

      nvgTranslate(ctx, center_screen.x(), center_screen.y());
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -center_screen.x(), -center_screen.y());
    }

    Vector2f top_left = canvas_to_global(Vector2f(x, y));
    nvgBeginPath(ctx);
    nvgRect(ctx, top_left.x(), top_left.y(), w * m_zoom, h * m_zoom);

    if (stroke.fill_style != FillStyle::None) {
      nvgFillColor(ctx, stroke.fill_color);
      nvgFill(ctx);
    }

    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width * m_zoom);
    nvgStroke(ctx);

    nvgRestore(ctx);
  }

  void draw_circle(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    nvgSave(ctx);

    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float dx = x2 - x1, dy = y2 - y1;
    float radius = std::sqrt(dx * dx + dy * dy);

    // Apply rotation if needed (circles don't visually rotate, but we keep consistency)
    if (std::abs(stroke.rotation) > 0.0001f) {
      Vector2f center_screen = canvas_to_global(Vector2f(x1, y1));

      nvgTranslate(ctx, center_screen.x(), center_screen.y());
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -center_screen.x(), -center_screen.y());
    }

    Vector2f center = canvas_to_global(Vector2f(x1, y1));
    nvgBeginPath(ctx);
    nvgCircle(ctx, center.x(), center.y(), radius * m_zoom);

    if (stroke.fill_style != FillStyle::None) {
      nvgFillColor(ctx, stroke.fill_color);
      nvgFill(ctx);
    }

    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width * m_zoom);
    nvgStroke(ctx);

    nvgRestore(ctx);
  }

  void draw_line(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    nvgSave(ctx);

    // Apply rotation if needed
    if (std::abs(stroke.rotation) > 0.0001f) {
      float center_x = (stroke.points[0].x + stroke.points[1].x) / 2.0f;
      float center_y = (stroke.points[0].y + stroke.points[1].y) / 2.0f;
      Vector2f center_screen = canvas_to_global(Vector2f(center_x, center_y));

      nvgTranslate(ctx, center_screen.x(), center_screen.y());
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -center_screen.x(), -center_screen.y());
    }

    Vector2f p0 = point_to_global(stroke.points[0]);
    Vector2f p1 = point_to_global(stroke.points[1]);

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, p0.x(), p0.y());
    nvgLineTo(ctx, p1.x(), p1.y());
    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width * m_zoom);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStroke(ctx);

    nvgRestore(ctx);
  }

  void draw_arrow(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    nvgSave(ctx);

    // Apply rotation if needed
    if (std::abs(stroke.rotation) > 0.0001f) {
      float center_x = (stroke.points[0].x + stroke.points[1].x) / 2.0f;
      float center_y = (stroke.points[0].y + stroke.points[1].y) / 2.0f;
      Vector2f center_screen = canvas_to_global(Vector2f(center_x, center_y));

      nvgTranslate(ctx, center_screen.x(), center_screen.y());
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -center_screen.x(), -center_screen.y());
    }

    Vector2f p0 = point_to_global(stroke.points[0]);
    Vector2f p1 = point_to_global(stroke.points[1]);
    float x1 = p0.x(), y1 = p0.y();
    float x2 = p1.x(), y2 = p1.y();

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x1, y1);
    nvgLineTo(ctx, x2, y2);
    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width * m_zoom);
    nvgStroke(ctx);

    float angle = std::atan2(y2 - y1, x2 - x1);
    float arrow_size = stroke.width * 3 * m_zoom;

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x2, y2);
    nvgLineTo(ctx, x2 - arrow_size * std::cos(angle - 0.5f),
              y2 - arrow_size * std::sin(angle - 0.5f));
    nvgLineTo(ctx, x2 - arrow_size * std::cos(angle + 0.5f),
              y2 - arrow_size * std::sin(angle + 0.5f));
    nvgClosePath(ctx);
    nvgFillColor(ctx, stroke.color);
    nvgFill(ctx);

    nvgRestore(ctx);
  }

  void draw_text(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.empty() || stroke.text.empty())
      return;

    nvgSave(ctx);

    Vector2f pos = canvas_to_global(Vector2f(stroke.points[0].x, stroke.points[0].y));

    // Apply rotation if needed
    if (std::abs(stroke.rotation) > 0.0001f) {
      nvgTranslate(ctx, pos.x(), pos.y());
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -pos.x(), -pos.y());
    }

    nvgFontSize(ctx, stroke.font_size * m_zoom);
    nvgFontFace(ctx, stroke.font_face.c_str());
    nvgTextAlign(ctx, stroke.text_align);
    nvgFillColor(ctx, stroke.color);
    nvgText(ctx, pos.x(), pos.y(), stroke.text.c_str(), nullptr);
    nvgRestore(ctx);
  }

  void draw_image(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2 || stroke.nvg_image_handle < 0)
      return;

    nvgSave(ctx);

    // Get image bounds
    Vector2f p1 = canvas_to_global(Vector2f(stroke.points[0].x, stroke.points[0].y));
    Vector2f p2 = canvas_to_global(Vector2f(stroke.points[1].x, stroke.points[1].y));

    float x = std::min(p1.x(), p2.x());
    float y = std::min(p1.y(), p2.y());
    float w = std::abs(p2.x() - p1.x());
    float h = std::abs(p2.y() - p1.y());

    // Apply rotation if needed
    if (std::abs(stroke.rotation) > 0.0001f) {
      float cx = x + w / 2.0f;
      float cy = y + h / 2.0f;
      nvgTranslate(ctx, cx, cy);
      nvgRotate(ctx, stroke.rotation);
      nvgTranslate(ctx, -cx, -cy);
    }

    // Create image pattern
    NVGpaint imgPaint = nvgImagePattern(ctx, x, y, w, h, 0.0f, stroke.nvg_image_handle, 1.0f);

    // Draw image
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillPaint(ctx, imgPaint);
    nvgFill(ctx);

    // Draw border if selected
    if (stroke.selected) {
      nvgBeginPath(ctx);
      nvgRect(ctx, x, y, w, h);
      nvgStrokeColor(ctx, Color(0, 120, 215, 255));
      nvgStrokeWidth(ctx, 2.0f);
      nvgStroke(ctx);
    }

    nvgRestore(ctx);
  }
};


}
