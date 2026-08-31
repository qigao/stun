#include <flexUI/application.h>
#include <flexUI/application_host_bridge.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/input_widget.h>

#include <tinytest.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct ModuleProbe {
  std::vector<std::string> created_sources;
  std::vector<std::string> created_names;
  std::vector<std::string> calls;
  std::vector<std::string> destroyed;
  std::vector<flexUI::ScriptEventSnapshot> events;
  std::vector<std::string> sequence;
};

class FakeApplicationModule final : public flexUI::IScriptModule {
public:
  FakeApplicationModule(std::vector<std::string> exports, std::shared_ptr<ModuleProbe> probe,
                        bool fail_mount, bool fail_event)
      : probe_(std::move(probe)), fail_mount_(fail_mount), fail_event_(fail_event) {
    std::uint64_t next_handle = 1;
    for (auto &name : exports) {
      const flexUI::ScriptExportHandle handle{next_handle++};
      names_.emplace(handle.value, name);
      handles_.emplace(std::move(name), handle);
    }
  }

  ~FakeApplicationModule() override { probe_->destroyed.push_back("module"); }

  flexUI::ScriptResolveResult resolve_export(std::string_view name,
                                             flexUI::ScriptCallbackKind) override {
    const auto found = handles_.find(std::string(name));
    return found == handles_.end() ? flexUI::ScriptResolveResult{}
                                   : flexUI::ScriptResolveResult{found->second, {}};
  }

  flexUI::ScriptCallResult call(flexUI::ScriptExportHandle handle,
                                const flexUI::ScriptCallContext &context) override {
    const auto found = names_.find(handle.value);
    if (found == names_.end()) {
      return {{flexUI::ScriptModuleErrorCode::InvalidExport, "unknown fake application export"}};
    }
    probe_->calls.push_back(found->second);
    probe_->sequence.push_back("script:" + found->second);
    if (context.event != nullptr) {
      probe_->events.push_back(*context.event);
    }
    if (fail_mount_ && found->second == "on_mount") {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected mount failure"}};
    }
    if (fail_event_ && context.callback == flexUI::ScriptCallbackKind::Event) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected event failure"}};
    }
    return {};
  }

private:
  std::shared_ptr<ModuleProbe> probe_;
  bool fail_mount_ = false;
  bool fail_event_ = false;
  std::unordered_map<std::string, flexUI::ScriptExportHandle> handles_;
  std::unordered_map<std::uint64_t, std::string> names_;
};

flexUI::ScriptModuleFactory fake_factory(const std::shared_ptr<ModuleProbe> &probe) {
  return [probe](std::string_view source, std::string_view module_name) {
    probe->created_sources.emplace_back(source);
    probe->created_names.emplace_back(module_name);

    std::vector<std::string> exports{"on_mount", "on_unmount", "parent_handler",
                                     "consumed_handler"};
    if (source != "missing-handler") {
      exports.emplace_back("save_document");
    }
    return flexUI::ScriptModuleFactoryResult{
        std::make_unique<FakeApplicationModule>(
            std::move(exports), probe, source == "mount-failure", source == "event-failure"),
        {}};
  };
}

std::string xml_entry(std::string_view id = "save") {
  return "<ui name=\"Desktop\" xmlns:on=\"urn:flexui:event\">"
         "<button id=\"" +
         std::string(id) +
         "\" text=\"Save\" on:click=\"save_document\"/>"
         "</ui>";
}

void check_single_value(const std::vector<std::string> &actual, std::string_view expected) {
  check_equal(actual.size(), std::size_t{1});
  if (!actual.empty()) {
    check_equal(actual.front(), std::string(expected));
  }
}

flexUI::DesktopApplicationResult dispatch_click(flexUI::DesktopApplication &application,
                                                float x = 10.0F, float y = 10.0F) {
  auto down = flexUI::Event::mouse_down(x, y);
  auto result = application.dispatch_event(down);
  if (!result) {
    return result;
  }
  auto up = flexUI::Event::mouse_up(x, y);
  return application.dispatch_event(up);
}

} // namespace

spec("FlexUI desktop application publishes complete XML candidates") {
  it("builds typed XML strict CSS and a mounted script controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 123px; }")
        .script("valid-controller", fake_factory(probe), "desktop-test");

    auto built = builder.build();
    check(static_cast<bool>(built));
    check_not_null(built.application.get());
    if (!built) {
      return;
    }

    auto *save = built.application->box().get_by_id("save");
    check_not_null(save);
    check_not_null(dynamic_cast<flexUI::ButtonWidget *>(save->widget));
    check_not_null(built.application->controller());
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);
    check_equal(built.application->program().name(), "Desktop");
    check_single_value(probe->created_sources, "valid-controller");
    check_single_value(probe->created_names, "desktop-test");
    check_single_value(probe->calls, "on_mount");

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check_within(save->style_.width, 123.0F, 0.001F);
  }

  it("rejects strict CSS diagnostics before publishing an application") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>")
        .stylesheet("#root { widht: 90px; width: 12wat; }");

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::CssLoadFailed);
    check_false(built.error.css_diagnostics.empty());
  }

  it("rejects missing controller exports before publication") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("missing-handler", fake_factory(probe));

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ControllerLoadFailed);
    check(built.error.controller_error.code == flexUI::ControllerErrorCode::MissingHandlerExport);
    check_equal(built.error.controller_error.element_id, "save");
    check_equal(built.error.controller_error.handler, "save_document");
  }

  it("rejects a failing mount without publishing candidate state") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("mount-failure", fake_factory(probe));

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ControllerMountFailed);
    check(built.error.controller_error.stage == flexUI::ControllerStage::Mount);
  }

  it("requires a script factory when XML declares event handlers") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry());

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ScriptRequired);
  }

  it("keeps the active application unchanged when reload fails") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 40px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *active_box = &built.application->box();
    const auto failed =
        built.application->reload({xml_entry("replacement"), "#replacement { width: 80px; }",
                                   "missing-handler", "reload-test"});

    check_false(static_cast<bool>(failed));
    check(failed.error.code == flexUI::DesktopApplicationErrorCode::ControllerLoadFailed);
    check_equal(&built.application->box(), active_box);
    check_not_null(built.application->box().get_by_id("save"));
    check_null(built.application->box().get_by_id("replacement"));
    check_equal(built.application->sources().script, "valid-controller");
  }

  it("publishes UI CSS and controller together after successful reload") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *previous_box = &built.application->box();
    const auto reloaded =
        built.application->reload({xml_entry("replacement"), "#replacement { width: 88px; }",
                                   "replacement-controller", "replacement-module"});

    check(static_cast<bool>(reloaded));
    check_not_equal(&built.application->box(), previous_box);
    check_null(built.application->box().get_by_id("save"));
    auto *replacement = built.application->box().get_by_id("replacement");
    check_not_null(replacement);
    check_not_null(dynamic_cast<flexUI::ButtonWidget *>(replacement->widget));
    check_equal(built.application->sources().script, "replacement-controller");
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);
  }

  it("routes a synthesized click through target and ancestor handlers after native bubbling") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder
        .xml_entry("<ui name=\"Bubble\" xmlns:on=\"urn:flexui:event\">"
                   "<main id=\"parent\" on:click=\"parent_handler\">"
                   "<button id=\"save\" text=\"Save\" on:click=\"save_document\"/>"
                   "</main></ui>")
        .stylesheet("#parent { width: 200px; height: 100px; }"
                    "#save { width: 120px; height: 36px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    probe->calls.clear();
    probe->events.clear();
    probe->sequence.clear();
    built.application->box().set_event_callback(
        [probe](flexUI::Element &element, const flexUI::Event &event) {
          if (event.type == flexUI::EventType::MouseUp) {
            probe->sequence.push_back("native:" + element.id());
          }
        });

    const auto dispatched = dispatch_click(*built.application);
    check(static_cast<bool>(dispatched));
    check_equal(probe->calls.size(), std::size_t{2});
    check_equal(probe->events.size(), std::size_t{2});
    check_equal(probe->sequence.size(), std::size_t{4});
    if (probe->calls.size() == 2 && probe->events.size() == 2 && probe->sequence.size() == 4) {
      check_equal(probe->calls[0], "save_document");
      check_equal(probe->calls[1], "parent_handler");
      check_equal(probe->events[0].target.id, "save");
      check_equal(probe->events[0].current_target.id, "save");
      check_equal(probe->events[1].target.id, "save");
      check_equal(probe->events[1].current_target.id, "parent");
      check_equal(probe->sequence[0], "native:save");
      check_equal(probe->sequence[1], "native:parent");
      check_equal(probe->sequence[2], "script:save_document");
      check_equal(probe->sequence[3], "script:parent_handler");
    }
  }

  it("does not notify script when a widget consumes the routed event") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder
        .xml_entry("<ui name=\"Consumed\" xmlns:on=\"urn:flexui:event\">"
                   "<slider id=\"volume\" on:mouse_down=\"consumed_handler\"/>"
                   "</ui>")
        .stylesheet("#volume { width: 160px; height: 32px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    probe->calls.clear();
    auto down = flexUI::Event::mouse_down(10.0F, 10.0F);
    const auto dispatched = built.application->dispatch_event(down);

    check(static_cast<bool>(dispatched));
    check_true(down.handled);
    check_false(down.propagate);
    check_true(probe->calls.empty());
  }

  it("returns a structured dispatch error and faults the controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("event-failure", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    const auto dispatched = dispatch_click(*built.application);

    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code == flexUI::DesktopApplicationErrorCode::ControllerDispatchFailed);
    check(dispatched.error.stage == flexUI::DesktopApplicationStage::ControllerEvent);
    check(dispatched.error.controller_error.code == flexUI::ControllerErrorCode::ModuleCallFailed);
    check(built.application->controller()->state() == flexUI::ControllerState::Faulted);
  }

  it("reports native callback exceptions without faulting the script controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    probe->calls.clear();
    built.application->box().set_event_callback([](flexUI::Element &, const flexUI::Event &event) {
      if (event.type == flexUI::EventType::MouseUp) {
        throw std::runtime_error("injected native callback failure");
      }
    });

    const auto dispatched = dispatch_click(*built.application);

    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code == flexUI::DesktopApplicationErrorCode::EventDispatchFailed);
    check(dispatched.error.stage == flexUI::DesktopApplicationStage::ControllerEvent);
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);
    check(probe->calls.empty());
  }

  it("preserves native Box callbacks when no script controller is configured") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Native\"><button id=\"save\" text=\"Save\"/></ui>")
        .stylesheet("#save { width: 120px; height: 36px; }");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    bool clicked = false;
    built.application->box().get_by_id("save")->on_click([&] { clicked = true; });
    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    const auto dispatched = dispatch_click(*built.application);

    check(static_cast<bool>(dispatched));
    check_true(clicked);
  }

  it("gates host text and composition input through the application facade") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Input\"><input id=\"editor\"/></ui>")
        .stylesheet("#editor { width: 160px; height: 32px; }");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *editor = built.application->box().get_by_id("editor");
    auto *input = dynamic_cast<flexUI::InputWidget *>(editor->widget);
    check_not_null(input);

    const auto ignored =
        flexUI::host::dispatch_text_input_if_focused(*built.application, "ignored");
    check(static_cast<bool>(ignored));
    check_false(ignored.dispatched);
    check(input->text().empty());

    built.application->box().set_focus(editor);
    const auto text = flexUI::host::dispatch_text_input_if_focused(*built.application, "x");
    const auto composition_start =
        flexUI::host::dispatch_composition_start_if_focused(*built.application);
    const auto composition_update =
        flexUI::host::dispatch_composition_update_if_focused(*built.application, "zh");
    const auto composition_end =
        flexUI::host::dispatch_composition_end_if_focused(*built.application);

    check(static_cast<bool>(text));
    check_true(text.dispatched);
    check_equal(input->text(), std::string("x"));
    check(static_cast<bool>(composition_start));
    check_true(composition_start.dispatched);
    check(static_cast<bool>(composition_update));
    check_true(composition_update.dispatched);
    check(static_cast<bool>(composition_end));
    check_true(composition_end.dispatched);
  }

  it("rejects host text input on a non-owner thread before reading Box state") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Input\"><input id=\"editor\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::host::HostApplicationDispatchResult dispatch_result;
    std::thread worker([&] {
      dispatch_result =
          flexUI::host::dispatch_text_input_if_focused(*built.application, "rejected");
    });
    worker.join();

    check_false(static_cast<bool>(dispatch_result));
    check_false(dispatch_result.dispatched);
    check(dispatch_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
  }

  it("rejects reload from a non-owner thread without touching active state") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"active\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *active_box = &built.application->box();
    flexUI::DesktopApplicationResult reload_result;
    std::thread worker([&] {
      reload_result = built.application->reload(
          {"<ui name=\"Replacement\"><div id=\"replacement\"/></ui>", "", "", ""});
    });
    worker.join();

    check_false(static_cast<bool>(reload_result));
    check(reload_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check_equal(&built.application->box(), active_box);
    check_not_null(built.application->box().get_by_id("active"));
    check_null(built.application->box().get_by_id("replacement"));
  }

  it("rejects event dispatch from a non-owner thread before routing native input") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><button id=\"save\" text=\"Save\"/></ui>")
        .stylesheet("#save { width: 120px; height: 36px; }");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    bool clicked = false;
    built.application->box().get_by_id("save")->on_click([&] { clicked = true; });
    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    flexUI::DesktopApplicationResult dispatch_result;
    std::thread worker([&] {
      auto down = flexUI::Event::mouse_down(10.0F, 10.0F);
      dispatch_result = built.application->dispatch_event(down);
    });
    worker.join();

    check_false(static_cast<bool>(dispatch_result));
    check(dispatch_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check_false(clicked);
  }

  it("keeps legacy Flex loading behind the compatibility entry") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.legacy_flex_entry_compatibility("ui Legacy { button old { text: \"Old\" } }");

    auto built = builder.build();
    check(static_cast<bool>(built));
    check_not_null(built.application.get());
    if (!built) {
      return;
    }
    check_true(built.application->uses_legacy_flex_compatibility());
    check_not_null(
        dynamic_cast<flexUI::ButtonWidget *>(built.application->box().get_by_id("old")->widget));
  }
}
