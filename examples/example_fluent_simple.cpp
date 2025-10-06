/*
    examples/example_fluent_simple.cpp -- Simple Fluent Design demo

    A minimal Fluent Design example that works with both GLFW and SDL3 backends.
    Demonstrates the core Fluent Design principles with basic controls,
    SVG rendering, and Lottie animations.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>
#include <chrono>
#include <nanogui/fluent_theme.h>
#include <nanogui/nanogui.h>
#include <nanogui/imageview.h>
#include <lunasvg.h>
#include <rlottie.h>
#include <rlottie.h>
#include <memory>
#include <chrono>


// Backend-agnostic key codes
#if defined(NANOGUI_USE_SDL3)
  #include <SDL3/SDL.h>
  #define KEY_ESCAPE SDLK_ESCAPE
  #define KEY_PRESS 1
#else
  #include <GLFW/glfw3.h>
  #define KEY_ESCAPE GLFW_KEY_ESCAPE
  #define KEY_PRESS GLFW_PRESS
#endif

using namespace nanogui;

// Custom widget to display Lottie animation using ImageView
class LottieWidget : public ImageView {
public:
  LottieWidget(Widget *parent, const std::string &file_path) : ImageView(parent) {
    // Load JSON from file
    std::ifstream file(file_path);
    if (!file.is_open()) {
      std::cerr << "Warning: Failed to open Lottie file: " << file_path << std::endl;
      return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_data = buffer.str();
    
    m_animation = rlottie::Animation::loadFromData(json_data, "");
    if (!m_animation) {
      std::cerr << "Warning: Failed to parse Lottie animation" << std::endl;
      return;
    }
    
    size_t w, h;
    m_animation->size(w, h);
    set_fixed_size(Vector2i(120, 120));
    m_frame_count = m_animation->totalFrame();
    
    if (m_frame_count == 0) {
      std::cerr << "Warning: Lottie animation has 0 frames" << std::endl;
      return;
    }
    
    m_start_time = std::chrono::steady_clock::now();
    m_buffer.resize(100 * 100);
    
    m_texture = new Texture(
      Texture::PixelFormat::RGBA,
      Texture::ComponentFormat::UInt8,
      Vector2i(100, 100),
      Texture::InterpolationMode::Nearest,
      Texture::InterpolationMode::Nearest
    );
    set_image(m_texture);
    reset();
    
    std::cout << "Lottie animation loaded: " << m_frame_count << " frames" << std::endl;
  }

  virtual void draw(NVGcontext *ctx) override {
    if (m_animation && m_frame_count > 0) {
      // Calculate current frame based on time
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time).count();
      size_t frame = (elapsed / 16) % m_frame_count; // ~60fps

      // Render frame to buffer
      rlottie::Surface surface((uint32_t*)m_buffer.data(), 100, 100, 100 * 4);
      m_animation->renderSync(frame, surface);
      
      // Upload to texture
      m_texture->upload((uint8_t*)m_buffer.data());
    }
    
    ImageView::draw(ctx);
  }

private:
  std::unique_ptr<rlottie::Animation> m_animation;
  std::vector<uint32_t> m_buffer;
  size_t m_frame_count = 0;
  std::chrono::steady_clock::time_point m_start_time;
  ref<Texture> m_texture;
};

// Custom widget to display SVG using ImageView
class SVGWidget : public ImageView {
public:
  SVGWidget(Widget *parent, const std::string &svg_data) : ImageView(parent) {
    m_document = lunasvg::Document::loadFromData(svg_data);
    if (m_document) {
      set_fixed_size(Vector2i(120, 120));
      update_texture();
    }
  }

  void update_texture() {
    if (!m_document) return;
    
    auto bitmap = m_document->renderToBitmap(100, 100);
    if (!bitmap.valid()) return;

    m_texture = new Texture(
      Texture::PixelFormat::RGBA,
      Texture::ComponentFormat::UInt8,
      Vector2i(100, 100),
      Texture::InterpolationMode::Nearest,
      Texture::InterpolationMode::Nearest
    );
    m_texture->upload(bitmap.data());
    set_image(m_texture);
    reset();
  }

private:
  std::unique_ptr<lunasvg::Document> m_document;
  ref<Texture> m_texture;
};

class SimpleFluentApp : public Screen {
public:
  SimpleFluentApp() : Screen(Vector2i(800, 600), "Fluent Design - Simple Demo") {
    // Apply Fluent theme
    auto *theme = new FluentTheme(nvg_context(), FluentTheme::Palette::Light);
    set_theme(theme);

    // Main window
    Window *window = new Window(this, "Fluent Controls");
    window->set_position(Vector2i(20, 20));
    window->set_layout(new GroupLayout(10));

    // Title
    new Label(window, "Fluent Design System", "sans-bold", 20);
    new Label(window, "Modern Microsoft design language", "sans", 14);

    // Buttons
    new Label(window, "Buttons:", "sans-bold");

    Button *primary = new Button(window, "Primary Button");
    primary->set_background_color(Color(0, 120, 212, 255));
    primary->set_text_color(Color(255, 255, 255, 255));
    primary->set_callback([] { std::cout << "Primary button clicked!" << std::endl; });

    Button *secondary = new Button(window, "Secondary Button");
    secondary->set_callback([] { std::cout << "Secondary button clicked!" << std::endl; });

    // Checkbox
    new Label(window, "Options:", "sans-bold");

    CheckBox *cb = new CheckBox(window, "Enable feature");
    cb->set_callback([](bool checked) {
      std::cout << "Feature " << (checked ? "enabled" : "disabled") << std::endl;
    });

    // Slider
    new Label(window, "Volume:", "sans-bold");

    Slider *slider = new Slider(window);
    slider->set_value(0.5f);
    slider->set_callback(
        [](float value) { std::cout << "Volume: " << (int)(value * 100) << "%" << std::endl; });

    // Theme toggle
    new Label(window, "Theme:", "sans-bold");

    auto *theme_panel = new Widget(window);
    theme_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));

    Button *light = new Button(theme_panel, "Light");
    light->set_callback([this] {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Light));
      std::cout << "Switched to Light theme" << std::endl;
    });

    Button *dark = new Button(theme_panel, "Dark");
    dark->set_callback([this] {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Dark));
      std::cout << "Switched to Dark theme" << std::endl;
    });

    perform_layout();

    // SVG & Animation window
    Window *media_window = new Window(this, "SVG & Animations");
    media_window->set_position(Vector2i(640, 20));
    media_window->set_layout(new GroupLayout(10));

    new Label(media_window, "SVG Graphics", "sans-bold", 18);
    new Label(media_window, "Rendered with lunasvg", "sans", 12);

    // SVG Demo 1
    new Label(media_window, "Checkmark Icon:", "sans-bold");
    
    std::string svg_icon = R"(
      <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
        <circle cx="50" cy="50" r="40" fill="#0078D4" />
        <path d="M 30 50 L 45 65 L 70 35" stroke="white" stroke-width="5" 
              fill="none" stroke-linecap="round" stroke-linejoin="round"/>
      </svg>
    )";
    
    new SVGWidget(media_window, svg_icon);

    // SVG Demo 2
    new Label(media_window, "Success Badge:", "sans-bold");
    
    std::string svg_icon2 = R"(
      <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
        <rect x="10" y="10" width="80" height="80" rx="10" fill="#107C10" />
        <text x="50" y="65" font-size="48" fill="white" text-anchor="middle" font-family="sans-serif">✓</text>
      </svg>
    )";
    
    new SVGWidget(media_window, svg_icon2);

    // Third SVG example
    new Label(media_window, "Warning Icon:", "sans-bold");
    
    std::string svg_icon3 = R"(
      <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
        <polygon points="50,10 90,80 10,80" fill="#FFB900" />
        <text x="50" y="70" font-size="40" fill="black" text-anchor="middle" font-family="sans-serif" font-weight="bold">!</text>
      </svg>
    )";
    
    new SVGWidget(media_window, svg_icon3);

    // Lottie Animation Demo
    new Label(media_window, "Lottie Animation:", "sans-bold");
    new LottieWidget(media_window, "resources/lottie/spinner.json");

    perform_layout();

    std::cout << "\n=== Fluent Design Demo ===" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  • Fluent Design controls" << std::endl;
    std::cout << "  • Theme switcher (Light/Dark)" << std::endl;
    std::cout << "  • SVG vector graphics (lunasvg)" << std::endl;
    std::cout << "  • Lottie animations (rlottie)" << std::endl;
    std::cout << "Press ESC to exit\n" << std::endl;
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;

    if (key == KEY_ESCAPE && action == KEY_PRESS) {
      set_visible(false);
      return true;
    }

    return false;
  }
};

int main(int argc, char **argv) {
  try {
    nanogui::init();

    {
      ref<SimpleFluentApp> app = new SimpleFluentApp();
      app->set_visible(true);

#if defined(NANOGUI_USE_SDL3)
      // SDL3 event loop
      while (app->process_events()) {
        app->draw_all();
      }
#else
      // GLFW event loop
      nanogui::mainloop();
#endif
    }

    nanogui::shutdown();
    std::cout << "✓ Demo completed!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "✗ Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
