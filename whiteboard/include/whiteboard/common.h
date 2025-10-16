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

namespace whiteboard {
using namespace nanogui;
using json = nlohmann::json;
} // namespace whiteboard
