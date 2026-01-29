/*
 * TUI - Terminal User Interface Library
 * 
 * A lightweight, zero-dependency TUI library for modern terminals.
 * Inspired by notcurses, bubbletea, and the Elm Architecture.
 * 
 * Features:
 * - Cell-based rendering (no pixel rasterization)
 * - Double-buffered differential updates (no flicker)
 * - True color (24-bit RGB)
 * - Unicode support (emoji, CJK, box drawing)
 * - Mouse input (SGR 1006 protocol)
 * - Cross-platform (Windows, Linux, macOS)
 * - Elm Architecture (TEA) application framework
 * - Built-in widgets
 * 
 * Usage:
 *   #include <tui.h>
 *   
 *   struct Model { int count = 0; };
 *   enum class Msg { Inc, Dec, Quit };
 *   
 *   int main() {
 *       auto app = tui::App<Model, Msg>::create(
 *           Model{},
 *           [](Msg msg, Model& m) {
 *               if (msg == Msg::Quit) return false;
 *               if (msg == Msg::Inc) m.count++;
 *               if (msg == Msg::Dec) m.count--;
 *               return true;
 *           },
 *           [](const Model& m, tui::Buffer& buf) {
 *               buf.clear(tui::Color{30, 30, 30});
 *               buf.text(2, 2, "Count: " + std::to_string(m.count));
 *           },
 *           [](const tui::Event& e) -> std::optional<Msg> {
 *               if (e.is('q')) return Msg::Quit;
 *               if (e.is('+')) return Msg::Inc;
 *               if (e.is('-')) return Msg::Dec;
 *               return std::nullopt;
 *           }
 *       );
 *       app.run();
 *   }
 */

#pragma once

#include "tui/cell.h"
#include "tui/buffer.h"
#include "tui/terminal.h"
#include "tui/input.h"
#include "tui/widgets.h"
#include "tui/app.h"
