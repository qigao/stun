#include "whiteboard/modern_whiteboard_app.h"

#include <iostream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

using namespace nanogui;

namespace {

static int whiteboard_entry(int argc, char **argv) {
  try {
    std::cout << "=== Modern Whiteboard Starting ===" << std::endl;
    std::cout << "Using LunaSVG for SVG rendering (clean RAII, no init/term needed)" << std::endl;

    std::cout << "Initializing NanoGUI..." << std::endl;
    nanogui::init();
    {
      std::cout << "Creating whiteboard application..." << std::endl;
      ref<whiteboard::ModernWhiteboardApp> app = new whiteboard::ModernWhiteboardApp();
      app->set_visible(true);

      std::cout << "Starting main loop..." << std::endl;
      nanogui::run(nanogui::RunMode::VSync);
    }

    std::cout << "Shutting down NanoGUI..." << std::endl;
    nanogui::shutdown();
    std::cout << "=== Modern Whiteboard closed cleanly ===" << std::endl;

  } catch (const std::exception &e) {
    std::string message = std::string("Fatal error: ") + e.what();
    std::cerr << message << std::endl;
#if defined(_WIN32)
    MessageBoxA(nullptr, message.c_str(), "Modern Whiteboard", MB_OK | MB_ICONERROR);
#else
    std::cerr << message << std::endl;
#endif
    return -1;
  } catch (...) {
    std::cerr << "Fatal error: unknown exception" << std::endl;
#if defined(_WIN32)
    MessageBoxA(nullptr, "Fatal error: unknown exception", "Modern Whiteboard",
                MB_OK | MB_ICONERROR);
#else
    std::cerr << "Fatal error: unknown exception" << std::endl;
#endif
    return -1;
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) { return whiteboard_entry(argc, argv); }
