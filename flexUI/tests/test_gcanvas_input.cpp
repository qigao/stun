#include <flexUI/gcanvas_input.h>

#include <tinytest.hpp>

#include <cmath>
#include <limits>
#include <string>

spec("gCanvas input normalizer validates pointer state") {
  it("normalizes logical pointer coordinates and reuses them for wheel events") {
    flexUI::GCanvasInputNormalizer normalizer;

    const auto missing_pointer = normalizer.scroll({1.0, -2.0});
    check_false(static_cast<bool>(missing_pointer));
    check(missing_pointer.error.code == flexUI::GCanvasInputErrorCode::MissingPointerPosition);

    const auto move = normalizer.mouse_move({12.5, 24.25});
    check(static_cast<bool>(move));
    check(move.event->type == flexUI::EventType::MouseMove);
    check_equal(move.event->x, 12.5F);
    check_equal(move.event->y, 24.25F);

    const auto wheel = normalizer.scroll({1.5, -2.5});
    check(static_cast<bool>(wheel));
    check(wheel.event->type == flexUI::EventType::MouseWheel);
    check_equal(wheel.event->x, 12.5F);
    check_equal(wheel.event->y, 24.25F);
    check_equal(wheel.event->delta_x, 1.5F);
    check_equal(wheel.event->delta_y, -2.5F);

    const auto invalid = normalizer.mouse_move({std::numeric_limits<double>::infinity(), 4.0});
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == flexUI::GCanvasInputErrorCode::NonFiniteScalar);

    const auto preserved_pointer = normalizer.scroll({0.0, 1.0});
    check(static_cast<bool>(preserved_pointer));
    check_equal(preserved_pointer.event->x, 12.5F);
    check_equal(preserved_pointer.event->y, 24.25F);
  }
}

spec("gCanvas input normalizer maps buttons keys and modifiers") {
  it("maps supported mouse buttons and rejects unsupported native values") {
    flexUI::GCanvasInputNormalizer normalizer;
    const auto mods =
        static_cast<gcanvas::mouse_mod>(gcanvas::MOUSE_MOD_SHIFT | gcanvas::MOUSE_MOD_CONTROL);
    const auto press = normalizer.mouse_button(
        {gcanvas::MOUSE_BUTTON_LEFT, gcanvas::ACTION_PRESS, mods, 8.0, 9.0});

    check(static_cast<bool>(press));
    check(press.event->type == flexUI::EventType::MouseDown);
    check(press.event->button == flexUI::MouseButton::Left);
    check_equal(press.event->mods, static_cast<int>(flexUI::KeyMod::Shift) |
                                       static_cast<int>(flexUI::KeyMod::Control));

    const auto unsupported_button =
        normalizer.mouse_button({gcanvas::MOUSE_BUTTON_4, gcanvas::ACTION_PRESS, mods, 8.0, 9.0});
    check_false(static_cast<bool>(unsupported_button));
    check(unsupported_button.error.code == flexUI::GCanvasInputErrorCode::UnsupportedMouseButton);

    const auto unsupported_action = normalizer.mouse_button(
        {gcanvas::MOUSE_BUTTON_LEFT, gcanvas::ACTION_REPEAT, mods, 8.0, 9.0});
    check_false(static_cast<bool>(unsupported_action));
    check(unsupported_action.error.code == flexUI::GCanvasInputErrorCode::UnsupportedAction);

    const auto invalid_mods = normalizer.mouse_button(
        {gcanvas::MOUSE_BUTTON_LEFT, gcanvas::ACTION_PRESS,
         static_cast<gcanvas::mouse_mod>(gcanvas::MOUSE_MOD_SHIFT | 0x4000), 8.0, 9.0});
    check_false(static_cast<bool>(invalid_mods));
    check(invalid_mods.error.code == flexUI::GCanvasInputErrorCode::InvalidModifiers);
  }

  it("treats key repeat as key down and filters lock-state modifiers") {
    flexUI::GCanvasInputNormalizer normalizer;
    const auto mods = static_cast<gcanvas::keyboard_mod>(
        gcanvas::KEY_MOD_ALT | gcanvas::KEY_MOD_SUPER | gcanvas::KEY_MOD_CAPS_LOCK);
    const auto repeated = normalizer.key({gcanvas::KEY_A, 0, gcanvas::ACTION_REPEAT, mods, "a"});

    check(static_cast<bool>(repeated));
    check(repeated.event->type == flexUI::EventType::KeyDown);
    check(repeated.event->key == flexUI::KeyCode::A);
    check_equal(repeated.event->mods,
                static_cast<int>(flexUI::KeyMod::Alt) | static_cast<int>(flexUI::KeyMod::Super));

    const auto invalid_mods = normalizer.key({gcanvas::KEY_A, 0, gcanvas::ACTION_PRESS,
                                              static_cast<gcanvas::keyboard_mod>(0x4000), "a"});
    check_false(static_cast<bool>(invalid_mods));
    check(invalid_mods.error.code == flexUI::GCanvasInputErrorCode::InvalidModifiers);
  }
}

spec("gCanvas input normalizer validates text and viewport metrics") {
  it("copies UTF-8 text and rejects null text") {
    flexUI::GCanvasInputNormalizer normalizer;
    std::string text = "中";
    const auto character = normalizer.character({0x4E2DU, text.c_str()});
    text.clear();

    check(static_cast<bool>(character));
    check(character.event->type == flexUI::EventType::TextInput);
    check_equal(character.event->text, std::string("中"));

    const auto null_text = normalizer.character({0x4E2DU, nullptr});
    check_false(static_cast<bool>(null_text));
    check(null_text.error.code == flexUI::GCanvasInputErrorCode::NullText);

    const auto mismatched_text = normalizer.character({0x4E2DU, "x"});
    check_false(static_cast<bool>(mismatched_text));
    check(mismatched_text.error.code == flexUI::GCanvasInputErrorCode::InvalidText);

    const auto invalid_codepoint = normalizer.character({0xD800U, "x"});
    check_false(static_cast<bool>(invalid_codepoint));
    check(invalid_codepoint.error.code == flexUI::GCanvasInputErrorCode::InvalidCodepoint);
  }

  it("accepts only positive logical viewport dimensions") {
    flexUI::GCanvasInputNormalizer normalizer;
    const auto valid = normalizer.resize({1280, 720});
    check(static_cast<bool>(valid));
    check_equal(valid.metrics->width, 1280);
    check_equal(valid.metrics->height, 720);

    const auto invalid = normalizer.resize({0, 720});
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == flexUI::GCanvasInputErrorCode::InvalidViewportSize);
  }
}
