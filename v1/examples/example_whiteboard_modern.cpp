/*
    examples/example_whiteboard_modern.cpp -- Modern Collaborative Whiteboard

    A modern whiteboard application inspired by Miro/Figma with:
    - Clean, minimal UI with floating panels
    - Left sidebar with tool icons
    - Sticky notes and shapes
    - Zoom & Pan navigation
    - Undo/Redo support
    - Selection and moving objects

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
*/

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stack>
#include <vector>

#include <nanogui.h>
#include <nanogui/fluent_web_message_bar.h>
#include <nanogui/fluent_web_theme.h>
#include <nanogui/opengl.h>
#include <nanovg.h>


#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

#if defined(NANOGUI_USE_SDL3)
  #include <SDL3/SDL.h>
  #define KEY_ESCAPE SDLK_ESCAPE
  #define KEY_Z SDLK_Z
  #define KEY_Y SDLK_Y
  #define KEY_DELETE SDLK_DELETE
  #define KEY_BACKSPACE SDLK_BACKSPACE
  #define KEY_SPACE SDLK_SPACE
  #define KEY_PRESS 1
  #define KEY_RELEASE 0
  #define MOD_CTRL SDL_KMOD_CTRL
  #define MOD_SHIFT SDL_KMOD_SHIFT
#else
  #include <GLFW/glfw3.h>
  #define KEY_ESCAPE GLFW_KEY_ESCAPE
  #define KEY_Z GLFW_KEY_Z
  #define KEY_Y GLFW_KEY_Y
  #define KEY_DELETE GLFW_KEY_DELETE
  #define KEY_BACKSPACE GLFW_KEY_BACKSPACE
  #define KEY_SPACE GLFW_KEY_SPACE
  #define KEY_PRESS GLFW_PRESS
  #define KEY_RELEASE GLFW_RELEASE
  #define MOD_CTRL GLFW_MOD_CONTROL
  #define MOD_SHIFT GLFW_MOD_SHIFT
#endif

using namespace nanogui;

enum class Tool { Select, Pan, Pen, Sticky, Rectangle, Circle, Line, Arrow };

enum class FillStyle { None, Solid };

struct Point {
  float x, y;
  Point(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Stroke {
  std::vector<Point> points;
  Color color;
  float width;
  Tool tool;
  FillStyle fill_style;
  Color fill_color;
  bool selected;

  Stroke() : width(3.0f), tool(Tool::Pen), fill_style(FillStyle::None), selected(false) {}

  void get_bounds(float &min_x, float &min_y, float &max_x, float &max_y) const {
    if (points.empty())
      return;
    min_x = max_x = points[0].x;
    min_y = max_y = points[0].y;
    for (const auto &p : points) {
      min_x = std::min(min_x, p.x);
      max_x = std::max(max_x, p.x);
      min_y = std::min(min_y, p.y);
      max_y = std::max(max_y, p.y);
    }
  }

  bool contains_point(float x, float y, float margin = 10.0f) const {
    float min_x, min_y, max_x, max_y;
    get_bounds(min_x, min_y, max_x, max_y);
    return x >= min_x - margin && x <= max_x + margin && y >= min_y - margin && y <= max_y + margin;
  }

  void move(float dx, float dy) {
    for (auto &p : points) {
      p.x += dx;
      p.y += dy;
    }
  }
};

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
  }

  virtual void draw(NVGcontext *ctx) override {
    Canvas::draw(ctx);

    // Draw infinite grid
    if (m_show_grid) {
      draw_infinite_grid(ctx);
    }

    // Draw all strokes
    for (const auto &stroke : m_strokes) {
      draw_stroke(ctx, stroke);
    }

    // Draw current stroke
    if (m_is_drawing && !m_current_stroke.points.empty()) {
      draw_stroke(ctx, m_current_stroke);
    }

    // Draw selection marquee while selecting
    if (m_is_selecting_area) {
      draw_selection_marquee(ctx);
    }

    // Draw single bounding box around all selected shapes
    if (!m_selected_indices.empty() && !m_is_selecting_area) {
      draw_group_selection_box(ctx);
    }

    // Draw coordinate display
    draw_coordinate_display(ctx);

    // Draw minimap
    draw_minimap(ctx);
  }

  virtual bool mouse_button_event(const Vector2i &p, int button, bool down,
                                  int modifiers) override {
    Vector2i local_p = p - absolute_position();

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

      // Start drawing
      m_is_drawing = true;
      m_current_stroke.points.clear();
      m_current_stroke.color = m_current_color;
      m_current_stroke.width = m_current_width;
      m_current_stroke.tool = m_current_tool;
      m_current_stroke.fill_style = m_fill_style;
      m_current_stroke.fill_color = m_fill_color;
      m_current_stroke.selected = false;
      m_current_stroke.points.push_back(Point(canvas_p.x(), canvas_p.y()));
      return true;
    }

    // Mouse released
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

    if (m_is_moving_shape) {
      m_is_moving_shape = false;
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

    if (m_selection_mode && m_is_moving_shape && !m_selected_indices.empty()) {
      Vector2f delta = canvas_p - m_move_start_canvas;
      for (int idx : m_selected_indices) {
        if (idx >= 0 && idx < (int)m_strokes.size()) {
          m_strokes[idx].move(delta.x(), delta.y());
        }
      }
      m_move_start_canvas = canvas_p;
      return true;
    }

    if (m_is_drawing) {
      if (m_current_tool == Tool::Pen) {
        m_current_stroke.points.push_back(Point(canvas_p.x(), canvas_p.y()));
      } else {
        if (m_current_stroke.points.size() > 1)
          m_current_stroke.points[1] = Point(canvas_p.x(), canvas_p.y());
        else
          m_current_stroke.points.push_back(Point(canvas_p.x(), canvas_p.y()));
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
    }
  }

  void redo() {
    if (!m_redo_stack.empty()) {
      m_undo_stack.push(m_strokes);
      m_strokes = m_redo_stack.top();
      m_redo_stack.pop();
      clear_selection();
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
        m_strokes.erase(m_strokes.begin() + idx);
      }
    }

    clear_selection();
    m_redo_stack = std::stack<std::vector<Stroke>>();
  }

  void clear() {
    m_undo_stack.push(m_strokes);
    m_strokes.clear();
    clear_selection();
    m_redo_stack = std::stack<std::vector<Stroke>>();
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

  Vector2f to_vec(const Vector2i &value) const {
    return Vector2f(static_cast<float>(value.x()), static_cast<float>(value.y()));
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
      if (m_strokes[i].contains_point(x, y)) {
        return i;
      }
    }
    return -1;
  }

  void clear_selection() {
    for (auto &stroke : m_strokes) {
      stroke.selected = false;
    }
    m_selected_indices.clear();
  }

  bool is_selected(int index) const {
    return std::find(m_selected_indices.begin(), m_selected_indices.end(), index) !=
           m_selected_indices.end();
  }

  void add_to_selection(int index) {
    if (index >= 0 && index < (int)m_strokes.size() && !is_selected(index)) {
      m_selected_indices.push_back(index);
      m_strokes[index].selected = true;
    }
  }

  void remove_from_selection(int index) {
    auto it = std::find(m_selected_indices.begin(), m_selected_indices.end(), index);
    if (it != m_selected_indices.end()) {
      m_selected_indices.erase(it);
      if (index >= 0 && index < (int)m_strokes.size()) {
        m_strokes[index].selected = false;
      }
    }
  }

  void select_shapes_in_area() {
    float min_x = std::min(m_selection_start.x(), m_selection_end.x());
    float min_y = std::min(m_selection_start.y(), m_selection_end.y());
    float max_x = std::max(m_selection_start.x(), m_selection_end.x());
    float max_y = std::max(m_selection_start.y(), m_selection_end.y());

    for (int i = 0; i < (int)m_strokes.size(); i++) {
      float s_min_x, s_min_y, s_max_x, s_max_y;
      m_strokes[i].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);

      // Check if shape is completely within selection area
      if (s_min_x >= min_x && s_max_x <= max_x && s_min_y >= min_y && s_max_y <= max_y) {
        add_to_selection(i);
      }
    }
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

    nvgRestore(ctx);
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
    if (stroke.points.size() == 1) {
      Vector2f center = point_to_global(stroke.points[0]);
      nvgBeginPath(ctx);
      nvgCircle(ctx, center.x(), center.y(), (stroke.width * m_zoom) / 2.f);
      nvgFillColor(ctx, stroke.color);
      nvgFill(ctx);
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
  }

  void draw_sticky(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float x = std::min(x1, x2), y = std::min(y1, y2);
    float w = std::abs(x2 - x1), h = std::abs(y2 - y1);

    Vector2f top_left = canvas_to_global(Vector2f(x, y));
    nvgBeginPath(ctx);
    nvgRect(ctx, top_left.x(), top_left.y(), w * m_zoom, h * m_zoom);
    nvgFillColor(ctx, stroke.fill_color);
    nvgFill(ctx);
    nvgStrokeColor(ctx, Color(200, 200, 100, 255));
    nvgStrokeWidth(ctx, 2.0f * m_zoom);
    nvgStroke(ctx);
  }

  void draw_rectangle(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float x = std::min(x1, x2), y = std::min(y1, y2);
    float w = std::abs(x2 - x1), h = std::abs(y2 - y1);

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
  }

  void draw_circle(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    float x1 = stroke.points[0].x, y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x, y2 = stroke.points[1].y;
    float dx = x2 - x1, dy = y2 - y1;
    float radius = std::sqrt(dx * dx + dy * dy);

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
  }

  void draw_line(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    Vector2f p0 = point_to_global(stroke.points[0]);
    Vector2f p1 = point_to_global(stroke.points[1]);

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, p0.x(), p0.y());
    nvgLineTo(ctx, p1.x(), p1.y());
    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width * m_zoom);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStroke(ctx);
  }

  void draw_arrow(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

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
  }
};

class ModernWhiteboardApp : public Screen {
public:
  ModernWhiteboardApp() : Screen(Vector2i(1400, 900), "Modern Whiteboard - Collaborative Design") {
    m_theme = new FluentWebTheme(nvg_context());
    set_theme(m_theme);
    set_background(Color(250, 250, 250, 255));

    // Create main canvas (below toolbar)
    m_canvas = new ModernCanvas(this);
    m_canvas->set_position(Vector2i(0, 60));
    m_canvas->set_fixed_size(Vector2i(width(), height() - 60));
    m_canvas->set_background_color(Color(245, 245, 245, 255));

    // Create UI
    create_top_toolbar();
    create_left_sidebar();
    create_floating_panels();
    create_zoom_controls();

    perform_layout(nvg_context());

    std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Modern Whiteboard v3.0              ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    std::cout << "\nFeatures:" << std::endl;
    std::cout << "  ✨ Modern UI inspired by Miro/Figma" << std::endl;
    std::cout << "  ✨ Left sidebar with tool icons" << std::endl;
    std::cout << "  ✨ Floating panels for content" << std::endl;
    std::cout << "  ✨ Sticky notes and shapes" << std::endl;
    std::cout << "  ✨ Zoom & Pan navigation" << std::endl;
    std::cout << "\nControls:" << std::endl;
    std::cout << "  🖱️  Click sidebar icons to select tools" << std::endl;
    std::cout << "  🖱️  Drag to create shapes and notes" << std::endl;
    std::cout << "  🖱️  Middle mouse or Hand tool to pan" << std::endl;
    std::cout << "  🖱️  Mouse wheel to zoom in/out" << std::endl;
    std::cout << "  ⌨️  Hold SPACE + drag for temporary pan" << std::endl;
    std::cout << "  ⌨️  Press Delete to remove selected items" << std::endl;
    std::cout << "  ⌨️  Ctrl+Z/Y for Undo/Redo" << std::endl;
    std::cout << "\nPress ESC to exit\n" << std::endl;
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;

    if (action == KEY_PRESS) {
      if (key == KEY_ESCAPE) {
        set_visible(false);
        return true;
      }
      if (key == KEY_SPACE) {
        m_canvas->enable_space_pan();
        show_notification("Pan Mode (Hold Space)");
        return true;
      }
      if ((modifiers & MOD_CTRL) && key == KEY_Z) {
        m_canvas->undo();
        show_notification("Undo");
        return true;
      }
      if ((modifiers & MOD_CTRL) && key == KEY_Y) {
        m_canvas->redo();
        show_notification("Redo");
        return true;
      }
      if (key == KEY_DELETE || key == KEY_BACKSPACE) {
        m_canvas->delete_selected();
        show_notification("Deleted");
        return true;
      }
    }

    if (action == KEY_RELEASE) {
      if (key == KEY_SPACE) {
        m_canvas->disable_space_pan();
        return true;
      }
    }

    return false;
  }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override {
    if (button == NANOGUI_MOUSE_BUTTON_LEFT) {
      if (down) {
        // Check if clicking on left sidebar resize edge (check BEFORE Screen event)
        if (m_left_sidebar) {
          Vector2i sidebar_pos = m_left_sidebar->absolute_position();
          int right_edge = sidebar_pos.x() + m_left_sidebar->width();
          if (p.x() >= right_edge - 5 && p.x() <= right_edge + 5 && p.y() >= sidebar_pos.y() &&
              p.y() <= sidebar_pos.y() + m_left_sidebar->height()) {
            m_resizing_left_sidebar = true;
            return true;
          }
        }

        // Check if clicking on zoom panel resize edge
        if (m_zoom_panel) {
          Vector2i panel_pos = m_zoom_panel->absolute_position();
          int left_edge = panel_pos.x();
          if (p.x() >= left_edge - 5 && p.x() <= left_edge + 5 && p.y() >= panel_pos.y() &&
              p.y() <= panel_pos.y() + m_zoom_panel->height()) {
            m_resizing_zoom_panel = true;
            return true;
          }
        }
      } else {
        m_resizing_left_sidebar = false;
        m_resizing_zoom_panel = false;
      }
    }

    // Let Screen handle other events
    if (Screen::mouse_button_event(p, button, down, modifiers))
      return true;

    return false;
  }

  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                        int modifiers) override {
    if (Screen::mouse_drag_event(p, rel, button, modifiers))
      return true;

    if (m_resizing_left_sidebar && m_left_sidebar) {
      Vector2i sidebar_pos = m_left_sidebar->position();
      int new_width = p.x() - sidebar_pos.x() - 15; // Account for initial offset
      m_left_sidebar_width = nanogui::clip(new_width, 50, 200);
      m_left_sidebar->set_fixed_width(m_left_sidebar_width);
      perform_layout(nvg_context());
      return true;
    }

    if (m_resizing_zoom_panel && m_zoom_panel) {
      int new_width = width() - p.x() - 20; // Distance from right edge
      m_zoom_panel_width = nanogui::clip(new_width, 50, 150);
      m_zoom_panel->set_fixed_width(m_zoom_panel_width);
      m_zoom_panel->set_position(
          Vector2i(width() - m_zoom_panel_width - 20, m_zoom_panel->position().y()));
      perform_layout(nvg_context());
      return true;
    }

    return false;
  }

  bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                          int modifiers) override {
    if (Screen::mouse_motion_event(p, rel, button, modifiers))
      return true;

    // Change cursor when hovering over resize edges
    bool on_resize_edge = false;

    if (m_left_sidebar) {
      Vector2i sidebar_pos = m_left_sidebar->absolute_position();
      int right_edge = sidebar_pos.x() + m_left_sidebar->width();
      if (p.x() >= right_edge - 5 && p.x() <= right_edge + 5 && p.y() >= sidebar_pos.y() &&
          p.y() <= sidebar_pos.y() + m_left_sidebar->height()) {
        on_resize_edge = true;
      }
    }

    if (m_zoom_panel) {
      Vector2i panel_pos = m_zoom_panel->absolute_position();
      int left_edge = panel_pos.x();
      if (p.x() >= left_edge - 5 && p.x() <= left_edge + 5 && p.y() >= panel_pos.y() &&
          p.y() <= panel_pos.y() + m_zoom_panel->height()) {
        on_resize_edge = true;
      }
    }

    if (on_resize_edge) {
      set_cursor(Cursor::HResize);
    } else if (!m_resizing_left_sidebar && !m_resizing_zoom_panel) {
      set_cursor(Cursor::Arrow);
    }

    return false;
  }

  void perform_layout(NVGcontext *ctx) override {
    Screen::perform_layout(ctx);
    if (m_canvas) {
      m_canvas->set_position(Vector2i(0, 60));
      m_canvas->set_fixed_size(Vector2i(width(), height() - 60));
    }
    update_ui_positions();
  }

  bool resize_event(const Vector2i &size) override {
    bool handled = Screen::resize_event(size);
    if (m_canvas) {
      m_canvas->set_position(Vector2i(0, 60));
      m_canvas->set_fixed_size(Vector2i(size.x(), size.y() - 60));
      m_canvas->set_size(Vector2i(size.x(), size.y() - 60));
    }
    update_ui_positions();
    perform_layout(nvg_context());
    return handled;
  }

  void update_ui_positions() {
    // Update positions of UI elements based on window size
    for (Widget *child : children()) {
      Window *window = dynamic_cast<Window *>(child);
      if (!window)
        continue;

      // Check if it's the toolbar (at top)
      if (window->position().y() == 0) {
        window->set_fixed_width(width());
      }
      // Check if it's the zoom controls (bottom-right)
      else if (window->fixed_width() == 60 && window->fixed_height() == 160) {
        window->set_position(Vector2i(width() - 80, height() - 250));
      }
      // Check if it's the avatar panel (bottom-center)
      else if (window->fixed_width() == 300 && window->fixed_height() == 60) {
        window->set_position(Vector2i(width() / 2 - 150, height() - 80));
      }
    }
  }

private:
  ref<FluentWebTheme> m_theme;
  ModernCanvas *m_canvas = nullptr;
  FluentWebMessageBar *m_info_bar = nullptr;
  Widget *m_left_sidebar = nullptr; // Changed from Window to Widget for resize
  Widget *m_zoom_panel = nullptr;   // Changed from Window to Widget for resize
  ColorPicker *m_color_picker = nullptr;
  bool m_resizing_left_sidebar = false;
  bool m_resizing_zoom_panel = false;
  int m_left_sidebar_width = 70;
  int m_zoom_panel_width = 60;

  struct CanvasPage {
    std::vector<Stroke> strokes;
    std::stack<std::vector<Stroke>> undo_stack;
    std::stack<std::vector<Stroke>> redo_stack;
    float zoom = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    Color current_color = Color(255, 100, 100, 255); // Per-page color
    Color fill_color = Color(255, 182, 193, 255);    // Per-page fill color
    float stroke_width = 3.0f;                       // Per-page pen size
    Tool current_tool = Tool::Pen;                   // Per-page selected tool
    bool left_sidebar_visible = true;                // Per-page left sidebar visibility
    bool right_panel_visible = true;                 // Per-page right panel visibility
    int left_sidebar_width = 70;                     // Per-page left sidebar width
    int right_panel_width = 60;                      // Per-page right panel width
    std::string name;
    bool exists = false;
  };
  std::vector<CanvasPage> m_pages;
  int m_current_page = 0;
  Widget *m_page_buttons_container = nullptr;

  void show_notification(const std::string &message) {
    if (m_info_bar) {
      m_info_bar->set_visible(false);
      remove_child(m_info_bar);
    }

    m_info_bar = new FluentWebMessageBar(this, message);
    m_info_bar->set_position(Vector2i(width() / 2 - 150, 80)); // Center top, below toolbar
    m_info_bar->set_fixed_width(300);
  }

  void create_top_toolbar() {
    Window *toolbar = new Window(this, "");
    toolbar->set_position(Vector2i(0, 0));
    toolbar->set_fixed_size(Vector2i(width(), 60));
    toolbar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 15, 15));

    // App icon and title
    auto *title_container = new Widget(toolbar);
    title_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 0));

    auto *icon_btn = new Button(title_container, "", FA_BARS);
    icon_btn->set_fixed_size(Vector2i(40, 40));
    icon_btn->set_icon_extra_scale(1.2f);

    new Label(title_container, "Weekly Team Sync", "sans-bold", 16);

    auto *dropdown_btn = new Button(title_container, "", FA_CHEVRON_DOWN);
    dropdown_btn->set_fixed_size(Vector2i(30, 30));

    // Undo/Redo buttons
    auto *undo_redo_container = new Widget(toolbar);
    undo_redo_container->set_layout(
        new BoxLayout(Orientation::Horizontal, Alignment::Middle, 4, 0));

    auto *undo_btn = new Button(undo_redo_container, "", FA_UNDO);
    undo_btn->set_fixed_size(Vector2i(40, 40));
    undo_btn->set_tooltip("Undo (Ctrl+Z)");
    undo_btn->set_icon_extra_scale(1.1f);
    undo_btn->set_callback([this]() {
      m_canvas->undo();
      show_notification("Undo");
    });

    auto *redo_btn = new Button(undo_redo_container, "", FA_REDO);
    redo_btn->set_fixed_size(Vector2i(40, 40));
    redo_btn->set_tooltip("Redo (Ctrl+Y)");
    redo_btn->set_icon_extra_scale(1.1f);
    redo_btn->set_callback([this]() {
      m_canvas->redo();
      show_notification("Redo");
    });

    // Spacer
    new Widget(toolbar);

    // Right side action buttons
    auto *actions = new Widget(toolbar);
    actions->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 0));

    create_icon_button(actions, FA_FOLDER_OPEN, "Open");    // Open file
    create_icon_button(actions, FA_SAVE, "Save");           // Save file
    create_icon_button(actions, FA_FILE_EXPORT, "Export");  // Export/Download
    create_icon_button(actions, FA_SHARE_ALT, "Share");     // Share
    create_icon_button(actions, FA_USERS, "Collaborators"); // Users/Team
    create_icon_button(actions, FA_COMMENT, "Comments");    // Comments
    create_icon_button(actions, FA_TH, "Grid View");        // Grid toggle
    create_icon_button(actions, FA_COG, "Settings");        // Settings
  }

  void create_left_sidebar() {
    // Create container as a plain Widget (not Window) so resize works
    m_left_sidebar = new Widget(this);
    m_left_sidebar->set_position(Vector2i(15, 100));
    m_left_sidebar->set_fixed_size(Vector2i(m_left_sidebar_width, 550));
    m_left_sidebar->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Middle, 8, 8));

    // Tool buttons with same style as top toolbar
    create_tool_icon_button(m_left_sidebar, FA_MOUSE_POINTER, Tool::Select, "Select");
    create_tool_icon_button(m_left_sidebar, FA_HAND_PAPER, Tool::Pan, "Pan");
    create_tool_icon_button(m_left_sidebar, FA_PENCIL_ALT, Tool::Pen, "Pen");
    create_tool_icon_button(m_left_sidebar, FA_STICKY_NOTE, Tool::Sticky, "Sticky Note");
    create_tool_icon_button(m_left_sidebar, FA_SQUARE, Tool::Rectangle, "Rectangle");
    create_tool_icon_button(m_left_sidebar, FA_CIRCLE, Tool::Circle, "Circle");
    create_tool_icon_button(m_left_sidebar, FA_MINUS, Tool::Line, "Line");
    create_tool_icon_button(m_left_sidebar, FA_LONG_ARROW_ALT_RIGHT, Tool::Arrow, "Arrow");

    // Add spacer
    auto *spacer = new Widget(m_left_sidebar);
    spacer->set_fixed_size(Vector2i(50, 10));

    // Color picker
    m_color_picker = new ColorPicker(m_left_sidebar, Color(255, 100, 100, 255));
    m_color_picker->set_fixed_size(Vector2i(50, 50));
    m_color_picker->set_tooltip("Choose Color");
    m_color_picker->set_final_callback([this](const Color &color) {
      m_canvas->set_color(color);
      m_canvas->set_fill_color(color);

      // If currently in Select or Pan mode, switch to Pen tool so user can draw
      Tool current_tool = m_canvas->get_tool();
      if (current_tool == Tool::Select || current_tool == Tool::Pan) {
        m_canvas->set_tool(Tool::Pen);
        m_canvas->set_selection_mode(false);
        show_notification("Color Changed - Switched to Pen");
      } else {
        show_notification("Color Changed");
      }
    });

    // Add spacer
    auto *spacer2 = new Widget(m_left_sidebar);
    spacer2->set_fixed_size(Vector2i(50, 5));

    // Pen size label - make it more visible
    auto *size_label = new Label(m_left_sidebar, "SIZE", "sans-bold", 12);
    size_label->set_fixed_width(50);
    size_label->set_color(Color(100, 100, 100, 255));

    create_size_button(m_left_sidebar, 1.0f, "Thin");
    create_size_button(m_left_sidebar, 3.0f, "Normal");
    create_size_button(m_left_sidebar, 6.0f, "Thick");
    create_size_button(m_left_sidebar, 10.0f, "Bold");
  }

  void create_size_button(Widget *parent, float size, const std::string &label) {
    auto *btn = new Button(parent, "", 0);
    btn->set_fixed_size(Vector2i(50, 30));
    btn->set_tooltip(label + " (" + std::to_string((int)size) + "px)");

    // Draw a circle to represent the size
    btn->set_callback([this, size, label]() {
      m_canvas->set_stroke_width(size);
      show_notification("Pen Size: " + label);
    });
  }

  void create_tool_icon_button(Widget *parent, int icon, Tool tool, const std::string &tooltip) {
    auto *btn = new Button(parent, "", icon);
    btn->set_fixed_size(Vector2i(50, 50));
    btn->set_tooltip(tooltip);
    btn->set_icon_extra_scale(1.2f);
    btn->set_callback([this, tool, tooltip]() {
      m_canvas->set_tool(tool);
      m_canvas->set_selection_mode(tool == Tool::Select);
      show_notification("Tool: " + tooltip);
    });
  }

  void create_floating_panels() {
    // Add some sticky notes on canvas
    m_canvas->add_sticky_note(Vector2i(420, 220), Color(255, 182, 193, 255));
    m_canvas->add_sticky_note(Vector2i(510, 220), Color(255, 160, 180, 255));
    m_canvas->add_sticky_note(Vector2i(600, 220), Color(255, 100, 120, 255));
    m_canvas->add_sticky_note(Vector2i(420, 320), Color(255, 255, 150, 255));

    // Add user avatars
    create_user_avatars();
  }

  Button *create_icon_button(Widget *parent, int icon, const std::string &tooltip) {
    auto *btn = new Button(parent, "", icon);
    btn->set_fixed_size(Vector2i(40, 40));
    btn->set_tooltip(tooltip);
    btn->set_icon_extra_scale(1.1f);
    return btn;
  }

  void create_user_avatars() {
    Window *avatar_panel = new Window(this, "");
    avatar_panel->set_position(Vector2i(width() / 2 - 150, height() - 80));
    avatar_panel->set_fixed_size(Vector2i(300, 60));
    avatar_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 8));

    // Container for page buttons
    m_page_buttons_container = new Widget(avatar_panel);
    m_page_buttons_container->set_layout(
        new BoxLayout(Orientation::Horizontal, Alignment::Middle, 4, 0));

    // Create first page by default
    if (m_pages.empty()) {
      create_new_page();
    }

    // Rebuild page buttons
    rebuild_page_buttons();

    // Add "+" button to create new pages
    auto *add_btn = new Button(avatar_panel, "", FA_PLUS);
    add_btn->set_fixed_size(Vector2i(40, 40));
    add_btn->set_tooltip("Add New Page");
    add_btn->set_background_color(Color(100, 200, 100, 255));
    add_btn->set_callback([this]() {
      create_new_page();
      rebuild_page_buttons();
      perform_layout(nvg_context());
      show_notification("Created Page " + std::to_string(m_pages.size()));
    });
  }

  void create_new_page() {
    // Save current page state before creating new one
    if (!m_pages.empty() && m_current_page >= 0 && m_current_page < (int)m_pages.size()) {
      save_current_page_state();
    }

    CanvasPage new_page;
    new_page.name = std::to_string(m_pages.size() + 1);
    new_page.exists = true;
    new_page.zoom = 1.0f;
    new_page.pan_x = 0.0f;
    new_page.pan_y = 0.0f;
    m_pages.push_back(new_page);

    // Switch to new page
    m_current_page = m_pages.size() - 1;
    load_page(m_current_page);
  }

  void rebuild_page_buttons() {
    // Clear existing buttons
    while (m_page_buttons_container->child_count() > 0) {
      m_page_buttons_container->remove_child_at(0);
    }

    // Create button for each page
    Color colors[] = {Color(255, 100, 100, 255), Color(100, 255, 100, 255),
                      Color(100, 100, 255, 255), Color(255, 200, 100, 255),
                      Color(200, 100, 255, 255), Color(255, 150, 200, 255),
                      Color(150, 255, 200, 255), Color(200, 150, 255, 255)};

    for (int i = 0; i < (int)m_pages.size(); i++) {
      auto *btn = new Button(m_page_buttons_container, m_pages[i].name, 0);
      btn->set_fixed_size(Vector2i(40, 40));
      btn->set_background_color(colors[i % 8]);
      btn->set_tooltip("Page " + m_pages[i].name);

      // Highlight current page
      if (i == m_current_page) {
        btn->set_font_size(18);
      }

      int page_index = i; // Capture for lambda
      btn->set_callback([this, page_index]() { switch_to_page(page_index); });
    }
  }

  void save_current_page_state() {
    if (m_current_page >= 0 && m_current_page < (int)m_pages.size()) {
      m_pages[m_current_page].strokes = m_canvas->get_strokes();
      m_pages[m_current_page].undo_stack = m_canvas->get_undo_stack();
      m_pages[m_current_page].redo_stack = m_canvas->get_redo_stack();
      m_pages[m_current_page].zoom = m_canvas->get_zoom();
      m_pages[m_current_page].pan_x = m_canvas->get_pan_x();
      m_pages[m_current_page].pan_y = m_canvas->get_pan_y();
      m_pages[m_current_page].current_color = m_canvas->get_color();
      m_pages[m_current_page].fill_color = m_canvas->get_fill_color();
      m_pages[m_current_page].stroke_width = m_canvas->get_stroke_width();
      m_pages[m_current_page].current_tool = m_canvas->get_tool();
      m_pages[m_current_page].left_sidebar_visible = m_left_sidebar ? m_left_sidebar->visible() : true;
      m_pages[m_current_page].right_panel_visible = m_zoom_panel ? m_zoom_panel->visible() : true;
      m_pages[m_current_page].left_sidebar_width = m_left_sidebar_width;
      m_pages[m_current_page].right_panel_width = m_zoom_panel_width;
    }
  }

  void switch_to_page(int page_index) {
    if (page_index < 0 || page_index >= (int)m_pages.size())
      return;

    // Save current page
    save_current_page_state();

    // Switch to new page
    m_current_page = page_index;
    load_page(page_index);

    // Update button highlights
    rebuild_page_buttons();
    perform_layout(nvg_context());

    show_notification("Switched to Page " + m_pages[page_index].name);
  }

  void load_page(int page_index) {
    if (page_index < 0 || page_index >= (int)m_pages.size())
      return;

    m_canvas->set_strokes(m_pages[page_index].strokes);
    m_canvas->set_undo_stack(m_pages[page_index].undo_stack);
    m_canvas->set_redo_stack(m_pages[page_index].redo_stack);
    m_canvas->set_view(m_pages[page_index].zoom, m_pages[page_index].pan_x,
                       m_pages[page_index].pan_y);
    m_canvas->set_color(m_pages[page_index].current_color);
    m_canvas->set_fill_color(m_pages[page_index].fill_color);
    m_canvas->set_stroke_width(m_pages[page_index].stroke_width);
    m_canvas->set_tool(m_pages[page_index].current_tool);
    m_canvas->set_selection_mode(m_pages[page_index].current_tool == Tool::Select);

    // Update the color picker to show the page's color
    if (m_color_picker) {
      m_color_picker->set_color(m_pages[page_index].current_color);
    }

    // Restore toolbar visibility and sizes
    if (m_left_sidebar) {
      m_left_sidebar->set_visible(m_pages[page_index].left_sidebar_visible);
      m_left_sidebar_width = m_pages[page_index].left_sidebar_width;
      m_left_sidebar->set_fixed_width(m_left_sidebar_width);
    }

    if (m_zoom_panel) {
      m_zoom_panel->set_visible(m_pages[page_index].right_panel_visible);
      m_zoom_panel_width = m_pages[page_index].right_panel_width;
      m_zoom_panel->set_fixed_width(m_zoom_panel_width);
      m_zoom_panel->set_position(Vector2i(width() - m_zoom_panel_width - 20, m_zoom_panel->position().y()));
    }

    perform_layout(nvg_context());
  }

  void create_zoom_controls() {
    // Zoom controls panel (bottom-right) - use Widget instead of Window for resize
    m_zoom_panel = new Widget(this);
    m_zoom_panel->set_position(Vector2i(width() - 80, height() - 250));
    m_zoom_panel->set_fixed_size(Vector2i(m_zoom_panel_width, 160));
    m_zoom_panel->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Middle, 5, 5));
    Widget *zoom_panel = m_zoom_panel;

    // Zoom In
    auto *zoom_in_btn = new Button(zoom_panel, "", FA_PLUS);
    zoom_in_btn->set_fixed_size(Vector2i(50, 40));
    zoom_in_btn->set_tooltip("Zoom In");
    zoom_in_btn->set_callback([this]() {
      m_canvas->zoom_in();
      show_notification("Zoomed In");
    });

    // Zoom Out
    auto *zoom_out_btn = new Button(zoom_panel, "", FA_MINUS);
    zoom_out_btn->set_fixed_size(Vector2i(50, 40));
    zoom_out_btn->set_tooltip("Zoom Out");
    zoom_out_btn->set_callback([this]() {
      m_canvas->zoom_out();
      show_notification("Zoomed Out");
    });

    // Reset Zoom
    auto *reset_btn = new Button(zoom_panel, "", FA_EXPAND);
    reset_btn->set_fixed_size(Vector2i(50, 40));
    reset_btn->set_tooltip("Reset View");
    reset_btn->set_callback([this]() {
      m_canvas->reset_zoom();
      show_notification("View Reset");
    });

    // Toggle Grid & Coordinates
    auto *grid_btn = new Button(zoom_panel, "", FA_TH);
    grid_btn->set_fixed_size(Vector2i(50, 30));
    grid_btn->set_tooltip("Toggle Grid & Coordinates");
    grid_btn->set_callback([this]() {
      m_canvas->toggle_grid();
      m_canvas->toggle_coordinates();
      show_notification("Grid & Coordinates Toggled");
    });
  }
};

int main(int argc, char **argv) {
  try {
    nanogui::init();

    {
      ref<ModernWhiteboardApp> app = new ModernWhiteboardApp();
      app->set_visible(true);

#if defined(NANOGUI_USE_SDL3)
      while (app->process_events()) {
        app->draw_all();
      }
#else
      app->draw_all();
      nanogui::mainloop();
#endif
    }

    nanogui::shutdown();
    std::cout << "\n✓ Modern Whiteboard closed!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "✗ Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
