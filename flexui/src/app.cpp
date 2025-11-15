#include "flexui/app.h"

#include <SDL3/SDL.h>
#include <fmtlog.h>
#include <nanovg_css.h>

#include <chrono>
#include <utility>

namespace flexui {

bool FlexApp::init(const FlexAppConfig &config) {
  m_config = config;
  if (!m_view.initialize(config.view)) {
    return false;
  }

  if (m_document && m_view.renderer()) {
    m_document->setRenderer(m_view.renderer());
  }

  for (auto &controller : m_controllers) {
    controller->attachDocument(m_document.get());
  }

  return true;
}

void FlexApp::setDocument(std::shared_ptr<FlexDocument> document) {
  m_document = std::move(document);
  if (m_document && m_view.renderer()) {
    m_document->setRenderer(m_view.renderer());
  }

  for (auto &controller : m_controllers) {
    controller->attachDocument(m_document.get());
  }
}

void FlexApp::addController(
    const std::shared_ptr<Flex> &controller) {
  m_controllers.push_back(controller);
  controller->attachDocument(m_document.get());
}

void FlexApp::setOverlayCallback(OverlayDrawCallback callback) {
  m_overlay_callback = std::move(callback);
}

void FlexApp::run() {
  if (!m_document) {
    loge("FlexApp: document must be set before run()");
    return;
  }

  for (auto &controller : m_controllers) {
    controller->onBeforeRun();
  }

  bool running = true;
  auto last_time = std::chrono::steady_clock::now();

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT && m_config.quit_on_close) {
        running = false;
      }

      for (auto &controller : m_controllers) {
        controller->handleEvent(event);
      }
    }

    auto now = std::chrono::steady_clock::now();
    const float dt =
        std::chrono::duration<float>(now - last_time).count();
    last_time = now;

    for (auto &controller : m_controllers) {
      controller->update(dt);
    }

    if (m_view.renderer()) {
      nvgcssUpdate(m_view.renderer(), dt);
    }

    m_view.render(*m_document, m_overlay_callback);
  }
}

} // namespace flexui
