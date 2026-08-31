#include <flexUI/application.h>
#include <flexUI/application_turboscript.h>

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
  constexpr auto ui = R"(
    <ui name="DesktopExample" xmlns:on="urn:flexui:event">
      <main id="content">
        <button id="save" text="Save" on:click="save_document"/>
      </main>
    </ui>
  )";
  constexpr auto stylesheet = R"(
    #content { padding: 16px; }
    #save { width: 120px; height: 36px; }
  )";
  constexpr auto controller = "func on_mount(){return null;};"
                              "func save_document(event){return map {mutations:list("
                              "map {type:\"set_text\",target:event.target,text:\"Saved\"})};};"
                              "export(\"on_mount\");export(\"save_document\");";

  flexUI::TurboScriptControllerOptions script_options;
  script_options.execution_mode = flexUI::TurboScriptExecutionMode::Interpreter;

  flexUI::DesktopApplicationBuilder builder(nullptr);
  builder.xml_entry(ui)
      .stylesheet(stylesheet)
      .script(controller, flexUI::make_turboscript_desktop_module_factory(script_options),
              "desktop-example");

  auto built = builder.build();
  if (!built) {
    std::cerr << "application build failed at stage " << static_cast<int>(built.error.stage) << ": "
              << built.error.message << '\n';
    return EXIT_FAILURE;
  }

  auto *save = built.application->box().get_by_id("save");
  if (save == nullptr || built.application->controller() == nullptr) {
    std::cerr << "application did not publish the required UI/controller state\n";
    return EXIT_FAILURE;
  }

  built.application->box().set_viewport(320.0F, 200.0F);
  built.application->box().update();
  const float click_x = save->absolute_x() + save->width() * 0.5F;
  const float click_y = save->absolute_y() + save->height() * 0.5F;
  auto down = flexUI::Event::mouse_down(click_x, click_y);
  auto up = flexUI::Event::mouse_up(click_x, click_y);
  const auto pressed = built.application->dispatch_event(down);
  const auto released = pressed ? built.application->dispatch_event(up) : pressed;
  if (!released) {
    std::cerr << "application event dispatch failed: " << released.error.message << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "button text after TurboScript dispatch: " << save->text() << '\n';
  return save->text() == "Saved" ? EXIT_SUCCESS : EXIT_FAILURE;
}
