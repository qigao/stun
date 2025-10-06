#include "display_resizable.h"
#include <cstdio>

// Include backend-specific headers for event polling
#if defined(NANOGUI_USE_SDL3)
  #include <SDL3/SDL.h>
  #define KEY_ESCAPE SDLK_ESCAPE
  #define KEY_SPACE SDLK_SPACE
  #define KEY_PRESS 1
#else
  #include <GLFW/glfw3.h>
  #define KEY_ESCAPE GLFW_KEY_ESCAPE
  #define KEY_SPACE GLFW_KEY_SPACE
  #define KEY_PRESS GLFW_PRESS
#endif

using namespace nanogui;

VideoCanvas::VideoCanvas(Widget *parent, unsigned video_width, unsigned video_height)
    : Canvas(parent, 1), video_width_(video_width), video_height_(video_height) {

  set_size(Vector2i(video_width, video_height));
  set_fixed_size(Vector2i(video_width, video_height));
  set_draw_border(false);

  video_shader_ = std::make_unique<VideoShader>();
  video_shader_->init(render_pass());
}

void VideoCanvas::update_frame(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches) {
  video_shader_->upload_yuv_frame(planes, pitches, video_width_, video_height_);
}

void VideoCanvas::draw_contents() {
  Matrix4f mvp(1.f);
  video_shader_->render(mvp, size());
}

void VideoCanvas::set_canvas_size(const Vector2i &new_size) {
  set_size(new_size);
  set_fixed_size(new_size);
}

ResizableVideoScreen::ResizableVideoScreen(const unsigned video_width, const unsigned video_height)
    : Screen(Vector2i(video_width, video_height), "Video Player", true), video_width_(video_width),
      video_height_(video_height) {

  set_background(Color(0, 0, 0, 255)); // Black background for letterboxing

  canvas_ = new VideoCanvas(this, video_width, video_height);
  canvas_->set_position(Vector2i(0, 0));
  canvas_->set_size(Vector2i(video_width, video_height));

  perform_layout();
  set_visible(true);
}

bool ResizableVideoScreen::resize_event(const Vector2i &new_size) {
  Screen::resize_event(new_size);

  // Calculate new canvas size maintaining aspect ratio
  float window_aspect = static_cast<float>(new_size.x()) / new_size.y();
  float video_aspect = static_cast<float>(video_width_) / video_height_;

  Vector2i canvas_size;
  Vector2i canvas_pos;

  if (window_aspect > video_aspect) {
    // Window is wider - fit to height
    canvas_size.y() = new_size.y();
    canvas_size.x() = static_cast<int>(new_size.y() * video_aspect);
    canvas_pos.x() = (new_size.x() - canvas_size.x()) / 2;
    canvas_pos.y() = 0;
  } else {
    // Window is taller - fit to width
    canvas_size.x() = new_size.x();
    canvas_size.y() = static_cast<int>(new_size.x() / video_aspect);
    canvas_pos.x() = 0;
    canvas_pos.y() = (new_size.y() - canvas_size.y()) / 2;
  }

  // Debug output
  printf("Window: %dx%d, Video: %dx%d, Canvas: %dx%d at (%d,%d)\n", new_size.x(), new_size.y(),
         video_width_, video_height_, canvas_size.x(), canvas_size.y(), canvas_pos.x(),
         canvas_pos.y());

  canvas_->set_position(canvas_pos);
  canvas_->set_canvas_size(canvas_size);

  perform_layout();
  redraw();

  return true;
}

void ResizableVideoScreen::update_frame(std::array<uint8_t *, 3> planes,
                                        std::array<size_t, 3> pitches) {
  canvas_->update_frame(planes, pitches);
  draw_all();
}

bool ResizableVideoScreen::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (Screen::keyboard_event(key, scancode, action, modifiers))
    return true;

  if (action == KEY_PRESS) {
    if (key == KEY_ESCAPE) {
      quit_ = true;
      set_visible(false);
      return true;
    } else if (key == KEY_SPACE) {
      play_ = !play_;
      return true;
    }
  }
  return false;
}

ResizableDisplay::ResizableDisplay(const unsigned width, const unsigned height) {
  nanogui::init();
  screen_ = std::make_unique<ResizableVideoScreen>(width, height);
}

void ResizableDisplay::refresh(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches) {
  screen_->update_frame(planes, pitches);
}

void ResizableDisplay::input() {
#if defined(NANOGUI_USE_SDL3)
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Events will be dispatched automatically
  }
#else
  glfwPollEvents();
#endif

  quit_ = screen_->get_quit();
  play_ = screen_->get_play();
}

bool ResizableDisplay::get_quit() { return quit_; }

bool ResizableDisplay::get_play() { return play_; }
