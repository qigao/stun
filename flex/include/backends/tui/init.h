/*
 * Flex Engine - TUI Backend (using tango library)
 */

#pragma once

#include "backends/renderer.h"
#include "flex/bridge/renderer_tui.h"

#include <memory>
#include <tui.h>

namespace flex {
namespace tui_backend {

void init();
void shutdown();
bool register_backend();
std::unique_ptr<Renderer> create_renderer(tui_terminal_t* term);

} // namespace tui_backend

inline std::unique_ptr<Renderer> create_renderer(RendererBackend backend, tui_terminal_t* term) {
    if (backend == RendererBackend::TUI) {
        tui_backend::register_backend();
        return create_tui_renderer(term);
    }
    return create_renderer(backend, static_cast<CanvasHandle>(term));
}

} // namespace flex
