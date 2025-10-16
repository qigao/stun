#include "adsorbing_window.h"
#include "oscilloscope_module.h"
#include "properties_panel_module.h"
#include "speedometer_module.h"
#include "speedometer_rack.h"
#include "style_panel_module.h"
#include "text_panel_module.h"
#include "toolbar_panel_module.h"
#include "vcv_module_panel.h"
#include "wasp_filter_module.h"
#include "window_adsorption.h"
#include <chrono>
#include <iostream>
#include <nanogui.h>
#include <nanogui/opengl.h>

using namespace nanogui;

class VCVRackApp : public Screen {
public:
  VCVRackApp() : Screen(Vector2i(1600, 900), "VCV Rack Style Modules") {
    inc_ref();
    std::cout << "VCVRackApp constructor called" << std::endl;

    set_background(Color(22, 25, 38, 255));

    // Initialize window adsorption manager
    m_adsorption = new WindowAdsorption(20);

    // Main modular rack window
    m_mainWindow = new AdsorbingWindow(this, "");
    m_mainWindow->set_position(Vector2i(15, 15));
    m_mainWindow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 10));
    m_mainWindow->setAdsorptionManager(m_adsorption);

    std::cout << "Creating VCV module panels..." << std::endl;
    m_module1 = new VCVModulePanel(m_mainWindow);
    m_waspFilter = new WaspFilterModule(m_mainWindow);
    m_module2 = new VCVModulePanel(m_mainWindow);

    // Add example patch cables
    m_module1->m_cables.push_back({2, 0, nvgRGBA(255, 200, 80, 255)});
    m_module2->m_cables.push_back({2, 0, nvgRGBA(100, 255, 100, 255)});

    // Separate window for oscilloscope
    m_scopeWindow = new AdsorbingWindow(this, "");
    m_scopeWindow->set_position(Vector2i(800, 15));
    m_scopeWindow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 10));
    m_scopeWindow->setAdsorptionManager(m_adsorption);
    m_scope = new OscilloscopeModule(m_scopeWindow);

    // Separate window for speedometer rack
    m_speedometerWindow = new AdsorbingWindow(this, "");
    m_speedometerWindow->set_position(Vector2i(15, 450));
    m_speedometerWindow->set_layout(
        new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 10));
    m_speedometerWindow->setAdsorptionManager(m_adsorption);
    m_speedometerRack = new SpeedometerRack(m_speedometerWindow);

    // Toolbar panel - directly on screen (no window border)
    m_toolbar = new ToolbarPanelModule(this);
    m_toolbar->set_position(Vector2i(400, 700));

    // Properties panel - directly on screen (no window border)
    m_propertiesPanel = new PropertiesPanelModule(this);
    m_propertiesPanel->set_position(Vector2i(1200, 50));

    // Style panel - directly on screen (no window border)
    m_stylePanel = new StylePanelModule(this);
    m_stylePanel->set_position(Vector2i(50, 50));

    // Text panel - directly on screen (no window border)
    m_textPanel = new TextPanelModule(this);
    m_textPanel->set_position(Vector2i(450, 50));

    // Register windows for adsorption
    m_adsorption->registerWindow(m_mainWindow);
    m_adsorption->registerWindow(m_scopeWindow);
    m_adsorption->registerWindow(m_speedometerWindow);

    std::cout << "Performing layout..." << std::endl;
    perform_layout();
    std::cout << "VCVRackApp initialized successfully" << std::endl;
  }

  void draw_all() override {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;

    if (m_scope) {
      m_scope->update(dt);
    }

    if (m_speedometerRack) {
      m_speedometerRack->update(dt);
    }

    Screen::draw_all();
  }

  void cleanup_resources() {
    std::cout << "Cleaning up resources..." << std::endl;

    // Clear all children to trigger cleanup of windows and their child widgets
    m_children.clear();

    // Clear all widget pointers
    m_textPanel = nullptr;
    m_stylePanel = nullptr;
    m_propertiesPanel = nullptr;
    m_toolbar = nullptr;
    m_speedometerWindow = nullptr;
    m_speedometerRack = nullptr;
    m_scopeWindow = nullptr;
    m_scope = nullptr;
    m_mainWindow = nullptr;
    m_module1 = nullptr;
    m_waspFilter = nullptr;
    m_module2 = nullptr;

    std::cout << "Resources cleanup complete" << std::endl;
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      set_visible(false);
      return true;
    }
    return false;
  }

private:
  OscilloscopeModule *m_scope = nullptr;
  SpeedometerRack *m_speedometerRack = nullptr;
  ToolbarPanelModule *m_toolbar = nullptr;
  PropertiesPanelModule *m_propertiesPanel = nullptr;
  StylePanelModule *m_stylePanel = nullptr;
  TextPanelModule *m_textPanel = nullptr;
  WaspFilterModule *m_waspFilter = nullptr;
  VCVModulePanel *m_module1 = nullptr;
  VCVModulePanel *m_module2 = nullptr;
  AdsorbingWindow *m_mainWindow = nullptr;
  AdsorbingWindow *m_scopeWindow = nullptr;
  AdsorbingWindow *m_speedometerWindow = nullptr;
  WindowAdsorption *m_adsorption = nullptr;
};

static int vcv_entry(int argc, char **argv) {
  try {
    std::cout << "=== VCV Rack Style Module Demo ===" << std::endl;

    std::cout << "Initializing NanoGUI..." << std::endl;
    nanogui::init();
    {
      std::cout << "Creating application window..." << std::endl;
      nanogui::ref<VCVRackApp> app = new VCVRackApp();
      app->dec_ref();
      app->set_visible(true);
      std::cout << "Starting main loop..." << std::endl;
      nanogui::run(nanogui::RunMode::VSync);
      app->cleanup_resources();
 
    }
    std::cout << "Shutting down NanoGUI..." << std::endl;
    nanogui::shutdown();

    std::cout << "=== Application exited cleanly ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  return vcv_entry(argc, argv);

}
