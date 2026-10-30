/*
 * TangoOpenGL host callbacks -> canonical C11 tui_event_t.
 * These are synchronous owner-thread translations of the LAST submitted
 * pixel frame. They do not create an input queue, parser, or second UI model.
 */
#pragma once

#include <backends/tango_opengl/cell_presenter.h>
#include <tui.h>

#include <cstdint>
#include <optional>

namespace flex::tango_opengl {

struct CellWindowExtents final {
    int window_width = 0;
    int window_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
};

enum class CellMouseAction : std::uint8_t {
    Press, Release, Move, WheelUp, WheelDown
};

enum class CellMouseButton : std::uint8_t {
    None, Left, Middle, Right
};

struct CellHostInput final {
    // The SAME Tango C11 event type used by the native TTY/ConPTY path.
    // Mouse x/y name the physical cell, not a guessed grapheme/text offset.
    tui_event_t event{};
    // Sidecar identity/normalization for the EXACT displayed GPU frame.
    std::uint64_t grid_revision = 0;
    int leading_column = -1;
    bool continuation = false;
};

// GLFW and other window hosts pass observed same-generation dimensions;
// no GLFW header or second event queue is needed in this module. A missing
// frame or out-of-bounds pointer produces no event. Wrong owner, unsupported
// event/modifiers, or incoherent resize metrics fail explicitly.
std::optional<CellHostInput> cell_input_pointer(
    const CellPixelPresenter& presenter, double window_x, double window_y,
    const CellWindowExtents& extents, CellMouseAction action,
    CellMouseButton button, std::uint8_t modifiers);

// Navigation/control key-down only. Printable text comes separately from
// the host's committed Unicode character callback (no key/text duplicates).
// No key-up or IME-preedit contract is claimed.
std::optional<CellHostInput> cell_input_key(
    const CellPixelPresenter& presenter, tui_key_t key,
    std::uint8_t modifiers);

// A host-provided committed Unicode scalar, not a new UTF-8 parser.
// Unpaired surrogates, C0/C1 controls, DEL and out-of-range values are
// refused. IME composition/preedit is NOT represented by this API.
std::optional<CellHostInput> cell_input_character(
    const CellPixelPresenter& presenter, std::uint32_t scalar);

} // namespace flex::tango_opengl
