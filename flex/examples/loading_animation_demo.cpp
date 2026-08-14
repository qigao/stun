#include <gcanvas/window.hpp>

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <flex.h>
#include <flex/render/engines/gcanvas.h>

#ifndef FLEX_EXAMPLE_DIR
#define FLEX_EXAMPLE_DIR "."
#endif

namespace {

constexpr int kInitialWidth = 800;
constexpr int kInitialHeight = 600;
constexpr int kSmokeFrames = 2;

struct Options {
  gcanvas::Backend backend = gcanvas::Backend::OpenGL;
  std::filesystem::path scene =
      std::filesystem::path(FLEX_EXAMPLE_DIR) / "loading_animation.flex";
  bool smoke = false;
};

Options parse_options(int argc, char **argv) {
  Options options;
  bool backend_selected = false;
  bool scene_selected = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (!backend_selected && argument == "opengl") {
      options.backend = gcanvas::Backend::OpenGL;
      backend_selected = true;
    } else if (!backend_selected && argument == "vulkan") {
      options.backend = gcanvas::Backend::Vulkan;
      backend_selected = true;
    } else if (argument == "--smoke") {
      options.smoke = true;
    } else if (!scene_selected && !argument.empty() && argument.front() != '-') {
      options.scene = argument;
      scene_selected = true;
    } else {
      throw std::invalid_argument(
          "usage: loading_animation_demo [opengl|vulkan] [scene.flex] [--smoke]");
    }
  }
  return options;
}

void start_animations(flex::Instance &instance) {
  static constexpr const char *kAnimations[] = {
      "spin",       "pulse",       "bounce1",   "bounce2",
      "bounce3",    "bounce4",     "progressFill", "squareSpin",
      "fadeWave1",  "fadeWave2",   "fadeWave3", "scalePulse",
  };
  for (const char *name : kAnimations) {
    if (instance.play_animation(name) == nullptr) {
      throw std::runtime_error(std::string("missing loading animation: ") + name);
    }
  }
}

void register_default_font(flex::Renderer &renderer) {
#ifdef _WIN32
  static const std::filesystem::path kFontCandidates[] = {
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/arial.ttf",
  };
  for (const auto &path : kFontCandidates) {
    if (std::filesystem::exists(path)) {
      renderer.register_font("sans-serif", path.string());
      return;
    }
  }
#else
  (void)renderer;
#endif
}

} // namespace

int main(int argc, char **argv) {
  try {
    const Options options = parse_options(argc, argv);
    const std::string scene_path = options.scene.string();
    auto definition = flex::Definition::load_file(scene_path.c_str());
    if (!definition || definition->has_error()) {
      throw std::runtime_error(
          definition ? definition->error_message() : "Flex definition allocation failed");
    }
    auto instance = flex::Instance::create(std::move(definition));
    if (!instance || !instance->scene()) {
      throw std::runtime_error("Flex loading animation scene creation failed");
    }

    gcanvas::WindowConfig config{"Flex loading animation on gCanvas", kInitialWidth,
                                 kInitialHeight};
    config.backend = options.backend;
    config.visible = !options.smoke;
    config.vsync = !options.smoke;
    config.native_pixel_size = true;
    auto window = gcanvas::Window::create(config);
    gcanvas::Context &context = window->create_context();
    auto renderer = flex::render::engines::gcanvas::create_renderer(context);
    register_default_font(*renderer);
    start_animations(*instance);

    auto previous = std::chrono::steady_clock::now();
    int frame_count = 0;
    while (window->is_running() && (!options.smoke || frame_count < kSmokeFrames)) {
      window->poll_events();
      const auto now = std::chrono::steady_clock::now();
      const float delta = std::chrono::duration<float>(now - previous).count();
      previous = now;
      instance->advance(delta);

      const float width = static_cast<float>(context.get_width());
      const float height = static_cast<float>(context.get_height());
      instance->scene()->set_size(width, height);
      renderer->begin_frame(width, height, 1.0f);
      renderer->clear(instance->scene()->background());
      instance->render(*renderer);
      renderer->end_frame();
      context.present_frame();
      ++frame_count;
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "loading_animation_demo failed: " << error.what() << '\n';
    return 1;
  }
}
