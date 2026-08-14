#pragma once

#include "flex/core/renderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flex::test_support::opengl_engine {

constexpr int kWidth = 320;
constexpr int kHeight = 180;

inline void glfw_error_callback(int, const char* description) {
  (void)description;
}

class Window final {
public:
  Window() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
      throw std::runtime_error("GLFW initialization failed");
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window_ = glfwCreateWindow(kWidth, kHeight, "Flex OpenGL engine test", nullptr,
                               nullptr);
    if (!window_) {
      glfwTerminate();
      throw std::runtime_error("OpenGL test window creation failed");
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(0);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
      glfwDestroyWindow(window_);
      window_ = nullptr;
      glfwTerminate();
      throw std::runtime_error("OpenGL test GLAD initialization failed");
    }
  }

  ~Window() {
    if (window_) {
      glfwMakeContextCurrent(window_);
      glfwDestroyWindow(window_);
    }
    glfwTerminate();
  }

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

private:
  GLFWwindow* window_ = nullptr;
};

inline void (*load_proc(void*, const char* name))() {
  return reinterpret_cast<void (*)()>(glfwGetProcAddress(name));
}

inline std::pair<std::string, std::string> test_fonts() {
  static constexpr std::array<std::pair<const char*, const char*>, 5> candidates{{
      {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf"},
      {"C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/arialbd.ttf"},
      {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
       "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
      {"/System/Library/Fonts/Supplemental/Arial.ttf",
       "/System/Library/Fonts/Supplemental/Arial Bold.ttf"},
      {"/Library/Fonts/Arial.ttf", "/Library/Fonts/Arial Bold.ttf"},
  }};
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate.first) &&
        std::filesystem::exists(candidate.second)) {
      return candidate;
    }
  }
  throw std::runtime_error("OpenGL engine tests require regular and bold test fonts");
}

inline void register_fonts(Renderer& renderer,
                           const std::pair<std::string, std::string>& fonts) {
  if (!renderer.register_font("parity", fonts.first) ||
      !renderer.register_font("parity-bold", fonts.second)) {
    throw std::runtime_error("OpenGL engine test font registration failed");
  }
}

struct SceneRecordTimings {
  double begin_clear_us = 0.0;
  double solid_rect_us = 0.0;
  double gradient_rect_us = 0.0;
  double ellipse_us = 0.0;
  double path_us = 0.0;
  double text_us = 0.0;
  double image_us = 0.0;
  double svg_us = 0.0;
};

inline void record_common_scene_commands(Renderer& renderer,
                                         const std::string& image_path,
                                         std::size_t repetitions = 1U,
                                         SceneRecordTimings* timings = nullptr) {
  static const std::string kPath =
      "M 0 20 C 10 0 34 0 44 20 C 52 38 34 48 20 44 C 4 40 -8 30 0 20 Z";
  static const std::string kSvg =
      "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='12'>"
      "<rect width='16' height='12' rx='3' fill='#ffcc33'/>"
      "<circle cx='8' cy='6' r='3' fill='#3344ff'/></svg>";
  const LinearGradient gradient = [] {
    LinearGradient value(0.0f, 0.0f, 1.0f, 0.0f);
    value.add_stop(0.0f, Color::Red);
    value.add_stop(0.5f, Color::Green);
    value.add_stop(1.0f, Color::Blue);
    return value;
  }();

  const auto measure = [timings](double SceneRecordTimings::*slot, auto&& operation) {
    if (timings == nullptr) {
      operation();
      return;
    }
    const auto begin = std::chrono::steady_clock::now();
    operation();
    const auto end = std::chrono::steady_clock::now();
    timings->*slot += std::chrono::duration<double, std::micro>(end - begin).count();
  };
  measure(&SceneRecordTimings::begin_clear_us, [&] {
    renderer.begin_frame(static_cast<float>(kWidth), static_cast<float>(kHeight), 1.0f);
    renderer.clear(Color{0.02f, 0.03f, 0.05f, 1.0f});
  });
  for (std::size_t index = 0; index < repetitions; ++index) {
    const float dx = static_cast<float>((index % 4U) * 2U);
    const float dy = static_cast<float>((index % 3U) * 2U);
    measure(&SceneRecordTimings::solid_rect_us, [&] {
      renderer.draw_rect(12.0f + dx, 12.0f + dy, 54.0f, 30.0f, 5.0f,
                         Paint::solid(Color{0.95f, 0.2f, 0.12f, 1.0f}), Paint::none(),
                         0.0f);
    });
    const Paint middle_fill = index == 0U
                                  ? Paint(gradient)
                                  : Paint::solid(Color{0.22f, 0.52f, 0.92f, 0.8f});
    measure(&SceneRecordTimings::gradient_rect_us, [&] {
      renderer.draw_rect(78.0f + dx, 12.0f + dy, 82.0f, 30.0f, 0.0f,
                         middle_fill, Paint::none(), 0.0f);
    });
    measure(&SceneRecordTimings::ellipse_us, [&] {
      renderer.draw_ellipse(198.0f + dx, 27.0f + dy, 30.0f, 15.0f,
                            Paint::solid(Color{0.12f, 0.78f, 0.35f, 1.0f}),
                            Paint::solid(Color::White), 2.0f);
    });
    measure(&SceneRecordTimings::path_us, [&] {
      renderer.save();
      renderer.translate(252.0f + dx, 8.0f + dy);
      renderer.clip_rect(0.0f, 5.0f, 48.0f, 32.0f);
      renderer.fill_path(kPath, Paint::solid(Color{0.4f, 0.5f, 1.0f, 1.0f}));
      renderer.restore();
    });
  }
  measure(&SceneRecordTimings::text_us, [&] {
    renderer.draw_text("Regular", 12.0f, 62.0f, "parity", 22.0f, false,
                       Color::White);
    renderer.draw_text("Bold", 126.0f, 62.0f, "parity", 22.0f, true,
                       Color::White);
  });
  measure(&SceneRecordTimings::image_us, [&] {
    renderer.draw_image(image_path, 214.0f, 58.0f, 38.0f, 30.0f);
  });
  measure(&SceneRecordTimings::svg_us, [&] {
    renderer.draw_svg_data(kSvg, 270.0f, 58.0f, 32.0f, 24.0f);
  });
}

inline void record_common_scene(Renderer& renderer, const std::string& image_path,
                                std::size_t repetitions = 1U) {
  record_common_scene_commands(renderer, image_path, repetitions);
  renderer.end_frame();
}

inline std::vector<std::uint8_t> read_pixels() {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kWidth * kHeight * 4));
  glFinish();
  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  return pixels;
}

inline std::array<std::uint8_t, 4> pixel_at(const std::vector<std::uint8_t>& pixels,
                                           int x, int y) {
  const int storage_y = kHeight - 1 - y;
  const std::size_t offset =
      static_cast<std::size_t>((storage_y * kWidth + x) * 4);
  return {pixels[offset], pixels[offset + 1U], pixels[offset + 2U],
          pixels[offset + 3U]};
}

} // namespace flex::test_support::opengl_engine
