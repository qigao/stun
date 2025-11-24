#include "display.h"

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

// Helper function for clamping (C++17 std::clamp alternative)
template <typename T> inline T clamp(T value, T min, T max) {
  return value < min ? min : (value > max ? max : value);
}

VideoScreen::VideoScreen(const unsigned width, const unsigned height)
    : Screen(Vector2i(width, height), "Video Player"), width_(width), height_(height) {

  // Create ImageView widget
  image_view_ = new ImageView(this);
  image_view_->set_position(Vector2i(0, 0));
  image_view_->set_size(Vector2i(width, height));

  // Create texture for video frames (RGB format)
  // Note: ImageView requires Nearest interpolation mode
  texture_ = new Texture(Texture::PixelFormat::RGB, Texture::ComponentFormat::UInt8,
                         Vector2i(width, height), Texture::InterpolationMode::Nearest,
                         Texture::InterpolationMode::Nearest);

  image_view_->set_image(texture_);
  image_view_->center();

  // Allocate RGB buffer
  rgb_buffer_.resize(width * height * 3);

  perform_layout();
  set_visible(true);
}

void VideoScreen::yuv_to_rgb(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches) {
  // YUV420P to RGB conversion
  uint8_t *y_plane = planes[0];
  uint8_t *u_plane = planes[1];
  uint8_t *v_plane = planes[2];

  size_t y_pitch = pitches[0];
  size_t u_pitch = pitches[1];
  size_t v_pitch = pitches[2];

  for (unsigned y = 0; y < height_; ++y) {
    for (unsigned x = 0; x < width_; ++x) {
      int y_val = y_plane[y * y_pitch + x];
      int u_val = u_plane[(y / 2) * u_pitch + (x / 2)] - 128;
      int v_val = v_plane[(y / 2) * v_pitch + (x / 2)] - 128;

      // YUV to RGB conversion
      int r = y_val + (1.370705f * v_val);
      int g = y_val - (0.337633f * u_val) - (0.698001f * v_val);
      int b = y_val + (1.732446f * u_val);

      // Clamp values
      r = clamp(r, 0, 255);
      g = clamp(g, 0, 255);
      b = clamp(b, 0, 255);

      size_t idx = (y * width_ + x) * 3;
      rgb_buffer_[idx + 0] = static_cast<uint8_t>(r);
      rgb_buffer_[idx + 1] = static_cast<uint8_t>(g);
      rgb_buffer_[idx + 2] = static_cast<uint8_t>(b);
    }
  }
}

void VideoScreen::update_frame(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches) {
  yuv_to_rgb(planes, pitches);
  texture_->upload(rgb_buffer_.data());
  draw_all();
}

bool VideoScreen::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (Screen::keyboard_event(key, scancode, action, modifiers))
    return true;

  // Handle key presses using backend-agnostic constants
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

Display::Display(const unsigned width, const unsigned height) {
  // Note: nanogui::init() should be called by the application before creating Display
  screen_ = std::make_unique<VideoScreen>(width, height);
}

void Display::refresh(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches) {
  screen_->update_frame(planes, pitches);
}

void Display::input() {
// Poll events using backend-specific method
#if defined(NANOGUI_USE_SDL3)
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Let the screen handle the event
    // Events will be dispatched to keyboard_event() automatically
  }
#else
  glfwPollEvents();
#endif

  quit_ = screen_->get_quit();
  play_ = screen_->get_play();
}

bool Display::get_quit() { return quit_; }

bool Display::get_play() { return play_; }
