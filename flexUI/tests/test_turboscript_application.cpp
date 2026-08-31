#include <flexUI/application.h>
#include <flexUI/application_turboscript.h>

#include <tinytest.hpp>

spec("FlexUI TurboScript desktop application composition") {
  it("builds XML CSS and TBS then applies a typed UI mutation") {
    constexpr std::string_view xml = R"(
      <ui name="TurboDesktop" xmlns:on="urn:flexui:event">
        <button id="save" text="Save" on:click="save_document"/>
      </ui>
    )";
    constexpr std::string_view script =
        "func on_mount(){return null;};"
        "func save_document(event){return map {mutations:list("
        "map {type:\"set_text\",target:event.target,text:\"Saved\"})};};"
        "export(\"on_mount\");export(\"save_document\");";

    flexUI::TurboScriptControllerOptions options;
    options.execution_mode = flexUI::TurboScriptExecutionMode::Interpreter;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(std::string(xml))
        .stylesheet("#save { width: 120px; }")
        .script(std::string(script), flexUI::make_turboscript_desktop_module_factory(options));

    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *save = built.application->box().get_by_id("save");
    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    auto down = flexUI::Event::mouse_down(10.0F, 10.0F);
    auto up = flexUI::Event::mouse_up(10.0F, 10.0F);
    check(built.application->dispatch_event(down));
    check(built.application->dispatch_event(up));
    check_equal(save->text(), "Saved");
  }
}
