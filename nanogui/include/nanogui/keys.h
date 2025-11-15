/*
    nanogui/keys.h -- Cross-platform key code definitions

    This header provides unified key code definitions that work across
    both GLFW and SDL3 backends.

    Example usage:

        bool MyWidget::keyboard_event(int key, int scancode, int action, int modifiers) {
            if (action == NANOGUI_KEY_PRESS) {
                // Check for Ctrl+C
                if (key == NANOGUI_KEY_C && NANOGUI_HAS_CTRL(modifiers)) {
                    copy_to_clipboard();
                    return true;
                }
                // Check for Escape
                if (key == NANOGUI_KEY_ESCAPE) {
                    close();
                    return true;
                }
            }
            return Widget::keyboard_event(key, scancode, action, modifiers);
        }

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

// Cross-platform key code definitions
#if defined(NANOGUI_USE_SDL3)
  #include <SDL3/SDL.h>

  // Common keys
  #define NANOGUI_KEY_ESCAPE SDLK_ESCAPE
  #define NANOGUI_KEY_SPACE SDLK_SPACE
  #define NANOGUI_KEY_ENTER SDLK_RETURN
  #define NANOGUI_KEY_DELETE SDLK_DELETE
  #define NANOGUI_KEY_BACKSPACE SDLK_BACKSPACE

  // Function keys
  #define NANOGUI_KEY_F1 SDLK_F1

  // Special keys
  #define NANOGUI_KEY_SLASH SDLK_SLASH

  // Arrow keys
  #define NANOGUI_KEY_UP SDLK_UP
  #define NANOGUI_KEY_DOWN SDLK_DOWN
  #define NANOGUI_KEY_LEFT SDLK_LEFT
  #define NANOGUI_KEY_RIGHT SDLK_RIGHT

  // Letter keys
  #define NANOGUI_KEY_A SDLK_A
  #define NANOGUI_KEY_C SDLK_C
  #define NANOGUI_KEY_D SDLK_D
  #define NANOGUI_KEY_F SDLK_F
  #define NANOGUI_KEY_G SDLK_G
  #define NANOGUI_KEY_M SDLK_M
  #define NANOGUI_KEY_O SDLK_O
  #define NANOGUI_KEY_R SDLK_R
  #define NANOGUI_KEY_S SDLK_S
  #define NANOGUI_KEY_V SDLK_V
  #define NANOGUI_KEY_X SDLK_X
  #define NANOGUI_KEY_Y SDLK_Y
  #define NANOGUI_KEY_Z SDLK_Z

  // Bracket keys
  #define NANOGUI_KEY_LEFTBRACKET SDLK_LEFTBRACKET
  #define NANOGUI_KEY_RIGHTBRACKET SDLK_RIGHTBRACKET

  // Punctuation keys
  #define NANOGUI_KEY_SEMICOLON SDLK_SEMICOLON

  // Key actions
  #define NANOGUI_KEY_PRESS 1
  #define NANOGUI_KEY_RELEASE 0

  // Modifier keys
  #define NANOGUI_MOD_CTRL SDL_KMOD_CTRL
  #define NANOGUI_MOD_SHIFT SDL_KMOD_SHIFT
  #define NANOGUI_MOD_ALT SDL_KMOD_ALT
  #define NANOGUI_MOD_SUPER SDL_KMOD_GUI

#else
  #include <GLFW/glfw3.h>

  // Common keys
  #define NANOGUI_KEY_ESCAPE GLFW_KEY_ESCAPE
  #define NANOGUI_KEY_SPACE GLFW_KEY_SPACE
  #define NANOGUI_KEY_ENTER GLFW_KEY_ENTER
  #define NANOGUI_KEY_DELETE GLFW_KEY_DELETE
  #define NANOGUI_KEY_BACKSPACE GLFW_KEY_BACKSPACE

  // Function keys
  #define NANOGUI_KEY_F1 GLFW_KEY_F1

  // Special keys
  #define NANOGUI_KEY_SLASH GLFW_KEY_SLASH

  // Arrow keys
  #define NANOGUI_KEY_UP GLFW_KEY_UP
  #define NANOGUI_KEY_DOWN GLFW_KEY_DOWN
  #define NANOGUI_KEY_LEFT GLFW_KEY_LEFT
  #define NANOGUI_KEY_RIGHT GLFW_KEY_RIGHT

  // Letter keys
  #define NANOGUI_KEY_A GLFW_KEY_A
  #define NANOGUI_KEY_C GLFW_KEY_C
  #define NANOGUI_KEY_D GLFW_KEY_D
  #define NANOGUI_KEY_F GLFW_KEY_F
  #define NANOGUI_KEY_G GLFW_KEY_G
  #define NANOGUI_KEY_M GLFW_KEY_M
  #define NANOGUI_KEY_O GLFW_KEY_O
  #define NANOGUI_KEY_R GLFW_KEY_R
  #define NANOGUI_KEY_S GLFW_KEY_S
  #define NANOGUI_KEY_V GLFW_KEY_V
  #define NANOGUI_KEY_X GLFW_KEY_X
  #define NANOGUI_KEY_Y GLFW_KEY_Y
  #define NANOGUI_KEY_Z GLFW_KEY_Z

  // Bracket keys
  #define NANOGUI_KEY_LEFTBRACKET GLFW_KEY_LEFT_BRACKET
  #define NANOGUI_KEY_RIGHTBRACKET GLFW_KEY_RIGHT_BRACKET

  // Punctuation keys
  #define NANOGUI_KEY_SEMICOLON GLFW_KEY_SEMICOLON

  // Key actions
  #define NANOGUI_KEY_PRESS GLFW_PRESS
  #define NANOGUI_KEY_RELEASE GLFW_RELEASE

  // Modifier keys
  #define NANOGUI_MOD_CTRL GLFW_MOD_CONTROL
  #define NANOGUI_MOD_SHIFT GLFW_MOD_SHIFT
  #define NANOGUI_MOD_ALT GLFW_MOD_ALT
  #define NANOGUI_MOD_SUPER GLFW_MOD_SUPER

#endif

// Utility macros for checking modifier combinations
#define NANOGUI_HAS_MOD(modifiers, mod) (((modifiers) & (mod)) != 0)
#define NANOGUI_ONLY_MOD(modifiers, mod) ((modifiers) == (mod))
#define NANOGUI_HAS_CTRL(modifiers) NANOGUI_HAS_MOD(modifiers, NANOGUI_MOD_CTRL)
#define NANOGUI_HAS_SHIFT(modifiers) NANOGUI_HAS_MOD(modifiers, NANOGUI_MOD_SHIFT)
#define NANOGUI_HAS_ALT(modifiers) NANOGUI_HAS_MOD(modifiers, NANOGUI_MOD_ALT)
#define NANOGUI_HAS_SUPER(modifiers) NANOGUI_HAS_MOD(modifiers, NANOGUI_MOD_SUPER)
