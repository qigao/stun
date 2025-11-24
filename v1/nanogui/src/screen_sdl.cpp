/*
    src/screen_sdl.cpp -- Top-level widget and interface between NanoGUI and SDL3

    Based on the working SDL3 implementation in examples/sdl/example3.cpp

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#if defined(NANOGUI_USE_SDL3)

  #include <nanogui/opengl.h>
  #include <nanogui/popup.h>
  #include <nanogui/rtimer.h>
  #include <nanogui/screen.h>
  #include <nanogui/shader.h>
  #include <nanogui/theme.h>
  #include <nanogui/window.h>

  #include <SDL3/SDL.h>
  #include <algorithm>
  #include <cstdlib>
  #include <iostream>

  #if defined(NANOGUI_USE_OPENGL) || defined(NANOGUI_USE_GLES)
    #include "opengl_check.h"
    // Include NanoVG declarations (implementation is in screen.cpp)
    #if defined(NANOGUI_USE_OPENGL)
      #define NANOVG_GL3
    #elif defined(NANOGUI_USE_GLES)
      #define NANOVG_GLES2
    #endif
    #include <nanovg_gl.h>
  #elif defined(NANOGUI_USE_METAL)
    #include <nanovg_mtl.h>
  #endif

NAMESPACE_BEGIN(nanogui)

  /// Delay in seconds before showing tooltips
  #define TOOLTIP_DELAY_SEC 0.2f

std::vector<std::pair<SDL_Window *, Screen *>> __nanogui_sdl_screens;

  #if defined(NANOGUI_GLAD)
static bool glad_initialized = false;
  #endif

/* Calculate pixel ratio for hi-dpi devices. */
static float get_pixel_ratio(SDL_Window *window) {
  int window_w, window_h;
  int drawable_w, drawable_h;
  SDL_GetWindowSize(window, &window_w, &window_h);
  SDL_GetWindowSizeInPixels(window, &drawable_w, &drawable_h);
  return (float)drawable_w / (float)window_w;
}

// SDL3 → NanoGUI mouse button mapping
static int sdl_button_to_nanogui(Uint8 button) {
  switch (button) {
  case SDL_BUTTON_LEFT:
    return NANOGUI_MOUSE_BUTTON_LEFT;
  case SDL_BUTTON_RIGHT:
    return NANOGUI_MOUSE_BUTTON_RIGHT;
  case SDL_BUTTON_MIDDLE:
    return NANOGUI_MOUSE_BUTTON_MIDDLE;
  default:
    return button;
  }
}

// SDL3 → NanoGUI modifier mapping
static int sdl_mods_to_nanogui(SDL_Keymod mods) {
  int result = 0;
  if (mods & SDL_KMOD_SHIFT)
    result |= GLFW_MOD_SHIFT;
  if (mods & SDL_KMOD_CTRL)
    result |= GLFW_MOD_CONTROL;
  if (mods & SDL_KMOD_ALT)
    result |= GLFW_MOD_ALT;
  if (mods & SDL_KMOD_GUI)
    result |= GLFW_MOD_SUPER;
  return result;
}

// SDL3 → GLFW-compatible key code mapping (partial - extend as needed)
static int sdl_key_to_glfw(SDL_Keycode key) {
  // Direct ASCII mappings
  if (key >= SDLK_SPACE && key <= SDLK_AT)
    return key;
  if (key >= SDLK_A && key <= SDLK_Z)
    return key;

  // Special keys
  switch (key) {
  case SDLK_ESCAPE:
    return GLFW_KEY_ESCAPE;
  case SDLK_RETURN:
    return GLFW_KEY_ENTER;
  case SDLK_TAB:
    return GLFW_KEY_TAB;
  case SDLK_BACKSPACE:
    return GLFW_KEY_BACKSPACE;
  case SDLK_INSERT:
    return GLFW_KEY_INSERT;
  case SDLK_DELETE:
    return GLFW_KEY_DELETE;
  case SDLK_RIGHT:
    return GLFW_KEY_RIGHT;
  case SDLK_LEFT:
    return GLFW_KEY_LEFT;
  case SDLK_DOWN:
    return GLFW_KEY_DOWN;
  case SDLK_UP:
    return GLFW_KEY_UP;
  case SDLK_PAGEUP:
    return GLFW_KEY_PAGE_UP;
  case SDLK_PAGEDOWN:
    return GLFW_KEY_PAGE_DOWN;
  case SDLK_HOME:
    return GLFW_KEY_HOME;
  case SDLK_END:
    return GLFW_KEY_END;
  case SDLK_CAPSLOCK:
    return GLFW_KEY_CAPS_LOCK;
  case SDLK_SCROLLLOCK:
    return GLFW_KEY_SCROLL_LOCK;
  case SDLK_NUMLOCKCLEAR:
    return GLFW_KEY_NUM_LOCK;
  case SDLK_PRINTSCREEN:
    return GLFW_KEY_PRINT_SCREEN;
  case SDLK_PAUSE:
    return GLFW_KEY_PAUSE;
  default:
    return GLFW_KEY_UNKNOWN;
  }
}

Screen::Screen()
    : Widget(nullptr), m_sdl_window(nullptr), m_sdl_context(nullptr), m_nvg_context(nullptr),
      m_cursor(Cursor::Arrow), m_background(0.3f, 0.3f, 0.32f, 1.f), m_fullscreen(false),
      m_depth_buffer(false), m_stencil_buffer(false), m_float_buffer(false), m_redraw(false),
      m_running(true), m_last_run_mode(RunMode::Stopped), m_frame_index(0) {
  memset(m_cursors, 0, sizeof(SDL_Cursor *) * (size_t)Cursor::CursorCount);
}

Screen::Screen(const Vector2i &size, std::string_view caption, bool resizable, bool maximized,
               bool fullscreen, bool depth_buffer, bool stencil_buffer, bool float_buffer,
               unsigned int gl_major, unsigned int gl_minor)
    : Widget(nullptr), m_sdl_window(nullptr), m_sdl_context(nullptr), m_nvg_context(nullptr),
      m_cursor(Cursor::Arrow), m_background(0.3f, 0.3f, 0.32f, 1.f), m_caption(caption),
      m_fullscreen(fullscreen), m_depth_buffer(depth_buffer), m_stencil_buffer(stencil_buffer),
      m_float_buffer(float_buffer), m_redraw(false), m_running(true),
      m_last_run_mode(RunMode::Stopped), m_frame_index(0) {
  memset(m_cursors, 0, sizeof(SDL_Cursor *) * (size_t)Cursor::CursorCount);

  // Initialize SDL with video and camera subsystems
  // SDL_INIT_CAMERA is required for camera API to work
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA)) {
    throw std::runtime_error(std::string("Failed to initialize SDL: ") + SDL_GetError());
  }

  // Set OpenGL attributes (based on example3.cpp)
  if (stencil_buffer) {
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  }

  if (depth_buffer) {
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, stencil_buffer ? 24 : 32);
  }

  #if defined(NANOGUI_USE_OPENGL)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_minor);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  #elif defined(NANOGUI_USE_GLES)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, NANOGUI_GLES_VERSION);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  #endif

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // Try to enable MSAA
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);

  // Create window
  Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (resizable)
    flags |= SDL_WINDOW_RESIZABLE;
  if (maximized)
    flags |= SDL_WINDOW_MAXIMIZED;
  if (fullscreen)
    flags |= SDL_WINDOW_FULLSCREEN;

  m_sdl_window = SDL_CreateWindow(m_caption.c_str(), size.x(), size.y(), flags);

  if (!m_sdl_window) {
    // Try without MSAA
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    m_sdl_window = SDL_CreateWindow(m_caption.c_str(), size.x(), size.y(), flags);

    if (!m_sdl_window) {
      SDL_Quit();
      throw std::runtime_error(std::string("Failed to create SDL window: ") + SDL_GetError());
    }
  }

  // Create OpenGL context
  m_sdl_context = SDL_GL_CreateContext(m_sdl_window);
  if (!m_sdl_context) {
    SDL_DestroyWindow(m_sdl_window);
    SDL_Quit();
    throw std::runtime_error(std::string("Failed to create OpenGL context: ") + SDL_GetError());
  }

  if (!SDL_GL_MakeCurrent(m_sdl_window, m_sdl_context)) {
    SDL_GL_DestroyContext(m_sdl_context);
    SDL_DestroyWindow(m_sdl_window);
    SDL_Quit();
    throw std::runtime_error(std::string("Failed to make OpenGL context current: ") +
                             SDL_GetError());
  }

  #if defined(NANOGUI_GLAD)
  if (!glad_initialized) {
    glad_initialized = true;
    if (!gladLoadGL()) {
      SDL_GL_DestroyContext(m_sdl_context);
      SDL_DestroyWindow(m_sdl_window);
      SDL_Quit();
      throw std::runtime_error("Could not initialize GLAD!");
    }
    glGetError(); // pull and ignore unhandled errors
  }
  #endif

  // Initialize NanoVG
  #if defined(NANOGUI_USE_OPENGL)
  m_nvg_context = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  #elif defined(NANOGUI_USE_GLES)
  m_nvg_context = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  #endif

  if (!m_nvg_context) {
    SDL_GL_DestroyContext(m_sdl_context);
    SDL_DestroyWindow(m_sdl_window);
    SDL_Quit();
    throw std::runtime_error("Could not initialize NanoVG!");
  }

  // Get actual framebuffer size and pixel ratio
  SDL_GetWindowSize(m_sdl_window, &m_size.x(), &m_size.y());
  SDL_GetWindowSizeInPixels(m_sdl_window, &m_fbsize.x(), &m_fbsize.y());
  m_pixel_ratio = get_pixel_ratio(m_sdl_window);

  // Set NanoVG pixel ratio
  nvgSetDevicePixelRatio(m_nvg_context, m_pixel_ratio);

  // Create standard cursors
  m_cursors[(size_t)Cursor::Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
  m_cursors[(size_t)Cursor::IBeam] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
  m_cursors[(size_t)Cursor::Crosshair] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
  m_cursors[(size_t)Cursor::Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
  m_cursors[(size_t)Cursor::HResize] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
  m_cursors[(size_t)Cursor::VResize] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);

  // Register this screen
  __nanogui_sdl_screens.push_back(std::make_pair(m_sdl_window, this));

  // Initialize state
  m_visible = true;
  m_mouse_pos = Vector2i(0);
  m_mouse_pos_f = Vector2f(0.f);
  m_mouse_state = 0;
  m_modifiers = 0;
  m_drag_active = false;
  m_drag_widget = nullptr;
  m_last_interaction = get_time();
  m_last_draw = get_time();
  m_bits_per_sample = 8; // TODO: Query actual value

  // Create default theme
  set_theme(new Theme(m_nvg_context));

  // Enable text input for the window
  SDL_StartTextInput(m_sdl_window);

  // Enable VSync by default
  SDL_GL_SetSwapInterval(1);
}

Screen::~Screen() {
  // Remove from screen list first
  auto it =
      std::find_if(__nanogui_sdl_screens.begin(), __nanogui_sdl_screens.end(),
                   [this](const std::pair<SDL_Window *, Screen *> &p) { return p.second == this; });
  if (it != __nanogui_sdl_screens.end()) {
    __nanogui_sdl_screens.erase(it);
  }

  // Clear focus path to avoid dangling pointers
  m_focus_path.clear();
  m_drag_widget = nullptr;

  // Make context current before cleanup
  if (m_sdl_window && m_sdl_context) {
    SDL_GL_MakeCurrent(m_sdl_window, m_sdl_context);
  }

  // CRITICAL: Release all OpenGL resources BEFORE destroying context
  #if !defined(NANOGUI_USE_METAL)
  m_color_texture = nullptr;
  m_color_pass = nullptr;
  #endif
  m_depth_stencil_texture = nullptr;
  m_tooltip_timer = nullptr;

  // Destroy all child widgets BEFORE destroying NanoVG context
  for (auto child : m_children) {
    if (child)
      child->dec_ref();
  }
  m_children.clear();

  // Cleanup NanoVG (after all widgets and textures are destroyed)
  if (m_nvg_context) {
  #if defined(NANOGUI_USE_OPENGL)
    nvgDeleteGL3(m_nvg_context);
  #elif defined(NANOGUI_USE_GLES)
    nvgDeleteGLES2(m_nvg_context);
  #endif
    m_nvg_context = nullptr;
  }

  // Destroy cursors
  for (size_t i = 0; i < (size_t)Cursor::CursorCount; ++i) {
    if (m_cursors[i]) {
      SDL_DestroyCursor(m_cursors[i]);
      m_cursors[i] = nullptr;
    }
  }

  // Cleanup SDL context
  if (m_sdl_context) {
    SDL_GL_DestroyContext(m_sdl_context);
    m_sdl_context = nullptr;
  }

  // Cleanup SDL window
  if (m_sdl_window) {
    SDL_DestroyWindow(m_sdl_window);
    m_sdl_window = nullptr;
  }

  // Note: SDL_Quit() is now called in nanogui::shutdown()
}

void Screen::initialize(SDL_Window *window, SDL_GLContext context) {
  m_sdl_window = window;
  m_sdl_context = context;

  // Make context current
  SDL_GL_MakeCurrent(m_sdl_window, m_sdl_context);

  // Initialize NanoVG
  #if defined(NANOGUI_USE_OPENGL)
  m_nvg_context = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  #elif defined(NANOGUI_USE_GLES)
  m_nvg_context = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  #endif

  if (!m_nvg_context) {
    throw std::runtime_error("Could not initialize NanoVG!");
  }

  // Get framebuffer size and pixel ratio
  SDL_GetWindowSize(m_sdl_window, &m_size.x(), &m_size.y());
  SDL_GetWindowSizeInPixels(m_sdl_window, &m_fbsize.x(), &m_fbsize.y());
  m_pixel_ratio = get_pixel_ratio(m_sdl_window);

  // Set NanoVG pixel ratio
  nvgSetDevicePixelRatio(m_nvg_context, m_pixel_ratio);

  // Create cursors
  m_cursors[(size_t)Cursor::Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
  m_cursors[(size_t)Cursor::IBeam] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
  m_cursors[(size_t)Cursor::Crosshair] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
  m_cursors[(size_t)Cursor::Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
  m_cursors[(size_t)Cursor::HResize] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
  m_cursors[(size_t)Cursor::VResize] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);

  // Register screen
  __nanogui_sdl_screens.push_back(std::make_pair(m_sdl_window, this));

  // Initialize state
  m_visible = true;
  m_mouse_pos = Vector2i(0);
  m_mouse_pos_f = Vector2f(0.f);
  m_mouse_state = 0;
  m_modifiers = 0;
  m_drag_active = false;
  m_drag_widget = nullptr;
  m_last_interaction = get_time();
  m_last_draw = 0.0;
  m_bits_per_sample = 8;

  // Create default theme
  set_theme(new Theme(m_nvg_context));

  // Enable text input for the window
  SDL_StartTextInput(m_sdl_window);

  m_running = true;
}

bool Screen::process_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    handle_sdl_event(event);

    if (event.type == SDL_EVENT_QUIT) {
      m_running = false;
      return false;
    }
  }

  return m_running;
}

void Screen::handle_sdl_event(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_QUIT:
    m_running = false;
    break;

  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP: {
    bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
    bool repeat = event.key.repeat;
    handle_key_event(event.key.key, event.key.scancode, pressed, repeat);
    break;
  }

  case SDL_EVENT_TEXT_INPUT:
    handle_text_input(event.text.text);
    break;

  case SDL_EVENT_MOUSE_MOTION:
    handle_mouse_motion((float)event.motion.x, (float)event.motion.y);
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    bool pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
    handle_mouse_button(event.button.button, pressed, (float)event.button.x, (float)event.button.y);
    break;
  }

  case SDL_EVENT_MOUSE_WHEEL:
    handle_mouse_wheel((float)event.wheel.x, (float)event.wheel.y);
    break;

  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
  case SDL_EVENT_WINDOW_MAXIMIZED:
  case SDL_EVENT_WINDOW_RESTORED:
    handle_window_event(event.window);
    break;

  case SDL_EVENT_DROP_FILE: {
    if (event.drop.data) {
      std::vector<std::string> files;
      files.push_back(event.drop.data);
      drop_event(files);
      // Note: SDL3 manages the memory for drop.data, don't free it
    }
    break;
  }

  default:
    break;
  }
}

void Screen::handle_key_event(SDL_Keycode key, SDL_Scancode scancode, bool pressed, bool repeat) {
  int glfw_key = sdl_key_to_glfw(key);
  int action = pressed ? (repeat ? GLFW_REPEAT : GLFW_PRESS) : GLFW_RELEASE;
  m_modifiers = sdl_mods_to_nanogui(SDL_GetModState());

  keyboard_event(glfw_key, (int)scancode, action, m_modifiers);
}

void Screen::handle_text_input(const char *text) {
  // Convert UTF-8 to UTF-32 codepoints
  if (!text || !text[0])
    return;

  // Decode UTF-8 string to UTF-32 codepoints
  const unsigned char *utf8 = (const unsigned char *)text;
  size_t i = 0;

  while (utf8[i]) {
    unsigned int codepoint = 0;

    if ((utf8[i] & 0x80) == 0) {
      // 1-byte character (ASCII)
      codepoint = utf8[i];
      i += 1;
    } else if ((utf8[i] & 0xE0) == 0xC0) {
      // 2-byte character
      if (utf8[i + 1]) {
        codepoint = ((utf8[i] & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
        i += 2;
      } else
        break;
    } else if ((utf8[i] & 0xF0) == 0xE0) {
      // 3-byte character
      if (utf8[i + 1] && utf8[i + 2]) {
        codepoint = ((utf8[i] & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
        i += 3;
      } else
        break;
    } else if ((utf8[i] & 0xF8) == 0xF0) {
      // 4-byte character
      if (utf8[i + 1] && utf8[i + 2] && utf8[i + 3]) {
        codepoint = ((utf8[i] & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) |
                    ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
        i += 4;
      } else
        break;
    } else {
      // Invalid UTF-8 sequence, skip byte
      i += 1;
      continue;
    }

    keyboard_character_event(codepoint);
  }
}

void Screen::handle_mouse_motion(float x, float y) {
  Vector2i new_pos((int)x, (int)y);
  Vector2f new_pos_f(x, y);
  Vector2i rel = new_pos - m_mouse_pos;
  Vector2f rel_f = new_pos_f - m_mouse_pos_f;

  m_mouse_pos = new_pos;
  m_mouse_pos_f = new_pos_f;

  m_last_interaction = get_time();

  if (!m_drag_active) {
    // Update cursor based on widget under mouse
    Widget *widget = find_widget(m_mouse_pos);
    while (widget && widget->cursor() == Cursor::Arrow)
      widget = widget->parent();

    m_cursor = widget ? widget->cursor() : Cursor::Arrow;
    if (m_cursors[(size_t)m_cursor]) {
      SDL_SetCursor(m_cursors[(size_t)m_cursor]);
    }

    mouse_motion_event(m_mouse_pos, rel, m_mouse_state, m_modifiers);
    mouse_motion_event_f(m_mouse_pos_f, rel_f, m_mouse_state, m_modifiers);
  } else {
    if (m_drag_widget) {
      m_drag_widget->mouse_drag_event(m_mouse_pos - m_drag_widget->parent()->absolute_position(),
                                      rel, m_mouse_state, m_modifiers);
    }
  }
}

void Screen::handle_mouse_button(Uint8 button, bool pressed, float x, float y) {
  m_mouse_pos = Vector2i((int)x, (int)y);
  m_mouse_pos_f = Vector2f(x, y);

  int nanogui_button = sdl_button_to_nanogui(button);
  m_modifiers = sdl_mods_to_nanogui(SDL_GetModState());

  m_last_interaction = get_time();

  if (pressed) {
    m_mouse_state |= (1 << nanogui_button);
  } else {
    m_mouse_state &= ~(1 << nanogui_button);
  }

  // Handle drag start/stop for left and right mouse buttons
  bool btn12 =
      (nanogui_button == NANOGUI_MOUSE_BUTTON_LEFT || nanogui_button == NANOGUI_MOUSE_BUTTON_RIGHT);

  // Handle drop event if dragging
  auto drop_widget = find_widget(m_mouse_pos);
  if (m_drag_active && !pressed && drop_widget != m_drag_widget && m_drag_widget) {
    m_drag_widget->mouse_button_event(m_mouse_pos - m_drag_widget->parent()->absolute_position(),
                                      nanogui_button, false, m_modifiers);
  }

  // Start drag on press
  if (!m_drag_active && pressed && btn12) {
    m_drag_widget = find_widget(m_mouse_pos);
    if (m_drag_widget == this)
      m_drag_widget = nullptr;
    m_drag_active = m_drag_widget != nullptr;

    if (!m_drag_active)
      update_focus(nullptr);
  } else if (m_drag_active && !pressed && btn12) {
    // Stop drag on release
    m_drag_active = false;
    m_drag_widget = nullptr;
  }

  mouse_button_event(m_mouse_pos, nanogui_button, pressed, m_modifiers);
}

void Screen::handle_mouse_wheel(float x, float y) { scroll_event(m_mouse_pos, Vector2f(x, y)); }

void Screen::handle_window_event(const SDL_WindowEvent &event) {
  switch (event.type) {
  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
    SDL_GetWindowSize(m_sdl_window, &m_size.x(), &m_size.y());
    SDL_GetWindowSizeInPixels(m_sdl_window, &m_fbsize.x(), &m_fbsize.y());
    m_pixel_ratio = get_pixel_ratio(m_sdl_window);
    resize_event(m_size);
    if (m_resize_callback) {
      m_resize_callback(m_size);
    }
    break;
  }

  case SDL_EVENT_WINDOW_MAXIMIZED:
    maximize_event(true);
    break;

  case SDL_EVENT_WINDOW_RESTORED:
    maximize_event(false);
    break;

  default:
    break;
  }
}

void Screen::draw_all() {
  // Update frame timing
  double current_time = get_time();
  if (m_last_draw != 0.0)
    m_frame_timer.put(current_time - m_last_draw);
  m_last_draw = current_time;
  m_frame_index++;

  draw_setup();
  draw_contents();
  draw_widgets(); // This calls nvgBeginFrame/draw/nvgEndFrame (from shared section)
  draw_teardown();
}

void Screen::draw_setup() {
  // Update framebuffer size
  SDL_GetWindowSize(m_sdl_window, &m_size.x(), &m_size.y());
  SDL_GetWindowSizeInPixels(m_sdl_window, &m_fbsize.x(), &m_fbsize.y());
  m_pixel_ratio = get_pixel_ratio(m_sdl_window);

  // Set viewport
  glViewport(0, 0, m_fbsize.x(), m_fbsize.y());
}

// Note: draw_contents() is implemented in screen.cpp (shared section)

void Screen::clear() {
  glClearColor(m_background.r(), m_background.g(), m_background.b(), m_background.a());
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void Screen::draw_teardown() {
  // Swap buffers
  SDL_GL_SwapWindow(m_sdl_window);
}

void Screen::set_visible(bool visible) {
  if (visible) {
    SDL_ShowWindow(m_sdl_window);
  } else {
    SDL_HideWindow(m_sdl_window);
  }
}

void Screen::set_caption(std::string_view caption) {
  m_caption = caption;
  SDL_SetWindowTitle(m_sdl_window, m_caption.c_str());
}

void Screen::set_size(const Vector2i &size) { SDL_SetWindowSize(m_sdl_window, size.x(), size.y()); }

void Screen::move_window(const Vector2i &rel) {
  int x, y;
  SDL_GetWindowPosition(m_sdl_window, &x, &y);
  SDL_SetWindowPosition(m_sdl_window, x + rel.x(), y + rel.y());
}

NAMESPACE_END(nanogui)

#endif // NANOGUI_USE_SDL3
