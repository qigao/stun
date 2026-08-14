/*
 * Meta Editor - Demo Application
 *
 * Demonstrates basic meta editor functionality.
 */

#include "full_editor_app.h"
#include <SDL2/SDL.h>
#include <flex/render/engines/thorvg.h>
#include <flex/bridge/renderer.h>
#include <iostream>
#include <thorvg.h>


// Use OpenGL for GPU-accelerated rendering (vs slow CPU software rendering)
#define USE_OPENGL_RENDERER 1

#if USE_OPENGL_RENDERER
  #include <SDL2/SDL_opengl.h>
  #include <SDL2/SDL_opengl_glext.h>

// GL extension function pointers (loaded via SDL)
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
static PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = nullptr;

static void loadGLFunctions() {
  glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers");
  glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteFramebuffers");
  glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
  glFramebufferTexture2D =
      (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D");
  glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBlitFramebuffer");
}

// Simple FBO wrapper for ThorVG GlCanvas
struct GLFrameBuffer {
  GLuint fbo = 0;
  GLuint texture = 0;
  int width, height;

  GLFrameBuffer(int w, int h) : width(w), height(h) {
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  ~GLFrameBuffer() {
    if (fbo)
      glDeleteFramebuffers(1, &fbo);
    if (texture)
      glDeleteTextures(1, &texture);
  }

  void resize(int w, int h) {
    width = w;
    height = h;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }

  void blitToScreen(int screenHeight) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    // No flip - make drawing upside down
    // Read entire FBO, write to screen without Y flipping
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, screenHeight, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
};
#endif

int main(int argc, char *argv[]) {
  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  // Create window
  const int WINDOW_WIDTH = 1200;
  const int WINDOW_HEIGHT = 800;

  std::cout << "Creating window " << WINDOW_WIDTH << "x" << WINDOW_HEIGHT << "..." << std::endl;

#if USE_OPENGL_RENDERER
  // OpenGL attributes for GPU rendering
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_Window *window = SDL_CreateWindow(
      "Meta Editor Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
      WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
#else
  SDL_Window *window =
      SDL_CreateWindow("Meta Editor Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#endif

  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }

#if USE_OPENGL_RENDERER
  // Create OpenGL context
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) {
    std::cerr << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1); // Enable VSync

  // Load GL extension functions
  loadGLFunctions();

  std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;
#else
  SDL_Surface *surface = SDL_GetWindowSurface(window);
  if (!surface) {
    std::cerr << "Failed to get window surface: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
#endif

  try {
    // Initialize flex engine (handles ThorVG init with proper thread count)
    std::cout << "Initializing Flex engine..." << std::endl;
    flex::render::engines::thorvg::init();

    // Load fonts (must be after flex::init which initializes ThorVG)
    flex::render::engines::thorvg::load_font("Arial", "C:/Windows/Fonts/arial.ttf");

#if USE_OPENGL_RENDERER
    // Create FBO for ThorVG to render into
    GLFrameBuffer glFbo(WINDOW_WIDTH, WINDOW_HEIGHT);

    // Create ThorVG GlCanvas - GPU accelerated
    std::unique_ptr<tvg::GlCanvas> tvg_canvas(tvg::GlCanvas::gen());
    if (!tvg_canvas) {
      throw std::runtime_error("Failed to create GlCanvas - OpenGL engine not supported");
    }
    // target(context, framebuffer_id, width, height, colorspace)
    if (tvg_canvas->target(gl_context, glFbo.fbo, WINDOW_WIDTH, WINDOW_HEIGHT,
                           tvg::ColorSpace::ABGR8888S) != tvg::Result::Success) {
      throw std::runtime_error("Failed to set GlCanvas target");
    }
#else
    // Create ThorVG SwCanvas - CPU software rendering (slow!)
    std::unique_ptr<tvg::SwCanvas> tvg_canvas(tvg::SwCanvas::gen());
    tvg_canvas->target(reinterpret_cast<uint32_t *>(surface->pixels), surface->w,
                       surface->pitch / 4, surface->h, tvg::ColorSpace::ARGB8888);
#endif

    // Create flex renderer
    auto renderer = flex::render::engines::thorvg::create_renderer(tvg_canvas.get());
    if (!renderer) {
      throw std::runtime_error("Failed to create flex renderer");
    }

    // Create full editor app (with all built-in panels)
    std::cout << "Initializing Full Editor App..." << std::endl;
    meta_editor::FullEditorApp editor((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
    editor.init(renderer.get());

    std::cout << "Full Editor Demo started!" << std::endl;

    // Event loop state
    bool running = true;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
      // Handle events
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          running = false;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
          // Handle window resize
          int new_width = event.window.data1;
          int new_height = event.window.data2;

#if USE_OPENGL_RENDERER
          // Update OpenGL viewport, FBO, and canvas target
          glViewport(0, 0, new_width, new_height);
          glFbo.resize(new_width, new_height);
          tvg_canvas->target(gl_context, glFbo.fbo, new_width, new_height,
                             tvg::ColorSpace::ABGR8888S);
          editor.set_viewport((float)new_width, (float)new_height);
#else
          // Update software surface
          surface = SDL_GetWindowSurface(window);
          if (surface) {
            tvg_canvas->target(reinterpret_cast<uint32_t *>(surface->pixels), surface->w,
                               surface->pitch / 4, surface->h, tvg::ColorSpace::ARGB8888);
            editor.set_viewport((float)new_width, (float)new_height);
          }
#endif
        } else {
          // Let editor handle event
          // If editor handles it, it won't propagate (e.g. to tool-specific logic)
          editor.handle_event(event);
        }
      }

      // Update & Render Flow
      // 1. Calculate delta time
      Uint32 current_time = SDL_GetTicks();
      float dt = (current_time - last_time) / 1000.0f;
      last_time = current_time;

      // 2. Update editor
      // This now includes UI update, layout and rendering because of WorkspaceWidget
      editor.update(dt);

      // 3. Update window surface
#if USE_OPENGL_RENDERER
      // Set viewport for screen and blit FBO
      int w, h;
      SDL_GetWindowSize(window, &w, &h);
      glViewport(0, 0, w, h);
      glFbo.blitToScreen(h);
      SDL_GL_SwapWindow(window);
#else
      // tvg_canvas renders directly to SDL surface
      SDL_UpdateWindowSurface(window);
#endif

      // Cap frame rate
      SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    std::cout << "Shutting down..." << std::endl;
    editor.shutdown();
    flex::render::engines::thorvg::shutdown(); // Also terminates ThorVG

  } catch (const std::exception &e) {
    std::cerr << "FATAL ERROR: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "FATAL ERROR: Unknown exception" << std::endl;
  }

#if USE_OPENGL_RENDERER
  SDL_GL_DeleteContext(gl_context);
#endif
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
