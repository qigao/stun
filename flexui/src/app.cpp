#include "flexui/app.h"
#include "flexui/document.h"
#include "flexui/controller.h"
#include <algorithm>
#include <chrono>

#include <nanovg.h>
#define NANOVG_GL3 1
#include <nanovg_gl.h>
#include <SDL3/SDL.h>
#include <fmtlog.h>
#include <glad/glad.h>

namespace flexui {

FlexApp::FlexApp()
{
}

FlexApp::~FlexApp()
{
  deinit();
}

bool FlexApp::init(const FlexAppConfig& config)
{
    return m_view.initialize(config);
}

void FlexApp::deinit()
{
    m_view.shutdown();
}

void FlexApp::setDocument(FlexDocument& document)
{
    m_document = &document;
    if (m_view.renderer()) {
        m_document->setRenderer(m_view.renderer());
    }

    // Notify owned controllers
    for (auto& controller : m_controllers) {
        if (controller) {
            controller->attachDocument(m_document);
        }
    }
    // Notify non-owning controller references
    for (auto* controller : m_controller_refs) {
        if (controller) {
            controller->attachDocument(m_document);
        }
    }
}

void FlexApp::setDocument(FlexDocument* document) {
    if (!document) {
        loge("FlexApp::setDocument: document must not be null");
        return;
    }
    setDocument(*document);
}

void FlexApp::addController(std::shared_ptr<FlexController> controller)
{
    if (controller) {
        m_controllers.push_back(controller);
        if (m_document) {
            controller->attachDocument(m_document);
        }
    }
}

void FlexApp::addController(FlexController& controller)
{
    m_controller_refs.push_back(&controller);
    if (m_document) {
        controller.attachDocument(m_document);
    }
}

void FlexApp::setOverlayCallback(OverlayDrawCallback callback)
{
    m_overlay_callback = callback;
}

void FlexApp::quit()
{
    m_running = false;
}

void FlexApp::run()
{
    if (!m_document) {
        loge("FlexApp: document must be set before run()");
        return;
    }

    m_running = true;
    auto last_time = std::chrono::steady_clock::now();

    // Notify before run - owned controllers
    for (auto& controller : m_controllers) {
        if (controller) controller->onBeforeRun();
    }
    // Notify before run - non-owning controller references
    for (auto* controller : m_controller_refs) {
        if (controller) controller->onBeforeRun();
    }
    onBeforeRun();

    while (m_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_running = false;
            }

            // Dispatch events to owned controllers
            for (auto& controller : m_controllers) {
                if (controller) controller->handleEvent(event);
            }
            // Dispatch events to non-owning controller references
            for (auto* controller : m_controller_refs) {
                if (controller) controller->handleEvent(event);
            }

            // Dispatch event to document (centralized dispatch)
            if (m_document) {
                m_document->handleEvent(event);
            }

            handleEvent(event);
        }

        auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        // Update owned controllers
        for (auto& controller : m_controllers) {
            if (controller) controller->update(dt);
        }
        // Update non-owning controller references
        for (auto* controller : m_controller_refs) {
            if (controller) controller->update(dt);
        }

        // Render
        m_view.render(*m_document, dt, m_overlay_callback);

        onFrame();
        onAfterFrame();
    }
}

void FlexApp::onBeforeRun() {}
void FlexApp::onFrame() {}
void FlexApp::onAfterFrame() {}
void FlexApp::handleEvent(const SDL_Event& event) { (void)event; }

} // namespace flexui
