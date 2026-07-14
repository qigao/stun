/*
 * Flex Engine - TUI Renderer Factory
 */

#pragma once

#include "flex/core/renderer.h"

#include <memory>

struct tui_terminal;
typedef struct tui_terminal tui_terminal_t;

namespace flex {

std::unique_ptr<Renderer> create_tui_renderer(CanvasHandle terminal);
std::unique_ptr<Renderer> create_tui_renderer(tui_terminal_t* term);

} // namespace flex
