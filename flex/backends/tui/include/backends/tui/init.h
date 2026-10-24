/*
 * Flex Engine - TUI Backend (using tango library)
 */

#pragma once

#include "flex/bridge/renderer.h"
#include "flex/bridge/renderer_tui.h"

#include <memory>
#include <tui.h>

namespace flex {
namespace tui_backend {

void init();
void shutdown();
bool register_backend();
std::unique_ptr<Renderer> create_renderer(tui_terminal_t* term);
// Headless owner-supplied terminal-cell target. The borrowed buffer must
// outlive the renderer; no native TTY/console access is required.
std::unique_ptr<Renderer> create_buffer_renderer(tui_buffer_t* buffer);

} // namespace tui_backend
} // namespace flex
