/*
 * Flex Engine - OpenGL + GLFW Demo
 *
 * Minimal end-to-end example for the OpenGL backend.
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>

#include <flex.h>
#include "backends/opengl/init.h"

namespace {

bool load_default_font() {
  if (flex::opengl_backend::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
    return true;
  }
  return flex::opengl_backend::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
}

void start_demo_animations(flex::Instance &instance) {
  static const char *kAnimations[] = {
      "spin",
      "pulse",
      "bounce1",
      "bounce2",
      "bounce3",
      "bounce4",
      "progressFill",
      "squareSpin",
      "fadeWave1",
      "fadeWave2",
      "fadeWave3",
      "scalePulse",
  };

  for (const char *name : kAnimations) {
    instance.play_animation(name);
  }
}

} // namespace

int main(int argc, char **argv) {
  const char *scene_path = argc > 1 ? argv[1] : "loading_animation.flex";

  if (!glfwInit()) {
    std::cerr << "GLFW init failed\n";
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(900, 640, "Flex OpenGL Demo", nullptr, nullptr);
  if (!window) {
    std::cerr << "Window creation failed\n";
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    std::cerr << "GLAD init failed\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  flex::opengl_backend::init();
  flex::opengl_backend::register_backend();
  load_default_font();

  std::cout << "Loading scene: " << scene_path << '\n';

  auto definition = flex::Definition::load_file(scene_path);
  if (!definition || definition->has_error()) {
    std::cerr << "Failed to load " << scene_path << ": "
              << (definition ? definition->error_message() : "null definition") << '\n';
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  std::string title = "Flex OpenGL Demo - ";
  title += std::filesystem::path(scene_path).stem().string();
  glfwSetWindowTitle(window, title.c_str());

  auto instance = flex::Instance::create(definition);
  flex::opengl_backend::OpenGLCanvas canvas;
  canvas.get_proc_address = [](void *, const char *name) {
    return reinterpret_cast<flex::opengl_backend::OpenGLProcAddress>(
        glfwGetProcAddress(name));
  };
  auto renderer = flex::create_renderer(flex::RendererBackend::OpenGL, &canvas);
  if (!instance || !renderer) {
    std::cerr << "Failed to create Flex instance or OpenGL renderer\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  start_demo_animations(*instance);

  auto last = std::chrono::steady_clock::now();

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last).count();
    last = now;

    int fbw = 0;
    int fbh = 0;
    int winw = 0;
    int winh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &winw, &winh);
    if (fbw <= 0 || fbh <= 0 || winw <= 0 || winh <= 0) {
      continue;
    }

    const float pixel_ratio = static_cast<float>(fbw) / static_cast<float>(winw);
    glViewport(0, 0, fbw, fbh);

    instance->advance(dt);

    renderer->begin_frame(static_cast<float>(winw), static_cast<float>(winh), pixel_ratio);
    if (instance->scene()) {
      renderer->clear(instance->scene()->background());
    } else {
      renderer->clear(flex::Color{0.10f, 0.11f, 0.14f, 1.0f});
    }
    instance->render(*renderer);
    renderer->end_frame();

    glfwSwapBuffers(window);
  }

  renderer.reset();
  flex::opengl_backend::shutdown();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
