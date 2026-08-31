#include "flexUI/gcanvas_input.h"

#include <gcanvas/font.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace flexUI {
namespace {

constexpr int kProjectedModifierMask = gcanvas::KEY_MOD_SHIFT | gcanvas::KEY_MOD_CONTROL |
                                       gcanvas::KEY_MOD_ALT | gcanvas::KEY_MOD_SUPER;
constexpr int kRecognizedModifierMask =
    kProjectedModifierMask | gcanvas::KEY_MOD_CAPS_LOCK | gcanvas::KEY_MOD_NUM_LOCK;
constexpr std::size_t kMaximumUtf8ScalarBytes = 4;

GCanvasInputResult input_error(GCanvasInputErrorCode code, const char *message) {
  return {std::nullopt, {code, message}};
}

std::optional<float> checked_float(double value) {
  constexpr double maximum = static_cast<double>(std::numeric_limits<float>::max());
  if (!std::isfinite(value) || value < -maximum || value > maximum) {
    return std::nullopt;
  }
  return static_cast<float>(value);
}

std::optional<MouseButton> map_mouse_button(gcanvas::mouse_button button) {
  switch (button) {
  case gcanvas::MOUSE_BUTTON_LEFT:
    return MouseButton::Left;
  case gcanvas::MOUSE_BUTTON_RIGHT:
    return MouseButton::Right;
  case gcanvas::MOUSE_BUTTON_MIDDL:
    return MouseButton::Middle;
  default:
    return std::nullopt;
  }
}

std::optional<int> map_modifiers(int modifiers) {
  if ((modifiers & ~kRecognizedModifierMask) != 0) {
    return std::nullopt;
  }

  int mapped = static_cast<int>(KeyMod::None);
  if ((modifiers & gcanvas::KEY_MOD_SHIFT) != 0) {
    mapped |= static_cast<int>(KeyMod::Shift);
  }
  if ((modifiers & gcanvas::KEY_MOD_CONTROL) != 0) {
    mapped |= static_cast<int>(KeyMod::Control);
  }
  if ((modifiers & gcanvas::KEY_MOD_ALT) != 0) {
    mapped |= static_cast<int>(KeyMod::Alt);
  }
  if ((modifiers & gcanvas::KEY_MOD_SUPER) != 0) {
    mapped |= static_cast<int>(KeyMod::Super);
  }
  return mapped;
}

KeyCode map_key(gcanvas::keyboard_key key) {
  const int value = static_cast<int>(key);
  if ((value >= static_cast<int>(gcanvas::KEY_A) && value <= static_cast<int>(gcanvas::KEY_Z)) ||
      (value >= static_cast<int>(gcanvas::KEY_0) && value <= static_cast<int>(gcanvas::KEY_9))) {
    return static_cast<KeyCode>(value);
  }

  switch (key) {
  case gcanvas::KEY_SPACE:
    return KeyCode::Space;
  case gcanvas::KEY_ESCAPE:
    return KeyCode::Escape;
  case gcanvas::KEY_ENTER:
  case gcanvas::KEY_KP_ENTER:
    return KeyCode::Enter;
  case gcanvas::KEY_TAB:
    return KeyCode::Tab;
  case gcanvas::KEY_BACKSPACE:
    return KeyCode::Backspace;
  case gcanvas::KEY_INSERT:
    return KeyCode::Insert;
  case gcanvas::KEY_DELETE:
    return KeyCode::Delete;
  case gcanvas::KEY_RIGHT:
    return KeyCode::Right;
  case gcanvas::KEY_LEFT:
    return KeyCode::Left;
  case gcanvas::KEY_DOWN:
    return KeyCode::Down;
  case gcanvas::KEY_UP:
    return KeyCode::Up;
  case gcanvas::KEY_PAGE_UP:
    return KeyCode::PageUp;
  case gcanvas::KEY_PAGE_DOWN:
    return KeyCode::PageDown;
  case gcanvas::KEY_HOME:
    return KeyCode::Home;
  case gcanvas::KEY_END:
    return KeyCode::End;
  case gcanvas::KEY_LEFT_SHIFT:
  case gcanvas::KEY_RIGHT_SHIFT:
    return KeyCode::Shift;
  case gcanvas::KEY_LEFT_CONTROL:
  case gcanvas::KEY_RIGHT_CONTROL:
    return KeyCode::Control;
  case gcanvas::KEY_LEFT_ALT:
  case gcanvas::KEY_RIGHT_ALT:
    return KeyCode::Alt;
  case gcanvas::KEY_LEFT_SUPER:
  case gcanvas::KEY_RIGHT_SUPER:
    return KeyCode::Super;
  default:
    return KeyCode::Unknown;
  }
}

bool is_unicode_scalar(unsigned int codepoint) {
  constexpr unsigned int maximum_codepoint = 0x10FFFFU;
  constexpr unsigned int surrogate_first = 0xD800U;
  constexpr unsigned int surrogate_last = 0xDFFFU;
  return codepoint <= maximum_codepoint &&
         (codepoint < surrogate_first || codepoint > surrogate_last);
}

std::optional<std::string> copy_scalar_utf8(const char *utf8) {
  if (utf8 == nullptr) {
    return std::nullopt;
  }
  std::size_t length = 0;
  while (length <= kMaximumUtf8ScalarBytes && utf8[length] != '\0') {
    ++length;
  }
  if (length == 0 || length > kMaximumUtf8ScalarBytes) {
    return std::nullopt;
  }
  return std::string(utf8, length);
}

} // namespace

GCanvasInputResult GCanvasInputNormalizer::mouse_move(const gcanvas::mouse_move_event &native) {
  const auto x = checked_float(native.x);
  const auto y = checked_float(native.y);
  if (!x || !y) {
    return input_error(GCanvasInputErrorCode::NonFiniteScalar,
                       "gCanvas pointer coordinates must be finite float values");
  }

  pointer_x_ = x;
  pointer_y_ = y;
  return {Event::mouse_move(*x, *y), {}};
}

GCanvasInputResult GCanvasInputNormalizer::mouse_button(const gcanvas::mouse_button_event &native) {
  const auto x = checked_float(native.x);
  const auto y = checked_float(native.y);
  if (!x || !y) {
    return input_error(GCanvasInputErrorCode::NonFiniteScalar,
                       "gCanvas pointer coordinates must be finite float values");
  }
  const auto button = map_mouse_button(native.button);
  if (!button) {
    return input_error(GCanvasInputErrorCode::UnsupportedMouseButton,
                       "gCanvas mouse button is not supported by FlexUI");
  }
  if (native.action != gcanvas::ACTION_PRESS && native.action != gcanvas::ACTION_RELEASE) {
    return input_error(GCanvasInputErrorCode::UnsupportedAction,
                       "gCanvas mouse action must be press or release");
  }
  const auto modifiers = map_modifiers(static_cast<int>(native.mods));
  if (!modifiers) {
    return input_error(GCanvasInputErrorCode::InvalidModifiers,
                       "gCanvas mouse modifiers contain unknown bits");
  }

  pointer_x_ = x;
  pointer_y_ = y;
  Event event = native.action == gcanvas::ACTION_PRESS ? Event::mouse_down(*x, *y, *button)
                                                       : Event::mouse_up(*x, *y, *button);
  event.mods = *modifiers;
  return {std::move(event), {}};
}

GCanvasInputResult GCanvasInputNormalizer::key(const gcanvas::key_event &native) const {
  const auto modifiers = map_modifiers(static_cast<int>(native.mods));
  if (!modifiers) {
    return input_error(GCanvasInputErrorCode::InvalidModifiers,
                       "gCanvas key modifiers contain unknown bits");
  }
  if (native.action == gcanvas::ACTION_RELEASE) {
    return {Event::key_up(map_key(native.key), *modifiers), {}};
  }
  if (native.action == gcanvas::ACTION_PRESS || native.action == gcanvas::ACTION_REPEAT) {
    return {Event::key_down(map_key(native.key), *modifiers), {}};
  }
  return input_error(GCanvasInputErrorCode::UnsupportedAction,
                     "gCanvas key action must be press, repeat or release");
}

GCanvasInputResult GCanvasInputNormalizer::character(const gcanvas::char_event &native) const {
  if (native.utf8 == nullptr) {
    return input_error(GCanvasInputErrorCode::NullText,
                       "gCanvas character event must provide UTF-8 text");
  }
  if (!is_unicode_scalar(native.unnicode)) {
    return input_error(GCanvasInputErrorCode::InvalidCodepoint,
                       "gCanvas character event has an invalid Unicode scalar");
  }
  const auto text = copy_scalar_utf8(native.utf8);
  if (!text || *text != gcanvas::Font::UnicodeToUTF8(native.unnicode)) {
    return input_error(GCanvasInputErrorCode::InvalidText,
                       "gCanvas character text must match its Unicode scalar");
  }
  return {Event::text_input(*text), {}};
}

GCanvasInputResult GCanvasInputNormalizer::scroll(const gcanvas::scroll_event &native) const {
  const auto delta_x = checked_float(native.xoffset);
  const auto delta_y = checked_float(native.yoffset);
  if (!delta_x || !delta_y) {
    return input_error(GCanvasInputErrorCode::NonFiniteScalar,
                       "gCanvas scroll offsets must be finite float values");
  }
  if (!pointer_x_ || !pointer_y_) {
    return input_error(GCanvasInputErrorCode::MissingPointerPosition,
                       "gCanvas scroll input requires a prior valid pointer position");
  }
  return {Event::mouse_wheel(*pointer_x_, *pointer_y_, *delta_x, *delta_y), {}};
}

GCanvasViewportResult GCanvasInputNormalizer::resize(const gcanvas::resize_event &native) const {
  if (native.width <= 0 || native.height <= 0) {
    return {std::nullopt,
            {GCanvasInputErrorCode::InvalidViewportSize,
             "gCanvas viewport dimensions must be positive"}};
  }
  return {GCanvasViewportMetrics{native.width, native.height}, {}};
}

} // namespace flexUI
