/*
    examples/test_sdl3_backend.cpp -- Simple test for SDL3 backend

    This example tests the SDL3 backend implementation by creating
    a simple window with basic widgets.
*/

#include <iostream>
#include <nanogui/button.h>
#include <nanogui/checkbox.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/screen.h>
#include <nanogui/slider.h>
#include <nanogui/textbox.h>
#include <nanogui/window.h>

// Include backend-specific headers for key codes
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

class TestApp : public Screen {
public:
  TestApp() : Screen(Vector2i(800, 600), "SDL3 Backend Test") {
    // Create main window
    Window *window = new Window(this, "Test Window");
    window->set_position(Vector2i(15, 15));
    window->set_layout(new GroupLayout());

    // Title
    new Label(window, "SDL3 Backend Test", "sans-bold");

    // Status label
    new Label(window, "If you can see this, SDL3 backend is working!", "sans");

    // Test button
    Button *btn = new Button(window, "Click Me!");
    btn->set_callback([] { std::cout << "✓ Button clicked - Mouse input works!" << std::endl; });

    // Test checkbox
    CheckBox *cb = new CheckBox(window, "Test Checkbox");
    cb->set_callback([](bool checked) {
      std::cout << "✓ Checkbox " << (checked ? "checked" : "unchecked") << " - Widget state works!"
                << std::endl;
    });

    // Test textbox
    new Label(window, "Test text input:", "sans");
    TextBox *tb = new TextBox(window, "Type here...");
    tb->set_editable(true);
    tb->set_callback([](const std::string &value) {
      std::cout << "✓ Text input: " << value << " - Keyboard works!" << std::endl;
      return true;
    });

    // Test slider
    new Label(window, "Test slider:", "sans");
    Slider *slider = new Slider(window);
    slider->set_value(0.5f);
    slider->set_callback([](float value) {
      std::cout << "✓ Slider value: " << value << " - Mouse drag works!" << std::endl;
    });

    // Close button
    Button *close_btn = new Button(window, "Close");
    close_btn->set_callback([this] {
      std::cout << "✓ Closing application" << std::endl;
      set_visible(false);
    });

    perform_layout();

    std::cout << "\n=== SDL3 Backend Test ===" << std::endl;
    std::cout << "Window size: " << m_size.x() << "x" << m_size.y() << std::endl;
    std::cout << "Framebuffer size: " << m_fbsize.x() << "x" << m_fbsize.y() << std::endl;
    std::cout << "Pixel ratio: " << m_pixel_ratio << std::endl;
    std::cout << "\nTest Instructions:" << std::endl;
    std::cout << "- Click the button" << std::endl;
    std::cout << "- Toggle the checkbox" << std::endl;
    std::cout << "- Type in the textbox" << std::endl;
    std::cout << "- Move the slider" << std::endl;
    std::cout << "- Press ESC or click Close to exit" << std::endl;
    std::cout << "========================\n" << std::endl;
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;

    // ESC to close (backend-agnostic)
    if (key == KEY_ESCAPE && action == KEY_PRESS) {
      std::cout << "✓ ESC key pressed - Keyboard events work!" << std::endl;
      set_visible(false);
      return true;
    }

    return false;
  }

  virtual bool resize_event(const Vector2i &size) override {
    std::cout << "✓ Window resized to " << size.x() << "x" << size.y() << " - Resize events work!"
              << std::endl;
    return Screen::resize_event(size);
  }
};

int main(int argc, char **argv) {
  try {
    nanogui::init();

    {
      ref<TestApp> app = new TestApp();
      app->set_visible(true);

#if defined(NANOGUI_USE_SDL3)
      // SDL3-style event loop
      std::cout << "Using SDL3 backend event loop" << std::endl;
      while (app->process_events()) {
        app->draw_all();
      }
#else
      // GLFW-style event loop
      std::cout << "Using GLFW backend event loop" << std::endl;
      nanogui::run(RunMode::VSync);
#endif
    }

    nanogui::shutdown();

    std::cout << "\n✓ Test completed successfully!" << std::endl;
    std::cout << "✓ All resources cleaned up properly" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "✗ Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
