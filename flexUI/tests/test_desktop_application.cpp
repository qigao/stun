#include <flexUI/application.h>
#include <flexUI/application_host_bridge.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/input_widget.h>

#include <tinytest.hpp>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

class TestStorageEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  flexUI::ApplicationServiceSubmitResult try_submit(
      const flexUI::ApplicationServiceRequest &,
      flexUI::IApplicationServiceCompletionSink &) override {
    return {};
  }
};

std::shared_ptr<TestStorageEndpoint> configure_storage_service(
    flexUI::DesktopApplicationBuilder &application_builder) {
  auto endpoint = std::make_shared<TestStorageEndpoint>();
  flexUI::ApplicationServiceRegistryBuilder registry_builder;
  check(registry_builder.register_service(
      {"storage/1", {{"write", 1024}}}, endpoint));
  auto registry = registry_builder.build();
  check(registry);
  application_builder.services(std::move(registry.registry),
                               {{"storage/1"}, {"storage/1"}});
  return endpoint;
}

struct ModuleProbe {
  std::vector<std::string> created_sources;
  std::vector<std::string> created_names;
  std::vector<std::string> calls;
  std::vector<std::string> destroyed;
  std::vector<flexUI::ScriptEventSnapshot> events;
  std::vector<double> frame_deltas;
  std::vector<std::string> sequence;
  flexUI::DesktopApplication *application = nullptr;
  flexUI::DesktopApplicationResult reentrant_shutdown;
  bool shutdown_during_event = false;
  bool emit_service_command = false;
  bool emit_valid_mutation = false;
  bool emit_invalid_mutation = false;
  std::uint64_t service_request_id = 71;
};

class FakeApplicationModule final : public flexUI::IScriptModule {
public:
  FakeApplicationModule(std::vector<std::string> exports, std::shared_ptr<ModuleProbe> probe,
                        bool fail_mount, bool fail_event, bool fail_frame, bool fail_unmount)
      : probe_(std::move(probe)), fail_mount_(fail_mount), fail_event_(fail_event),
        fail_frame_(fail_frame), fail_unmount_(fail_unmount) {
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
    if (context.callback == flexUI::ScriptCallbackKind::Frame) {
      probe_->frame_deltas.push_back(context.delta_seconds);
    }
    if (fail_frame_ && context.callback == flexUI::ScriptCallbackKind::Frame) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected frame failure"}};
    }
    if (fail_unmount_ && context.callback == flexUI::ScriptCallbackKind::Unmount) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected unmount failure"}};
    }
    if (probe_->shutdown_during_event && context.callback == flexUI::ScriptCallbackKind::Event &&
        probe_->application != nullptr) {
      static_cast<void>(probe_->application->request_close());
      probe_->reentrant_shutdown = probe_->application->shutdown();
    }
    flexUI::ScriptCallResult result;
    if (context.callback == flexUI::ScriptCallbackKind::Event && probe_->emit_service_command) {
      const auto appended =
          result.commands.append(
              {probe_->service_request_id, "storage/1", "write", "document"});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message}};
      }
    }
    if (context.callback == flexUI::ScriptCallbackKind::Event &&
        (probe_->emit_valid_mutation || probe_->emit_invalid_mutation)) {
      const flexUI::UiHandle target =
          probe_->emit_invalid_mutation
              ? flexUI::UiHandle{"missing", context.event->current_target.generation}
              : context.event->current_target;
      const auto appended = result.mutations.append(flexUI::SetTextMutation{target, "Saved"});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message}};
      }
    }
    return result;
  }

private:
  std::shared_ptr<ModuleProbe> probe_;
  bool fail_mount_ = false;
  bool fail_event_ = false;
  bool fail_frame_ = false;
  bool fail_unmount_ = false;
  std::unordered_map<std::string, flexUI::ScriptExportHandle> handles_;
  std::unordered_map<std::uint64_t, std::string> names_;
};

flexUI::ScriptModuleFactory fake_factory(const std::shared_ptr<ModuleProbe> &probe) {
  return [probe](std::string_view source, std::string_view module_name) {
    probe->created_sources.emplace_back(source);
    probe->created_names.emplace_back(module_name);

    std::vector<std::string> exports{"on_mount", "on_frame", "on_unmount", "parent_handler",
                                     "consumed_handler"};
    if (source != "missing-handler") {
      exports.emplace_back("save_document");
    }
    return flexUI::ScriptModuleFactoryResult{
        std::make_unique<FakeApplicationModule>(
            std::move(exports), probe, source == "mount-failure", source == "event-failure",
            source == "frame-failure", source == "unmount-failure"),
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

  it("publishes service commands after UI mutation commit and resolves completions") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    probe->emit_valid_mutation = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe));
    auto endpoint = configure_storage_service(builder);
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    auto *save = built.application->box().get_by_id("save");
    check(dispatch_click(*built.application));
    check_equal(save->text(), std::string("Saved"));

    auto request = built.application->try_receive_service_request();
    check(request);
    check_equal(request.request->script_request_id, std::uint64_t{71});
    check_equal(request.request->capability, std::string("storage/1"));
    check_equal(request.request->operation, std::string("write"));
    check_equal(request.request->payload, std::string("document"));
    check_equal(request.request->token.generation, built.application->generation());
    auto resolved_endpoint =
        built.application->resolve_service_request(*request.request);
    check(resolved_endpoint);
    check(resolved_endpoint.endpoint == endpoint);

    flexUI::ApplicationCompletion completion;
    completion.token = request.request->token;
    completion.payload = "stored";
    check(built.application->completion_mailbox().try_post(completion));

    auto resolved = built.application->try_receive_service_completion();
    check(resolved);
    check_equal(resolved.completion->script_request_id, std::uint64_t{71});
    check_equal(resolved.completion->payload, std::string("stored"));
    check(built.application->try_receive_service_completion().status ==
          flexUI::ApplicationServicePollStatus::Empty);
    check_equal(built.application->service_statistics().completed, std::uint64_t{1});
  }

  it("discards reserved service commands when UI mutation preparation fails") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    probe->emit_invalid_mutation = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("invalid-mutation", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    const auto dispatched = dispatch_click(*built.application);
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.controller_error.code == flexUI::ControllerErrorCode::MutationFailed);
    check(built.application->try_receive_service_request().status ==
          flexUI::ApplicationServicePollStatus::Empty);
    const auto stats = built.application->service_statistics();
    check_equal(stats.current_pending, std::size_t{0});
    check_equal(stats.discarded, std::uint64_t{1});
    check_equal(stats.published, std::uint64_t{0});
  }

  it("rejects service saturation before applying same-callback UI mutations") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationLimits limits;
    limits.service_requests.capacity = 1;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe))
        .limits(limits);
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto *save = built.application->box().get_by_id("save");
    save->set_text("Before");
    probe->service_request_id = 72;
    probe->emit_valid_mutation = true;

    const auto saturated = dispatch_click(*built.application);
    check_false(static_cast<bool>(saturated));
    check(saturated.error.controller_error.code == flexUI::ControllerErrorCode::CommandFailed);
    check(saturated.error.controller_error.command_error.code ==
          flexUI::ApplicationCommandErrorCode::QueueFull);
    check_equal(save->text(), std::string("Before"));
    check_equal(built.application->service_statistics().current_pending, std::size_t{1});
  }

  it("rejects unauthorized service commands before same-callback UI mutation") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    probe->emit_valid_mutation = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("unauthorized-service", fake_factory(probe));
    auto built = builder.build();
    check(built);
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    auto *save = built.application->box().get_by_id("save");
    save->set_text("Before");
    auto dispatched = dispatch_click(*built.application);
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.controller_error.code ==
          flexUI::ControllerErrorCode::CommandFailed);
    check(dispatched.error.controller_error.command_error.code ==
          flexUI::ApplicationCommandErrorCode::UnknownCapability);
    check_equal(save->text(), std::string("Before"));
    check_equal(built.application->service_statistics().current_pending,
                std::size_t{0});
  }

  it("rejects a missing required service while building the application") {
    flexUI::ApplicationServiceRegistryBuilder registry_builder;
    auto registry = registry_builder.build();
    check(registry);
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).services(
        std::move(registry.registry),
        {{"document.storage/1"}, {"document.storage/1"}});

    auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code ==
          flexUI::DesktopApplicationErrorCode::ServiceRegistryFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::ServiceRegistry);
    check(built.error.registry_error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::MissingRequiredCapability);
  }

  it("cancels pending service identities when reload publishes a new generation") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe));
    auto endpoint = configure_storage_service(builder);
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto old_request = built.application->try_receive_service_request();
    check(old_request);
    auto old_endpoint =
        built.application->resolve_service_request(*old_request.request);
    check(old_endpoint);
    check(old_endpoint.endpoint == endpoint);
    const auto old_generation = built.application->generation();

    check(built.application->reload({xml_entry("replacement"),
                                     "#replacement { width: 120px; height: 36px; }",
                                     "replacement-controller", "replacement-module"}));
    check_equal(built.application->generation(), old_generation + 1);
    check_equal(built.application->service_statistics().cancelled, std::uint64_t{1});

    auto stale_resolution =
        built.application->resolve_service_request(*old_request.request);
    check_false(static_cast<bool>(stale_resolution));
    check(stale_resolution.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidRequest);

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto current_request = built.application->try_receive_service_request();
    check(current_request);
    auto current_endpoint =
        built.application->resolve_service_request(*current_request.request);
    check(current_endpoint);
    check(current_endpoint.endpoint == endpoint);

    flexUI::ApplicationCompletion stale;
    stale.token = old_request.request->token;
    auto rejected = built.application->completion_mailbox().try_post(stale);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code == flexUI::ApplicationCompletionErrorCode::StaleGeneration);
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

spec("FlexUI desktop application owns close and shutdown state") {
  it("dispatches validated owner-thread frames and reports controller failures") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto frame = built.application->frame(0.016);
    check(static_cast<bool>(frame));
    check_equal(probe->frame_deltas.size(), std::size_t{1});
    if (!probe->frame_deltas.empty()) {
      check_within(probe->frame_deltas.front(), 0.016, 0.000001);
    }

    const auto invalid = built.application->frame(std::numeric_limits<double>::quiet_NaN());
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == flexUI::DesktopApplicationErrorCode::InvalidArgument);
    check(invalid.error.stage == flexUI::DesktopApplicationStage::ControllerFrame);

    check(built.application->request_close());
    const auto closed = built.application->frame(0.016);
    check_false(static_cast<bool>(closed));
    check(closed.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
  }

  it("nests on_frame module errors and faults the controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("frame-failure", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto frame = built.application->frame(0.016);
    check_false(static_cast<bool>(frame));
    check(frame.error.code == flexUI::DesktopApplicationErrorCode::ControllerFrameFailed);
    check(frame.error.controller_error.stage == flexUI::ControllerStage::Frame);
    check(built.application->controller()->state() == flexUI::ControllerState::Faulted);
  }

  it("rejects frames from a non-owner thread") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::DesktopApplicationResult frame_result;
    std::thread worker([&] { frame_result = built.application->frame(0.016); });
    worker.join();

    check_false(static_cast<bool>(frame_result));
    check(frame_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check(frame_result.error.stage == flexUI::DesktopApplicationStage::ControllerFrame);
  }

  it("stops new work after close and unmounts exactly once during shutdown") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
    const auto premature_shutdown = built.application->shutdown();
    check_false(static_cast<bool>(premature_shutdown));
    check(premature_shutdown.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
    check(premature_shutdown.error.stage == flexUI::DesktopApplicationStage::Lifecycle);
    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
    check(built.application->request_close());
    check(built.application->request_close());
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);

    auto event = flexUI::Event::mouse_move(1.0F, 2.0F);
    const auto dispatch = built.application->dispatch_event(event);
    const auto reload =
        built.application->reload({"<ui name=\"Closed\"><div id=\"closed\"/></ui>", "", "", ""});
    check_false(static_cast<bool>(dispatch));
    check(dispatch.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
    check_false(static_cast<bool>(reload));
    check(reload.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);

    check(built.application->shutdown());
    check(built.application->state() == flexUI::DesktopApplicationState::Shutdown);
    check_not_null(built.application->box().get_by_id("save"));
    check(built.application->controller()->state() == flexUI::ControllerState::Empty);
    check(built.application->shutdown());
    check_equal(probe->calls.size(), std::size_t{2});
    if (probe->calls.size() == 2) {
      check_equal(probe->calls[0], "on_mount");
      check_equal(probe->calls[1], "on_unmount");
    }
  }

  it("closes the completion mailbox before publishing CloseRequested") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto generation = built.application->generation();
    flexUI::ApplicationCompletion queued;
    queued.token = {1, generation};
    queued.payload = "finished";
    check(built.application->completion_mailbox().try_post(queued));

    check(built.application->request_close());
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);
    check(built.application->completion_mailbox().state() ==
          flexUI::ApplicationCompletionMailboxState::Closed);
    check_equal(built.application->completion_mailbox().statistics().cancelled, std::uint64_t{1});

    flexUI::ApplicationCompletion late;
    late.token = {2, generation};
    auto rejected = built.application->completion_mailbox().try_post(late);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code == flexUI::ApplicationCompletionErrorCode::Closed);
  }

  it("advances completion generation atomically with application reload") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Before\"><div id=\"before\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto old_generation = built.application->generation();
    flexUI::ApplicationCompletion queued;
    queued.token = {1, old_generation};
    check(built.application->completion_mailbox().try_post(queued));

    check(built.application->reload({"<ui name=\"After\"><div id=\"after\"/></ui>", "", "", ""}));
    const auto current_generation = built.application->generation();
    check_equal(current_generation, old_generation + 1);
    check_not_null(built.application->box().get_by_id("after"));
    check_equal(built.application->completion_mailbox().statistics().cancelled, std::uint64_t{1});

    flexUI::ApplicationCompletion stale;
    stale.token = {2, old_generation};
    auto rejected = built.application->completion_mailbox().try_post(stale);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code == flexUI::ApplicationCompletionErrorCode::StaleGeneration);

    flexUI::ApplicationCompletion current;
    current.token = {3, current_generation};
    check(built.application->completion_mailbox().try_post(current));
    auto received = built.application->completion_mailbox().try_receive();
    check(received.completion.has_value());
    check_equal(received.completion->token.id, std::uint64_t{3});
  }

  it("fails application publication when completion limits are invalid") {
    flexUI::DesktopApplicationLimits limits;
    limits.completion.capacity = 3;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>").limits(limits);

    auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::DesktopApplicationErrorCode::CompletionMailboxFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::CompletionMailbox);
    check(built.error.completion_error.code ==
          flexUI::ApplicationCompletionErrorCode::InvalidCapacity);
  }

  it("fails application publication when service request limits are invalid") {
    flexUI::DesktopApplicationLimits limits;
    limits.service_requests.capacity = 0;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>").limits(limits);

    auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ServiceRequestFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::ServiceRequests);
    check(built.error.service_error.code == flexUI::ApplicationServiceErrorCode::InvalidCapacity);
  }

  it("cancels pending service requests before publishing CloseRequested") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    check_equal(built.application->service_statistics().current_pending, std::size_t{1});

    check(built.application->request_close());
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);
    check_equal(built.application->service_statistics().cancelled, std::uint64_t{1});
    check(built.application->try_receive_service_request().status ==
          flexUI::ApplicationServicePollStatus::Closed);
    check(built.application->try_receive_service_completion().status ==
          flexUI::ApplicationServicePollStatus::Closed);
  }

  it("rejects service polling from a non-owner thread") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::ApplicationServiceRequestResult request;
    flexUI::ApplicationServiceCompletionResult completion;
    std::thread worker([&] {
      request = built.application->try_receive_service_request();
      completion = built.application->try_receive_service_completion();
    });
    worker.join();

    check(request.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(completion.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
  }

  it("retains shutdown state while reporting an on_unmount failure") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("unmount-failure", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.application->request_close());
    const auto shutdown = built.application->shutdown();

    check_false(static_cast<bool>(shutdown));
    check(shutdown.error.code == flexUI::DesktopApplicationErrorCode::ControllerUnmountFailed);
    check(shutdown.error.controller_error.stage == flexUI::ControllerStage::Unmount);
    check(built.application->state() == flexUI::DesktopApplicationState::Shutdown);
    check(built.application->controller()->state() == flexUI::ControllerState::Empty);
    check_not_null(built.application->box().get_by_id("save"));
  }

  it("rejects reentrant shutdown without losing the later cleanup opportunity") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->shutdown_during_event = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("shutdown-during-event", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    probe->application = built.application.get();
    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();

    check(dispatch_click(*built.application));
    check_false(static_cast<bool>(probe->reentrant_shutdown));
    check(probe->reentrant_shutdown.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidState);
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);

    probe->shutdown_during_event = false;
    check(built.application->request_close());
    check(built.application->shutdown());
    check(built.application->state() == flexUI::DesktopApplicationState::Shutdown);
  }

  it("rejects lifecycle transitions from a non-owner thread") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::DesktopApplicationResult close_result;
    flexUI::DesktopApplicationResult shutdown_result;
    std::thread worker([&] {
      close_result = built.application->request_close();
      shutdown_result = built.application->shutdown();
    });
    worker.join();

    check_false(static_cast<bool>(close_result));
    check(close_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check(close_result.error.stage == flexUI::DesktopApplicationStage::Lifecycle);
    check_false(static_cast<bool>(shutdown_result));
    check(shutdown_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check(shutdown_result.error.stage == flexUI::DesktopApplicationStage::Lifecycle);
    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
  }
}
