#include <flexUI/gcanvas_application_input.h>
#include <flexUI/widgets/input_widget.h>

#include <tinytest.hpp>

#include <string>
#include <thread>

namespace {

flexUI::DesktopApplicationBuildResult build_input_application() {
  flexUI::DesktopApplicationBuilder builder(nullptr);
  builder
      .xml_entry("<ui name=\"Input\"><div id=\"root\"><button id=\"button\" "
                 "text=\"Run\"/><input id=\"editor\"/></div></ui>")
      .stylesheet("#button { width: 120px; height: 40px; } "
                  "#editor { width: 160px; height: 32px; }");
  return builder.build();
}

void prepare_layout(flexUI::DesktopApplication &application) {
  application.box().set_viewport(320.0F, 200.0F);
  application.box().update();
}

} // namespace

spec("gCanvas application router dispatches validated native input") {
  it("routes pointer button wheel and repeated key events through the application") {
    auto built = build_input_application();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    prepare_layout(*built.application);

    bool clicked = false;
    built.application->box().get_by_id("button")->on_click([&] { clicked = true; });
    flexUI::GCanvasApplicationInputRouter router(*built.application);

    const auto moved = router.mouse_move({12.0, 24.0});
    const auto pressed = router.mouse_button(
        {gcanvas::MOUSE_BUTTON_LEFT, gcanvas::ACTION_PRESS, gcanvas::MOUSE_MOD_SHIFT, 12.0, 24.0});
    const auto released = router.mouse_button({gcanvas::MOUSE_BUTTON_LEFT, gcanvas::ACTION_RELEASE,
                                               gcanvas::MOUSE_MOD_SHIFT, 12.0, 24.0});
    const auto wheel = router.scroll({0.0, -1.0});
    const auto repeated =
        router.key({gcanvas::KEY_A, 0, gcanvas::ACTION_REPEAT, gcanvas::KEY_MOD_CONTROL, "a"});

    check(static_cast<bool>(moved));
    check_true(moved.processed);
    check(static_cast<bool>(pressed));
    check_true(pressed.processed);
    check(static_cast<bool>(released));
    check_true(released.processed);
    check_true(clicked);
    check(static_cast<bool>(wheel));
    check_true(wheel.processed);
    check(static_cast<bool>(repeated));
    check_true(repeated.processed);
  }

  it("preserves normalization errors without dispatching") {
    auto built = build_input_application();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    flexUI::GCanvasApplicationInputRouter router(*built.application);

    const auto invalid = router.mouse_button(
        {gcanvas::MOUSE_BUTTON_4, gcanvas::ACTION_PRESS, gcanvas::MOUSE_MOD_SHIFT, 1.0, 2.0});

    check_false(static_cast<bool>(invalid));
    check_false(invalid.processed);
    check_equal(static_cast<int>(invalid.error.code),
                static_cast<int>(flexUI::GCanvasApplicationInputErrorCode::NormalizationFailed));
    check_equal(static_cast<int>(invalid.error.input_error.code),
                static_cast<int>(flexUI::GCanvasInputErrorCode::UnsupportedMouseButton));
    check_equal(static_cast<int>(invalid.error.application_error.code),
                static_cast<int>(flexUI::DesktopApplicationErrorCode::None));
  }
}

spec("gCanvas application router gates text and owner-thread state") {
  it("distinguishes ignored text from text dispatched to the focused editor") {
    auto built = build_input_application();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    prepare_layout(*built.application);

    auto *editor = built.application->box().get_by_id("editor");
    check_not_null(editor);
    if (editor == nullptr) {
      return;
    }
    auto *input = dynamic_cast<flexUI::InputWidget *>(editor->widget);
    check_not_null(input);
    if (input == nullptr) {
      return;
    }

    flexUI::GCanvasApplicationInputRouter router(*built.application);
    const auto ignored = router.character({static_cast<unsigned int>('x'), "x"});
    built.application->box().set_focus(editor);
    const auto accepted = router.character({static_cast<unsigned int>('x'), "x"});

    check(static_cast<bool>(ignored));
    check_false(ignored.processed);
    check(static_cast<bool>(accepted));
    check_true(accepted.processed);
    check_equal(input->text(), std::string("x"));
  }

  it("rejects a foreign thread before changing cached pointer state") {
    auto built = build_input_application();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    flexUI::GCanvasApplicationInputRouter router(*built.application);
    flexUI::GCanvasApplicationInputResult wrong_thread;

    std::thread worker([&] { wrong_thread = router.mouse_move({4.0, 8.0}); });
    worker.join();
    const auto wheel = router.scroll({0.0, 1.0});

    check_false(static_cast<bool>(wrong_thread));
    check_false(wrong_thread.processed);
    check_equal(static_cast<int>(wrong_thread.error.code),
                static_cast<int>(flexUI::GCanvasApplicationInputErrorCode::ApplicationFailed));
    check_equal(static_cast<int>(wrong_thread.error.application_error.code),
                static_cast<int>(flexUI::DesktopApplicationErrorCode::WrongThread));
    check_false(static_cast<bool>(wheel));
    check_equal(static_cast<int>(wheel.error.input_error.code),
                static_cast<int>(flexUI::GCanvasInputErrorCode::MissingPointerPosition));
  }
}

spec("gCanvas application router applies validated logical viewport metrics") {
  it("invalidates the Box only after a valid positive resize") {
    auto built = build_input_application();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    flexUI::GCanvasApplicationInputRouter router(*built.application);

    const auto invalid = router.resize({0, 720});
    const auto valid = router.resize({1280, 720});

    check_false(static_cast<bool>(invalid));
    check_equal(static_cast<int>(invalid.error.input_error.code),
                static_cast<int>(flexUI::GCanvasInputErrorCode::InvalidViewportSize));
    check(static_cast<bool>(valid));
    check_true(valid.processed);
    check_equal(built.application->box().viewport_width(), 1280.0F);
    check_equal(built.application->box().viewport_height(), 720.0F);
    check_true(built.application->box().is_dirty());
  }
}

spec("gCanvas application router clears interaction state on focus loss") {
  it("keeps focus gain inert and clears focus and capture on loss") {
    auto built = build_input_application();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    auto *editor = built.application->box().get_by_id("editor");
    check_not_null(editor);
    if (editor == nullptr) {
      return;
    }
    built.application->box().set_focus(editor);
    built.application->box().set_mouse_capture(editor);
    flexUI::GCanvasApplicationInputRouter router(*built.application);

    const auto gained = router.focus({true});
    check(static_cast<bool>(gained));
    check_false(gained.processed);
    check_equal(built.application->box().focused_element(), editor);
    check_equal(built.application->box().capturing_element(), editor);

    const auto lost = router.focus({false});
    check(static_cast<bool>(lost));
    check_true(lost.processed);
    check_null(built.application->box().focused_element());
    check_null(built.application->box().capturing_element());
  }

  it("rejects focus loss from a foreign thread without changing interaction state") {
    auto built = build_input_application();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    auto *editor = built.application->box().get_by_id("editor");
    built.application->box().set_focus(editor);
    built.application->box().set_mouse_capture(editor);
    flexUI::GCanvasApplicationInputRouter router(*built.application);
    flexUI::GCanvasApplicationInputResult result;

    std::thread worker([&] { result = router.focus({false}); });
    worker.join();

    check_false(static_cast<bool>(result));
    check(result.error.application_error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check_equal(built.application->box().focused_element(), editor);
    check_equal(built.application->box().capturing_element(), editor);
  }
}
