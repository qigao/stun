/*
 * flexUI - RenderFrame
 *
 * A backend-neutral description of one rendered frame.
 */

#ifndef FLEXUI_RENDER_FRAME_H
#define FLEXUI_RENDER_FRAME_H

#include "types.h"

namespace flexUI {

class Element;

struct RenderViewport {
  float width = 0.0f;
  float height = 0.0f;
  float pixel_ratio = 1.0f;
};

struct RenderFrame {
  Element* root = nullptr;
  RenderViewport viewport{};
  Color clear_color{0.12f, 0.12f, 0.12f, 1.0f};
};

} // namespace flexUI

#endif // FLEXUI_RENDER_FRAME_H
