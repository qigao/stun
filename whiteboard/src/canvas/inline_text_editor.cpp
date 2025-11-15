/**
 * \file inline_text_editor.cpp
 * \brief Simple inline text editor for SVG shape names.
 */

#include "whiteboard/canvas/inline_text_editor.h"
#include <fmtlog.h>
#include <nanogui/keys.h>
#include <sstream> // Used for split_into_lines()

#ifdef NANOGUI_USE_SDL3
  #include <SDL3/SDL.h>
  #define KEY_ENTER SDLK_RETURN
  #define KEY_KP_ENTER SDLK_KP_ENTER
  #define KEY_ESCAPE SDLK_ESCAPE
  #define KEY_BACKSPACE SDLK_BACKSPACE
  #define KEY_DELETE SDLK_DELETE
  #define KEY_LEFT SDLK_LEFT
  #define KEY_RIGHT SDLK_RIGHT
  #define KEY_PRESS SDL_EVENT_KEY_DOWN
#else
  #include <GLFW/glfw3.h>
  #define KEY_ENTER GLFW_KEY_ENTER
  #define KEY_KP_ENTER GLFW_KEY_KP_ENTER
  #define KEY_ESCAPE GLFW_KEY_ESCAPE
  #define KEY_BACKSPACE GLFW_KEY_BACKSPACE
  #define KEY_DELETE GLFW_KEY_DELETE
  #define KEY_LEFT GLFW_KEY_LEFT
  #define KEY_RIGHT GLFW_KEY_RIGHT
  #define KEY_PRESS GLFW_PRESS
#endif

namespace whiteboard {

// Static member definition
InlineTextEditor* InlineTextEditor::s_active_editor = nullptr;

InlineTextEditor::InlineTextEditor(nanogui::Widget *parent, WhiteboardDocument *document,
                                   SVGShapeLibrary *library, int stroke_index,
                                   const std::string &parameter_name,
                                   const nanogui::Vector2i &position,
                                   int line_index,
                                   EditorMode mode)
    : nanogui::Widget(parent), m_document(document), m_library(library),
      m_stroke_index(stroke_index), m_parameter_name(parameter_name), m_cursor_pos(0),
      m_cursor_line(0), m_cursor_column(0),
      m_line_index(line_index), m_mode(mode), m_show_line_numbers(true),
      m_scroll_offset(0.0f), m_max_scroll_offset(0.0f),
      m_first_visible_line(0), m_last_visible_line(0),
      m_is_dragging_scrollbar(false), m_scrollbar_drag_start_y(0.0f),
      m_scrollbar_drag_start_offset(0.0f), m_clicked_line_index(-1) {

  // Get current value
  const auto &strokes = m_document->get_strokes();
  if (stroke_index >= 0 && stroke_index < static_cast<int>(strokes.size())) {
    const Stroke &stroke = strokes[stroke_index];
    auto it = stroke.svg_parameters.find(parameter_name);
    if (it != stroke.svg_parameters.end()) {
      m_original_value = it->second;
    }
  }

  // Detect if this is a multi-line parameter
  m_is_multi_line = (parameter_name == "methods" || parameter_name == "attributes" ||
                     parameter_name == "properties" || parameter_name == "operations");

  // For multi-line with specific line index, extract just that line
  if (m_is_multi_line && line_index >= 0) {
    std::vector<std::string> lines;
    std::stringstream ss(m_original_value);
    std::string line;
    while (std::getline(ss, line)) {
      lines.push_back(line);
    }
    
    if (line_index < static_cast<int>(lines.size())) {
      m_current_value = lines[line_index];
    } else {
      m_current_value = ""; // New line
    }
  } else {
    m_current_value = m_original_value;
  }

  m_cursor_pos = static_cast<int>(m_current_value.length());
  m_should_close = false;

  // For MultiLine and FullField modes, split text into lines
  if (m_mode == EditorMode::MultiLine || m_mode == EditorMode::FullField) {
    split_into_lines();
    // Position cursor at end of last line
    m_cursor_line = static_cast<int>(m_lines.size()) - 1;
    m_cursor_column = static_cast<int>(m_lines[m_cursor_line].length());
  }

  // Set size based on mode
  set_position(position);
  update_size_for_mode();

  // Set as active editor
  s_active_editor = this;
  
  logi("InlineTextEditor: Created at ({}, {}) for parameter '{}'{} with value '{}' in mode {} ({} lines), cursor at ({}, {})", 
       position.x(), position.y(), parameter_name, 
       (line_index >= 0 ? " line " + std::to_string(line_index) : ""),
       m_current_value,
       static_cast<int>(m_mode),
       m_lines.size(),
       m_cursor_line,
       m_cursor_column);
}

bool InlineTextEditor::keyboard_event(int key, int scancode, int action, int modifiers) {
  // Ignore events if marked for closure
  if (m_should_close) {
    return false;
  }

  logi("InlineTextEditor::keyboard_event: key={}, action={}, modifiers={}", key, action, modifiers);
  if (action != 1) {
    return false;
  }

  // Check if Shift is pressed
  bool shift = (modifiers & 1) != 0;
  // Check if Ctrl is pressed
  bool ctrl = (modifiers & 2) != 0;
  // Check if Alt is pressed
  bool alt = (modifiers & 4) != 0;

  // Ctrl+D: Duplicate current line (key 68 = 'D')
  if (ctrl && key == 68 && (m_mode == EditorMode::MultiLine || m_mode == EditorMode::FullField)) {
    logi("  -> Ctrl+D pressed, duplicating current line {}", m_cursor_line);
    duplicate_line(m_cursor_line);
    return true;
  }

  // Enter: In multi-line mode, insert newline; in single-line mode, commit
  if (key == 257 || key == 335) { // GLFW_KEY_ENTER or GLFW_KEY_KP_ENTER
    if (m_mode == EditorMode::MultiLine || m_mode == EditorMode::FullField) {
      // Multi-line mode: Enter inserts newline, Ctrl+Enter commits
      if (ctrl) {
        // Ctrl+Enter: Commit
        logi("  -> Ctrl+Enter pressed, committing");
        commit();
        return true;
      } else {
        // Enter: Insert newline
        logi("  -> Enter pressed, inserting newline at line {} column {}", m_cursor_line, m_cursor_column);
        std::string current_line = m_lines[m_cursor_line];
        std::string before = current_line.substr(0, m_cursor_column);
        std::string after = current_line.substr(m_cursor_column);
        
        m_lines[m_cursor_line] = before;
        m_lines.insert(m_lines.begin() + m_cursor_line + 1, after);
        
        // Move cursor to start of new line
        m_cursor_line++;
        m_cursor_column = 0;
        
        // Ensure cursor is visible
        scroll_to_cursor(24.0f);
        
        return true;
      }
    } else {
      // Single-line mode: Enter commits
      logi("  -> Enter pressed, committing with value '{}'", m_current_value);
      commit();
      return true;
    }
  }

  // Escape: Cancel editing (key 256)
  if (key == 256) { // GLFW_KEY_ESCAPE
    logi("  -> Escape pressed, cancelling");
    cancel();
    return true;
  }

  // Backspace: Delete character (key 259)
  if (key == 259) { // GLFW_KEY_BACKSPACE
    delete_char();
    return true;
  }

  // Delete: Delete character after cursor (key 261)
  if (key == 261) { // GLFW_KEY_DELETE
    if (m_cursor_pos < static_cast<int>(m_current_value.length())) {
      m_current_value.erase(m_cursor_pos, 1);
    }
    return true;
  }

  // Left arrow: Move cursor left (key 263)
  if (key == 263) { // GLFW_KEY_LEFT
    if (m_mode == EditorMode::SingleLine) {
      if (m_cursor_pos > 0) {
        m_cursor_pos--;
      }
    } else {
      // Multi-line mode: move within line or to previous line
      if (m_cursor_column > 0) {
        m_cursor_column--;
      } else if (m_cursor_line > 0) {
        // Move to end of previous line
        m_cursor_line--;
        m_cursor_column = static_cast<int>(m_lines[m_cursor_line].length());
      }
    }
    return true;
  }

  // Right arrow: Move cursor right (key 262)
  if (key == 262) { // GLFW_KEY_RIGHT
    if (m_mode == EditorMode::SingleLine) {
      if (m_cursor_pos < static_cast<int>(m_current_value.length())) {
        m_cursor_pos++;
      }
    } else {
      // Multi-line mode: move within line or to next line
      int line_length = static_cast<int>(m_lines[m_cursor_line].length());
      if (m_cursor_column < line_length) {
        m_cursor_column++;
      } else if (m_cursor_line < static_cast<int>(m_lines.size()) - 1) {
        // Move to start of next line
        m_cursor_line++;
        m_cursor_column = 0;
      }
    }
    return true;
  }

  // Up arrow: Move cursor up, or Alt+Up to move line up (key 265)
  if (key == 265) { // GLFW_KEY_UP
    if (m_mode != EditorMode::SingleLine) {
      if (alt && m_cursor_line > 0) {
        // Alt+Up: Move current line up
        logi("  -> Alt+Up pressed, moving line {} up", m_cursor_line);
        move_line(m_cursor_line, m_cursor_line - 1);
        m_cursor_line--; // Follow the line
      } else if (m_cursor_line > 0) {
        // Regular Up: Move cursor up
        m_cursor_line--;
        clamp_cursor(); // Ensure column is valid for new line
        scroll_to_cursor(24.0f); // Ensure cursor is visible
      }
    }
    return true;
  }

  // Down arrow: Move cursor down, or Alt+Down to move line down (key 264)
  if (key == 264) { // GLFW_KEY_DOWN
    if (m_mode != EditorMode::SingleLine) {
      if (alt && m_cursor_line < static_cast<int>(m_lines.size()) - 1) {
        // Alt+Down: Move current line down
        logi("  -> Alt+Down pressed, moving line {} down", m_cursor_line);
        move_line(m_cursor_line, m_cursor_line + 1);
        m_cursor_line++; // Follow the line
      } else if (m_cursor_line < static_cast<int>(m_lines.size()) - 1) {
        // Regular Down: Move cursor down
        m_cursor_line++;
        clamp_cursor(); // Ensure column is valid for new line
        scroll_to_cursor(24.0f); // Ensure cursor is visible
      }
    }
    return true;
  }

  // Printable characters (ASCII 32-126)
  if (key >= 32 && key <= 126) {
    logi("  -> Inserting character: '{}' ({})", static_cast<char>(key), key);
    insert_char(static_cast<char>(key));
    return true;
  }

  logd("  -> Unhandled key: {}", key);
  return nanogui::Widget::keyboard_event(key, scancode, action, modifiers);
}

bool InlineTextEditor::focus_event(bool focused) {
  logi("InlineTextEditor::focus_event: focused={}", focused);
  // User must press Enter to commit or Escape to cancel
  // This is actually better UX - explicit action required

  return nanogui::Widget::focus_event(focused);
}

bool InlineTextEditor::mouse_button_event(const nanogui::Vector2i &p, int button, bool down, int modifiers) {
  // Handle right-click for line-specific context menu
  if (button == 1 && down) { // Right mouse button down
    if (m_mode != EditorMode::SingleLine) {
      // Detect which line was clicked
      int clicked_line = detect_clicked_line(p);
      if (clicked_line >= 0) {
        m_clicked_line_index = clicked_line;
        logi("InlineTextEditor: Right-clicked on line {}: '{}'", clicked_line, m_lines[clicked_line]);
        return false;
      }
    }
  }
  
  if (m_mode == EditorMode::SingleLine || !is_scrollbar_needed()) {
    return nanogui::Widget::mouse_button_event(p, button, down, modifiers);
  }
  
  // Check if click is on scrollbar
  nanogui::Vector2i pos = absolute_position();
  const float scrollbar_width = 12.0f;
  const float scrollbar_x = pos.x() + width() - scrollbar_width - 2.0f;
  
  if (p.x() >= scrollbar_x && p.x() <= scrollbar_x + scrollbar_width) {
    if (down && button == 0) { // Left mouse button down
      // Start dragging scrollbar
      m_is_dragging_scrollbar = true;
      m_scrollbar_drag_start_y = p.y();
      m_scrollbar_drag_start_offset = m_scroll_offset;
      return true;
    }
  }
  
  if (!down && button == 0) { // Left mouse button up
    if (m_is_dragging_scrollbar) {
      m_is_dragging_scrollbar = false;
      return true;
    }
  }
  
  return nanogui::Widget::mouse_button_event(p, button, down, modifiers);
}

bool InlineTextEditor::mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel, int button, int modifiers) {
  if (m_is_dragging_scrollbar) {
    // Calculate how much to scroll based on mouse movement
    float delta_y = p.y() - m_scrollbar_drag_start_y;
    
    // Convert mouse delta to scroll delta
    // The ratio is: scroll_range / available_track_height
    float track_height = height() - 4.0f;
    float thumb_height = get_scrollbar_height();
    float available_track = track_height - thumb_height;
    
    if (available_track > 0.0f) {
      float scroll_delta = (delta_y / available_track) * m_max_scroll_offset;
      m_scroll_offset = m_scrollbar_drag_start_offset + scroll_delta;
      
      // Clamp scroll offset
      m_scroll_offset = std::max(0.0f, std::min(m_scroll_offset, m_max_scroll_offset));
    }
    
    return true;
  }
  
  return nanogui::Widget::mouse_motion_event(p, rel, button, modifiers);
}

bool InlineTextEditor::scroll_event(const nanogui::Vector2i &p, const nanogui::Vector2f &rel) {
  if (m_mode == EditorMode::SingleLine || !is_scrollbar_needed()) {
    return nanogui::Widget::scroll_event(p, rel);
  }
  
  // Scroll by mouse wheel
  const float line_height = 24.0f;
  const float scroll_speed = 3.0f; // Lines per scroll tick
  
  m_scroll_offset -= rel.y() * scroll_speed * line_height;
  
  // Clamp scroll offset
  m_scroll_offset = std::max(0.0f, std::min(m_scroll_offset, m_max_scroll_offset));
  
  return true;
}

void InlineTextEditor::draw(NVGcontext *ctx) {
  static int draw_count = 0;
  if (draw_count++ < 5) { // Only log first 5 draws
    logi("InlineTextEditor::draw called! pos=({}, {}), size={}x{}, visible={}", position().x(),
         position().y(), width(), height(), visible());
  }

  nanogui::Widget::draw(ctx);

  nanogui::Vector2i pos = absolute_position();
  nanogui::Vector2i size = m_size;

  // Draw clean background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, pos.x(), pos.y(), size.x(), size.y(), 3.0f);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 250)); // White background
  nvgFill(ctx);

  // Draw border
  nvgStrokeColor(ctx, nvgRGBA(0, 120, 215, 255)); // Blue border
  nvgStrokeWidth(ctx, 2.0f);
  nvgStroke(ctx);

  // Draw text
  nvgFontSize(ctx, 16.0f);
  nvgFontFace(ctx, "sans");
  nvgFillColor(ctx, nvgRGBA(0, 0, 0, 255));

  if (m_mode == EditorMode::SingleLine) {
    // Single line: centered text
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    float text_x = pos.x() + size.x() / 2.0f;
    float text_y = pos.y() + size.y() / 2.0f;
    nvgText(ctx, text_x, text_y, m_current_value.c_str(), nullptr);

    // Draw cursor
    std::string before_cursor = m_current_value.substr(0, m_cursor_pos);
    float bounds[4];
    nvgTextBounds(ctx, text_x, text_y, before_cursor.c_str(), nullptr, bounds);

    float cursor_x = bounds[2];
    if (before_cursor.empty()) {
      cursor_x = text_x - nvgTextBounds(ctx, 0, 0, m_current_value.c_str(), nullptr, bounds) / 2.0f;
    }

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cursor_x, text_y - 8);
    nvgLineTo(ctx, cursor_x, text_y + 8);
    nvgStrokeColor(ctx, nvgRGBA(0, 0, 0, 255));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);
  } else {
    // Multi-line: left-aligned text with line-by-line rendering
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    const float line_height = 24.0f;
    const float padding = 8.0f;
    
    // Update scroll offset and visible line range
    update_scroll_offset();
    update_visible_line_range(line_height);
    
    // Calculate line number width if enabled
    float line_number_width = 0.0f;
    if (m_show_line_numbers) {
      line_number_width = calculate_line_number_width(ctx);
    }
    
    float text_x = pos.x() + padding + line_number_width;
    float text_y = pos.y() + padding - m_scroll_offset;

    // Set up clipping region to prevent text from drawing outside editor bounds
    nvgSave(ctx);
    nvgScissor(ctx, pos.x(), pos.y(), size.x(), size.y());

    // Draw line numbers if enabled (only visible lines)
    if (m_show_line_numbers) {
      draw_line_numbers(ctx, pos.x() + padding, text_y, line_height);
    }

    // Draw only visible lines
    for (int i = m_first_visible_line; i <= m_last_visible_line && i < static_cast<int>(m_lines.size()); ++i) {
      float y = text_y + i * line_height;
      nvgText(ctx, text_x, y, m_lines[i].c_str(), nullptr);
    }

    // Draw cursor (only if visible)
    if (m_cursor_line >= m_first_visible_line && m_cursor_line <= m_last_visible_line) {
      float cursor_y = text_y + m_cursor_line * line_height;
      std::string before_cursor = m_lines[m_cursor_line].substr(0, m_cursor_column);
      float bounds[4];
      nvgTextBounds(ctx, text_x, cursor_y, before_cursor.c_str(), nullptr, bounds);
      float cursor_x = bounds[2];
      if (before_cursor.empty()) {
        cursor_x = text_x;
      }

      nvgBeginPath(ctx);
      nvgMoveTo(ctx, cursor_x, cursor_y);
      nvgLineTo(ctx, cursor_x, cursor_y + 18);
      nvgStrokeColor(ctx, nvgRGBA(0, 0, 0, 255));
      nvgStrokeWidth(ctx, 1.5f);
      nvgStroke(ctx);
    }

    nvgRestore(ctx);

    // Draw scrollbar if needed
    if (is_scrollbar_needed()) {
      draw_scrollbar(ctx);
    }
  }
}

void InlineTextEditor::activate() {
  set_visible(true);
  request_focus();

  logi("InlineTextEditor: Activated with value '{}'", m_current_value);
}

void InlineTextEditor::commit() {
  if (!m_document || !m_library) {
    if (parent()) {
      parent()->remove_child(this);
    }
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (m_stroke_index < 0 || m_stroke_index >= static_cast<int>(strokes.size())) {
    if (parent()) {
      parent()->remove_child(this);
    }
    return;
  }

  Stroke stroke = strokes[m_stroke_index];
  std::string new_full_value;

  // For multi-line modes, join lines back into single string
  if (m_mode == EditorMode::MultiLine || m_mode == EditorMode::FullField) {
    join_from_lines();
  }

  // For multi-line with specific line index, update just that line
  if (m_is_multi_line && m_line_index >= 0) {
    std::vector<std::string> lines;
    std::stringstream ss(m_original_value);
    std::string line;
    while (std::getline(ss, line)) {
      lines.push_back(line);
    }

    // Update the specific line
    if (m_line_index < static_cast<int>(lines.size())) {
      lines[m_line_index] = m_current_value;
    } else {
      // Adding a new line
      lines.push_back(m_current_value);
    }

    // Rebuild the full text
    for (size_t i = 0; i < lines.size(); ++i) {
      if (i > 0) new_full_value += "\n";
      new_full_value += lines[i];
    }
  } else {
    new_full_value = m_current_value;
  }

  // Only update if value changed
  if (new_full_value != m_original_value) {
    stroke.svg_parameters[m_parameter_name] = new_full_value;

    // Regenerate SVG
    stroke.svg_data = m_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);

    if (!stroke.svg_data.empty()) {
      m_document->update_stroke(m_stroke_index, stroke);
      logi("✓ Updated parameter '{}'{} from '{}' to '{}'", m_parameter_name,
           (m_line_index >= 0 ? " line " + std::to_string(m_line_index) : ""),
           m_original_value, new_full_value);
    } else {
      loge("Failed to regenerate SVG for shape: {}", stroke.svg_shape_id);
    }
  }

  // Clear active editor reference
  if (s_active_editor == this) {
    s_active_editor = nullptr;
  }
  
  // Mark for deletion (don't delete immediately to avoid crash)
  m_should_close = true;
  set_visible(false);

  // Schedule deletion for next frame
  if (parent()) {
    parent()->remove_child(this);
  }
}

void InlineTextEditor::cancel() {
  logi("InlineTextEditor: Cancelled (value was '{}')", m_current_value);

  // Clear active editor reference
  if (s_active_editor == this) {
    s_active_editor = nullptr;
  }
  
  // Mark for deletion
  m_should_close = true;
  set_visible(false);

  // Schedule deletion for next frame
  if (parent()) {
    parent()->remove_child(this);
  }
}

void InlineTextEditor::insert_char(char c) {
  if (m_mode == EditorMode::SingleLine) {
    // Single line mode: use m_current_value
    const size_t MAX_LENGTH = 30;
    if (m_current_value.length() >= MAX_LENGTH) {
      return;
    }
    m_current_value.insert(m_cursor_pos, 1, c);
    m_cursor_pos++;
  } else {
    // Multi-line mode: use m_lines
    const size_t MAX_LINE_LENGTH = 100;
    if (m_lines[m_cursor_line].length() >= MAX_LINE_LENGTH) {
      return;
    }
    m_lines[m_cursor_line].insert(m_cursor_column, 1, c);
    m_cursor_column++;
    
    // Ensure cursor is visible after typing
    scroll_to_cursor(24.0f);
  }
}

void InlineTextEditor::delete_char() {
  if (m_mode == EditorMode::SingleLine) {
    // Single line mode: use m_current_value
    if (m_cursor_pos > 0) {
      m_current_value.erase(m_cursor_pos - 1, 1);
      m_cursor_pos--;
    }
  } else {
    // Multi-line mode: use m_lines
    if (m_cursor_column > 0) {
      // Delete character in current line
      m_lines[m_cursor_line].erase(m_cursor_column - 1, 1);
      m_cursor_column--;
    } else if (m_cursor_line > 0) {
      // At start of line: merge with previous line
      std::string current_line = m_lines[m_cursor_line];
      m_cursor_line--;
      m_cursor_column = static_cast<int>(m_lines[m_cursor_line].length());
      m_lines[m_cursor_line] += current_line;
      m_lines.erase(m_lines.begin() + m_cursor_line + 1);
    }
    
    // Ensure cursor is visible after deletion
    scroll_to_cursor(24.0f);
  }
}

void InlineTextEditor::set_mode(EditorMode mode) {
  m_mode = mode;
  update_size_for_mode();
  logi("InlineTextEditor: Mode changed to {}", static_cast<int>(mode));
}

void InlineTextEditor::update_size_for_mode() {
  switch (m_mode) {
    case EditorMode::SingleLine:
      // Single line: compact height
      set_fixed_width(300);
      set_fixed_height(30);
      break;
    
    case EditorMode::MultiLine:
      // Multi-line: show 3-5 lines
      set_fixed_width(400);
      set_fixed_height(120); // ~4 lines at 30px per line
      break;
    
    case EditorMode::FullField:
      // Full field: larger area with room for line numbers and features
      set_fixed_width(500);
      set_fixed_height(200); // ~6-7 lines
      break;
  }
}

void InlineTextEditor::split_into_lines() {
  m_lines.clear();
  
  if (m_current_value.empty()) {
    m_lines.push_back(""); // At least one empty line
    return;
  }
  
  std::stringstream ss(m_current_value);
  std::string line;
  while (std::getline(ss, line)) {
    m_lines.push_back(line);
  }
  
  // Ensure at least one line
  if (m_lines.empty()) {
    m_lines.push_back("");
  }
  
  logi("InlineTextEditor: Split into {} lines", m_lines.size());
}

void InlineTextEditor::join_from_lines() {
  m_current_value.clear();
  
  for (size_t i = 0; i < m_lines.size(); ++i) {
    if (i > 0) {
      m_current_value += "\n";
    }
    m_current_value += m_lines[i];
  }
  
  logi("InlineTextEditor: Joined {} lines into text of length {}", m_lines.size(), m_current_value.length());
}

void InlineTextEditor::clamp_cursor() {
  // Clamp cursor line
  if (m_cursor_line < 0) {
    m_cursor_line = 0;
  }
  if (m_cursor_line >= static_cast<int>(m_lines.size())) {
    m_cursor_line = static_cast<int>(m_lines.size()) - 1;
  }
  
  // Ensure we have at least one line
  if (m_lines.empty()) {
    m_lines.push_back("");
    m_cursor_line = 0;
  }
  
  // Clamp cursor column
  int line_length = static_cast<int>(m_lines[m_cursor_line].length());
  if (m_cursor_column < 0) {
    m_cursor_column = 0;
  }
  if (m_cursor_column > line_length) {
    m_cursor_column = line_length;
  }
}

void InlineTextEditor::enable_line_numbers(bool enable) {
  m_show_line_numbers = enable;
  logi("InlineTextEditor: Line numbers {}", enable ? "enabled" : "disabled");
}

float InlineTextEditor::calculate_line_number_width(NVGcontext *ctx) const {
  if (!m_show_line_numbers || m_lines.empty()) {
    return 0.0f;
  }
  
  // Calculate the width needed for the largest line number
  int max_line_number = static_cast<int>(m_lines.size());
  std::string max_line_str = std::to_string(max_line_number);
  
  // Measure text width
  nvgFontSize(ctx, 14.0f);
  nvgFontFace(ctx, "sans");
  float bounds[4];
  nvgTextBounds(ctx, 0, 0, max_line_str.c_str(), nullptr, bounds);
  
  // Add padding: 4px left + text width + 8px right (separator space)
  return 4.0f + (bounds[2] - bounds[0]) + 8.0f;
}

void InlineTextEditor::draw_line_numbers(NVGcontext *ctx, float x, float y, float line_height) {
  if (!m_show_line_numbers || m_lines.empty()) {
    return;
  }
  
  // Set up styling for line numbers
  nvgFontSize(ctx, 14.0f);
  nvgFontFace(ctx, "sans");
  nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
  
  // Use a subtle gray color for line numbers
  nvgFillColor(ctx, nvgRGBA(150, 150, 150, 255));
  
  // Calculate the right edge of the line number column
  float line_number_width = calculate_line_number_width(ctx);
  float line_number_x = x + line_number_width - 8.0f; // Subtract right padding
  
  // Draw only visible line numbers
  for (int i = m_first_visible_line; i <= m_last_visible_line && i < static_cast<int>(m_lines.size()); ++i) {
    float line_y = y + i * line_height;
    std::string line_number = std::to_string(i + 1); // Line numbers start at 1
    nvgText(ctx, line_number_x, line_y, line_number.c_str(), nullptr);
  }
  
  // Draw a subtle separator line between line numbers and text
  float separator_x = x + line_number_width - 4.0f;
  nanogui::Vector2i pos = absolute_position();
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, separator_x, pos.y());
  nvgLineTo(ctx, separator_x, pos.y() + height());
  nvgStrokeColor(ctx, nvgRGBA(200, 200, 200, 255));
  nvgStrokeWidth(ctx, 1.0f);
  nvgStroke(ctx);
}

void InlineTextEditor::update_visible_line_range(float line_height) {
  if (m_lines.empty()) {
    m_first_visible_line = 0;
    m_last_visible_line = 0;
    return;
  }
  
  const float padding = 8.0f;
  float visible_height = height() - 2 * padding;
  
  // Calculate first and last visible lines based on scroll offset
  m_first_visible_line = static_cast<int>(m_scroll_offset / line_height);
  m_last_visible_line = static_cast<int>((m_scroll_offset + visible_height) / line_height);
  
  // Clamp to valid range
  m_first_visible_line = std::max(0, m_first_visible_line);
  m_last_visible_line = std::min(static_cast<int>(m_lines.size()) - 1, m_last_visible_line);
}

void InlineTextEditor::update_scroll_offset() {
  if (m_lines.empty()) {
    m_scroll_offset = 0.0f;
    m_max_scroll_offset = 0.0f;
    return;
  }
  
  const float line_height = 24.0f;
  const float padding = 8.0f;
  
  // Calculate total content height
  float content_height = m_lines.size() * line_height;
  float visible_height = height() - 2 * padding;
  
  // Calculate maximum scroll offset
  m_max_scroll_offset = std::max(0.0f, content_height - visible_height);
  
  // Clamp scroll offset
  m_scroll_offset = std::max(0.0f, std::min(m_scroll_offset, m_max_scroll_offset));
}

void InlineTextEditor::draw_scrollbar(NVGcontext *ctx) {
  nanogui::Vector2i pos = absolute_position();
  nanogui::Vector2i size = m_size;
  
  const float scrollbar_width = 12.0f;
  const float scrollbar_x = pos.x() + size.x() - scrollbar_width - 2.0f;
  const float scrollbar_y = pos.y() + 2.0f;
  const float scrollbar_track_height = size.y() - 4.0f;
  
  // Draw scrollbar track
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_track_height, 6.0f);
  nvgFillColor(ctx, nvgRGBA(240, 240, 240, 255));
  nvgFill(ctx);
  
  // Calculate scrollbar thumb dimensions
  float thumb_height = get_scrollbar_height();
  float thumb_y = scrollbar_y + get_scrollbar_position();
  
  // Draw scrollbar thumb
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, scrollbar_x + 2.0f, thumb_y, scrollbar_width - 4.0f, thumb_height, 4.0f);
  
  // Use different color if dragging
  if (m_is_dragging_scrollbar) {
    nvgFillColor(ctx, nvgRGBA(100, 100, 100, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(150, 150, 150, 255));
  }
  nvgFill(ctx);
}

bool InlineTextEditor::is_scrollbar_needed() const {
  if (m_mode == EditorMode::SingleLine) {
    return false;
  }
  
  const float line_height = 24.0f;
  const float padding = 8.0f;
  
  float content_height = m_lines.size() * line_height;
  float visible_height = height() - 2 * padding;
  
  return content_height > visible_height;
}

void InlineTextEditor::scroll_to_cursor(float line_height) {
  if (m_mode == EditorMode::SingleLine || m_lines.empty()) {
    return;
  }
  
  const float padding = 8.0f;
  float visible_height = height() - 2 * padding;
  
  // Calculate cursor position in pixels
  float cursor_y = m_cursor_line * line_height;
  
  // Check if cursor is above visible area
  if (cursor_y < m_scroll_offset) {
    m_scroll_offset = cursor_y;
  }
  
  // Check if cursor is below visible area
  if (cursor_y + line_height > m_scroll_offset + visible_height) {
    m_scroll_offset = cursor_y + line_height - visible_height;
  }
  
  // Clamp scroll offset
  m_scroll_offset = std::max(0.0f, std::min(m_scroll_offset, m_max_scroll_offset));
}

float InlineTextEditor::get_scrollbar_height() const {
  const float line_height = 24.0f;
  const float padding = 8.0f;
  
  float content_height = m_lines.size() * line_height;
  float visible_height = height() - 2 * padding;
  float track_height = height() - 4.0f;
  
  if (content_height <= 0.0f) {
    return track_height;
  }
  
  // Thumb height is proportional to visible/total ratio
  float thumb_height = (visible_height / content_height) * track_height;
  
  // Minimum thumb height for usability
  return std::max(30.0f, thumb_height);
}

float InlineTextEditor::get_scrollbar_position() const {
  if (m_max_scroll_offset <= 0.0f) {
    return 0.0f;
  }
  
  float track_height = height() - 4.0f;
  float thumb_height = get_scrollbar_height();
  float available_track = track_height - thumb_height;
  
  // Position is proportional to scroll offset
  return (m_scroll_offset / m_max_scroll_offset) * available_track;
}

int InlineTextEditor::detect_clicked_line(const nanogui::Vector2i &p) const {
  // Only works for multi-line modes
  if (m_mode == EditorMode::SingleLine || m_lines.empty()) {
    return -1;
  }
  
  // Convert to local coordinates
  nanogui::Vector2i pos = absolute_position();
  int local_x = p.x() - pos.x();
  int local_y = p.y() - pos.y();
  
  // Check if click is within editor bounds
  if (local_x < 0 || local_x >= width() || local_y < 0 || local_y >= height()) {
    return -1;
  }
  
  // Constants matching the draw() method
  const float line_height = 24.0f;
  const float padding = 8.0f;
  
  // Calculate line number width if enabled
  float line_number_width = 0.0f;
  if (m_show_line_numbers) {
    // We need to calculate this without NVGcontext, so use an approximation
    // Based on typical font metrics: ~8px per digit + padding
    int max_line_number = static_cast<int>(m_lines.size());
    int num_digits = std::to_string(max_line_number).length();
    line_number_width = 4.0f + (num_digits * 8.0f) + 8.0f;
  }
  
  // Check if click is in the text area (not in line numbers or scrollbar)
  float text_start_x = padding + line_number_width;
  const float scrollbar_width = 12.0f;
  float text_end_x = width() - scrollbar_width - 2.0f;
  
  if (is_scrollbar_needed()) {
    // If scrollbar is visible, exclude it from hit detection
    if (local_x >= text_end_x) {
      return -1; // Clicked on scrollbar
    }
  }
  
  // Calculate which line was clicked based on Y position
  // Account for scroll offset
  float adjusted_y = local_y - padding + m_scroll_offset;
  int line_index = static_cast<int>(adjusted_y / line_height);
  
  // Clamp to valid range
  if (line_index < 0 || line_index >= static_cast<int>(m_lines.size())) {
    return -1;
  }
  
  logi("InlineTextEditor::detect_clicked_line: pos=({}, {}), local=({}, {}), adjusted_y={:.1f}, line_index={}", 
       p.x(), p.y(), local_x, local_y, adjusted_y, line_index);
  return line_index;
}

bool InlineTextEditor::get_line_bounds(int line_index, float &x, float &y, float &width, float &height) const {
  // Only works for multi-line modes
  if (m_mode == EditorMode::SingleLine || m_lines.empty()) {
    return false;
  }
  
  // Validate line index
  if (line_index < 0 || line_index >= static_cast<int>(m_lines.size())) {
    return false;
  }
  
  // Constants matching the draw() method
  const float line_height = 24.0f;
  const float padding = 8.0f;
  
  // Calculate line number width if enabled
  float line_number_width = 0.0f;
  if (m_show_line_numbers) {
    int max_line_number = static_cast<int>(m_lines.size());
    int num_digits = std::to_string(max_line_number).length();
    line_number_width = 4.0f + (num_digits * 8.0f) + 8.0f;
  }
  
  // Get absolute position
  nanogui::Vector2i pos = absolute_position();
  
  // Calculate bounds in screen coordinates
  x = pos.x() + padding + line_number_width;
  y = pos.y() + padding + (line_index * line_height) - m_scroll_offset;
  
  // Width extends to scrollbar or edge
  const float scrollbar_width = 12.0f;
  if (is_scrollbar_needed()) {
    width = this->width() - padding - line_number_width - scrollbar_width - 2.0f;
  } else {
    width = this->width() - padding - line_number_width - padding;
  }
  
  height = line_height;
  
  return true;
}

void InlineTextEditor::insert_line(int position, const std::string& text) {
  // Only works for multi-line modes
  if (m_mode == EditorMode::SingleLine) {
    loge("InlineTextEditor::insert_line: Cannot insert line in SingleLine mode");
    return;
  }
  
  // Ensure we have at least one line
  if (m_lines.empty()) {
    m_lines.push_back("");
  }
  
  // Clamp position to valid range [0, m_lines.size()]
  int insert_pos = std::max(0, std::min(position, static_cast<int>(m_lines.size())));
  
  // Insert the new line
  m_lines.insert(m_lines.begin() + insert_pos, text);
  
  logi("InlineTextEditor::insert_line: Inserted line at position {} with text '{}'", insert_pos, text);
  m_cursor_line = insert_pos;
  m_cursor_column = static_cast<int>(text.length());
  
  // Ensure cursor is visible
  scroll_to_cursor(24.0f);
  
  // Update scroll offset for new content
  update_scroll_offset();
  
  // Update SVG preview
  update_svg_preview();
}

void InlineTextEditor::delete_line(int line_number) {
  // Only works for multi-line modes
  if (m_mode == EditorMode::SingleLine) {
    loge("InlineTextEditor::delete_line: Cannot delete line in SingleLine mode");
    return;
  }
  
  // Validate line number
  if (line_number < 0 || line_number >= static_cast<int>(m_lines.size())) {
    loge("InlineTextEditor::delete_line: Invalid line number {} (total lines: {})", line_number, m_lines.size());
    return;
  }
  
  // Don't allow deleting the last line - keep at least one empty line
  if (m_lines.size() == 1) {
    logi("InlineTextEditor::delete_line: Clearing last line instead of deleting");
    m_lines[0] = "";
    m_cursor_line = 0;
    m_cursor_column = 0;
    update_svg_preview();
    return;
  }
  
  logi("InlineTextEditor::delete_line: Deleting line {} with text '{}'", line_number, m_lines[line_number]);
  m_lines.erase(m_lines.begin() + line_number);
  
  // Adjust cursor position if needed
  if (m_cursor_line == line_number) {
    // Cursor was on deleted line - move to previous line (or next if first line)
    if (line_number > 0) {
      m_cursor_line = line_number - 1;
    } else {
      m_cursor_line = 0;
    }
    m_cursor_column = static_cast<int>(m_lines[m_cursor_line].length());
  } else if (m_cursor_line > line_number) {
    // Cursor was after deleted line - adjust index
    m_cursor_line--;
  }
  
  // Clamp cursor to valid position
  clamp_cursor();
  
  // Ensure cursor is visible
  scroll_to_cursor(24.0f);
  
  // Update scroll offset for new content
  update_scroll_offset();
  
  // Update SVG preview
  update_svg_preview();
}

void InlineTextEditor::duplicate_line(int line_number) {
  // Only works for multi-line modes
  if (m_mode == EditorMode::SingleLine) {
    loge("InlineTextEditor::duplicate_line: Cannot duplicate line in SingleLine mode");
    return;
  }
  
  // Validate line number
  if (line_number < 0 || line_number >= static_cast<int>(m_lines.size())) {
    loge("InlineTextEditor::duplicate_line: Invalid line number {} (total lines: {})", line_number, m_lines.size());
    return;
  }
  
  // Get the line to duplicate
  std::string line_text = m_lines[line_number];
  
  logi("InlineTextEditor::duplicate_line: Duplicating line {} with text '{}'", line_number, line_text);
  m_lines.insert(m_lines.begin() + line_number + 1, line_text);
  
  // Move cursor to the duplicated line
  m_cursor_line = line_number + 1;
  m_cursor_column = static_cast<int>(line_text.length());
  
  // Ensure cursor is visible
  scroll_to_cursor(24.0f);
  
  // Update scroll offset for new content
  update_scroll_offset();
  
  // Update SVG preview
  update_svg_preview();
}

void InlineTextEditor::move_line(int from, int to) {
  // Only works for multi-line modes
  if (m_mode == EditorMode::SingleLine) {
    loge("InlineTextEditor::move_line: Cannot move line in SingleLine mode");
    return;
  }
  
  // Validate line numbers
  if (from < 0 || from >= static_cast<int>(m_lines.size())) {
    loge("InlineTextEditor::move_line: Invalid source line number {} (total lines: {})", from, m_lines.size());
    return;
  }
  
  if (to < 0 || to >= static_cast<int>(m_lines.size())) {
    loge("InlineTextEditor::move_line: Invalid destination line number {} (total lines: {})", to, m_lines.size());
    return;
  }
  
  // No-op if moving to same position
  if (from == to) {
    return;
  }
  
  logi("InlineTextEditor::move_line: Moving line {} to position {}", from, to);
  std::string line_text = m_lines[from];
  
  // Remove from original position
  m_lines.erase(m_lines.begin() + from);
  
  // Insert at new position
  // Note: if to > from, the index shifts down by 1 after removal
  int insert_pos = (to > from) ? to : to;
  m_lines.insert(m_lines.begin() + insert_pos, line_text);
  
  // Update cursor position if it was on the moved line
  if (m_cursor_line == from) {
    m_cursor_line = insert_pos;
  } else if (from < to) {
    // Line moved down: adjust cursor if it was in the affected range
    if (m_cursor_line > from && m_cursor_line <= to) {
      m_cursor_line--;
    }
  } else {
    // Line moved up: adjust cursor if it was in the affected range
    if (m_cursor_line >= to && m_cursor_line < from) {
      m_cursor_line++;
    }
  }
  
  // Clamp cursor to valid position
  clamp_cursor();
  
  // Ensure cursor is visible
  scroll_to_cursor(24.0f);
  
  // Update scroll offset for new content
  update_scroll_offset();
  
  // Update SVG preview
  update_svg_preview();
}

void InlineTextEditor::update_svg_preview() {
  if (!m_document || !m_library) {
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (m_stroke_index < 0 || m_stroke_index >= static_cast<int>(strokes.size())) {
    return;
  }

  // Join lines back into single string for preview
  std::string preview_value;
  if (m_mode == EditorMode::MultiLine || m_mode == EditorMode::FullField) {
    for (size_t i = 0; i < m_lines.size(); ++i) {
      if (i > 0) {
        preview_value += "\n";
      }
      preview_value += m_lines[i];
    }
  } else {
    preview_value = m_current_value;
  }

  // Create a temporary stroke with updated parameter
  Stroke stroke = strokes[m_stroke_index];
  stroke.svg_parameters[m_parameter_name] = preview_value;

  // Regenerate SVG
  stroke.svg_data = m_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);

  if (!stroke.svg_data.empty()) {
    // Update the document with preview (this will trigger a redraw)
    m_document->update_stroke(m_stroke_index, stroke);
    logi("InlineTextEditor::update_svg_preview: Updated SVG preview for parameter '{}'", m_parameter_name);
  } else {
    loge("InlineTextEditor::update_svg_preview: Failed to regenerate SVG for shape: {}", stroke.svg_shape_id);
  }
}

} // namespace whiteboard
