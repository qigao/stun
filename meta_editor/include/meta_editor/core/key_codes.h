/*
 * Meta Editor - Key Codes
 *
 * Platform-agnostic key codes for editor input handling.
 */

#pragma once

namespace meta_editor {

enum class Key {
    Unknown = 0,
    
    // Letters
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Numbers
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    
    // Function keys
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    
    // Navigation
    Left = 263,
    Right = 262,
    Up = 265,
    Down = 264,
    Home = 268,
    End = 269,
    PageUp = 266,
    PageDown = 267,
    
    // Editing
    Backspace = 259,
    Delete = 261,
    Enter = 257,
    Tab = 258,
    Space = 32,
    Escape = 256,
    Insert = 260,
    
    // Modifiers (for reference, usually handled via mods flags)
    LeftShift = 340,
    RightShift = 344,
    LeftControl = 341,
    RightControl = 345,
    LeftAlt = 342,
    RightAlt = 346,
};

enum class Mod {
    None = 0,
    Shift = 0x0001,
    Control = 0x0002,
    Alt = 0x0004,
    Super = 0x0008,
};

inline bool has_shift(int mods) { return (mods & static_cast<int>(Mod::Shift)) != 0; }
inline bool has_ctrl(int mods) { return (mods & static_cast<int>(Mod::Control)) != 0; }
inline bool has_alt(int mods) { return (mods & static_cast<int>(Mod::Alt)) != 0; }

} // namespace meta_editor
