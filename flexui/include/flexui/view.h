#pragma once

#include "flexui/document.h"

#include <SDL3/SDL.h>
#include <nanovg.h>

#include <functional>
#include <string>

struct NVGCSSRenderer;

namespace flexui {

struct FlexViewConfig {
  FlexViewConfig();

  std::string window_title;
  int width;
  int height;
  bool resizable;
  bool vsync;
  int nvg_flags;
  float clear_color_r;
  float clear_color_g;
  float clear_color_b;
  float clear_color_a;
};

using OverlayDrawCallback =
    std::function<void(NVGcontext *vg, int width, int height)>;

class FlexView {
public:
  FlexView() = default;
  ~FlexView();

  bool initialize(const FlexViewConfig &config);
  void shutdown();

  void render(FlexDocument &document, float dt, const OverlayDrawCallback &overlay);

  NVGcontext *nvgContext() const { return m_vg; }
  NVGCSSRenderer *renderer() const { return m_renderer; }
  SDL_Window *window() const { return m_window; }

private:
  FlexViewConfig m_config;
  SDL_Window *m_window = nullptr;
  SDL_GLContext m_gl_context = nullptr;
  NVGcontext *m_vg = nullptr;
  NVGCSSRenderer *m_renderer = nullptr;
  bool m_initialized = false;
  bool m_owns_sdl = false;
};

} // namespace flexui
