/*
 * flexUI - Text Edit API Header
 *
 * Declares the C++ API for text editing.
 * Implementation uses stb_textedit.h for robust text manipulation with undo/redo.
 */

#ifndef FLEXUI_TEXTEDIT_H
#define FLEXUI_TEXTEDIT_H

#include <string>

namespace flexUI {

// Forward declaration
struct TextEditState;

/**
 * TextEdit - Text editing engine using stb_textedit
 *
 * Features:
 * - Cursor movement (char, word, line, document)
 * - Selection (shift+movement, click+drag, select all)
 * - Editing (insert, delete, backspace)
 * - Undo/Redo (64 undo states)
 * - Multi-line support (optional)
 */
class TextEdit {
public:
  TextEdit();
  ~TextEdit();

  // Non-copyable
  TextEdit(const TextEdit&) = delete;
  TextEdit& operator=(const TextEdit&) = delete;

  /**
   * Initialize with a string pointer
   *
   * @param text Pointer to the string to edit (must outlive TextEdit)
   * @param char_width Approximate character width for cursor positioning
   * @param line_height Line height for multi-line
   * @param multiline Enable multi-line editing
   */
  void init(std::string* text, float char_width, float line_height, bool multiline = false);

  // ========================================================================
  // Key Input
  // ========================================================================

  /** Handle key press. Returns true if key was consumed. */
  bool key(int key, bool shift = false, bool ctrl = false);

  /** Insert text at cursor. Deletes selection if any. */
  void insert_text(const std::string& text);

  // ========================================================================
  // Mouse Input
  // ========================================================================

  /** Click at position (sets cursor, clears selection) */
  void click(float x, float y);

  /** Drag to position (extends selection) */
  void drag(float x, float y);

  // ========================================================================
  // Clipboard
  // ========================================================================

  /** Cut selected text (returns cut text, clears selection) */
  std::string cut();

  /** Copy selected text */
  std::string copy() const;

  /** Paste text at cursor */
  void paste(const std::string& text);

  // ========================================================================
  // Undo/Redo
  // ========================================================================

  void undo();
  void redo();

  // ========================================================================
  // Selection
  // ========================================================================

  bool has_selection() const;
  void select_all();
  void clear_selection();

  int selection_start() const;
  int selection_end() const;
  std::string selected_text() const;
  void set_selection(int start, int end);

  // ========================================================================
  // Cursor
  // ========================================================================

  int cursor() const;
  void set_cursor(int pos);

  // ========================================================================
  // Key Codes (for direct stb_textedit access)
  // ========================================================================

  static int k_left();
  static int k_right();
  static int k_up();
  static int k_down();
  static int k_linestart();
  static int k_lineend();
  static int k_textstart();
  static int k_textend();
  static int k_delete();
  static int k_backspace();
  static int k_undo();
  static int k_redo();
  static int k_wordleft();
  static int k_wordright();
  static int k_shift();

private:
  TextEditState* state_ = nullptr;
  std::string* text_ = nullptr;
};

// ============================================================================
// Helper: Map platform keys to stb_textedit keys
// ============================================================================

/**
 * Map platform-specific key codes to stb_textedit key codes
 *
 * @param key Platform key code (GLFW/SDL style)
 * @param shift Shift modifier
 * @param ctrl Control modifier
 * @return stb_textedit key code, or 0 if not handled
 */
int map_key_to_stb(int key, bool shift, bool ctrl);

} // namespace flexUI

#endif // FLEXUI_TEXTEDIT_H
