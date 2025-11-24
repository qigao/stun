#pragma once

#include "flexui/fwd.h"
#include "flexui/document.h"
#include "flexui/controller.h"
#include "flexui/view.h"

#include <nanovg.h>
#include <SDL3/SDL.h>
#include <vector>
#include <memory>

namespace flexui {

struct FlexAppConfig : public FlexViewConfig {
    // Add app-specific config if needed
};

class FlexApp {
public:
  FlexApp();
  virtual ~FlexApp();

  // Configuration and Initialization
  virtual bool init(const FlexAppConfig& config);
  virtual void deinit();

  // Main Loop
  virtual void run();
  void quit(); // Request to quit the app

  // View Access
  FlexView& view() { return m_view; }
  const FlexView& view() const { return m_view; }

  // Document Management
  // Ownership: FlexApp does NOT own the document. Caller manages lifetime.
  // Document must remain valid for the lifetime of FlexApp usage.
  void setDocument(FlexDocument& document);
  void setDocument(FlexDocument* document); // Convenience overload, document must not be null

  // Controller Management
  // Ownership: FlexApp owns controllers added via shared_ptr
  void addController(std::shared_ptr<FlexController> controller);
  // Non-owning reference - caller manages controller lifetime
  void addController(FlexController& controller);

  // Callbacks
  void setOverlayCallback(OverlayDrawCallback callback);

protected:
  virtual void onBeforeRun();
  virtual void onFrame();
  virtual void onAfterFrame();
  virtual void handleEvent(const SDL_Event& event);

private:
  FlexView m_view;
  FlexDocument* m_document = nullptr;  // Non-owning pointer, caller manages lifetime
  std::vector<std::shared_ptr<FlexController>> m_controllers;
  std::vector<FlexController*> m_controller_refs;  // Non-owning references
  OverlayDrawCallback m_overlay_callback;
  bool m_running = false;
};

} // namespace flexui
