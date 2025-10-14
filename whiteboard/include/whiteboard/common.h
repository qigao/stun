#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stack>
#include <string>
#include <vector>

#include <nanogui.h>
#include <nanovg.h>
#include <nfd.h>

#include <nlohmann/json.hpp>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

#include "clipboard_manager.h"
#include "export_manager.h"
#include "image_io.h"
#include "types.h"

// Key definitions are now provided by <nanogui/keys.h> (included via <nanogui.h>)
// Use NANOGUI_KEY_* for key codes (e.g., NANOGUI_KEY_ESCAPE, NANOGUI_KEY_C)
// Use NANOGUI_MOD_* for modifiers (e.g., NANOGUI_MOD_CTRL, NANOGUI_MOD_SHIFT)
// Use NANOGUI_HAS_CTRL(modifiers) to check if Ctrl is pressed
// Example: if (key == NANOGUI_KEY_C && NANOGUI_HAS_CTRL(modifiers)) { /* Ctrl+C */ }

namespace whiteboard {
using namespace nanogui;
using json = nlohmann::json;
} // namespace whiteboard
