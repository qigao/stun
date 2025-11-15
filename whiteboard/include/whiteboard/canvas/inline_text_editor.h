/**
 * \file inline_text_editor.h
 * \brief Inline text editor for SVG shape parameters.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/svg/svg_shape_library.h"
#include <nanogui/window.h>
#include <nanogui/textbox.h>
#include <string>

namespace whiteboard {

/**
 * \class InlineTextEditor
 * \brief Inline text editor for SVG shape className/interfaceName.
 *
 * Simple custom widget that draws text input directly on canvas.
 * Only edits className or interfaceName parameters.
 * Avoids NanoGUI TextBox "string too long" bug.
 */
class InlineTextEditor : public nanogui::Widget {
public:
  /**
   * \brief Editor mode for different editing scenarios.
   */
  enum class EditorMode {
    SingleLine,   ///< Edit a single line of text
    MultiLine,    ///< Edit multiple lines with basic features
    FullField     ///< Edit entire field with advanced features (line numbers, snippets, etc.)
  };
  /**
   * \brief Constructor.
   * \param parent Parent widget
   * \param document The document model
   * \param library The shape library
   * \param stroke_index Index of stroke being edited
   * \param parameter_name Name of the parameter to edit (className or interfaceName)
   * \param position Position in screen coordinates
   * \param line_index For multi-line parameters, which line to edit (default -1 = edit all)
   * \param mode Editor mode (default SingleLine)
   */
  InlineTextEditor(nanogui::Widget *parent, 
                   WhiteboardDocument *document,
                   SVGShapeLibrary *library, 
                   int stroke_index,
                   const std::string& parameter_name,
                   const nanogui::Vector2i& position,
                   int line_index = -1,
                   EditorMode mode = EditorMode::SingleLine);

  /**
   * \brief Handle keyboard events.
   */
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;

  /**
   * \brief Handle focus loss.
   */
  bool focus_event(bool focused) override;

  /**
   * \brief Handle mouse button events.
   */
  bool mouse_button_event(const nanogui::Vector2i &p, int button, bool down, int modifiers) override;

  /**
   * \brief Handle mouse motion events.
   */
  bool mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel, int button, int modifiers) override;

  /**
   * \brief Handle scroll events.
   */
  bool scroll_event(const nanogui::Vector2i &p, const nanogui::Vector2f &rel) override;

  /**
   * \brief Draw the editor.
   */
  void draw(NVGcontext *ctx) override;

  /**
   * \brief Activate the editor.
   */
  void activate();

  /**
   * \brief Set the editor mode.
   */
  void set_mode(EditorMode mode);

  /**
   * \brief Get the current editor mode.
   */
  EditorMode get_mode() const { return m_mode; }

  /**
   * \brief Enable or disable line numbers display.
   */
  void enable_line_numbers(bool enable);

  /**
   * \brief Check if line numbers are enabled.
   */
  bool line_numbers_enabled() const { return m_show_line_numbers; }

  /**
   * \brief Detect which line was clicked at a given position.
   * \param p Position in screen coordinates
   * \return Line index (0-based), or -1 if no line was clicked
   */
  int detect_clicked_line(const nanogui::Vector2i &p) const;

  /**
   * \brief Get the clicked line index (for context menu actions).
   * \return The last clicked line index, or -1 if none
   */
  int get_clicked_line_index() const { return m_clicked_line_index; }

  /**
   * \brief Get the bounds of a specific line in screen coordinates.
   * \param line_index The line index (0-based)
   * \param x Output: left edge of line
   * \param y Output: top edge of line
   * \param width Output: width of line area
   * \param height Output: height of line
   * \return true if bounds were calculated, false if line_index is invalid
   */
  bool get_line_bounds(int line_index, float &x, float &y, float &width, float &height) const;

  /**
   * \brief Get the currently active inline text editor (if any).
   * \return Pointer to active editor, or nullptr if none
   */
  static InlineTextEditor* get_active_editor() { return s_active_editor; }

  /**
   * \brief Insert a new line at the specified position.
   * \param position Line position (0-based index) where to insert
   * \param text Text content for the new line
   */
  void insert_line(int position, const std::string& text);

  /**
   * \brief Delete a line at the specified line number.
   * \param line_number Line number (0-based index) to delete
   */
  void delete_line(int line_number);

  /**
   * \brief Duplicate a line at the specified line number.
   * \param line_number Line number (0-based index) to duplicate
   */
  void duplicate_line(int line_number);

  /**
   * \brief Move a line from one position to another.
   * \param from Source line number (0-based index)
   * \param to Destination line number (0-based index)
   */
  void move_line(int from, int to);

  /**
   * \brief Update the SVG preview after line operations.
   * This regenerates the SVG with current editor content without committing.
   */
  void update_svg_preview();

private:
  WhiteboardDocument *m_document;
  SVGShapeLibrary *m_library;
  int m_stroke_index;
  std::string m_parameter_name;
  std::string m_original_value;
  std::string m_current_value; // For SingleLine mode compatibility
  std::vector<std::string> m_lines; // Line-based storage for MultiLine/FullField modes
  int m_cursor_pos; // For SingleLine mode (character position)
  int m_cursor_line; // For MultiLine/FullField modes (line number)
  int m_cursor_column; // For MultiLine/FullField modes (column position)
  bool m_should_close;
  bool m_is_multi_line;
  int m_line_index; // For multi-line parameters, which line to edit (-1 = all)
  EditorMode m_mode; // Current editor mode
  bool m_show_line_numbers; // Whether to show line numbers
  
  // Scrolling support
  float m_scroll_offset; // Vertical scroll offset in pixels
  float m_max_scroll_offset; // Maximum scroll offset
  int m_first_visible_line; // First visible line index
  int m_last_visible_line; // Last visible line index
  bool m_is_dragging_scrollbar; // Whether user is dragging scrollbar
  float m_scrollbar_drag_start_y; // Y position where scrollbar drag started
  float m_scrollbar_drag_start_offset; // Scroll offset when drag started
  
  // Right-click line detection
  int m_clicked_line_index; // Index of line that was right-clicked (-1 if none)
  
  // Static member to track active editor
  static InlineTextEditor* s_active_editor;
  
  void commit();
  void cancel();
  void insert_char(char c);
  void delete_char();
  void update_size_for_mode(); // Update widget size based on mode
  void split_into_lines(); // Convert m_current_value to m_lines
  void join_from_lines(); // Convert m_lines to m_current_value
  void clamp_cursor(); // Ensure cursor is within valid bounds
  float calculate_line_number_width(NVGcontext *ctx) const; // Calculate width needed for line numbers
  void draw_line_numbers(NVGcontext *ctx, float x, float y, float line_height); // Draw line numbers
  
  // Scrolling methods
  void update_visible_line_range(float line_height); // Update which lines are visible
  void update_scroll_offset(); // Update scroll offset based on content
  void draw_scrollbar(NVGcontext *ctx); // Draw vertical scrollbar
  bool is_scrollbar_needed() const; // Check if scrollbar is needed
  void scroll_to_cursor(float line_height); // Ensure cursor is visible
  float get_scrollbar_height() const; // Calculate scrollbar thumb height
  float get_scrollbar_position() const; // Calculate scrollbar thumb position
};

} // namespace whiteboard
