/*
 * Meta Editor SDK - GLFW Event Adapter
 *
 * Converts GLFW events to platform-agnostic EditorEvent.
 */

#pragma once

#include "editor_event.h"

// Don't let GLFW include OpenGL headers (we use glad)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace meta_editor {

inline uint16_t glfw_mods_to_flags(int mods) {
    uint16_t flags = 0;
    if (mods & GLFW_MOD_SHIFT)   flags |= Mod_Shift;
    if (mods & GLFW_MOD_CONTROL) flags |= Mod_Ctrl;
    if (mods & GLFW_MOD_ALT)     flags |= Mod_Alt;
    if (mods & GLFW_MOD_SUPER)   flags |= Mod_Super;
    return flags;
}

inline EditorEvent glfw_mouse_button_event(int button, int action, int mods, double x, double y) {
    EditorEvent e;
    e.type = (action == GLFW_PRESS) ? EditorEvent::Type::PointerDown : EditorEvent::Type::PointerUp;
    e.x = (float)x;
    e.y = (float)y;
    e.button = (button == GLFW_MOUSE_BUTTON_RIGHT) ? MouseButton::Right :
               (button == GLFW_MOUSE_BUTTON_MIDDLE) ? MouseButton::Middle :
               MouseButton::Left;
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

inline EditorEvent glfw_cursor_event(double x, double y, int mods) {
    EditorEvent e;
    e.type = EditorEvent::Type::PointerMove;
    e.x = (float)x;
    e.y = (float)y;
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

inline EditorEvent glfw_scroll_event(double xoffset, double yoffset, double mx, double my, int mods) {
    EditorEvent e;
    e.type = EditorEvent::Type::Wheel;
    e.x = (float)mx;
    e.y = (float)my;
    e.wheel_x = (float)xoffset;
    e.wheel_y = (float)yoffset;
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

inline EditorEvent glfw_key_event(int key, int action, int mods) {
    EditorEvent e;
    e.type = (action == GLFW_PRESS || action == GLFW_REPEAT) ? 
             EditorEvent::Type::KeyDown : EditorEvent::Type::KeyUp;
    e.key = key;
    e.mods = glfw_mods_to_flags(mods);
    return e;
}

inline EditorEvent glfw_char_event(unsigned int codepoint) {
    char buf[5] = {0};
    if (codepoint < 0x80) {
        buf[0] = (char)codepoint;
    } else if (codepoint < 0x800) {
        buf[0] = (char)(0xC0 | (codepoint >> 6));
        buf[1] = (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        buf[0] = (char)(0xE0 | (codepoint >> 12));
        buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (codepoint & 0x3F));
    } else {
        buf[0] = (char)(0xF0 | (codepoint >> 18));
        buf[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (codepoint & 0x3F));
    }
    return EditorEvent::text_input(buf);
}

} // namespace meta_editor
