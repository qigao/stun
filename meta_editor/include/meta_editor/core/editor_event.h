/*
 * Meta Editor SDK - Platform-agnostic Event System
 *
 * Abstracts input events from platform-specific implementations (SDL, GLFW, etc.)
 */

#pragma once

#include <cstdint>

namespace meta_editor {

// Mouse button identifiers
enum class MouseButton : uint8_t {
    Left = 0,
    Middle = 1,
    Right = 2
};

// Keyboard modifier flags
enum ModifierFlags : uint16_t {
    Mod_None  = 0,
    Mod_Shift = 1 << 0,
    Mod_Ctrl  = 1 << 1,
    Mod_Alt   = 1 << 2,
    Mod_Super = 1 << 3  // Cmd on Mac, Win on Windows
};

// Platform-agnostic editor event
struct EditorEvent {
    enum class Type : uint8_t {
        None,
        PointerDown,
        PointerMove,
        PointerUp,
        Wheel,
        KeyDown,
        KeyUp,
        TextInput
    };

    Type type = Type::None;

    // Pointer data (screen coordinates)
    float x = 0;
    float y = 0;

    // Wheel data
    float wheel_x = 0;
    float wheel_y = 0;

    // Button/Key data
    MouseButton button = MouseButton::Left;
    int key = 0;           // Platform-neutral key code (use ASCII for letters)
    uint16_t mods = 0;     // ModifierFlags

    // Text input (for TextInput events)
    char text[32] = {0};

    // Factory methods
    static EditorEvent pointer_down(float x, float y, MouseButton btn = MouseButton::Left) {
        EditorEvent e;
        e.type = Type::PointerDown;
        e.x = x;
        e.y = y;
        e.button = btn;
        return e;
    }

    static EditorEvent pointer_move(float x, float y) {
        EditorEvent e;
        e.type = Type::PointerMove;
        e.x = x;
        e.y = y;
        return e;
    }

    static EditorEvent pointer_up(float x, float y, MouseButton btn = MouseButton::Left) {
        EditorEvent e;
        e.type = Type::PointerUp;
        e.x = x;
        e.y = y;
        e.button = btn;
        return e;
    }

    static EditorEvent wheel(float x, float y, float dx, float dy) {
        EditorEvent e;
        e.type = Type::Wheel;
        e.x = x;
        e.y = y;
        e.wheel_x = dx;
        e.wheel_y = dy;
        return e;
    }

    static EditorEvent key_down(int key, uint16_t mods = 0) {
        EditorEvent e;
        e.type = Type::KeyDown;
        e.key = key;
        e.mods = mods;
        return e;
    }

    static EditorEvent key_up(int key, uint16_t mods = 0) {
        EditorEvent e;
        e.type = Type::KeyUp;
        e.key = key;
        e.mods = mods;
        return e;
    }

    static EditorEvent text_input(const char* txt) {
        EditorEvent e;
        e.type = Type::TextInput;
        for (int i = 0; i < 31 && txt[i]; ++i) {
            e.text[i] = txt[i];
        }
        return e;
    }

    // Helper methods
    bool has_shift() const { return (mods & Mod_Shift) != 0; }
    bool has_ctrl() const { return (mods & Mod_Ctrl) != 0; }
    bool has_alt() const { return (mods & Mod_Alt) != 0; }
};

} // namespace meta_editor
