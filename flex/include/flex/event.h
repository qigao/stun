/*
 * Flex Engine - Event System
 *
 * Pointer events, keyboard events, hit testing, and event callbacks.
 * Supports event bubbling (target → root) and capture (root → target).
 */

#pragma once

#include <string>
#include <functional>
#include "flex/types.h"

namespace flex {

// Forward declarations
class Node;

// ============================================================================
// Event Types
// ============================================================================

enum class PointerEventType : uint8_t {
    Down,       // Pointer pressed
    Up,         // Pointer released
    Move,       // Pointer moved
    Enter,      // Pointer entered node bounds
    Leave,      // Pointer left node bounds
};

enum class KeyEventType : uint8_t {
    Down,       // Key pressed
    Up,         // Key released
};

enum class EventPhase : uint8_t {
    None,       // Event not dispatched yet
    Capture,    // Propagating from root to target
    Target,     // At the target node
    Bubble,     // Propagating from target to root
};

// ============================================================================
// Key Codes - Common key identifiers
// ============================================================================

enum class KeyCode : uint16_t {
    Unknown = 0,

    // Letters
    A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G', H = 'H',
    I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N', O = 'O', P = 'P',
    Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U', V = 'V', W = 'W', X = 'X',
    Y = 'Y', Z = 'Z',

    // Numbers
    Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
    Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

    // Function keys
    F1 = 256, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Navigation
    Escape = 300, Tab, Enter, Backspace, Delete, Insert,
    Home, End, PageUp, PageDown,
    Left, Right, Up, Down,

    // Modifiers (for key events, not just as modifiers)
    Shift = 340, Control, Alt, Super,

    // Special
    Space = 32,
    Comma = ',', Period = '.', Slash = '/',
    Semicolon = ';', Quote = '\'',
    BracketLeft = '[', BracketRight = ']', Backslash = '\\',
    Minus = '-', Equal = '=', Grave = '`',
};

// ============================================================================
// Key Modifiers - Modifier key state
// ============================================================================

struct KeyModifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false;  // Windows/Command key

    bool none() const { return !shift && !ctrl && !alt && !super; }
    bool any() const { return shift || ctrl || alt || super; }
};

// ============================================================================
// PointerEvent - Pointer interaction data
// ============================================================================

struct PointerEvent {
    PointerEventType type = PointerEventType::Move;
    EventPhase phase = EventPhase::None;
    float x = 0;            // Global x coordinate
    float y = 0;            // Global y coordinate
    float local_x = 0;      // Local x (relative to current_target)
    float local_y = 0;      // Local y (relative to current_target)
    Node* target = nullptr;         // Original target (deepest hit node)
    Node* current_target = nullptr; // Current node in propagation chain

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
    bool repeat = false;    // True if this is a key repeat event
    Node* target = nullptr;         // Focused node receiving the event
    Node* current_target = nullptr; // Current node in propagation chain

    bool is_down() const { return type == KeyEventType::Down; }
    bool is_up() const { return type == KeyEventType::Up; }

    // Convenience checks for common shortcuts (single modifier only)
    bool is_ctrl_key(KeyCode k) const { return key == k && modifiers.ctrl && !modifiers.shift && !modifiers.alt && !modifiers.super; }
    bool is_shift_key(KeyCode k) const { return key == k && modifiers.shift && !modifiers.ctrl && !modifiers.alt && !modifiers.super; }
    bool is_alt_key(KeyCode k) const { return key == k && modifiers.alt && !modifiers.ctrl && !modifiers.shift && !modifiers.super; }

    // Propagation control
    void stop_propagation() { propagation_stopped_ = true; }
    bool propagation_stopped() const { return propagation_stopped_; }

private:
    bool propagation_stopped_ = false;
};

// ============================================================================
// Event Callback Types
// ============================================================================

using PointerEventCallback = std::function<void(PointerEvent&)>;
using KeyEventCallback = std::function<void(KeyEvent&)>;
using ClickCallback = std::function<void()>;
using FocusCallback = std::function<void(bool)>;  // true = gained focus, false = lost focus

} // namespace flex
