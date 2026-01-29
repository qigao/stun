/*
 * Meta Editor SDK - GLFW Event Adapter
 *
 * Converts GLFW events to platform-agnostic EditorEvent.
 * Include this only if your application uses GLFW.
 *
 * Usage:
 *   #include <meta_editor/core/glfw_adapter.h>
 *
 *   // In GLFW callbacks:
 *   void mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
 *       double x, y;
 *       glfwGetCursorPos(w, &x, &y);
 *       EditorEvent ev = meta_editor::glfw_mouse_button_event(x, y, button, action, mods);
 *       editor.handle_event(ev);
 *   }
 */

#pragma once

#include "editor_event.h"
#include <GLFW/glfw3.h>

namespace meta_editor {

// Convert GLFW modifier bits to EditorEvent modifier flags
inline uint16_t glfw_mods_to_flags(int glfw_mods) {
    uint16_t flags = 0;
    if (glfw_mods & GLFW_MOD_SHIFT)   flags |= Mod_Shift;
    if (glfw_mods & GLFW_MOD_CONTROL) flags |= Mod_Ctrl;
    if (glfw_mods & GLFW_MOD_ALT)     flags |= Mod_Alt;
    if (glfw_mods & GLFW_MOD_SUPER)   flags |= Mod_Super;
    return flags;
}

// Convert GLFW mouse button to MouseButton
inline MouseButton glfw_to_mouse_button(int button) {
    switch (button) {
        case GLFW_MOUSE_BUTTON_RIGHT:  return MouseButton::Right;
        case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
        default:                       return MouseButton::Left;
    }
}

// Create EditorEvent from GLFW mouse button callback
// Call from: glfwSetMouseButtonCallback
inline EditorEvent glfw_mouse_button_event(double x, double y, int button, int action, int mods) {
    EditorEvent e;
    e.type = (action == GLFW_PRESS) ? EditorEvent::Type::PointerDown : EditorEvent::Type::PointerUp;
    e.x = (float)x;
    e.y = (float)y;
    e.button = glfw_to_mouse_button(button);
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

// Create EditorEvent from GLFW cursor position callback
// Call from: glfwSetCursorPosCallback
inline EditorEvent glfw_cursor_pos_event(double x, double y, int mods = 0) {
    EditorEvent e;
    e.type = EditorEvent::Type::PointerMove;
    e.x = (float)x;
    e.y = (float)y;
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

// Create EditorEvent from GLFW scroll callback
// Call from: glfwSetScrollCallback
inline EditorEvent glfw_scroll_event(double cursor_x, double cursor_y, double xoffset, double yoffset, int mods = 0) {
    EditorEvent e;
    e.type = EditorEvent::Type::Wheel;
    e.x = (float)cursor_x;
    e.y = (float)cursor_y;
    e.wheel_x = (float)xoffset;
    e.wheel_y = (float)yoffset;
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

// Create EditorEvent from GLFW key callback
// Call from: glfwSetKeyCallback
inline EditorEvent glfw_key_event(int key, int scancode, int action, int mods) {
    if (action == GLFW_REPEAT) {
        // Treat repeat as key down
        EditorEvent e;
        e.type = EditorEvent::Type::KeyDown;
        e.key = key;
        e.mods = glfw_mods_to_flags(mods);
        return e;
    }

    EditorEvent e;
    e.type = (action == GLFW_PRESS) ? EditorEvent::Type::KeyDown : EditorEvent::Type::KeyUp;
    e.key = key;
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

// Create EditorEvent from GLFW character callback
// Call from: glfwSetCharCallback
inline EditorEvent glfw_char_event(unsigned int codepoint) {
    EditorEvent e;
    e.type = EditorEvent::Type::TextInput;
    // Convert Unicode codepoint to UTF-8
    if (codepoint < 0x80) {
        e.text[0] = (char)codepoint;
        e.text[1] = 0;
    } else if (codepoint < 0x800) {
        e.text[0] = (char)(0xC0 | (codepoint >> 6));
        e.text[1] = (char)(0x80 | (codepoint & 0x3F));
        e.text[2] = 0;
    } else if (codepoint < 0x10000) {
        e.text[0] = (char)(0xE0 | (codepoint >> 12));
        e.text[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        e.text[2] = (char)(0x80 | (codepoint & 0x3F));
        e.text[3] = 0;
    } else {
        e.text[0] = (char)(0xF0 | (codepoint >> 18));
        e.text[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        e.text[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        e.text[3] = (char)(0x80 | (codepoint & 0x3F));
        e.text[4] = 0;
    }
    return e;
}

// Helper: Check if middle mouse button (for panning)
inline bool is_middle_button(int button) {
    return button == GLFW_MOUSE_BUTTON_MIDDLE;
}

} // namespace meta_editor
