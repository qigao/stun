/*
 * Meta Editor SDK - SDL Event Adapter
 *
 * Converts SDL events to platform-agnostic EditorEvent.
 * Include this only if your application uses SDL.
 *
 * Usage:
 *   #include <meta_editor/core/sdl_adapter.h>
 *   
 *   SDL_Event sdl_event;
 *   while (SDL_PollEvent(&sdl_event)) {
 *       EditorEvent ev = meta_editor::sdl_to_editor_event(sdl_event);
 *       if (ev.type != EditorEvent::Type::None) {
 *           editor.handle_event(ev);
 *       }
 *   }
 */

#pragma once

#include "editor_event.h"
#include <SDL2/SDL.h>

namespace meta_editor {

// Convert SDL modifier state to EditorEvent modifier flags
inline uint16_t sdl_mods_to_flags(int sdl_mods) {
    uint16_t flags = 0;
    if (sdl_mods & KMOD_SHIFT) flags |= Mod_Shift;
    if (sdl_mods & KMOD_CTRL)  flags |= Mod_Ctrl;
    if (sdl_mods & KMOD_ALT)   flags |= Mod_Alt;
    if (sdl_mods & KMOD_GUI)   flags |= Mod_Super;
    return flags;
}

// Convert SDL_Event to EditorEvent
inline EditorEvent sdl_to_editor_event(const SDL_Event& event) {
    EditorEvent e;
    uint16_t mods = sdl_mods_to_flags(SDL_GetModState());

    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            e.type = EditorEvent::Type::PointerDown;
            e.x = (float)event.button.x;
            e.y = (float)event.button.y;
            e.button = (event.button.button == SDL_BUTTON_RIGHT) ? MouseButton::Right :
                       (event.button.button == SDL_BUTTON_MIDDLE) ? MouseButton::Middle :
                       MouseButton::Left;
            e.mods = mods;
            break;

        case SDL_MOUSEBUTTONUP:
            e.type = EditorEvent::Type::PointerUp;
            e.x = (float)event.button.x;
            e.y = (float)event.button.y;
            e.button = (event.button.button == SDL_BUTTON_RIGHT) ? MouseButton::Right :
                       (event.button.button == SDL_BUTTON_MIDDLE) ? MouseButton::Middle :
                       MouseButton::Left;
            e.mods = mods;
            break;

        case SDL_MOUSEMOTION:
            e.type = EditorEvent::Type::PointerMove;
            e.x = (float)event.motion.x;
            e.y = (float)event.motion.y;
            e.mods = mods;
            break;

        case SDL_MOUSEWHEEL: {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            e.type = EditorEvent::Type::Wheel;
            e.x = (float)mx;
            e.y = (float)my;
            e.wheel_x = (float)event.wheel.x;
            e.wheel_y = (float)event.wheel.y;
            e.mods = mods;
            break;
        }

        case SDL_KEYDOWN:
            e.type = EditorEvent::Type::KeyDown;
            e.key = event.key.keysym.sym;
            e.mods = sdl_mods_to_flags(event.key.keysym.mod);
            break;

        case SDL_KEYUP:
            e.type = EditorEvent::Type::KeyUp;
            e.key = event.key.keysym.sym;
            e.mods = sdl_mods_to_flags(event.key.keysym.mod);
            break;

        case SDL_TEXTINPUT:
            e = EditorEvent::text_input(event.text.text);
            break;

        default:
            e.type = EditorEvent::Type::None;
            break;
    }

    return e;
}

// Check if SDL event is a window resize
inline bool is_window_resize(const SDL_Event& event) {
    return event.type == SDL_WINDOWEVENT && 
           event.window.event == SDL_WINDOWEVENT_RESIZED;
}

// Check if SDL event is quit
inline bool is_quit_event(const SDL_Event& event) {
    return event.type == SDL_QUIT;
}

// Check if middle mouse button is pressed (for panning)
inline bool is_middle_button_down(const SDL_Event& event) {
    return event.type == SDL_MOUSEBUTTONDOWN && 
           event.button.button == SDL_BUTTON_MIDDLE;
}

inline bool is_middle_button_up(const SDL_Event& event) {
    return event.type == SDL_MOUSEBUTTONUP && 
           event.button.button == SDL_BUTTON_MIDDLE;
}

} // namespace meta_editor
