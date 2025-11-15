#include "flexui/view.h"

#include <fmtlog.h>
#include <glad/glad.h>
#include <nanovg_css.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

namespace flexui {

FlexViewConfig::FlexViewConfig()
    : window_title("FlexUI Demo"), width(1280), height(720), resizable(true),
      vsync(true), nvg_flags(NVG_ANTIALIAS | NVG_STENCIL_STROKES),
      clear_color_r(0.95f), clear_color_g(0.96f), clear_color_b(0.98f),
      clear_color_a(1.0f) {}

FlexView::~FlexView() { shutdown(); }

bool FlexView::initialize(const FlexViewConfig &config) {
  if (m_initialized) {
    return true;
  }

  m_config = config;

  if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
      loge("FlexView: SDL_Init failed: {}", SDL_GetError());
      shutdown();
      return false;
    }
    m_owns_sdl = true;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  const Uint32 window_flags =
      SDL_WINDOW_OPENGL | (config.resizable ? SDL_WINDOW_RESIZABLE : 0);

  m_window = SDL_CreateWindow(config.window_title.c_str(), config.width,
                              config.height, window_flags);
  if (!m_window) {
    loge("FlexView: SDL_CreateWindow failed: {}", SDL_GetError());
    shutdown();
    return false;
  }

  m_gl_context = SDL_GL_CreateContext(m_window);
  if (!m_gl_context) {
    loge("FlexView: SDL_GL_CreateContext failed: {}", SDL_GetError());
    shutdown();
    return false;
  }

  if (!gladLoadGL()) {
    loge("FlexView: gladLoadGL failed");
    shutdown();
    return false;
  }

  SDL_GL_SetSwapInterval(config.vsync ? 1 : 0);

  m_vg = nvgCreateGL3(config.nvg_flags);
  if (!m_vg) {
    loge("FlexView: failed to create NanoVG context");
    shutdown();
    return false;
  }

  m_renderer = nvgcssCreateRenderer(m_vg);
  if (!m_renderer) {
    loge("FlexView: failed to create NVGCSS renderer");
    shutdown();
    return false;
  }

  m_initialized = true;
  return true;
}

void FlexView::shutdown() {
  if (m_renderer) {
    nvgcssDeleteRenderer(m_renderer);
    m_renderer = nullptr;
  }

  if (m_vg) {
    nvgDeleteGL3(m_vg);
    m_vg = nullptr;
  }

  if (m_gl_context) {
    SDL_GL_DestroyContext(m_gl_context);
    m_gl_context = nullptr;
  }

  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }

  if (m_owns_sdl) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    m_owns_sdl = false;
  }

  m_initialized = false;
}

void FlexView::render(FlexDocument &document,
                      const OverlayDrawCallback &overlay) {
  if (!m_initialized || !m_renderer || !m_window) {
    return;
  }

  if (document.renderer() != m_renderer) {
    document.setRenderer(m_renderer);
  }

  int win_width = 0;
  int win_height = 0;
  SDL_GetWindowSize(m_window, &win_width, &win_height);

  int fb_width = 0;
  int fb_height = 0;
  SDL_GetWindowSizeInPixels(m_window, &fb_width, &fb_height);

  const float pixel_ratio =
      win_width > 0 ? static_cast<float>(fb_width) / win_width : 1.0f;

  glViewport(0, 0, fb_width, fb_height);
  glClearColor(m_config.clear_color_r, m_config.clear_color_g,
               m_config.clear_color_b, m_config.clear_color_a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  nvgBeginFrame(m_vg, win_width, win_height, pixel_ratio);
  nvgcssSetViewport(m_renderer, static_cast<float>(win_width),
                    static_cast<float>(win_height));
  nvgcssRender(m_renderer);

  if (overlay) {
    overlay(m_vg, win_width, win_height);
  }

  nvgEndFrame(m_vg);
  SDL_GL_SwapWindow(m_window);
}

} // namespace flexui
