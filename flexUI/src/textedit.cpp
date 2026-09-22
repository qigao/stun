/*
 * flexUI - Text Edit Implementation
 *
 * Uses stb_textedit.h for robust text manipulation.
 * All configuration macros must be defined BEFORE including stb_textedit.h
 */

#include <flexUI/textedit.h>
#include <flexUI/text_util.h>
#include <string>
#include <cstring>
#include <algorithm>

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
  if (!state_ || !state_->initialized) return false;

  int stb_key = map_key_to_stb(key, shift, ctrl);
  if (stb_key == 0) return false;

  // stb stores byte offsets. Move by one decoded scalar while retaining its
  // selection-anchor and selection-collapse behavior for each arrow key.
  size_t key_steps = 1;
  const int navigation_key = stb_key & ~STB_TEXTEDIT_K_SHIFT;
  const bool scalar_navigation = navigation_key == STB_TEXTEDIT_K_LEFT ||
                                 navigation_key == STB_TEXTEDIT_K_RIGHT;
  if (scalar_navigation) {
    stb_textedit_clamp(&state_->string, &state_->stb_state);
  }
  if (scalar_navigation && (shift || !has_selection())) {
    const size_t cursor = static_cast<size_t>(state_->stb_state.cursor);
    if (navigation_key == STB_TEXTEDIT_K_RIGHT && cursor < text_->size()) {
      size_t next = cursor;
      (void)utf8_next_scalar(*text_, next);
      key_steps = next - cursor;
    } else if (navigation_key == STB_TEXTEDIT_K_LEFT && cursor > 0) {
      size_t previous = 0;
      for (size_t next = 0; next < cursor;) {
        previous = next;
        (void)utf8_next_scalar(*text_, next);
      }
      key_steps = cursor - previous;
    }
  }
  for (size_t step = 0; step < key_steps; ++step) {
    stb_textedit_key(&state_->string, &state_->stb_state, stb_key);
  }
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
