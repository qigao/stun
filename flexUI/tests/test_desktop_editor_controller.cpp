#include <flexUI/controller_turboscript.h>

#include <tinytest.hpp>
#include <turbo_fs.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

#ifndef FLEXUI_EDITOR_CONTROLLER_PATH
  #error FLEXUI_EDITOR_CONTROLLER_PATH must name the desktop editor TurboScript source
#endif

namespace {

constexpr std::uint64_t kMaximumControllerBytes = 64U * 1024U;
constexpr std::uint64_t kSaveGeneration = 7;

struct LoadedController {
  std::unique_ptr<flexUI::IScriptModule> module;
  flexUI::ScriptExportHandle save;
  flexUI::ScriptExportHandle completion;
};

std::string load_controller_source() {
  turbo_fs_stat_t metadata{};
  check_equal(turbo_fs_stat(FLEXUI_EDITOR_CONTROLLER_PATH, &metadata), 0);
  check_true(metadata.is_file);
  check_greater(metadata.size, std::uint64_t{0});
  check_less(metadata.size, kMaximumControllerBytes);
  if (!metadata.is_file || metadata.size == 0 || metadata.size >= kMaximumControllerBytes) {
    return {};
  }

  turbo_fs_buf_t bytes{};
  check_equal(turbo_fs_read_file(FLEXUI_EDITOR_CONTROLLER_PATH, &bytes), 0);
  if (bytes.base == nullptr || bytes.len == 0 || bytes.len >= kMaximumControllerBytes) {
    turbo_fs_buf_free(&bytes);
    return {};
  }
  std::string source(bytes.base, bytes.len);
  turbo_fs_buf_free(&bytes);
  return source;
}

LoadedController load_controller() {
  const auto source = load_controller_source();
  if (source.empty()) {
    return {};
  }

  flexUI::TurboScriptControllerOptions options;
  options.execution_mode = flexUI::TurboScriptExecutionMode::Jit;
  auto created = flexUI::create_turboscript_controller_module(
      source, "flexui-desktop-editor-contract", options);
  check(static_cast<bool>(created), "%s", created.error.message.c_str());
  if (!created) {
    return {};
  }

  const auto save =
      created.module->resolve_export("save_document", flexUI::ScriptCallbackKind::Event);
  check(static_cast<bool>(save), "%s", save.error.message.c_str());
  check(save.handle.has_value());
  const auto completion = created.module->resolve_export(
      "on_service_completion", flexUI::ScriptCallbackKind::ServiceCompletion);
  check(static_cast<bool>(completion), "%s", completion.error.message.c_str());
  check(completion.handle.has_value());
  if (!save || !save.handle.has_value() || !completion || !completion.handle.has_value()) {
    return {};
  }
  return {std::move(created.module), *save.handle, *completion.handle};
}

flexUI::ScriptCallResult call_save(LoadedController &controller) {
  flexUI::ScriptEventSnapshot event;
  event.event = flexUI::UiEventKind::Click;
  event.target = {"save", kSaveGeneration};
  event.current_target = event.target;

  flexUI::ScriptCallContext context;
  context.callback = flexUI::ScriptCallbackKind::Event;
  context.event = &event;
  return controller.module->call(controller.save, context);
}

flexUI::ScriptCallResult call_completion(LoadedController &controller, std::uint64_t request_id,
                                         flexUI::ApplicationCompletionStatus status,
                                         std::string payload = {}, std::string error_code = {},
                                         std::string error_message = {}) {
  const flexUI::ApplicationServiceCompletion completion{
      request_id, status, std::move(payload), std::move(error_code), std::move(error_message)};
  flexUI::ScriptCallContext context;
  context.callback = flexUI::ScriptCallbackKind::ServiceCompletion;
  context.service_completion = &completion;
  return controller.module->call(controller.completion, context);
}

void check_text_mutation(const flexUI::ScriptCallResult &result, const std::string &text) {
  check(static_cast<bool>(result), "%s", result.error.message.c_str());
  check_equal(result.mutations.size(), std::size_t{1});
  if (result.mutations.size() != 1) {
    return;
  }
  const auto &mutation = result.mutations.mutations().front();
  check(std::holds_alternative<flexUI::SetTextMutation>(mutation));
  if (!std::holds_alternative<flexUI::SetTextMutation>(mutation)) {
    return;
  }
  const auto &set_text = std::get<flexUI::SetTextMutation>(mutation);
  check_equal(set_text.target.id, std::string("save"));
  check_equal(set_text.target.generation, kSaveGeneration);
  check_equal(set_text.text, text);
}

} // namespace

spec("FlexUI desktop editor TurboScript controller") {
  it("emits one save request and gates a concurrent click") {
    auto controller = load_controller();
    check_not_null(controller.module.get());
    if (!controller.module) {
      return;
    }

    const auto first = call_save(controller);
    check_text_mutation(first, "Saving...");
    check_equal(first.commands.size(), std::size_t{1});
    if (first.commands.size() == 1) {
      const auto &command = first.commands.commands().front();
      check_equal(command.request_id, std::uint64_t{1});
      check_equal(command.capability, std::string("document.save/1"));
      check_equal(command.operation, std::string("save"));
      check_equal(command.payload, std::string("Saved by native document service"));
    }

    const auto duplicate = call_save(controller);
    check(static_cast<bool>(duplicate), "%s", duplicate.error.message.c_str());
    check(duplicate.mutations.empty());
    check(duplicate.commands.empty());
  }

  it("recovers from failure after releasing the single in-flight identity") {
    auto controller = load_controller();
    check_not_null(controller.module.get());
    if (!controller.module) {
      return;
    }

    check(static_cast<bool>(call_save(controller)));
    const auto failed = call_completion(controller, 1, flexUI::ApplicationCompletionStatus::Failed,
                                        {}, "write-failed", "disk full");
    check_text_mutation(failed, "disk full");

    const auto second = call_save(controller);
    check(static_cast<bool>(second), "%s", second.error.message.c_str());
    check_equal(second.commands.size(), std::size_t{1});
    if (second.commands.size() == 1) {
      check_equal(second.commands.commands().front().request_id, std::uint64_t{1});
    }
  }

  it("applies one successful completion and rejects a duplicate") {
    auto controller = load_controller();
    check_not_null(controller.module.get());
    if (!controller.module) {
      return;
    }

    check(static_cast<bool>(call_save(controller)));
    const auto succeeded =
        call_completion(controller, 1, flexUI::ApplicationCompletionStatus::Succeeded, "Saved");
    check_text_mutation(succeeded, "Saved");

    const auto duplicate =
        call_completion(controller, 1, flexUI::ApplicationCompletionStatus::Succeeded, "duplicate");
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code == flexUI::ScriptModuleErrorCode::RuntimeFailure);
    check(duplicate.mutations.empty());
    check(duplicate.commands.empty());
  }

  it("rejects a completion that does not match the active request") {
    auto controller = load_controller();
    check_not_null(controller.module.get());
    if (!controller.module) {
      return;
    }

    check(static_cast<bool>(call_save(controller)));
    const auto stale =
        call_completion(controller, 2, flexUI::ApplicationCompletionStatus::Succeeded, "stale");
    check_false(static_cast<bool>(stale));
    check(stale.error.code == flexUI::ScriptModuleErrorCode::RuntimeFailure);
    check(stale.mutations.empty());
    check(stale.commands.empty());
  }
}
