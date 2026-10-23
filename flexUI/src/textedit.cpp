/*
 * flexUI - Text Edit Implementation
 *
 * Uses stb_textedit.h for robust text manipulation.
 * All configuration macros must be defined BEFORE including stb_textedit.h
 */

#include <flexUI/textedit.h>
#include <flexUI/text_util.h>
#include <salts_unicode.h>
#include <string>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace flexUI {

// Text edit string type for stb_textedit
struct TextEditString {
  std::string* text;
  float char_width;
  float line_height;
};

} // namespace flexUI

// ============================================================================
// stb_textedit configuration - must be before including stb_textedit.h
// ============================================================================

#define STB_TEXTEDIT_CHARTYPE         char
#define STB_TEXTEDIT_POSITIONTYPE     int
#define STB_TEXTEDIT_UNDOSTATECOUNT   64
#define STB_TEXTEDIT_UNDOCHARCOUNT    256
#define STB_TEXTEDIT_STRING           flexUI::TextEditString

#define STB_TEXTEDIT_STRINGLEN(obj) \
    static_cast<int>((obj)->text->length())

#define STB_TEXTEDIT_GETCHAR(obj, i) \
    ((obj)->text->at(i))

#define STB_TEXTEDIT_GETWIDTH(obj, n, i) \
    ((obj)->char_width)

#define STB_TEXTEDIT_LAYOUTROW(r, obj, n) \
    do { \
        (r)->x0 = 0; \
        (r)->x1 = static_cast<float>((obj)->text->length()) * (obj)->char_width; \
        (r)->baseline_y_delta = (obj)->line_height; \
        (r)->ymin = 0; \
        (r)->ymax = (obj)->line_height; \
        (r)->num_chars = static_cast<int>((obj)->text->length()) - (n); \
    } while(0)

// Key definitions
#define STB_TEXTEDIT_K_LEFT       0x10000
#define STB_TEXTEDIT_K_RIGHT      0x10001
#define STB_TEXTEDIT_K_UP         0x10002
#define STB_TEXTEDIT_K_DOWN       0x10003
#define STB_TEXTEDIT_K_LINESTART  0x10004
#define STB_TEXTEDIT_K_LINEEND    0x10005
#define STB_TEXTEDIT_K_TEXTSTART  0x10006
#define STB_TEXTEDIT_K_TEXTEND    0x10007
#define STB_TEXTEDIT_K_DELETE     0x10008
#define STB_TEXTEDIT_K_BACKSPACE  0x10009
#define STB_TEXTEDIT_K_UNDO       0x1000A
#define STB_TEXTEDIT_K_REDO       0x1000B
#define STB_TEXTEDIT_K_WORDLEFT   0x1000C
#define STB_TEXTEDIT_K_WORDRIGHT  0x1000D
#define STB_TEXTEDIT_K_PGUP       0x1000E
#define STB_TEXTEDIT_K_PGDOWN     0x1000F
#define STB_TEXTEDIT_K_SHIFT      0x20000
#define STB_TEXTEDIT_K_CONTROL    0x40000

#define STB_TEXTEDIT_NEWLINE      '\n'

// Check if character is whitespace (for word navigation)
#define STB_TEXTEDIT_IS_SPACE(ch) \
    ((ch) == ' ' || (ch) == '\t' || (ch) == '\n' || (ch) == '\r')

// Convert key to text character (for printable keys)
#define STB_TEXTEDIT_KEYTOTEXT(k) \
    (((k) >= 32 && (k) < 127) ? (k) : -1)

// Delete n characters starting at i
static int flexUI_textedit_delete(flexUI::TextEditString* obj, int i, int n) {
  if (obj && obj->text && i >= 0 && n > 0 && i + n <= static_cast<int>(obj->text->length())) {
    obj->text->erase(i, n);
    return 1;
  }
  return 0;
}
#define STB_TEXTEDIT_DELETECHARS(obj, i, n) \
    flexUI_textedit_delete(obj, i, n)

// Insert n characters at position i
static int flexUI_textedit_insert(flexUI::TextEditString* obj, int i, const char* text, int n) {
  if (obj && obj->text && i >= 0 && i <= static_cast<int>(obj->text->length())) {
    obj->text->insert(i, text, n);
    return 1;
  }
  return 0;
}
#define STB_TEXTEDIT_INSERTCHARS(obj, i, text, n) \
    flexUI_textedit_insert(obj, i, text, n)

// Now include the implementation
#define STB_TEXTEDIT_IMPLEMENTATION
#include <stb_textedit.h>

namespace flexUI {

// ============================================================================
// TextEditState - wraps STB_TexteditState
// ============================================================================

struct TextEditState {
  STB_TexteditState stb_state;
  TextEditString string;
  bool initialized = false;
};

namespace {

[[noreturn]] void throw_unicode_edit_error(salts_unicode_status status,
                                           const char* operation) {
  if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
    throw std::invalid_argument("FlexUI text contains invalid UTF-8");
  }
  throw std::runtime_error(operation);
}

size_t next_grapheme_boundary(const std::string& text, size_t position) {
  size_t cursor = 0u;
  vstr cluster{};
  const vstr input = vstr_from_buf(text.data(), text.size());

  while (cursor < text.size()) {
    const salts_unicode_status status =
        salts_unicode_grapheme_next(input, &cursor, &cluster);
    if (status != SALTS_UNICODE_OK) {
      throw_unicode_edit_error(status,
                               "Salts::Unicode failed grapheme navigation");
    }
    if (cursor > position) {
      return cursor;
    }
  }
  return text.size();
}

size_t previous_grapheme_boundary(const std::string& text, size_t position) {
  if (position == 0u) return 0u;

  size_t cursor = 0u;
  vstr cluster{};
  const vstr input = vstr_from_buf(text.data(), text.size());

  while (cursor < text.size()) {
    const size_t start = cursor;
    const salts_unicode_status status =
        salts_unicode_grapheme_next(input, &cursor, &cluster);
    if (status != SALTS_UNICODE_OK) {
      throw_unicode_edit_error(status,
                               "Salts::Unicode failed grapheme navigation");
    }
    if (cursor >= position) {
      return start;
    }
  }
  return text.size();
}

size_t next_word_boundary(const std::string& text, size_t position) {
  size_t cursor = 0u;
  vstr segment{};
  const vstr input = vstr_from_buf(text.data(), text.size());

  while (cursor < text.size()) {
    const salts_unicode_status status =
        salts_unicode_word_next(input, &cursor, &segment);
    if (status != SALTS_UNICODE_OK) {
      throw_unicode_edit_error(status,
                               "Salts::Unicode failed word navigation");
    }
    if (cursor > position) {
      return cursor;
    }
  }
  return text.size();
}

size_t previous_word_boundary(const std::string& text, size_t position) {
  if (position == 0u) return 0u;

  size_t cursor = 0u;
  vstr segment{};
  const vstr input = vstr_from_buf(text.data(), text.size());

  while (cursor < text.size()) {
    const size_t start = cursor;
    const salts_unicode_status status =
        salts_unicode_word_next(input, &cursor, &segment);
    if (status != SALTS_UNICODE_OK) {
      throw_unicode_edit_error(status,
                               "Salts::Unicode failed word navigation");
    }
    if (cursor >= position) {
      return start;
    }
  }
  return text.size();
}

void move_textedit_cursor(STB_TexteditState* state, size_t target, bool shift) {
  const int clamped = static_cast<int>(target);
  if (shift) {
    if (state->select_start == state->select_end) {
      state->select_start = state->cursor;
    }
    state->cursor = clamped;
    state->select_end = clamped;
    return;
  }

  state->cursor = clamped;
  state->select_start = clamped;
  state->select_end = clamped;
}

} // namespace

// ============================================================================
// TextEdit Implementation
// ============================================================================

TextEdit::TextEdit() {
  state_ = new TextEditState();
}

TextEdit::~TextEdit() {
  delete state_;
}

void TextEdit::init(std::string* text, float char_width, float line_height, bool multiline) {
  if (!state_ || !text) return;

  text_ = text;
  state_->string.text = text;
  state_->string.char_width = char_width;
  state_->string.line_height = line_height;
  stb_textedit_initialize_state(&state_->stb_state, multiline ? 1 : 0);
  state_->initialized = true;
}

bool TextEdit::key(int key, bool shift, bool ctrl) {
  if (!state_ || !state_->initialized || !text_) return false;

  const int stb_key = map_key_to_stb(key, shift, ctrl);
  if (stb_key == 0) return false;

  stb_textedit_clamp(&state_->string, &state_->stb_state);
  const int navigation_key = stb_key & ~STB_TEXTEDIT_K_SHIFT;

  const bool boundary_navigation =
      navigation_key == STB_TEXTEDIT_K_LEFT ||
      navigation_key == STB_TEXTEDIT_K_RIGHT ||
      navigation_key == STB_TEXTEDIT_K_WORDLEFT ||
      navigation_key == STB_TEXTEDIT_K_WORDRIGHT;

  if (boundary_navigation) {
    const bool moving_left = navigation_key == STB_TEXTEDIT_K_LEFT ||
                             navigation_key == STB_TEXTEDIT_K_WORDLEFT;

    if (!shift && has_selection()) {
      const size_t target = static_cast<size_t>(
          moving_left ? selection_start() : selection_end());
      move_textedit_cursor(&state_->stb_state, target, false);
      return true;
    }

    const size_t cursor = static_cast<size_t>(state_->stb_state.cursor);
    size_t target = cursor;
    if (navigation_key == STB_TEXTEDIT_K_LEFT) {
      target = previous_grapheme_boundary(*text_, cursor);
    } else if (navigation_key == STB_TEXTEDIT_K_RIGHT) {
      target = next_grapheme_boundary(*text_, cursor);
    } else if (navigation_key == STB_TEXTEDIT_K_WORDLEFT) {
      target = previous_word_boundary(*text_, cursor);
    } else {
      target = next_word_boundary(*text_, cursor);
    }
    move_textedit_cursor(&state_->stb_state, target, shift);
    return true;
  }

  if (!has_selection() &&
      (navigation_key == STB_TEXTEDIT_K_BACKSPACE ||
       navigation_key == STB_TEXTEDIT_K_DELETE)) {
    const size_t cursor = static_cast<size_t>(state_->stb_state.cursor);
    size_t start = cursor;
    size_t end = cursor;

    if (navigation_key == STB_TEXTEDIT_K_BACKSPACE) {
      start = previous_grapheme_boundary(*text_, cursor);
    } else {
      end = next_grapheme_boundary(*text_, cursor);
    }

    if (start != end) {
      state_->stb_state.select_start = static_cast<int>(start);
      state_->stb_state.select_end = static_cast<int>(end);
      state_->stb_state.cursor = static_cast<int>(end);
      stb_textedit_key(&state_->string, &state_->stb_state, navigation_key);
    }
    return true;
  }

  stb_textedit_key(&state_->string, &state_->stb_state, stb_key);
  return true;
}

void TextEdit::insert_text(const std::string& text) {
  if (!state_ || !state_->initialized || text.empty()) return;

  // A TextInput payload is one edit and may contain UTF-8 bytes that the
  // printable-key filter intentionally rejects.
  stb_textedit_paste(&state_->string, &state_->stb_state, text.data(),
                     static_cast<int>(text.size()));
}

void TextEdit::click(float x, float y) {
  if (!state_ || !state_->initialized) return;
  stb_textedit_click(&state_->string, &state_->stb_state, x, y);
}

void TextEdit::drag(float x, float y) {
  if (!state_ || !state_->initialized) return;
  stb_textedit_drag(&state_->string, &state_->stb_state, x, y);
}

std::string TextEdit::cut() {
  if (!state_ || !state_->initialized || !has_selection()) return "";

  std::string result = selected_text();
  stb_textedit_cut(&state_->string, &state_->stb_state);
  return result;
}

std::string TextEdit::copy() const {
  return selected_text();
}

void TextEdit::paste(const std::string& text) {
  if (!state_ || !state_->initialized || text.empty()) return;
  stb_textedit_paste(&state_->string, &state_->stb_state, text.c_str(), static_cast<int>(text.length()));
}

void TextEdit::undo() {
  if (!state_ || !state_->initialized) return;
  stb_textedit_key(&state_->string, &state_->stb_state, STB_TEXTEDIT_K_UNDO);
}

void TextEdit::redo() {
  if (!state_ || !state_->initialized) return;
  stb_textedit_key(&state_->string, &state_->stb_state, STB_TEXTEDIT_K_REDO);
}

bool TextEdit::has_selection() const {
  if (!state_ || !state_->initialized) return false;
  return state_->stb_state.select_start != state_->stb_state.select_end;
}

void TextEdit::select_all() {
  if (!state_ || !state_->initialized || !text_) return;
  state_->stb_state.select_start = 0;
  state_->stb_state.select_end = static_cast<int>(text_->length());
  state_->stb_state.cursor = state_->stb_state.select_end;
}

void TextEdit::clear_selection() {
  if (!state_ || !state_->initialized) return;
  state_->stb_state.select_start = state_->stb_state.cursor;
  state_->stb_state.select_end = state_->stb_state.cursor;
}

int TextEdit::selection_start() const {
  if (!state_ || !state_->initialized) return 0;
  return std::min(state_->stb_state.select_start, state_->stb_state.select_end);
}

int TextEdit::selection_end() const {
  if (!state_ || !state_->initialized) return 0;
  return std::max(state_->stb_state.select_start, state_->stb_state.select_end);
}

std::string TextEdit::selected_text() const {
  if (!state_ || !state_->initialized || !text_ || !has_selection()) return "";
  int start = selection_start();
  int end = selection_end();
  return text_->substr(start, end - start);
}

void TextEdit::set_selection(int start, int end) {
  if (!state_ || !state_->initialized || !text_) return;
  const int length = static_cast<int>(text_->length());
  start = std::max(0, std::min(start, length));
  end = std::max(0, std::min(end, length));
  state_->stb_state.select_start = start;
  state_->stb_state.select_end = end;
  state_->stb_state.cursor = end;
}

int TextEdit::cursor() const {
  if (!state_ || !state_->initialized) return 0;
  return state_->stb_state.cursor;
}

void TextEdit::set_cursor(int pos) {
  if (!state_ || !state_->initialized || !text_) return;
  pos = std::max(0, std::min(pos, static_cast<int>(text_->length())));
  state_->stb_state.cursor = pos;
  state_->stb_state.select_start = pos;
  state_->stb_state.select_end = pos;
}

// Static key code accessors
int TextEdit::k_left() { return STB_TEXTEDIT_K_LEFT; }
int TextEdit::k_right() { return STB_TEXTEDIT_K_RIGHT; }
int TextEdit::k_up() { return STB_TEXTEDIT_K_UP; }
int TextEdit::k_down() { return STB_TEXTEDIT_K_DOWN; }
int TextEdit::k_linestart() { return STB_TEXTEDIT_K_LINESTART; }
int TextEdit::k_lineend() { return STB_TEXTEDIT_K_LINEEND; }
int TextEdit::k_textstart() { return STB_TEXTEDIT_K_TEXTSTART; }
int TextEdit::k_textend() { return STB_TEXTEDIT_K_TEXTEND; }
int TextEdit::k_delete() { return STB_TEXTEDIT_K_DELETE; }
int TextEdit::k_backspace() { return STB_TEXTEDIT_K_BACKSPACE; }
int TextEdit::k_undo() { return STB_TEXTEDIT_K_UNDO; }
int TextEdit::k_redo() { return STB_TEXTEDIT_K_REDO; }
int TextEdit::k_wordleft() { return STB_TEXTEDIT_K_WORDLEFT; }
int TextEdit::k_wordright() { return STB_TEXTEDIT_K_WORDRIGHT; }
int TextEdit::k_shift() { return STB_TEXTEDIT_K_SHIFT; }

// ============================================================================
// Helper: Map platform keys to stb_textedit keys
// ============================================================================

int map_key_to_stb(int key, bool shift, bool ctrl) {
  int stb_key = 0;

  // These match flexUI::KeyCode values from event.h (GLFW style)
  constexpr int KEY_BACKSPACE = 259;
  constexpr int KEY_DELETE = 261;
  constexpr int KEY_LEFT = 263;
  constexpr int KEY_RIGHT = 262;
  constexpr int KEY_UP = 265;
  constexpr int KEY_DOWN = 264;
  constexpr int KEY_HOME = 268;
  constexpr int KEY_END = 269;
  constexpr int KEY_PAGEUP = 266;
  constexpr int KEY_PAGEDOWN = 267;

  switch (key) {
    case KEY_LEFT:
      stb_key = ctrl ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT;
      break;
    case KEY_RIGHT:
      stb_key = ctrl ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT;
      break;
    case KEY_UP:
      stb_key = STB_TEXTEDIT_K_UP;
      break;
    case KEY_DOWN:
      stb_key = STB_TEXTEDIT_K_DOWN;
      break;
    case KEY_HOME:
      stb_key = ctrl ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_LINESTART;
      break;
    case KEY_END:
      stb_key = ctrl ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_LINEEND;
      break;
    case KEY_BACKSPACE:
      stb_key = STB_TEXTEDIT_K_BACKSPACE;
      break;
    case KEY_DELETE:
      stb_key = STB_TEXTEDIT_K_DELETE;
      break;
    case KEY_PAGEUP:
      stb_key = STB_TEXTEDIT_K_PGUP;
      break;
    case KEY_PAGEDOWN:
      stb_key = STB_TEXTEDIT_K_PGDOWN;
      break;
    default:
      return 0;  // Not handled
  }

  if (shift && stb_key) {
    stb_key |= STB_TEXTEDIT_K_SHIFT;
  }

  return stb_key;
}

} // namespace flexUI
