#include <backends/tango_opengl/cell_input.h>

#include <stdexcept>
#include <cmath>
#include <limits>

namespace flex::tango_opengl {
namespace {

void require_owner(const CellPixelPresenter& presenter) {
    if (!presenter.on_owner_thread())
        throw std::logic_error("TangoOpenGL host input must run on its GL/UI owner");
}

void validate_modifiers(std::uint8_t modifiers) {
    constexpr auto supported = static_cast<std::uint8_t>(
        TUI_MOD_SHIFT | TUI_MOD_CTRL | TUI_MOD_ALT);
    if ((modifiers & static_cast<std::uint8_t>(~supported)) != 0)
        throw std::invalid_argument("TangoOpenGL host input has unsupported modifiers");
}

std::uint64_t submitted_revision(const CellPixelPresenter& presenter) {
    require_owner(presenter);
    const auto* plan = presenter.last_submitted_plan();
    return plan ? plan->grid_revision() : 0;
}

tui_key_t mouse_button_key(CellMouseButton button) {
    switch (button) {
        case CellMouseButton::Left: return TUI_KEY_MOUSE_LEFT;
        case CellMouseButton::Middle: return TUI_KEY_MOUSE_MIDDLE;
        case CellMouseButton::Right: return TUI_KEY_MOUSE_RIGHT;
        case CellMouseButton::None: return TUI_KEY_NONE;
    }
    return TUI_KEY_NONE;
}

bool special_key(tui_key_t key) {
    switch (key) {
        case TUI_KEY_ENTER:
        case TUI_KEY_TAB:
        case TUI_KEY_BACKSPACE:
        case TUI_KEY_ESCAPE:
        case TUI_KEY_UP:
        case TUI_KEY_DOWN:
        case TUI_KEY_LEFT:
        case TUI_KEY_RIGHT:
        case TUI_KEY_HOME:
        case TUI_KEY_END:
        case TUI_KEY_PAGE_UP:
        case TUI_KEY_PAGE_DOWN:
        case TUI_KEY_INSERT:
        case TUI_KEY_DELETE:
        case TUI_KEY_F1:
        case TUI_KEY_F2:
        case TUI_KEY_F3:
        case TUI_KEY_F4:
        case TUI_KEY_F5:
        case TUI_KEY_F6:
        case TUI_KEY_F7:
        case TUI_KEY_F8:
        case TUI_KEY_F9:
        case TUI_KEY_F10:
        case TUI_KEY_F11:
        case TUI_KEY_F12:
            return true;
        default:
            return false;
    }
}

bool printable_scalar(std::uint32_t scalar) {
    return scalar >= 0x20u &&
           scalar != 0x7fu &&
           !(scalar >= 0x80u && scalar <= 0x9fu) &&
           !(scalar >= 0xd800u && scalar <= 0xdfffu) &&
           scalar <= 0x10ffffu;
}

} // namespace

std::optional<CellHostInput> cell_input_pointer(
    const CellPixelPresenter& presenter, double window_x, double window_y,
    const CellWindowExtents& extents, CellMouseAction action,
    CellMouseButton button, std::uint8_t modifiers) {
    require_owner(presenter);
    validate_modifiers(modifiers);

    const bool button_event =
        action == CellMouseAction::Press || action == CellMouseAction::Release;
    const tui_key_t key = mouse_button_key(button);
    if ((button_event && key == TUI_KEY_NONE) ||
        (!button_event && button != CellMouseButton::None))
        throw std::invalid_argument("TangoOpenGL mouse action/button combination unsupported");

    const auto hit = presenter.hit_test_window_pointer(
        window_x, window_y, extents.window_width, extents.window_height,
        extents.framebuffer_width, extents.framebuffer_height);
    if (!hit) return std::nullopt;
    CellHostInput result{};
    result.grid_revision = hit->grid_revision;
    result.leading_column = hit->leading_column;
    result.continuation = hit->continuation;
    result.event.x = hit->column;
    result.event.y = hit->row;
    result.event.mod = modifiers;
    switch (action) {
        case CellMouseAction::Press:
            result.event.type = TUI_EVENT_MOUSE_PRESS;
            result.event.key = key;
            break;
        case CellMouseAction::Release:
            result.event.type = TUI_EVENT_MOUSE_RELEASE;
            result.event.key = key;
            break;
        case CellMouseAction::Move:
            result.event.type = TUI_EVENT_MOUSE_MOVE;
            break;
        case CellMouseAction::WheelUp:
            result.event.type = TUI_EVENT_MOUSE_PRESS;
            result.event.key = TUI_KEY_MOUSE_WHEEL_UP;
            break;
        case CellMouseAction::WheelDown:
            result.event.type = TUI_EVENT_MOUSE_PRESS;
            result.event.key = TUI_KEY_MOUSE_WHEEL_DOWN;
            break;
        default:
            throw std::invalid_argument("TangoOpenGL mouse action is unsupported");
    }
    return result;
}

std::optional<CellHostInput> cell_input_key(
    const CellPixelPresenter& presenter, tui_key_t key,
    std::uint8_t modifiers) {
    const auto revision = submitted_revision(presenter);
    validate_modifiers(modifiers);
    if (!revision || !special_key(key)) return std::nullopt;
    CellHostInput result{};
    result.grid_revision = revision;
    result.event.type = TUI_EVENT_KEY;
    result.event.key = key;
    result.event.mod = modifiers;
    if (key == TUI_KEY_ENTER) result.event.ch = '\n';
    if (key == TUI_KEY_TAB) result.event.ch = '\t';
    if (key == TUI_KEY_ESCAPE) result.event.ch = 27u;
    if (key == TUI_KEY_BACKSPACE) result.event.ch = 127u;
    return result;
}

std::optional<CellHostInput> cell_input_character(
    const CellPixelPresenter& presenter, std::uint32_t scalar) {
    const auto revision = submitted_revision(presenter);
    if (!revision || !printable_scalar(scalar)) return std::nullopt;
    CellHostInput result{};
    result.grid_revision = revision;
    result.event.type = TUI_EVENT_KEY;
    result.event.ch = scalar;
    if (scalar <= 0x7eu)
        result.event.key = static_cast<tui_key_t>(scalar);
    return result;
}



CellPixelMetrics fit_cell_metrics_to_window(
    const tui_grid_snapshot_t* snapshot,
    const CellWindowExtents& observed,
    const CellPixelMetrics& base) {
    if (!snapshot ||
        observed.window_width <= 0 || observed.window_height <= 0 ||
        observed.framebuffer_width <= 0 || observed.framebuffer_height <= 0 ||
        !std::isfinite(base.cell_width) || base.cell_width <= 0.0f ||
        !std::isfinite(base.cell_height) || base.cell_height <= 0.0f ||
        !std::isfinite(base.font_size) || base.font_size <= 0.0f ||
        !std::isfinite(base.baseline) || base.baseline <= 0.0f ||
        !base.expected_grid_revision)
        throw std::invalid_argument(
            "TangoOpenGL resize requires an exact positive source/profile and host");
    if (tui_grid_snapshot_revision(snapshot) != base.expected_grid_revision)
        throw std::logic_error(
            "TangoOpenGL resize cannot reproject a stale C11 grid revision");
    const int columns = tui_grid_snapshot_width(snapshot);
    const int rows = tui_grid_snapshot_height(snapshot);
    if (columns <= 0 || rows <= 0)
        throw std::invalid_argument(
            "TangoOpenGL resize source has no cell geometry");

    const double ratio_x = static_cast<double>(observed.framebuffer_width) /
                           observed.window_width;
    const double ratio_y = static_cast<double>(observed.framebuffer_height) /
                           observed.window_height;
    // A single device-pixel ratio is the documented projection. Reject an
    // X/Y mismatch, including an in-progress host generation, rather than
    // warping CJK glyphs or pretending TTY cells are physical device pixels.
    if (!std::isfinite(ratio_x) || !std::isfinite(ratio_y) ||
        ratio_x <= 0.0 || ratio_y <= 0.0 ||
        std::fabs(ratio_x - ratio_y) > ratio_x * 0.000001)
        throw std::logic_error(
            "TangoOpenGL resize observed nonuniform or inconsistent host DPI");

    const double cell_width = static_cast<double>(observed.window_width) / columns;
    const double cell_height = static_cast<double>(observed.window_height) / rows;
    const double font_scale = cell_height / base.cell_height;
    const double font_size = base.font_size * font_scale;
    const double baseline = base.baseline * font_scale;
    const double float_max = (std::numeric_limits<float>::max)();
    if (cell_width <= 0.0 || cell_height <= 0.0 || font_size <= 0.0 ||
        baseline <= 0.0 || cell_width > float_max ||
        cell_height > float_max || font_size > float_max ||
        baseline > float_max || ratio_x > float_max)
        throw std::length_error("TangoOpenGL resize exceeds cell/font coordinate domain");

    CellPixelMetrics adapted = base;
    adapted.cell_width = static_cast<float>(cell_width);
    adapted.cell_height = static_cast<float>(cell_height);
    adapted.pixel_ratio = static_cast<float>(ratio_x);
    adapted.font_size = static_cast<float>(font_size);
    adapted.baseline = static_cast<float>(baseline);

    // The exact floats sent to begin_frame MUST still round onto the same
    // actual measured framebuffer, including fractional cell bounds.
    const double projected_width =
        static_cast<double>(adapted.cell_width) * columns * adapted.pixel_ratio;
    const double projected_height =
        static_cast<double>(adapted.cell_height) * rows * adapted.pixel_ratio;
    const double int_max = (std::numeric_limits<int>::max)();
    if (!std::isfinite(projected_width) || !std::isfinite(projected_height) ||
        projected_width > int_max || projected_height > int_max ||
        std::lround(projected_width) != observed.framebuffer_width ||
        std::lround(projected_height) != observed.framebuffer_height)
        throw std::logic_error(
            "TangoOpenGL host framebuffer cannot be represented by exact cell metrics");
    return adapted;
}

CellHostFrameGate::CellHostFrameGate(CellPixelPresenter& presenter) noexcept
    : presenter_(&presenter) {}

void CellHostFrameGate::require_owner() const {
    if (!presenter_->on_owner_thread())
        throw std::logic_error(
            "TangoOpenGL host frame admission accessed off its GL/UI owner");
}

void CellHostFrameGate::set_focus(bool focused) {
    require_owner();
    // Even true->true and false->false are conservative: the host must
    // publish a freshly swapped frame after a focus notification.
    focused_ = focused;
    admitted_ = false;
}

void CellHostFrameGate::invalidate_geometry() {
    require_owner();
    admitted_ = false;
}

void CellHostFrameGate::accept_presented_frame(
    const CellWindowExtents& observed) {
    require_owner();
    admitted_ = false;
    if (!focused_) return;
    const auto* plan = presenter_->last_submitted_plan();
    if (!plan || !presenter_->submission_id())
        throw std::logic_error(
            "TangoOpenGL cannot admit input without a completed pixel frame");
    if (presenter_->submission_id() <= submission_)
        throw std::logic_error(
            "TangoOpenGL focus/resize admission requires a fresh completed frame");
    // The same function as pointer hit testing checks the actual painted
    // framebuffer and rejects zero/mismatched dimensions; no guessed scaling.
    if (!hit_test_window_pointer(
            *plan, 0.0, 0.0, observed.window_width,
            observed.window_height, observed.framebuffer_width,
            observed.framebuffer_height))
        throw std::logic_error(
            "TangoOpenGL cannot admit an incoherent host pixel frame");
    accepted_ = observed;
    revision_ = plan->grid_revision();
    submission_ = presenter_->submission_id();
    admitted_ = true;
}

bool CellHostFrameGate::admitted_for(const CellWindowExtents& current) {
    require_owner();
    if (!focused_ || !admitted_) return false;
    if (accepted_.window_width != current.window_width ||
        accepted_.window_height != current.window_height ||
        accepted_.framebuffer_width != current.framebuffer_width ||
        accepted_.framebuffer_height != current.framebuffer_height) {
        admitted_ = false;
        return false;
    }
    const auto* plan = presenter_->last_submitted_plan();
    if (!plan || plan->grid_revision() != revision_ ||
        presenter_->submission_id() != submission_) {
        admitted_ = false;
        return false;
    }
    return true;
}

std::optional<CellHostInput> CellHostFrameGate::pointer(
    double x, double y, const CellWindowExtents& current,
    CellMouseAction action, CellMouseButton button, std::uint8_t modifiers) {
    if (!admitted_for(current)) return std::nullopt;
    return cell_input_pointer(
        *presenter_, x, y, current, action, button, modifiers);
}

std::optional<CellHostInput> CellHostFrameGate::key(
    tui_key_t key, std::uint8_t modifiers, const CellWindowExtents& current) {
    if (!admitted_for(current)) return std::nullopt;
    return cell_input_key(*presenter_, key, modifiers);
}

std::optional<CellHostInput> CellHostFrameGate::character(
    std::uint32_t scalar, const CellWindowExtents& current) {
    if (!admitted_for(current)) return std::nullopt;
    return cell_input_character(*presenter_, scalar);
}

} // namespace flex::tango_opengl
