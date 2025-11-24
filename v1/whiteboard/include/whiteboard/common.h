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

// Font Awesome icon codes (subset for context menu)
#define FA_CUT 0xf0c4
#define FA_COPY 0xf0c5
#define FA_PASTE 0xf0ea
#define FA_TRASH 0xf1f8
#define FA_CLONE 0xf24d
#define FA_OBJECT_GROUP 0xf247
#define FA_OBJECT_UNGROUP 0xf248
#define FA_ARROW_UP 0xf062
#define FA_ARROW_DOWN 0xf063
#define FA_ANGLE_UP 0xf106
#define FA_ANGLE_DOWN 0xf107
#define FA_LOCK 0xf023
#define FA_UNLOCK 0xf09c
#define FA_EYE 0xf06e
#define FA_EYE_SLASH 0xf070

#include "clipboard_manager.h"
#include "export_manager.h"
#include "image_io.h"
#include "types.h"

namespace whiteboard {
using namespace nanogui;
using json = nlohmann::json;
} // namespace whiteboard
