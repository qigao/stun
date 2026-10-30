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

// Refit one immutable C11 cell grid onto the host's REAL current logical
// window and framebuffer (positive resize). This profile deliberately keeps
// the existing grid count/grapheme/span, glyph font and renderer: no second
// layout or Unicode parser. Uniform device pixel ratio is mandatory; a
// transient nonuniform/mismatched host generation fails before drawing.
// Font/baseline are scaled by the cell-height change; exact GPU glyph preflight
// still rejects unrepresentable or overflowing text. Do not claim physical
// monitor HiDPI evidence from synthetic CPU metrics.
CellPixelMetrics fit_cell_metrics_to_window(
    const tui_grid_snapshot_t* snapshot,
    const CellWindowExtents& observed,
    const CellPixelMetrics& base);

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


// Synchronous host focus/resize/input admission. A successful GPU frame is
// NOT automatically an input frame: its host must have focus and explicitly
// publish the exact observed window/framebuffer metrics AFTER the host swap.
// Any focus transition, resize callback, silently changed host dimensions,
// renderer failure, or unacknowledged repaint retires input without changing
// the renderer or inventing an event queue. This is not an IME model.
class CellHostFrameGate final {
public:
    explicit CellHostFrameGate(CellPixelPresenter& presenter) noexcept;
    CellHostFrameGate(const CellHostFrameGate&) = delete;
    CellHostFrameGate& operator=(const CellHostFrameGate&) = delete;

    // UI-owner-only; either focus transition requires a fresh successful
    // present/swap before any key/character/pointer can be admitted.
    void set_focus(bool focused);
    // Call on ANY logical window, framebuffer or DPI generation change.
    void invalidate_geometry();

    // After a real present+swap, bind input to this EXACT successful submission,
    // grid revision and same-generation native host sizes. A missing frame or
    // incoherent framebuffer size fails, never accepting an old frame.
    void accept_presented_frame(const CellWindowExtents& observed);

    std::optional<CellHostInput> pointer(
        double x, double y, const CellWindowExtents& current,
        CellMouseAction action, CellMouseButton button, std::uint8_t modifiers);
    std::optional<CellHostInput> key(
        tui_key_t key, std::uint8_t modifiers,
        const CellWindowExtents& current);
    std::optional<CellHostInput> character(
        std::uint32_t scalar, const CellWindowExtents& current);

private:
    void require_owner() const;
    bool admitted_for(const CellWindowExtents& current);
    CellPixelPresenter* presenter_ = nullptr;
    CellWindowExtents accepted_{};
    std::uint64_t revision_ = 0;
    std::uint64_t submission_ = 0;
    bool focused_ = false;
    bool admitted_ = false;
};

} // namespace flex::tango_opengl
