/*
    nanogui/screen.h -- Top-level widget and interface between NanoGUI and windowing backend

    This header automatically includes the appropriate backend implementation
    based on build configuration (GLFW or SDL2).

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/
/** \file */

#pragma once

// Include the appropriate backend implementation
#if defined(NANOGUI_BUILD_GLFW)
    #include <nanogui/screen_glfw.h>
#elif defined(NANOGUI_USE_SDL3)
    #include <nanogui/screen_sdl.h>
#elif defined(NANOGUI_USE_SDL2)
    #include <nanogui/screen_sdl.h>
#else
    #error "No windowing backend selected. Enable NANOGUI_BUILD_GLFW, NANOGUI_USE_SDL3, or NANOGUI_USE_SDL2"
#endif
