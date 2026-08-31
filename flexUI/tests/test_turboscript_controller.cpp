#include <flexUI/box.h>
#include <flexUI/box_mutation_host.h>
#include <flexUI/controller_turboscript.h>

#include <tinytest.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace {

std::shared_ptr<const flexUI::CompiledUiProgram> controller_program() {
  const auto compiled =
      flexUI::compile_ui_document("ui Main { button save { on.click: \"save_document\" } }");
  check(static_cast<bool>(compiled));
  return compiled.program;
}

flexUI::TurboScriptModuleCreateResult
create_module(std::string_view source, flexUI::TurboScriptExecutionMode mode,
              flexUI::TurboScriptInterruptCallback interrupt = nullptr,
              void *interrupt_user_data = nullptr) {
  flexUI::TurboScriptControllerOptions options;
  options.execution_mode = mode;
  options.interrupt = interrupt;
  options.interrupt_user_data = interrupt_user_data;
  return flexUI::create_turboscript_controller_module(source, "flexui-controller-test", options);
}

flexUI::ScriptEventSnapshot click_event() {
  flexUI::ScriptEventSnapshot event;
  event.event = flexUI::UiEventKind::Click;
  event.target = {"save", 7};
  event.x = 12.5;
  event.y = 24.0;
  event.timestamp_ms = 42.0;
  return event;
}

void run_lifecycle(flexUI::TurboScriptExecutionMode mode) {
  static constexpr std::string_view source =
      "state=0;"
      "func on_mount(){state=1;return null;};"
      "func on_frame(delta){if(state!=1){throw \"frame state\";};"
      "if(delta!=0.25){throw \"frame delta\";};state=2;return null;};"
      "func save_document(event){if(state!=2){throw \"event state\";};"
      "if(event.x!=12.5){throw \"event x\";};"
      "if(event.target.generation!=7){throw \"event generation\";};"
      "if(event.current_target.generation!=7){throw \"current target generation\";};"
      "if(!event.current_target_valid){throw \"current target validity\";};"
      "state=3;return null;};"
      "func on_unmount(){if(state!=3){throw \"unmount state\";};"
      "state=4;return null;};"
      "export(\"on_mount\");export(\"on_frame\");"
      "export(\"save_document\");export(\"on_unmount\");";
  auto created = create_module(source, mode);
  check(static_cast<bool>(created));
  if (!created) {
    return;
  }

  flexUI::ScriptController controller;
  check(controller.load(std::move(created.module), controller_program()));
  check(controller.mount());
  check(controller.frame(0.25));
  check(controller.dispatch(click_event()));
  check(controller.unmount());
  check(controller.state() == flexUI::ControllerState::Empty);
}

bool interrupt_immediately(void *user_data) {
  auto *checks = static_cast<std::uint32_t *>(user_data);
  ++*checks;
  return true;
}

} // namespace

spec("FlexUI TurboScript controller adapter") {
  it("runs the controller lifecycle with the interpreter") {
    run_lifecycle(flexUI::TurboScriptExecutionMode::Interpreter);
  }

  it("runs the controller lifecycle with the JIT") {
    run_lifecycle(flexUI::TurboScriptExecutionMode::Jit);
  }

  it("reports source compilation errors without exposing a partial module") {
    const auto created =
        create_module("func broken( {", flexUI::TurboScriptExecutionMode::Interpreter);
    check_false(static_cast<bool>(created));
    check_null(created.module.get());
    check(created.error.code == flexUI::ScriptModuleErrorCode::CompileFailure);
    check_false(created.error.message.empty());
  }

  it("fails a callback whose export arity does not match") {
    auto created = create_module("func on_mount(value){return value;};"
                                 "func save_document(event){return event;};"
                                 "export(\"on_mount\");export(\"save_document\");",
                                 flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    flexUI::ScriptController controller;
    const auto loaded = controller.load(std::move(created.module), controller_program());
    check_false(static_cast<bool>(loaded));
    check(loaded.error.code == flexUI::ControllerErrorCode::ModuleResolutionFailed);
    check(loaded.error.module_error.code == flexUI::ScriptModuleErrorCode::InvalidExport);
    check(controller.state() == flexUI::ControllerState::Empty);
  }

  it("maps a per-call loop quota breach into a resource failure") {
    flexUI::TurboScriptControllerOptions options;
    options.execution_mode = flexUI::TurboScriptExecutionMode::Interpreter;
    options.call_limits.max_loop_iterations = 2;
    auto created = flexUI::create_turboscript_controller_module(
        "func on_frame(delta){i=0;while(i<3){i=i+1;};};"
        "func save_document(event){return event;};"
        "export(\"on_frame\");export(\"save_document\");",
        "flexui-quota-test", options);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    flexUI::ScriptController controller;
    check(controller.load(std::move(created.module), controller_program()));
    check(controller.mount());
    const auto framed = controller.frame(0.016);
    check_false(static_cast<bool>(framed));
    check(framed.error.module_error.code == flexUI::ScriptModuleErrorCode::ResourceLimitExceeded);
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("interrupts an unbounded callback and faults the controller") {
    std::uint32_t checks = 0;
    auto created =
        create_module("func on_frame(delta){i=0;while(1){i=i+1;};};"
                      "func save_document(event){return event;};"
                      "export(\"on_frame\");export(\"save_document\");",
                      flexUI::TurboScriptExecutionMode::Jit, interrupt_immediately, &checks);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    flexUI::ScriptController controller;
    check(controller.load(std::move(created.module), controller_program()));
    check(controller.mount());
    const auto framed = controller.frame(0.016);
    check_false(static_cast<bool>(framed));
    check(framed.error.code == flexUI::ControllerErrorCode::ModuleCallFailed);
    check(framed.error.module_error.code == flexUI::ScriptModuleErrorCode::Interrupted);
    check_greater_equal(checks, std::uint32_t{1});
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("maps an uncaught script exception into a runtime failure") {
    auto created = create_module("func on_frame(delta){throw \"frame failed\";};"
                                 "func save_document(event){return event;};"
                                 "export(\"on_frame\");export(\"save_document\");",
                                 flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    flexUI::ScriptController controller;
    check(controller.load(std::move(created.module), controller_program()));
    check(controller.mount());
    const auto framed = controller.frame(0.016);
    check_false(static_cast<bool>(framed));
    check(framed.error.module_error.code == flexUI::ScriptModuleErrorCode::RuntimeFailure);
    check_false(framed.error.module_error.message.empty());
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("rejects export resolution from a non-owner thread") {
    auto created = create_module("func save_document(event){return event;};"
                                 "export(\"save_document\");",
                                 flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    flexUI::ScriptResolveResult resolved;
    std::thread worker([&] {
      resolved = created.module->resolve_export("save_document", flexUI::ScriptCallbackKind::Event);
    });
    worker.join();
    check_false(static_cast<bool>(resolved));
    check(resolved.error.code == flexUI::ScriptModuleErrorCode::RuntimeFailure);
  }

  it("decodes one transactional effect envelope") {
    auto created = create_module(
        "func emit(event){return map {"
        "mutations:list("
        "map {type:\"set_text\",target:event.target,text:\"\"},"
        "map {type:\"set_attribute\",target:event.target,name:\"role\",value:\"button\"},"
        "map {type:\"remove_attribute\",target:event.target,name:\"old\"},"
        "map {type:\"set_classes\",target:event.target,classes:\"primary\"},"
        "map {type:\"set_utilities\",target:event.target,utilities:\"px-2\"},"
        "map {type:\"set_binding_number\",name:\"progress\",value:0.5},"
        "map {type:\"set_binding_bool\",name:\"ready\",value:true},"
        "map {type:\"set_binding_string\",name:\"title\",value:\"Saved\"}),"
        "commands:list(map {request_id:41,capability:\"storage\","
        "operation:\"write\",payload:\"{}\"})};};export(\"emit\");",
        flexUI::TurboScriptExecutionMode::Jit);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    const auto resolved = created.module->resolve_export("emit", flexUI::ScriptCallbackKind::Event);
    check(static_cast<bool>(resolved));
    check(resolved.handle.has_value());
    if (!resolved || !resolved.handle.has_value()) {
      return;
    }
    const auto event = click_event();
    flexUI::ScriptCallContext context;
    context.callback = flexUI::ScriptCallbackKind::Event;
    context.event = &event;
    const auto called = created.module->call(*resolved.handle, context);
    check(static_cast<bool>(called));
    check_equal(called.mutations.size(), std::size_t{8});
    check_equal(called.commands.size(), std::size_t{1});
    if (called.mutations.size() == 8) {
      check(std::holds_alternative<flexUI::SetTextMutation>(called.mutations.mutations()[0]));
      const auto &text = std::get<flexUI::SetTextMutation>(called.mutations.mutations()[0]);
      check_equal(text.target.id, "save");
      check_equal(text.target.generation, std::uint64_t{7});
      check(text.text.empty());
      check(std::holds_alternative<flexUI::SetBindingInputStringMutation>(
          called.mutations.mutations()[7]));
    }
    if (called.commands.size() == 1) {
      const auto &command = called.commands.commands()[0];
      check_equal(command.request_id, std::uint64_t{41});
      check_equal(command.capability, "storage");
      check_equal(command.operation, "write");
      check_equal(command.payload, "{}");
    }
  }

  it("rejects malformed effect envelopes without partial output") {
    auto created = create_module("func emit(event){return map {mutations:list("
                                 "map {type:\"set_text\",target:event.target,text:\"valid\"},"
                                 "map {type:\"unknown\"})};};export(\"emit\");",
                                 flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    const auto resolved = created.module->resolve_export("emit", flexUI::ScriptCallbackKind::Event);
    check(static_cast<bool>(resolved));
    if (!resolved.handle.has_value()) {
      return;
    }
    const auto event = click_event();
    const auto called =
        created.module->call(*resolved.handle, {flexUI::ScriptCallbackKind::Event, &event, 0.0});
    check_false(static_cast<bool>(called));
    check(called.error.code == flexUI::ScriptModuleErrorCode::RuntimeFailure);
    check(called.mutations.empty());
    check(called.commands.empty());
  }

  it("rejects an oversized mutation batch without partial output") {
    std::string source = "func emit(event){return map {mutations:list(";
    for (std::size_t index = 0; index <= flexUI::MutationLimits::kDefaultMaxMutations; ++index) {
      if (index != 0) {
        source += ',';
      }
      source += "map {type:\"set_text\",target:event.target,text:\"x\"}";
    }
    source += ")};};export(\"emit\");";

    auto created = create_module(source, flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    const auto resolved = created.module->resolve_export("emit", flexUI::ScriptCallbackKind::Event);
    check(static_cast<bool>(resolved));
    if (!resolved.handle.has_value()) {
      return;
    }
    const auto event = click_event();
    const auto called =
        created.module->call(*resolved.handle, {flexUI::ScriptCallbackKind::Event, &event, 0.0});
    check_false(static_cast<bool>(called));
    check(called.error.code == flexUI::ScriptModuleErrorCode::ResourceLimitExceeded);
    check(called.mutations.empty());
    check(called.commands.empty());
  }

  it("rejects callback records outside the effect schema") {
    auto created = create_module("func emit(event){return event;};export(\"emit\");",
                                 flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    const auto resolved = created.module->resolve_export("emit", flexUI::ScriptCallbackKind::Event);
    check(static_cast<bool>(resolved));
    if (!resolved.handle.has_value()) {
      return;
    }
    const auto event = click_event();
    const auto called =
        created.module->call(*resolved.handle, {flexUI::ScriptCallbackKind::Event, &event, 0.0});
    check_false(static_cast<bool>(called));
    check(called.error.code == flexUI::ScriptModuleErrorCode::RuntimeFailure);
    check(called.mutations.empty());
    check(called.commands.empty());
  }

  it("commits a real TurboScript mutation through the controller and Box") {
    flexUI::Box box(nullptr);
    auto *save = box.create("button", "save");
    box.set_root(save);
    save->set_text("before");

    auto created = create_module("func save_document(event){return map {mutations:list("
                                 "map {type:\"set_text\",target:event.target,"
                                 "text:\"committed by TurboScript\"})};};"
                                 "export(\"save_document\");",
                                 flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }

    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);
    auto event = click_event();
    event.target = box.handle_for(*save);

    check(controller.load(std::move(created.module), controller_program()));
    check(controller.mount());
    check(controller.dispatch(event));
    check_equal(save->text(), std::string("committed by TurboScript"));
    check(controller.state() == flexUI::ControllerState::Mounted);
    check(controller.unmount());
  }

  it("keeps a missing event export as a controller load error") {
    auto created = create_module("func on_mount(){return 1;};export(\"on_mount\");",
                                 flexUI::TurboScriptExecutionMode::Interpreter);
    check(static_cast<bool>(created));
    if (!created) {
      return;
    }
    flexUI::ScriptController controller;
    const auto loaded = controller.load(std::move(created.module), controller_program());
    check_false(static_cast<bool>(loaded));
    check(loaded.error.code == flexUI::ControllerErrorCode::MissingHandlerExport);
    check_equal(loaded.error.handler, "save_document");
    check(controller.state() == flexUI::ControllerState::Empty);
  }
}
