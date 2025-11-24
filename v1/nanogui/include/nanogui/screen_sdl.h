/*
    nanogui/screen_sdl.h -- Top-level widget and interface between NanoGUI and SDL3

    Based on the working SDL3 implementation in examples/sdl/example3.cpp

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#if defined(NANOGUI_USE_SDL3)

  #include <SDL3/SDL.h>
  #include <nanogui/colorpass.h>
  #include <nanogui/ema.h>
  #include <nanogui/texture.h>
  #include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

class Texture;
class RestartableTimer;

/**
 * \class Screen screen_sdl.h nanogui/screen_sdl.h
 *
 * \brief Represents a display surface (i.e. a full-screen or windowed SDL3 window)
 * and forms the root element of a hierarchy of nanogui widgets.
 */
class NANOGUI_EXPORT Screen : public Widget {
  friend class Widget;
  friend class Window;

public:
  /**
   * Create a new Screen instance
   *
   * \param size
   *     Size in pixels at 96 dpi (on high-DPI screens, the actual resolution
   *     in terms of hardware pixels may be larger by an integer factor)
   *
   * \param caption
   *     Window title (in UTF-8 encoding)
   *
   * \param resizable
   *     If creating a window, should it be resizable?
   *
   * \param maximized
   *     Specifies whether the window should be maximized upon creation
   *
   * \param fullscreen
   *     Specifies whether to create a windowed or full-screen view
   *
   * \param depth_buffer
   *     Should a depth buffer be allocated?
   *
   * \param stencil_buffer
   *     Should an 8-bit stencil buffer be allocated? NanoVG requires this to
   *     rasterize non-convex polygons. (NanoGUI does not render such
   *     polygons, but your application might.)
   *
   * \param float_buffer
   *     Should NanoGUI try to allocate a floating point framebuffer? This
   *     is useful for HDR and wide-gamut displays.
   *
   * \param gl_major
   *     The requested OpenGL Major version number.  The default is 3, if
   *     changed the value must correspond to a forward compatible core
   *     profile (for portability reasons).  For example, set this to 4 and
   *     \ref gl_minor to 1 for a forward compatible core OpenGL 4.1 profile.
   *     Requesting an invalid profile will result in no context (and
   *     therefore no GUI) being created. This attribute is ignored when
   *     targeting OpenGL ES 2 or Metal.
   *
   * \param gl_minor
   *     The requested OpenGL Minor version number.  The default is 2, if
   *     changed the value must correspond to a forward compatible core
   *     profile (for portability reasons).  For example, set this to 1 and
   *     \ref gl_major to 4 for a forward compatible core OpenGL 4.1 profile.
   *     Requesting an invalid profile will result in no context (and
   *     therefore no GUI) being created. This attribute is ignored when
   *     targeting OpenGL ES 2 or Metal.
   */
  Screen(const Vector2i &size, std::string_view caption = "Unnamed", bool resizable = true,
         bool maximized = false, bool fullscreen = false, bool depth_buffer = true,
         bool stencil_buffer = true, bool float_buffer = false, unsigned int gl_major = 3,
         unsigned int gl_minor = 2);

  /// Release all resources
  virtual ~Screen();

  /// Get the window title bar caption
  std::string_view caption() const { return m_caption; }

  /// Set the window title bar caption
  void set_caption(std::string_view caption);

  /// Return the screen's background color
  const Color &background() const { return m_background; }

  /// Set the screen's background color
  void set_background(const Color &background) { m_background = background; }

  /// Set the top-level window visibility
  void set_visible(bool visible);

  /// Move window relatively
  void move_window(const Vector2i &rel);

  /// Set window size
  void set_size(const Vector2i &size);

  /// Return the framebuffer size (potentially larger than size() on high-DPI screens)
  const Vector2i &framebuffer_size() const { return m_fbsize; }

  /// Send an event that will cause the screen to be redrawn at the next event loop iteration
  void redraw();

  /**
   * \brief Process SDL3 events and return true if the application should continue running
   *
   * This is the SDL3-style event loop. Call this in a while loop:
   * \code
   * while (screen->process_events()) {
   *     screen->draw_all();
   * }
   * \endcode
   *
   * Returns false when the window is closed or the application should quit.
   */
  virtual bool process_events();

  /**
   * \brief Redraw the screen if the redraw flag is set
   *
   * This function does everything -- it calls \ref draw_setup(), \ref
   * draw_contents() (which also clears the screen by default), \ref draw(),
   * and finally \ref draw_teardown().
   *
   * \sa redraw
   */
  virtual void draw_all();

  /**
   * \brief Clear the screen with the background color (glClearColor, glClear, etc.)
   *
   * You typically won't need to call this function yourself, as it is called by
   * the default implementation of \ref draw_contents() (which is called by \ref draw_all())
   */
  virtual void clear();

  /**
   * \brief Prepare the graphics pipeline for the next frame
   *
   * This involves steps such as querying the drawable resolution,
   * setting the viewport used for drawing, etc..
   *
   * You typically won't need to call this function yourself, as it is called
   * by \ref draw_all(), which is executed by the run loop.
   */
  virtual void draw_setup();

  /// Calls clear() and draws the window contents --- put your rendering code here.
  virtual void draw_contents();

  /**
   * \brief Wrap up drawing of the current frame
   *
   * This involves steps such as swapping the framebuffer, etc.
   *
   * You typically won't need to call this function yourself, as it is called
   * by \ref draw_all(), which is executed by the run loop.
   */
  virtual void draw_teardown();

  /// Return the ratio between pixel and device coordinates (e.g. >= 2 on Mac Retina displays)
  float pixel_ratio() const { return m_pixel_ratio; }

  /// Handle a file drop event
  virtual bool drop_event(const std::vector<std::string> & /* filenames */) {
    return false; /* To be overridden */
  }

  /// Default keyboard event handler
  virtual bool keyboard_event(int key, int scancode, int action, int modifiers);

  /// Text input event handler: codepoint is native endian UTF-32 format
  virtual bool keyboard_character_event(unsigned int codepoint);

  /// Window resize event handler
  virtual bool resize_event(const Vector2i &size);

  /// Window maximization event handler
  virtual bool maximize_event(bool maximized);

  /// Retrieve the resize callback
  const std::function<void(Vector2i)> &resize_callback() const { return m_resize_callback; }

  /// Set the resize callback
  void set_resize_callback(const std::function<void(Vector2i)> &callback) {
    m_resize_callback = callback;
  }

  /// Return the last observed mouse position value
  Vector2i mouse_pos() const { return m_mouse_pos; }

  /// Return a pointer to the underlying SDL window data structure
  SDL_Window *sdl_window() const { return m_sdl_window; }

  /// Return a pointer to the underlying SDL OpenGL context
  SDL_GLContext sdl_context() const { return m_sdl_context; }

  /// Backend-agnostic timing function (returns seconds since start)
  static double get_time() { return SDL_GetTicks() / 1000.0; }

  /// Return a pointer to the underlying NanoVG draw context
  NVGcontext *nvg_context() const { return m_nvg_context; }

  /// Return the component format underlying the screen
  Texture::ComponentFormat component_format() const;

  /// Return the pixel format underlying the screen
  Texture::PixelFormat pixel_format() const;

  /// Does the framebuffer have a depth buffer
  bool has_depth_buffer() const { return m_depth_buffer; }

  /// Does the framebuffer have a stencil buffer
  bool has_stencil_buffer() const { return m_stencil_buffer; }

  /// Does the framebuffer use a floating point representation
  bool has_float_buffer() const { return m_float_buffer; }

  /// Get the index of the last (or current) frame being rendered
  uint64_t frame_index() const { return m_frame_index; }

  /// Get a smoothed estimate of the rendering time per frame (second-based)
  double frame_time() const { return m_frame_timer.value(); }

  /// How many bits per sample does the framebuffer use?
  uint32_t bits_per_sample() const { return m_bits_per_sample; }

  /// Flush all queued up NanoVG rendering commands
  void nvg_flush();

  using Widget::perform_layout;

  /// Compute the layout of all widgets
  void perform_layout() { this->perform_layout(m_nvg_context); }

public:
  /********* API for applications which manage SDL3 themselves *********/

  /**
   * \brief Default constructor
   *
   * Performs no initialization at all. Use this if the application is
   * responsible for setting up SDL3, OpenGL, etc.
   *
   * In this case, override \ref Screen and call \ref initialize() with a
   * pointer to an existing \c SDL_Window instance
   */
  Screen();

  /// Initialize the \ref Screen
  void initialize(SDL_Window *window, SDL_GLContext context);

  /// Like mouse_motion_event(), but also capture fractional motion
  virtual bool mouse_motion_event_f(const Vector2f &p, const Vector2f &rel, int button,
                                    int modifiers);

  /* Internal helper functions */
  void update_focus(Widget *widget);
  void dispose_widget(Widget *widget);
  void center_window(Window *window);
  void move_window_to_front(Window *window);
  void draw_widgets();
  void draw_tooltip();

  #if defined(NANOGUI_USE_OPENGL) || defined(NANOGUI_USE_GLES)
  uint32_t framebuffer_handle() const;
  #endif

protected:
  SDL_Window *m_sdl_window = nullptr;
  SDL_GLContext m_sdl_context = nullptr;
  NVGcontext *m_nvg_context = nullptr;
  SDL_Cursor *m_cursors[(size_t)Cursor::CursorCount];
  Cursor m_cursor;
  std::vector<Widget *> m_focus_path;
  Vector2i m_fbsize;
  float m_pixel_ratio;
  int m_mouse_state, m_modifiers;
  Vector2i m_mouse_pos;
  Vector2f m_mouse_pos_f;
  bool m_drag_active;
  Widget *m_drag_widget = nullptr;
  double m_last_interaction;
  double m_last_draw;
  Color m_background;
  std::string m_caption;
  bool m_fullscreen;
  bool m_depth_buffer;
  bool m_stencil_buffer;
  bool m_float_buffer;
  uint32_t m_bits_per_sample;
  bool m_redraw;
  bool m_running;
  std::function<void(Vector2i)> m_resize_callback;
  RunMode m_last_run_mode;
  ref<Texture> m_depth_stencil_texture;
  ref<RestartableTimer> m_tooltip_timer;
  bool m_tooltip_force_visible = false;
  EMA<double> m_frame_timer;
  uint64_t m_frame_index;
  #if !defined(NANOGUI_USE_METAL)
  ref<Texture> m_color_texture;
  ref<ColorPass> m_color_pass;
  #endif

  // SDL3-specific event handlers
  void handle_sdl_event(const SDL_Event &event);
  void handle_key_event(SDL_Keycode key, SDL_Scancode scancode, bool pressed, bool repeat);
  void handle_text_input(const char *text);
  void handle_mouse_motion(float x, float y);
  void handle_mouse_button(Uint8 button, bool pressed, float x, float y);
  void handle_mouse_wheel(float x, float y);
  void handle_window_event(const SDL_WindowEvent &event);
};

NAMESPACE_END(nanogui)

#endif // NANOGUI_USE_SDL3
