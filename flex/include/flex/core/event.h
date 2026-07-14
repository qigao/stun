/*
 * Flex Engine - Event System
 *
 * Pointer events, keyboard events, hit testing, and event callbacks.
 * Supports event bubbling (target → root) and capture (root → target).
 */

#pragma once

#include "flex/core/types.h"
#include <functional>
#include <string>


namespace flex {

// Forward declarations
class Node;

// ============================================================================
// Event Types
// ============================================================================

enum class PointerEventType : uint8_t {
  Down,  // Pointer pressed
  Up,    // Pointer released
  Move,  // Pointer moved
  Enter, // Pointer entered node bounds
  Leave, // Pointer left node bounds
};

enum class KeyEventType : uint8_t {
  Down, // Key pressed
  Up,   // Key released
};

enum class FocusChangeReason : uint8_t {
  Pointer,
  Keyboard,
  Programmatic,
  Clear,
};

enum class CompositionEventType : uint8_t {
  Start,
  Update,
  End,
};

enum class EventPhase : uint8_t {
  None,    // Event not dispatched yet
  Capture, // Propagating from root to target
  Target,  // At the target node
  Bubble,  // Propagating from target to root
};

enum class MouseButton : uint8_t {
  Left = 0,
  Right = 1,
  Middle = 2,
  X1 = 3,
  X2 = 4,
  None = 255
};

enum class KeyMod : uint16_t {
  None = 0,
  Shift = 1 << 0,
  Control = 1 << 1,
  Alt = 1 << 2,
  Super = 1 << 3,
};

// ============================================================================
// Key Codes - Common key identifiers
// ============================================================================

enum class KeyCode : uint16_t {
  Unknown = 0,

  // Letters
  A = 'A',
  B = 'B',
  C = 'C',
  D = 'D',
  E = 'E',
  F = 'F',
  G = 'G',
  H = 'H',
  I = 'I',
  J = 'J',
  K = 'K',
  L = 'L',
  M = 'M',
  N = 'N',
  O = 'O',
  P = 'P',
  Q = 'Q',
  R = 'R',
  S = 'S',
  T = 'T',
  U = 'U',
  V = 'V',
  W = 'W',
  X = 'X',
  Y = 'Y',
  Z = 'Z',

  // Numbers
  Num0 = '0',
  Num1 = '1',
  Num2 = '2',
  Num3 = '3',
  Num4 = '4',
  Num5 = '5',
  Num6 = '6',
  Num7 = '7',
  Num8 = '8',
  Num9 = '9',

  // Function keys
  F1 = 256,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,

  // Navigation
  Escape = 300,
  Tab,
  Enter,
  Backspace,
  Delete,
  Insert,
  Home,
  End,
  PageUp,
  PageDown,
  Left,
  Right,
  Up,
  Down,

  // Modifiers (for key events, not just as modifiers)
  Shift = 340,
  Control,
  Alt,
  Super,

  // Special
  Space = 32,
  Comma = ',',
  Period = '.',
  Slash = '/',
  Semicolon = ';',
  Quote = '\'',
  BracketLeft = '[',
  BracketRight = ']',
  Backslash = '\\',
  Minus = '-',
  Equal = '=',
  Grave = '`',
};

// ============================================================================
// Key Modifiers - Modifier key state
// ============================================================================

struct KeyModifiers {
  bool shift = false;
  bool ctrl = false;
  bool alt = false;
  bool super = false; // Windows/Command key

  bool none() const { return !shift && !ctrl && !alt && !super; }
  bool any() const { return shift || ctrl || alt || super; }
};

// ============================================================================
// PointerEvent - Pointer interaction data
// ============================================================================

struct PointerEvent {
  PointerEventType type = PointerEventType::Move;
  EventPhase phase = EventPhase::None;
  float x = 0;                    // Global x coordinate
  float y = 0;                    // Global y coordinate
  float local_x = 0;              // Local x (relative to current_target)
  float local_y = 0;              // Local y (relative to current_target)
  MouseButton button = MouseButton::Left; // Button that triggered the event
  Node *target = nullptr;         // Original target (deepest hit node)
  Node *current_target = nullptr; // Current node in propagation chain

  bool is_down() const { return type == PointerEventType::Down; }
  bool is_up() const { return type == PointerEventType::Up; }
  bool is_move() const { return type == PointerEventType::Move; }
  bool is_enter() const { return type == PointerEventType::Enter; }
  bool is_leave() const { return type == PointerEventType::Leave; }

  // Propagation control
  void stop_propagation() { propagation_stopped_ = true; }
  bool propagation_stopped() const { return propagation_stopped_; }

private:
  bool propagation_stopped_ = false;
};

// ============================================================================
// KeyEvent - Keyboard interaction data
// ============================================================================

struct KeyEvent {
  KeyEventType type = KeyEventType::Down;
  EventPhase phase = EventPhase::None;
  KeyCode key = KeyCode::Unknown;
  KeyModifiers modifiers;
  bool repeat = false;            // True if this is a key repeat event
  Node *target = nullptr;         // Focused node receiving the event
  Node *current_target = nullptr; // Current node in propagation chain

  bool is_down() const { return type == KeyEventType::Down; }
  bool is_up() const { return type == KeyEventType::Up; }

  // Convenience checks for common shortcuts (single modifier only)
  bool is_ctrl_key(KeyCode k) const {
    return key == k && modifiers.ctrl && !modifiers.shift && !modifiers.alt && !modifiers.super;
  }
  bool is_shift_key(KeyCode k) const {
    return key == k && modifiers.shift && !modifiers.ctrl && !modifiers.alt && !modifiers.super;
  }
  bool is_alt_key(KeyCode k) const {
    return key == k && modifiers.alt && !modifiers.ctrl && !modifiers.shift && !modifiers.super;
  }

  // Propagation control
  void stop_propagation() { propagation_stopped_ = true; }
  bool propagation_stopped() const { return propagation_stopped_; }

private:
  bool propagation_stopped_ = false;
};

// ============================================================================
// FocusEvent - Focus ownership changes
// ============================================================================

struct FocusEvent {
  bool gained = false;
  FocusChangeReason reason = FocusChangeReason::Programmatic;
  Node *target = nullptr;
  Node *related_target = nullptr;
  Node *current_target = nullptr;
};

// ============================================================================
// TextInputEvent - Committed UTF-8 text input
// ============================================================================

struct TextInputEvent {
  std::string text;
  bool from_ime = false;
  Node *target = nullptr;
  Node *current_target = nullptr;
};

// ============================================================================
// CompositionEvent - IME/preedit text input
// ============================================================================

struct CompositionEvent {
  CompositionEventType type = CompositionEventType::Update;
  std::string text;
  int selection_start = -1;
  int selection_end = -1;
  Node *target = nullptr;
  Node *current_target = nullptr;
};

// ============================================================================
// Event Callback Types
// ============================================================================

using PointerEventCallback = std::function<void(PointerEvent &)>;
using KeyEventCallback = std::function<void(KeyEvent &)>;
using FocusEventCallback = std::function<void(FocusEvent &)>;
using TextInputEventCallback = std::function<void(TextInputEvent &)>;
using CompositionEventCallback = std::function<void(CompositionEvent &)>;
using ClickCallback = std::function<void()>;
using FocusCallback = std::function<void(bool)>; // true = gained focus, false = lost focus

// ============================================================================
// EventDispatcher - On-demand event callback storage (Phase 2.1 Optimization)
// ============================================================================

// Only allocated when a Node actually registers event handlers.
// Reduces memory overhead from 360B per node to 8B (pointer only).
// 90% of nodes never use events, so this saves significant memory.
struct EventDispatcher {
  // Pointer events
  PointerEventCallback on_pointer_down;
  PointerEventCallback on_pointer_up;
  PointerEventCallback on_pointer_move;
  PointerEventCallback on_hover_enter;
  PointerEventCallback on_hover_leave;
  ClickCallback on_click;

  // Keyboard events
  KeyEventCallback on_key_down;
  KeyEventCallback on_key_up;
  FocusCallback on_focus;
  FocusEventCallback on_focus_event;
  TextInputEventCallback on_text_input;
  CompositionEventCallback on_composition;

  // Helper: Check if any pointer handlers are registered
  bool has_pointer_handlers() const {
    return on_pointer_down || on_pointer_up || on_pointer_move || on_hover_enter ||
           on_hover_leave || on_click;
  }

  // Helper: Check if any keyboard handlers are registered
  bool has_key_handlers() const {
    return on_key_down || on_key_up || on_focus || on_focus_event || on_text_input ||
           on_composition;
  }
};

} // namespace flex
