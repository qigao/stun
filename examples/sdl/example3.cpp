/*
    examples/sdl/example3.cpp -- SDL3 + NanoVG example application.
    Demonstrates basic SDL3 window creation with OpenGL context and NanoVG rendering.

    NanoVG was developed by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <iostream>

#define NANOVG_GL2_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_gl.h"
#include "nanovg_gl_utils.h"

class SDLNanoVGApp final {
public:
  explicit SDLNanoVGApp(int width = 1024, int height = 800)
      : m_width(width), m_height(height), m_running(true) {
    if (!initialize()) {
      throw std::runtime_error("Failed to initialize SDL/OpenGL/NanoVG");
    }
  }

  ~SDLNanoVGApp() { cleanup(); }

  void run() {
    Uint64 last_time = SDL_GetTicks();
    while (m_running) {
      Uint64 current_time = SDL_GetTicks();
      float delta = (current_time - last_time) / 1000.0f;
      last_time = current_time;
      m_time += delta;

      process_events();
      render();
      SDL_GL_SwapWindow(m_window);
    }
  }

private:
  SDL_Window *m_window = nullptr;
  SDL_GLContext m_context = nullptr;
  NVGcontext *m_vg = nullptr;
  int m_width;
  int m_height;
  bool m_running;
  float m_time = 0.0f;
  int m_mouse_x = 0;
  int m_mouse_y = 0;

  // Text input state
  std::string m_text_input1 = "Text input";
  std::string m_text_input2 = "50";
  std::string m_text_input3 = "user@example.com";
  int m_active_textbox = -1; // -1 = none, 0-2 = textbox index
  bool m_show_cursor = true;
  float m_cursor_blink_time = 0.0f;

  // Popup menu state
  bool m_show_popup = false;
  int m_popup_x = 0;
  int m_popup_y = 0;
  int m_popup_hover_item = -1;
  
  // Window widget state
  bool m_show_window = true;
  float m_window_x = 700.0f;
  float m_window_y = 100.0f;
  float m_window_width = 300.0f;
  float m_window_height = 400.0f;
  bool m_window_dragging = false;
  float m_drag_offset_x = 0.0f;
  float m_drag_offset_y = 0.0f;

  bool initialize() {
    std::cout << "Initializing SDL..." << std::endl;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
      std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
      return false;
    }
    std::cout << "SDL initialized successfully" << std::endl;

    if (!setup_gl_attributes()) {
      std::cerr << "Failed to setup GL attributes" << std::endl;
      return false;
    }
    std::cout << "GL attributes set" << std::endl;

    if (!create_window()) {
      std::cerr << "Failed to create window" << std::endl;
      return false;
    }
    std::cout << "Window created successfully" << std::endl;

    if (!initialize_opengl()) {
      std::cerr << "Failed to initialize OpenGL" << std::endl;
      return false;
    }
    std::cout << "OpenGL initialized successfully" << std::endl;

    if (!initialize_nanovg()) {
      std::cerr << "Failed to initialize NanoVG" << std::endl;
      return false;
    }
    std::cout << "NanoVG initialized successfully" << std::endl;

    return true;
  }

  bool setup_gl_attributes() {
    if (!SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8)) {
      std::cerr << "Failed to set stencil size: " << SDL_GetError() << std::endl;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2)) {
      std::cerr << "Failed to set GL major version: " << SDL_GetError() << std::endl;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0)) {
      std::cerr << "Failed to set GL minor version: " << SDL_GetError() << std::endl;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
      std::cerr << "Failed to set double buffer: " << SDL_GetError() << std::endl;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1)) {
      std::cerr << "Failed to set multisample buffers: " << SDL_GetError() << std::endl;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8)) {
      std::cerr << "Failed to set multisample samples: " << SDL_GetError() << std::endl;
    }
    return true;
  }

  bool create_window() {
    m_window =
        SDL_CreateWindow("SDL3/OpenGL/NanoVG Example", m_width, m_height,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!m_window) {
      SDL_Log("Failed to create window with MSAA, trying without...");
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

      m_window = SDL_CreateWindow("SDL3/OpenGL/NanoVG Example", m_width, m_height,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_HIGH_PIXEL_DENSITY);

      if (!m_window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return false;
      }
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context) {
      std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
      return false;
    }

    if (!SDL_GL_MakeCurrent(m_window, m_context)) {
      std::cerr << "Failed to make GL context current: " << SDL_GetError() << std::endl;
      return false;
    }
    return true;
  }

  bool initialize_opengl() {
    if (!gladLoadGL()) {
      std::cerr << "Failed to load OpenGL functions via GLAD" << std::endl;
      return false;
    }
    return true;
  }

  bool initialize_nanovg() {
    m_vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (!m_vg) {
      std::cerr << "Failed to create NanoVG context" << std::endl;
      return false;
    }

    // Load fonts
    if (nvgCreateFont(m_vg, "sans", "resources/Roboto-Regular.ttf") == -1) {
      std::cerr << "Warning: Could not load Roboto-Regular.ttf font" << std::endl;
    } else {
      std::cout << "Loaded font: Roboto-Regular" << std::endl;
    }

    if (nvgCreateFont(m_vg, "sans-bold", "resources/Roboto-Bold.ttf") == -1) {
      std::cerr << "Warning: Could not load Roboto-Bold.ttf font" << std::endl;
    } else {
      std::cout << "Loaded font: Roboto-Bold" << std::endl;
    }

    return true;
  }

  void process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      handle_event(event);
    }
  }

  void handle_event(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      m_running = false;
      break;

    case SDL_EVENT_KEY_DOWN:
      handle_key_event(event.key.key);
      break;

    case SDL_EVENT_TEXT_INPUT:
      handle_text_input(event.text.text);
      break;

    case SDL_EVENT_WINDOW_RESIZED:
      SDL_GetWindowSize(m_window, &m_width, &m_height);
      break;

    case SDL_EVENT_MOUSE_MOTION:
      m_mouse_x = static_cast<int>(event.motion.x);
      m_mouse_y = static_cast<int>(event.motion.y);
      
      // Handle window dragging
      if (m_window_dragging) {
        m_window_x = m_mouse_x - m_drag_offset_x;
        m_window_y = m_mouse_y - m_drag_offset_y;
      }
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      handle_mouse_down(static_cast<int>(event.button.x), static_cast<int>(event.button.y), event.button.button);
      handle_mouse_click(static_cast<int>(event.button.x), static_cast<int>(event.button.y));
      handle_mouse_button(event.button.button);
      break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
      m_window_dragging = false;
      break;

    default:
      break;
    }
  }

  void handle_key_event(SDL_Keycode key) {
    // Handle backspace for text input
    if (m_active_textbox >= 0 && key == SDLK_BACKSPACE) {
      std::string *text = get_active_text();
      if (text && !text->empty()) {
        text->pop_back();
      }
      return;
    }

    // Handle tab to switch between textboxes
    if (key == SDLK_TAB) {
      m_active_textbox = (m_active_textbox + 1) % 3;
      if (m_active_textbox == 0)
        m_active_textbox = -1; // Cycle back to none
      return;
    }

    switch (key) {
    case SDLK_ESCAPE:
      if (m_active_textbox >= 0) {
        m_active_textbox = -1; // Deselect textbox
      } else {
        m_running = false;
      }
      break;

    case SDLK_R:
      std::cout << "Reset requested" << std::endl;
      break;

    case SDLK_P:
    case SDLK_SPACE:
      std::cout << "Pause/Play toggled" << std::endl;
      break;

    default:
      break;
    }
  }

  void handle_text_input(const char *text) {
    if (m_active_textbox >= 0) {
      std::string *active_text = get_active_text();
      if (active_text) {
        *active_text += text;
      }
    }
  }

  void handle_mouse_click(int x, int y) {
    // Check if click is on any textbox
    float tx = 450, ty = 450;

    // Textbox 1
    if (x >= tx && x <= tx + 200 && y >= ty && y <= ty + 28) {
      m_active_textbox = 0;
      SDL_StartTextInput(m_window);
      std::cout << "Textbox 1 selected" << std::endl;
      return;
    }

    // Textbox 2
    ty += 40;
    if (x >= tx && x <= tx + 200 && y >= ty && y <= ty + 28) {
      m_active_textbox = 1;
      SDL_StartTextInput(m_window);
      std::cout << "Textbox 2 selected" << std::endl;
      return;
    }

    // Textbox 3
    ty += 40;
    if (x >= tx && x <= tx + 200 && y >= ty && y <= ty + 28) {
      m_active_textbox = 2;
      SDL_StartTextInput(m_window);
      std::cout << "Textbox 3 selected" << std::endl;
      return;
    }

    // Click outside - deselect
    m_active_textbox = -1;
    SDL_StopTextInput(m_window);
  }

  std::string *get_active_text() {
    switch (m_active_textbox) {
    case 0:
      return &m_text_input1;
    case 1:
      return &m_text_input2;
    case 2:
      return &m_text_input3;
    default:
      return nullptr;
    }
  }

  void handle_mouse_button(Uint8 button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
      // Check if clicking on popup menu
      if (m_show_popup && handle_popup_click(m_mouse_x, m_mouse_y)) {
        return;
      }
      // Otherwise hide popup
      m_show_popup = false;
      break;

    case SDL_BUTTON_MIDDLE:
      std::cout << "Middle mouse button clicked" << std::endl;
      break;

    case SDL_BUTTON_RIGHT:
      // Show popup menu at mouse position
      m_popup_x = m_mouse_x;
      m_popup_y = m_mouse_y;
      m_show_popup = true;
      std::cout << "Right click - showing popup menu" << std::endl;
      break;

    default:
      break;
    }
  }

  void render() {
    int fb_width, fb_height;
    SDL_GetWindowSize(m_window, &m_width, &m_height);
    fb_width = m_width;
    fb_height = m_height;
    float px_ratio = static_cast<float>(fb_width) / static_cast<float>(m_width);

    glViewport(0, 0, fb_width, fb_height);
    glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(m_vg, static_cast<float>(m_width), static_cast<float>(m_height), px_ratio);
    draw_content();
    nvgEndFrame(m_vg);
  }

  void draw_content() {
    draw_header();
    draw_rectangles();
    draw_circles();
    draw_rounded_rectangles();
    draw_gradients();
    draw_arcs_and_ellipses();
    draw_text_samples();
    draw_lines();
    draw_paths();
    draw_animated_widgets();
    draw_text_boxes();

    // Draw window widget
    if (m_show_window) {
      draw_window_widget();
    }

    // Draw popup menu last (on top)
    if (m_show_popup) {
      draw_popup_menu();
    }
  }

  void draw_header() {
    // Title
    nvgFontSize(m_vg, 24.0f);
    nvgFontFace(m_vg, "sans-bold");
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, 20, 20, "SDL3 + NanoVG Widget Demo", nullptr);

    // Subtitle
    nvgFontSize(m_vg, 14.0f);
    nvgFontFace(m_vg, "sans");
    nvgFillColor(m_vg, nvgRGBA(200, 200, 200, 255));
    nvgText(m_vg, 20, 50, "Press ESC to exit | R to reset | SPACE to pause", nullptr);
  }

  void draw_rectangles() {
    float x = 20, y = 90;

    // Solid rectangle
    nvgBeginPath(m_vg);
    nvgRect(m_vg, x, y, 100, 60);
    nvgFillColor(m_vg, nvgRGBA(255, 100, 100, 255));
    nvgFill(m_vg);

    // Rectangle with stroke
    nvgBeginPath(m_vg);
    nvgRect(m_vg, x + 120, y, 100, 60);
    nvgFillColor(m_vg, nvgRGBA(100, 255, 100, 255));
    nvgFill(m_vg);
    nvgStrokeColor(m_vg, nvgRGBA(0, 150, 0, 255));
    nvgStrokeWidth(m_vg, 3.0f);
    nvgStroke(m_vg);

    // Semi-transparent rectangle
    nvgBeginPath(m_vg);
    nvgRect(m_vg, x + 240, y, 100, 60);
    nvgFillColor(m_vg, nvgRGBA(100, 100, 255, 180));
    nvgFill(m_vg);

    // Labels
    nvgFontSize(m_vg, 12.0f);
    nvgFontFace(m_vg, "sans");
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x + 50, y + 30, "Solid", nullptr);
    nvgText(m_vg, x + 170, y + 30, "Stroked", nullptr);
    nvgText(m_vg, x + 290, y + 30, "Alpha", nullptr);
  }

  void draw_circles() {
    float x = 70, y = 200;

    // Filled circle
    nvgBeginPath(m_vg);
    nvgCircle(m_vg, x, y, 40);
    nvgFillColor(m_vg, nvgRGBA(255, 200, 0, 255));
    nvgFill(m_vg);

    // Stroked circle
    nvgBeginPath(m_vg);
    nvgCircle(m_vg, x + 140, y, 40);
    nvgStrokeColor(m_vg, nvgRGBA(255, 100, 200, 255));
    nvgStrokeWidth(m_vg, 4.0f);
    nvgStroke(m_vg);

    // Filled and stroked
    nvgBeginPath(m_vg);
    nvgCircle(m_vg, x + 280, y, 40);
    nvgFillColor(m_vg, nvgRGBA(100, 200, 255, 255));
    nvgFill(m_vg);
    nvgStrokeColor(m_vg, nvgRGBA(0, 100, 200, 255));
    nvgStrokeWidth(m_vg, 3.0f);
    nvgStroke(m_vg);
  }

  void draw_rounded_rectangles() {
    float x = 20, y = 280;

    // Small radius
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x, y, 100, 60, 5);
    nvgFillColor(m_vg, nvgRGBA(200, 100, 255, 255));
    nvgFill(m_vg);

    // Medium radius
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x + 120, y, 100, 60, 15);
    nvgFillColor(m_vg, nvgRGBA(255, 150, 100, 255));
    nvgFill(m_vg);

    // Large radius
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x + 240, y, 100, 60, 30);
    nvgFillColor(m_vg, nvgRGBA(100, 255, 200, 255));
    nvgFill(m_vg);

    // Labels
    nvgFontSize(m_vg, 12.0f);
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x + 50, y + 30, "r=5", nullptr);
    nvgText(m_vg, x + 170, y + 30, "r=15", nullptr);
    nvgText(m_vg, x + 290, y + 30, "r=30", nullptr);
  }

  void draw_gradients() {
    float x = 20, y = 380;

    // Linear gradient
    NVGpaint gradient = nvgLinearGradient(m_vg, x, y, x, y + 60, nvgRGBA(255, 100, 100, 255),
                                          nvgRGBA(100, 100, 255, 255));
    nvgBeginPath(m_vg);
    nvgRect(m_vg, x, y, 100, 60);
    nvgFillPaint(m_vg, gradient);
    nvgFill(m_vg);

    // Radial gradient
    NVGpaint radial = nvgRadialGradient(m_vg, x + 170, y + 30, 10, 50, nvgRGBA(255, 255, 100, 255),
                                        nvgRGBA(255, 100, 100, 128));
    nvgBeginPath(m_vg);
    nvgRect(m_vg, x + 120, y, 100, 60);
    nvgFillPaint(m_vg, radial);
    nvgFill(m_vg);

    // Box gradient
    NVGpaint box = nvgBoxGradient(m_vg, x + 240, y, 100, 60, 10, 20, nvgRGBA(100, 255, 100, 255),
                                  nvgRGBA(100, 100, 255, 255));
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x + 240, y, 100, 60, 10);
    nvgFillPaint(m_vg, box);
    nvgFill(m_vg);

    // Labels
    nvgFontSize(m_vg, 12.0f);
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x + 50, y + 30, "Linear", nullptr);
    nvgText(m_vg, x + 170, y + 30, "Radial", nullptr);
    nvgText(m_vg, x + 290, y + 30, "Box", nullptr);
  }

  void draw_text_samples() {
    float x = 20, y = 480;

    // Draw a background box to show the text area
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x - 5, y - 5, 350, 90, 5);
    nvgFillColor(m_vg, nvgRGBA(40, 40, 40, 200));
    nvgFill(m_vg);

    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    // Different sizes - try to render, will show if fonts are available
    nvgFontSize(m_vg, 12.0f);
    nvgFontFace(m_vg, "sans");
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x, y, "Small text (12px)", nullptr);

    nvgFontSize(m_vg, 18.0f);
    nvgText(m_vg, x, y + 25, "Medium text (18px)", nullptr);

    nvgFontSize(m_vg, 24.0f);
    nvgFontFace(m_vg, "sans-bold");
    nvgText(m_vg, x, y + 55, "Large bold text (24px)", nullptr);

    // Colored text
    nvgFontSize(m_vg, 16.0f);
    nvgFontFace(m_vg, "sans");
    nvgFillColor(m_vg, nvgRGBA(255, 100, 100, 255));
    nvgText(m_vg, x + 250, y, "Red", nullptr);
    nvgFillColor(m_vg, nvgRGBA(100, 255, 100, 255));
    nvgText(m_vg, x + 250, y + 25, "Green", nullptr);
    nvgFillColor(m_vg, nvgRGBA(100, 100, 255, 255));
    nvgText(m_vg, x + 250, y + 50, "Blue", nullptr);

    // If fonts aren't showing, draw a note
    nvgFontSize(m_vg, 10.0f);
    nvgFillColor(m_vg, nvgRGBA(200, 200, 100, 255));
    nvgText(m_vg, x, y + 80, "(Note: Text requires font files to be loaded)", nullptr);
  }

  void draw_arcs_and_ellipses() {
    float x = 450, y = 90;

    // Arc
    nvgBeginPath(m_vg);
    nvgArc(m_vg, x + 50, y + 30, 40, 0, 3.14159f, NVG_CW);
    nvgStrokeColor(m_vg, nvgRGBA(255, 200, 100, 255));
    nvgStrokeWidth(m_vg, 3.0f);
    nvgStroke(m_vg);

    // Full arc (circle via arc)
    nvgBeginPath(m_vg);
    nvgArc(m_vg, x + 150, y + 30, 30, 0, 6.28318f, NVG_CCW);
    nvgFillColor(m_vg, nvgRGBA(100, 255, 200, 255));
    nvgFill(m_vg);

    // Ellipse
    nvgBeginPath(m_vg);
    nvgEllipse(m_vg, x + 250, y + 30, 50, 30);
    nvgFillColor(m_vg, nvgRGBA(255, 150, 255, 255));
    nvgFill(m_vg);
    nvgStrokeColor(m_vg, nvgRGBA(200, 100, 200, 255));
    nvgStrokeWidth(m_vg, 2.0f);
    nvgStroke(m_vg);

    // Labels
    nvgFontSize(m_vg, 12.0f);
    nvgFontFace(m_vg, "sans");
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x + 50, y + 75, "Arc", nullptr);
    nvgText(m_vg, x + 150, y + 75, "Circle", nullptr);
    nvgText(m_vg, x + 250, y + 75, "Ellipse", nullptr);
  }

  void draw_lines() {
    float x = 20, y = 600;

    // Thin line
    nvgBeginPath(m_vg);
    nvgMoveTo(m_vg, x, y);
    nvgLineTo(m_vg, x + 100, y);
    nvgStrokeColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgStrokeWidth(m_vg, 1.0f);
    nvgStroke(m_vg);

    // Medium line
    nvgBeginPath(m_vg);
    nvgMoveTo(m_vg, x + 120, y);
    nvgLineTo(m_vg, x + 220, y);
    nvgStrokeColor(m_vg, nvgRGBA(255, 200, 100, 255));
    nvgStrokeWidth(m_vg, 3.0f);
    nvgStroke(m_vg);

    // Thick line
    nvgBeginPath(m_vg);
    nvgMoveTo(m_vg, x + 240, y);
    nvgLineTo(m_vg, x + 340, y);
    nvgStrokeColor(m_vg, nvgRGBA(100, 200, 255, 255));
    nvgStrokeWidth(m_vg, 6.0f);
    nvgStroke(m_vg);

    // Bezier curve
    nvgBeginPath(m_vg);
    nvgMoveTo(m_vg, x, y + 40);
    nvgBezierTo(m_vg, x + 50, y + 10, x + 100, y + 70, x + 150, y + 40);
    nvgStrokeColor(m_vg, nvgRGBA(255, 100, 255, 255));
    nvgStrokeWidth(m_vg, 2.0f);
    nvgStroke(m_vg);

    // Polyline
    nvgBeginPath(m_vg);
    nvgMoveTo(m_vg, x + 200, y + 40);
    nvgLineTo(m_vg, x + 230, y + 20);
    nvgLineTo(m_vg, x + 260, y + 50);
    nvgLineTo(m_vg, x + 290, y + 30);
    nvgLineTo(m_vg, x + 320, y + 40);
    nvgStrokeColor(m_vg, nvgRGBA(100, 255, 100, 255));
    nvgStrokeWidth(m_vg, 2.0f);
    nvgStroke(m_vg);
  }

  void draw_paths() {
    float x = 450, y = 200;

    // Triangle path
    nvgBeginPath(m_vg);
    nvgMoveTo(m_vg, x + 50, y);
    nvgLineTo(m_vg, x + 100, y + 60);
    nvgLineTo(m_vg, x, y + 60);
    nvgClosePath(m_vg);
    nvgFillColor(m_vg, nvgRGBA(255, 200, 100, 255));
    nvgFill(m_vg);

    // Star path
    nvgBeginPath(m_vg);
    float cx = x + 170, cy = y + 30;
    for (int i = 0; i < 5; i++) {
      float angle = (i * 4.0f * 3.14159f / 5.0f) - 3.14159f / 2.0f;
      float px = cx + cosf(angle) * 30;
      float py = cy + sinf(angle) * 30;
      if (i == 0)
        nvgMoveTo(m_vg, px, py);
      else
        nvgLineTo(m_vg, px, py);
    }
    nvgClosePath(m_vg);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 100, 255));
    nvgFill(m_vg);
    nvgStrokeColor(m_vg, nvgRGBA(200, 200, 0, 255));
    nvgStrokeWidth(m_vg, 2.0f);
    nvgStroke(m_vg);

    // Pentagon
    nvgBeginPath(m_vg);
    cx = x + 270;
    cy = y + 30;
    for (int i = 0; i < 5; i++) {
      float angle = (i * 2.0f * 3.14159f / 5.0f) - 3.14159f / 2.0f;
      float px = cx + cosf(angle) * 30;
      float py = cy + sinf(angle) * 30;
      if (i == 0)
        nvgMoveTo(m_vg, px, py);
      else
        nvgLineTo(m_vg, px, py);
    }
    nvgClosePath(m_vg);
    nvgFillColor(m_vg, nvgRGBA(100, 255, 255, 255));
    nvgFill(m_vg);

    // Labels
    nvgFontSize(m_vg, 12.0f);
    nvgFontFace(m_vg, "sans");
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x + 50, y + 75, "Triangle", nullptr);
    nvgText(m_vg, x + 170, y + 75, "Star", nullptr);
    nvgText(m_vg, x + 270, y + 75, "Pentagon", nullptr);
  }

  void draw_animated_widgets() {
    float x = 450, y = 320;

    // Spinner
    draw_spinner(x + 50, y + 40, 30);

    // Color wheel
    draw_color_wheel(x + 150, y, 100, 100);

    // Eyes
    draw_eyes(x + 280, y + 10, 120, 80);

    // Labels
    nvgFontSize(m_vg, 12.0f);
    nvgFontFace(m_vg, "sans");
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x + 50, y + 85, "Spinner", nullptr);
    nvgText(m_vg, x + 200, y + 110, "Color Wheel", nullptr);
    nvgText(m_vg, x + 340, y + 100, "Eyes", nullptr);
  }

  void draw_spinner(float cx, float cy, float r) {
    float a0 = 0.0f + m_time * 6;
    float a1 = 3.14159f + m_time * 6;
    float r0 = r;
    float r1 = r * 0.75f;
    float ax, ay, bx, by;

    nvgSave(m_vg);

    nvgBeginPath(m_vg);
    nvgArc(m_vg, cx, cy, r0, a0, a1, NVG_CW);
    nvgArc(m_vg, cx, cy, r1, a1, a0, NVG_CCW);
    nvgClosePath(m_vg);
    ax = cx + cosf(a0) * (r0 + r1) * 0.5f;
    ay = cy + sinf(a0) * (r0 + r1) * 0.5f;
    bx = cx + cosf(a1) * (r0 + r1) * 0.5f;
    by = cy + sinf(a1) * (r0 + r1) * 0.5f;
    NVGpaint paint =
        nvgLinearGradient(m_vg, ax, ay, bx, by, nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 128));
    nvgFillPaint(m_vg, paint);
    nvgFill(m_vg);

    nvgRestore(m_vg);
  }

  void draw_color_wheel(float x, float y, float w, float h) {
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    float r1 = (w < h ? w : h) * 0.5f - 5.0f;
    float r0 = r1 - 20.0f;
    float aeps = 0.5f / r1;
    float hue = sinf(m_time * 0.12f);

    nvgSave(m_vg);

    for (int i = 0; i < 6; i++) {
      float a0 = (float)i / 6.0f * 3.14159f * 2.0f - aeps;
      float a1 = (float)(i + 1.0f) / 6.0f * 3.14159f * 2.0f + aeps;
      nvgBeginPath(m_vg);
      nvgArc(m_vg, cx, cy, r0, a0, a1, NVG_CW);
      nvgArc(m_vg, cx, cy, r1, a1, a0, NVG_CCW);
      nvgClosePath(m_vg);
      float ax = cx + cosf(a0) * (r0 + r1) * 0.5f;
      float ay = cy + sinf(a0) * (r0 + r1) * 0.5f;
      float bx = cx + cosf(a1) * (r0 + r1) * 0.5f;
      float by = cy + sinf(a1) * (r0 + r1) * 0.5f;
      NVGpaint paint =
          nvgLinearGradient(m_vg, ax, ay, bx, by, nvgHSLA(a0 / (3.14159f * 2), 1.0f, 0.55f, 255),
                            nvgHSLA(a1 / (3.14159f * 2), 1.0f, 0.55f, 255));
      nvgFillPaint(m_vg, paint);
      nvgFill(m_vg);
    }

    nvgBeginPath(m_vg);
    nvgCircle(m_vg, cx, cy, r0 - 0.5f);
    nvgCircle(m_vg, cx, cy, r1 + 0.5f);
    nvgStrokeColor(m_vg, nvgRGBA(0, 0, 0, 64));
    nvgStrokeWidth(m_vg, 1.0f);
    nvgStroke(m_vg);

    // Selector
    nvgSave(m_vg);
    nvgTranslate(m_vg, cx, cy);
    nvgRotate(m_vg, hue * 3.14159f * 2);

    nvgStrokeWidth(m_vg, 2.0f);
    nvgBeginPath(m_vg);
    nvgRect(m_vg, r0 - 1, -3, r1 - r0 + 2, 6);
    nvgStrokeColor(m_vg, nvgRGBA(255, 255, 255, 192));
    nvgStroke(m_vg);

    nvgRestore(m_vg);
    nvgRestore(m_vg);
  }

  void draw_eyes(float x, float y, float w, float h) {
    float ex = w * 0.23f;
    float ey = h * 0.5f;
    float lx = x + ex;
    float ly = y + ey;
    float rx = x + w - ex;
    float ry = y + ey;
    float br = (ex < ey ? ex : ey) * 0.5f;
    float blink = 1 - powf(sinf(m_time * 0.5f), 200) * 0.8f;

    NVGpaint bg = nvgLinearGradient(m_vg, x, y + h * 0.5f, x + w * 0.1f, y + h,
                                    nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 16));
    nvgBeginPath(m_vg);
    nvgEllipse(m_vg, lx + 3.0f, ly + 16.0f, ex, ey);
    nvgEllipse(m_vg, rx + 3.0f, ry + 16.0f, ex, ey);
    nvgFillPaint(m_vg, bg);
    nvgFill(m_vg);

    bg = nvgLinearGradient(m_vg, x, y + h * 0.25f, x + w * 0.1f, y + h, nvgRGBA(220, 220, 220, 255),
                           nvgRGBA(128, 128, 128, 255));
    nvgBeginPath(m_vg);
    nvgEllipse(m_vg, lx, ly, ex, ey);
    nvgEllipse(m_vg, rx, ry, ex, ey);
    nvgFillPaint(m_vg, bg);
    nvgFill(m_vg);

    // Left pupil
    float dx = (m_mouse_x - lx) / (ex * 10);
    float dy = (m_mouse_y - ly) / (ey * 10);
    float d = sqrtf(dx * dx + dy * dy);
    if (d > 1.0f) {
      dx /= d;
      dy /= d;
    }
    dx *= ex * 0.4f;
    dy *= ey * 0.5f;
    nvgBeginPath(m_vg);
    nvgEllipse(m_vg, lx + dx, ly + dy + ey * 0.25f * (1 - blink), br, br * blink);
    nvgFillColor(m_vg, nvgRGBA(32, 32, 32, 255));
    nvgFill(m_vg);

    // Right pupil
    dx = (m_mouse_x - rx) / (ex * 10);
    dy = (m_mouse_y - ry) / (ey * 10);
    d = sqrtf(dx * dx + dy * dy);
    if (d > 1.0f) {
      dx /= d;
      dy /= d;
    }
    dx *= ex * 0.4f;
    dy *= ey * 0.5f;
    nvgBeginPath(m_vg);
    nvgEllipse(m_vg, rx + dx, ry + dy + ey * 0.25f * (1 - blink), br, br * blink);
    nvgFillColor(m_vg, nvgRGBA(32, 32, 32, 255));
    nvgFill(m_vg);

    // Gloss left
    NVGpaint gloss = nvgRadialGradient(m_vg, lx - ex * 0.25f, ly - ey * 0.5f, ex * 0.1f, ex * 0.75f,
                                       nvgRGBA(255, 255, 255, 128), nvgRGBA(255, 255, 255, 0));
    nvgBeginPath(m_vg);
    nvgEllipse(m_vg, lx, ly, ex, ey);
    nvgFillPaint(m_vg, gloss);
    nvgFill(m_vg);

    // Gloss right
    gloss = nvgRadialGradient(m_vg, rx - ex * 0.25f, ry - ey * 0.5f, ex * 0.1f, ex * 0.75f,
                              nvgRGBA(255, 255, 255, 128), nvgRGBA(255, 255, 255, 0));
    nvgBeginPath(m_vg);
    nvgEllipse(m_vg, rx, ry, ex, ey);
    nvgFillPaint(m_vg, gloss);
    nvgFill(m_vg);
  }

  void draw_text_boxes() {
    float x = 450, y = 450;

    // Update cursor blink
    m_cursor_blink_time += 0.016f; // Approximate frame time
    if (m_cursor_blink_time > 1.0f) {
      m_cursor_blink_time = 0.0f;
      m_show_cursor = !m_show_cursor;
    }

    // Simple text box
    draw_edit_box(m_text_input1.c_str(), x, y, 200, 28, m_active_textbox == 0);

    // Numeric text box with units
    draw_edit_box_num(m_text_input2.c_str(), "kg", x, y + 40, 200, 28, m_active_textbox == 1);

    // Another text box
    draw_edit_box(m_text_input3.c_str(), x, y + 80, 200, 28, m_active_textbox == 2);

    // Labels
    nvgFontSize(m_vg, 12.0f);
    nvgFontFace(m_vg, "sans");
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(m_vg, nvgRGBA(200, 200, 200, 255));
    nvgText(m_vg, x, y - 18, "Text Input (click to edit):", nullptr);
    nvgText(m_vg, x, y + 22, "Numeric Input:", nullptr);
    nvgText(m_vg, x, y + 62, "Email Input:", nullptr);

    // Instructions
    nvgFontSize(m_vg, 10.0f);
    nvgFillColor(m_vg, nvgRGBA(150, 150, 150, 255));
    nvgText(m_vg, x, y + 120, "Click textbox to edit, ESC to deselect, TAB to switch", nullptr);
  }

  void draw_edit_box_base(float x, float y, float w, float h) {
    NVGpaint bg = nvgBoxGradient(m_vg, x + 1, y + 1 + 1.5f, w - 2, h - 2, 3, 4,
                                 nvgRGBA(255, 255, 255, 32), nvgRGBA(32, 32, 32, 32));
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x + 1, y + 1, w - 2, h - 2, 4 - 1);
    nvgFillPaint(m_vg, bg);
    nvgFill(m_vg);

    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x + 0.5f, y + 0.5f, w - 1, h - 1, 4 - 0.5f);
    nvgStrokeColor(m_vg, nvgRGBA(0, 0, 0, 48));
    nvgStroke(m_vg);
  }

  void draw_edit_box(const char *text, float x, float y, float w, float h, bool active = false) {
    draw_edit_box_base(x, y, w, h);

    // Highlight if active
    if (active) {
      nvgBeginPath(m_vg);
      nvgRoundedRect(m_vg, x + 0.5f, y + 0.5f, w - 1, h - 1, 4 - 0.5f);
      nvgStrokeColor(m_vg, nvgRGBA(100, 150, 255, 255));
      nvgStrokeWidth(m_vg, 2.0f);
      nvgStroke(m_vg);
    }

    nvgFontSize(m_vg, 17.0f);
    nvgFontFace(m_vg, "sans");
    nvgFillColor(m_vg, active ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(255, 255, 255, 128));
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float text_x = x + h * 0.3f;
    nvgText(m_vg, text_x, y + h * 0.5f, text, nullptr);

    // Draw cursor if active
    if (active && m_show_cursor) {
      float text_width = nvgTextBounds(m_vg, 0, 0, text, nullptr, nullptr);
      nvgBeginPath(m_vg);
      nvgRect(m_vg, text_x + text_width + 2, y + h * 0.2f, 2, h * 0.6f);
      nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
      nvgFill(m_vg);
    }
  }

  void draw_edit_box_num(const char *text, const char *units, float x, float y, float w, float h,
                         bool active = false) {
    draw_edit_box_base(x, y, w, h);

    // Highlight if active
    if (active) {
      nvgBeginPath(m_vg);
      nvgRoundedRect(m_vg, x + 0.5f, y + 0.5f, w - 1, h - 1, 4 - 0.5f);
      nvgStrokeColor(m_vg, nvgRGBA(100, 150, 255, 255));
      nvgStrokeWidth(m_vg, 2.0f);
      nvgStroke(m_vg);
    }

    float uw = nvgTextBounds(m_vg, 0, 0, units, nullptr, nullptr);

    nvgFontSize(m_vg, 15.0f);
    nvgFontFace(m_vg, "sans");
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 64));
    nvgTextAlign(m_vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(m_vg, x + w - h * 0.3f, y + h * 0.5f, units, nullptr);

    nvgFontSize(m_vg, 17.0f);
    nvgFontFace(m_vg, "sans");
    nvgFillColor(m_vg, active ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(255, 255, 255, 128));
    nvgTextAlign(m_vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(m_vg, x + w - uw - h * 0.5f, y + h * 0.5f, text, nullptr);
  }

  void draw_popup_menu() {
    const char *menu_items[] = {"Copy", "Paste", "Delete", "---", "Properties", "Exit"};
    const int num_items = 6;
    const float item_height = 25.0f;
    const float menu_width = 150.0f;
    const float menu_height = item_height * num_items;

    float x = static_cast<float>(m_popup_x);
    float y = static_cast<float>(m_popup_y);

    // Check hover
    m_popup_hover_item = -1;
    if (m_mouse_x >= x && m_mouse_x <= x + menu_width && m_mouse_y >= y &&
        m_mouse_y <= y + menu_height) {
      m_popup_hover_item = static_cast<int>((m_mouse_y - y) / item_height);
    }

    // Draw shadow
    NVGpaint shadow = nvgBoxGradient(m_vg, x, y + 2, menu_width, menu_height, 5, 10,
                                     nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(m_vg);
    nvgRect(m_vg, x - 5, y - 5, menu_width + 10, menu_height + 10);
    nvgRoundedRect(m_vg, x, y, menu_width, menu_height, 5);
    nvgPathWinding(m_vg, NVG_HOLE);
    nvgFillPaint(m_vg, shadow);
    nvgFill(m_vg);

    // Draw menu background
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x, y, menu_width, menu_height, 5);
    nvgFillColor(m_vg, nvgRGBA(50, 50, 55, 240));
    nvgFill(m_vg);

    // Draw border
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x + 0.5f, y + 0.5f, menu_width - 1, menu_height - 1, 4.5f);
    nvgStrokeColor(m_vg, nvgRGBA(0, 0, 0, 100));
    nvgStroke(m_vg);

    // Draw menu items
    nvgFontSize(m_vg, 14.0f);
    nvgFontFace(m_vg, "sans");
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    for (int i = 0; i < num_items; i++) {
      float item_y = y + i * item_height;

      // Check if separator
      if (strcmp(menu_items[i], "---") == 0) {
        nvgBeginPath(m_vg);
        nvgMoveTo(m_vg, x + 5, item_y + item_height * 0.5f);
        nvgLineTo(m_vg, x + menu_width - 5, item_y + item_height * 0.5f);
        nvgStrokeColor(m_vg, nvgRGBA(100, 100, 100, 100));
        nvgStroke(m_vg);
        continue;
      }

      // Highlight hovered item
      if (i == m_popup_hover_item) {
        nvgBeginPath(m_vg);
        nvgRect(m_vg, x + 2, item_y + 2, menu_width - 4, item_height - 4);
        nvgFillColor(m_vg, nvgRGBA(80, 120, 200, 200));
        nvgFill(m_vg);
      }

      // Draw text
      nvgFillColor(m_vg, i == m_popup_hover_item ? nvgRGBA(255, 255, 255, 255)
                                                 : nvgRGBA(220, 220, 220, 255));
      nvgText(m_vg, x + 15, item_y + item_height * 0.5f, menu_items[i], nullptr);
    }
  }

  bool handle_popup_click(int x, int y) {
    const int num_items = 6;
    const float item_height = 25.0f;
    const float menu_width = 150.0f;
    const float menu_height = item_height * num_items;

    float menu_x = static_cast<float>(m_popup_x);
    float menu_y = static_cast<float>(m_popup_y);

    // Check if click is inside menu
    if (x >= menu_x && x <= menu_x + menu_width && y >= menu_y && y <= menu_y + menu_height) {

      int clicked_item = static_cast<int>((y - menu_y) / item_height);

      const char *menu_items[] = {"Copy", "Paste", "Delete", "---", "Properties", "Exit"};

      if (clicked_item >= 0 && clicked_item < num_items &&
          strcmp(menu_items[clicked_item], "---") != 0) {
        std::cout << "Menu item clicked: " << menu_items[clicked_item] << std::endl;

        // Handle specific actions
        if (strcmp(menu_items[clicked_item], "Exit") == 0) {
          m_running = false;
        } else if (strcmp(menu_items[clicked_item], "Copy") == 0) {
          std::cout << "Copy action" << std::endl;
        } else if (strcmp(menu_items[clicked_item], "Paste") == 0) {
          std::cout << "Paste action" << std::endl;
        } else if (strcmp(menu_items[clicked_item], "Delete") == 0) {
          std::cout << "Delete action" << std::endl;
        } else if (strcmp(menu_items[clicked_item], "Properties") == 0) {
          std::cout << "Properties dialog" << std::endl;
        }

        m_show_popup = false;
        return true;
      }
    }

    return false;
  }

  void handle_mouse_down(int x, int y, Uint8 button) {
    if (button != SDL_BUTTON_LEFT || !m_show_window) return;

    float title_bar_height = 30.0f;
    
    // Check if clicking on window title bar
    if (x >= m_window_x && x <= m_window_x + m_window_width &&
        y >= m_window_y && y <= m_window_y + title_bar_height) {
      
      // Check close button
      float close_x = m_window_x + m_window_width - 25;
      float close_y = m_window_y + 5;
      if (x >= close_x && x <= close_x + 20 && y >= close_y && y <= close_y + 20) {
        m_show_window = false;
        std::cout << "Window closed" << std::endl;
        return;
      }
      
      // Start dragging
      m_window_dragging = true;
      m_drag_offset_x = x - m_window_x;
      m_drag_offset_y = y - m_window_y;
      std::cout << "Window drag started" << std::endl;
    }
  }

  void draw_window_widget() {
    float x = m_window_x;
    float y = m_window_y;
    float w = m_window_width;
    float h = m_window_height;
    float corner_radius = 5.0f;

    nvgSave(m_vg);

    // Drop shadow
    NVGpaint shadow_paint = nvgBoxGradient(m_vg, x, y + 2, w, h, corner_radius * 2, 10,
                                           nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(m_vg);
    nvgRect(m_vg, x - 10, y - 10, w + 20, h + 30);
    nvgRoundedRect(m_vg, x, y, w, h, corner_radius);
    nvgPathWinding(m_vg, NVG_HOLE);
    nvgFillPaint(m_vg, shadow_paint);
    nvgFill(m_vg);

    // Window background
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x, y, w, h, corner_radius);
    nvgFillColor(m_vg, nvgRGBA(40, 42, 46, 240));
    nvgFill(m_vg);

    // Title bar
    NVGpaint header_paint = nvgLinearGradient(m_vg, x, y, x, y + 30,
                                              nvgRGBA(70, 72, 76, 255), nvgRGBA(50, 52, 56, 255));
    nvgBeginPath(m_vg);
    nvgRoundedRectVarying(m_vg, x + 1, y + 1, w - 2, 30, corner_radius - 1, corner_radius - 1, 0, 0);
    nvgFillPaint(m_vg, header_paint);
    nvgFill(m_vg);

    // Title bar separator
    nvgBeginPath(m_vg);
    nvgMoveTo(m_vg, x + 0.5f, y + 30.5f);
    nvgLineTo(m_vg, x + w - 0.5f, y + 30.5f);
    nvgStrokeColor(m_vg, nvgRGBA(0, 0, 0, 64));
    nvgStroke(m_vg);

    // Window title
    nvgFontSize(m_vg, 16.0f);
    nvgFontFace(m_vg, "sans-bold");
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(m_vg, nvgRGBA(220, 220, 220, 255));
    nvgText(m_vg, x + 15, y + 15, "Widget Window", nullptr);

    // Close button
    float close_x = x + w - 25;
    float close_y = y + 5;
    bool close_hover = (m_mouse_x >= close_x && m_mouse_x <= close_x + 20 &&
                        m_mouse_y >= close_y && m_mouse_y <= close_y + 20);

    nvgBeginPath(m_vg);
    nvgCircle(m_vg, close_x + 10, close_y + 10, 8);
    nvgFillColor(m_vg, close_hover ? nvgRGBA(220, 80, 80, 255) : nvgRGBA(180, 60, 60, 200));
    nvgFill(m_vg);

    // Close X
    nvgFontSize(m_vg, 14.0f);
    nvgFontFace(m_vg, "sans-bold");
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, close_x + 10, close_y + 10, "×", nullptr);

    // Window content
    draw_window_content(x + 15, y + 45, w - 30, h - 60);

    nvgRestore(m_vg);
  }

  void draw_window_content(float x, float y, float w, float h) {
    // Content title
    nvgFontSize(m_vg, 18.0f);
    nvgFontFace(m_vg, "sans-bold");
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(m_vg, nvgRGBA(200, 200, 200, 255));
    nvgText(m_vg, x, y, "Window Content", nullptr);

    // Some sample content
    nvgFontSize(m_vg, 14.0f);
    nvgFontFace(m_vg, "sans");
    nvgFillColor(m_vg, nvgRGBA(180, 180, 180, 255));
    nvgText(m_vg, x, y + 30, "This is a draggable window widget.", nullptr);
    nvgText(m_vg, x, y + 50, "Click and drag the title bar to move it.", nullptr);
    nvgText(m_vg, x, y + 70, "Click the X button to close.", nullptr);

    // Draw some sample widgets in the window
    float widget_y = y + 110;

    // Sample button
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x, widget_y, 120, 30, 3);
    nvgFillColor(m_vg, nvgRGBA(80, 120, 200, 255));
    nvgFill(m_vg);
    nvgFontSize(m_vg, 14.0f);
    nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
    nvgText(m_vg, x + 60, widget_y + 15, "Button", nullptr);

    // Sample checkbox
    widget_y += 50;
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x, widget_y, 20, 20, 3);
    nvgStrokeColor(m_vg, nvgRGBA(150, 150, 150, 255));
    nvgStrokeWidth(m_vg, 2.0f);
    nvgStroke(m_vg);
    nvgFillColor(m_vg, nvgRGBA(80, 120, 200, 255));
    nvgFill(m_vg);

    nvgFontSize(m_vg, 14.0f);
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(m_vg, nvgRGBA(200, 200, 200, 255));
    nvgText(m_vg, x + 30, widget_y + 10, "Checkbox option", nullptr);

    // Sample slider
    widget_y += 50;
    nvgFontSize(m_vg, 12.0f);
    nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(m_vg, nvgRGBA(180, 180, 180, 255));
    nvgText(m_vg, x, widget_y, "Slider:", nullptr);

    widget_y += 20;
    nvgBeginPath(m_vg);
    nvgRoundedRect(m_vg, x, widget_y, w - 30, 6, 3);
    nvgFillColor(m_vg, nvgRGBA(60, 60, 60, 255));
    nvgFill(m_vg);

    float slider_pos = 0.6f;
    nvgBeginPath(m_vg);
    nvgCircle(m_vg, x + (w - 30) * slider_pos, widget_y + 3, 10);
    nvgFillColor(m_vg, nvgRGBA(100, 140, 220, 255));
    nvgFill(m_vg);
  }

  void cleanup() {
    if (m_vg) {
      nvgDeleteGL2(m_vg);
      m_vg = nullptr;
    }

    if (m_context) {
      SDL_GL_DestroyContext(m_context);
      m_context = nullptr;
    }

    if (m_window) {
      SDL_DestroyWindow(m_window);
      m_window = nullptr;
    }

    SDL_Quit();
  }
};

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  std::cout << "Starting SDL3 + NanoVG Example..." << std::endl;

  try {
    SDLNanoVGApp app(1024, 800);
    std::cout << "Application initialized, starting main loop..." << std::endl;
    app.run();
    std::cout << "Application exited normally" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    std::cerr << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
