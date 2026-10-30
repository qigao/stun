#include <backends/tango_opengl/cell_input.h>

#include <stdexcept>

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

} // namespace flex::tango_opengl
