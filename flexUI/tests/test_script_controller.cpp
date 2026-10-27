#include <flexUI/box.h>
#include <flexUI/box_mutation_host.h>
#include <flexUI/controller.h>
#include <flexUI/ui_xml.h>
#include <flexUI/widget_registry.h>

#include <tinytest.hpp>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

class FakeScriptModule final : public flexUI::IScriptModule {
public:
  explicit FakeScriptModule(std::vector<std::string> exports,
                            std::vector<std::string> *lifecycle = nullptr)
      : lifecycle_(lifecycle) {
    std::uint64_t next_handle = 1;
    for (auto &name : exports) {
      const flexUI::ScriptExportHandle handle{next_handle++};
      handles_.emplace(name, handle);
      names_.emplace(handle.value, std::move(name));
    }
  }

  ~FakeScriptModule() override {
    if (lifecycle_ != nullptr) {
      lifecycle_->push_back("destroy");
    }
  }

  flexUI::ScriptResolveResult
  resolve_export(std::string_view name,
                 flexUI::ScriptCallbackKind callback) override {
    resolved.emplace_back(name, callback);
    if (name == zero_handle_export) {
      return {flexUI::ScriptExportHandle{}, {}};
    }
    const auto found = handles_.find(std::string(name));
    if (found == handles_.end()) {
      return {};
    }
    return {found->second, {}};
  }

  flexUI::ScriptCallResult
  call(flexUI::ScriptExportHandle handle,
       const flexUI::ScriptCallContext &context) override {
    const auto found = names_.find(handle.value);
    if (found == names_.end()) {
      return {{flexUI::ScriptModuleErrorCode::InvalidExport,
               "unknown fake export"}};
    }
    calls.push_back(found->second);
    if (on_event_call && context.callback == flexUI::ScriptCallbackKind::Event)
      on_event_call();
    if (lifecycle_ != nullptr) {
      lifecycle_->push_back(found->second);
    }
    if (context.service_completion != nullptr) {
      service_completions.push_back(*context.service_completion);
    }
    if (found->second == failing_export) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
               "injected callback failure"}};
    }
    if (found->second == timeout_export) {
      return {{flexUI::ScriptModuleErrorCode::Timeout,
               "injected callback timeout"}};
    }
    if (found->second == throwing_export) {
      throw std::runtime_error("injected script exception");
    }
    flexUI::ScriptCallResult result;
    if (found->second == mutation_export) {
      const flexUI::UiHandle target =
          context.event != nullptr ? context.event->target
                                   : flexUI::UiHandle{"save", 1};
      const auto appended = result.mutations.append(
          flexUI::SetTextMutation{target, mutation_text});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                 appended.error.message}};
      }
    }
    if (found->second == command_export) {
      const auto appended = result.commands.append(
          {41, "storage", "write", command_payload});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                 appended.error.message}};
      }
    }
    return result;
  }

  std::vector<std::string> calls;
  std::vector<std::pair<std::string, flexUI::ScriptCallbackKind>> resolved;
  std::vector<flexUI::ApplicationServiceCompletion> service_completions;
  std::string failing_export;
  std::string timeout_export;
  std::string throwing_export;
  std::string zero_handle_export;
  std::string mutation_export;
  std::string mutation_text;
  std::string command_export;
  std::string command_payload;
  std::function<void()> on_event_call;

private:
  std::unordered_map<std::string, flexUI::ScriptExportHandle> handles_;
  std::unordered_map<std::uint64_t, std::string> names_;
  std::vector<std::string> *lifecycle_ = nullptr;
};

std::shared_ptr<const flexUI::CompiledUiProgram> controller_program() {
  const auto compiled = flexUI::compile_ui_document(
      "ui Main { button save { on.click: \"save_document\" } }");
  check(static_cast<bool>(compiled));
  return compiled.program;
}

flexUI::ScriptEventSnapshot click_event() {
  flexUI::ScriptEventSnapshot event;
  event.event = flexUI::UiEventKind::Click;
  event.target = {"save", 1};
  event.timestamp_ms = 42.0;
  return event;
}

std::shared_ptr<const flexUI::CompiledUiProgram> scoped_controller_program() {
  const auto base = flexUI::compile_ui_xml(
      "<ui name=\"App\"><main id=\"root\">"
      "<section id=\"sale_host\"/><section id=\"sales_host\"/></main></ui>");
  const auto panel = flexUI::compile_ui_xml(
      "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
      "<div id=\"panel\">"
      "<div id=\"action\" on:click=\"save_document\">Action</div>"
      "<div id=\"nested\" on:key_down=\"parent_handler\">Nested</div>"
      "</div></ui>");
  check(static_cast<bool>(base));
  check(static_cast<bool>(panel));
  if (!base || !panel) return {};
  const auto combined = flexUI::compile_scoped_ui_components(
      *base.program,
      {{"sale", "sale_host", panel.program},
       {"sales", "sales_host", panel.program}},
      flexUI::WidgetRegistry::builtins());
  check(static_cast<bool>(combined));
  return combined.program;
}

flexUI::ScriptEventSnapshot scoped_event(
    std::string id, flexUI::UiEventKind event = flexUI::UiEventKind::Click) {
  flexUI::ScriptEventSnapshot snapshot;
  snapshot.event = event;
  snapshot.target = {std::move(id), 1};
  snapshot.current_target = snapshot.target;
  return snapshot;
}

void check_calls(const std::vector<std::string> &actual,
                 std::initializer_list<std::string_view> expected) {
  check_equal(actual.size(), expected.size());
  if (actual.size() != expected.size()) {
    return;
  }
  std::size_t index = 0;
  for (const auto item : expected) {
    check_equal(actual[index], std::string(item));
    ++index;
  }
}

} // namespace

namespace {

class ControllerPreparedMutation final : public flexUI::IPreparedUiMutation {
public:
  ControllerPreparedMutation(int &commits, std::string &text,
                             std::string next_text,
                             std::vector<std::string> *lifecycle)
      : commits_(commits), text_(text), next_text_(std::move(next_text)),
        lifecycle_(lifecycle) {}

  void commit() noexcept override {
    text_.swap(next_text_);
    ++commits_;
    if (lifecycle_ != nullptr) {
      lifecycle_->push_back("ui_commit");
    }
  }

private:
  int &commits_;
  std::string &text_;
  std::string next_text_;
  std::vector<std::string> *lifecycle_ = nullptr;
};

class ControllerMutationHost final : public flexUI::IUiMutationHost {
public:
  explicit ControllerMutationHost(
      std::vector<std::string> *lifecycle = nullptr)
      : lifecycle(lifecycle) {}

  flexUI::MutationPrepareResult
  prepare(const flexUI::UiMutationBatch &batch) override {
    ++prepare_calls;
    if (lifecycle != nullptr) {
      lifecycle->push_back("ui_prepare");
    }
    if (fail_prepare) {
      return {{}, {flexUI::MutationErrorCode::HostPrepareFailed, 0,
                   "injected controller prepare failure"}};
    }
    const auto &mutation =
        std::get<flexUI::SetTextMutation>(batch.mutations().front());
    return {std::make_unique<ControllerPreparedMutation>(
                commits, text, mutation.text, lifecycle),
            {}};
  }

  int prepare_calls = 0;
  int commits = 0;
  std::string text = "unchanged";
  bool fail_prepare = false;
  std::vector<std::string> *lifecycle = nullptr;
};

class ControllerPreparedCommands final
    : public flexUI::IPreparedApplicationCommands {
public:
  ControllerPreparedCommands(int &reserved, int &published,
                             std::vector<std::string> *lifecycle)
      : reserved_(reserved), published_(published), lifecycle_(lifecycle) {}

  ~ControllerPreparedCommands() override {
    if (!published_once_) {
      --reserved_;
    }
  }

  void publish() noexcept override {
    if (published_once_) {
      return;
    }
    published_once_ = true;
    --reserved_;
    ++published_;
    if (lifecycle_ != nullptr) {
      lifecycle_->push_back("publish");
    }
  }

private:
  int &reserved_;
  int &published_;
  std::vector<std::string> *lifecycle_ = nullptr;
  bool published_once_ = false;
};

class ControllerCommandQueue final : public flexUI::IApplicationCommandQueue {
public:
  explicit ControllerCommandQueue(
      std::vector<std::string> *lifecycle = nullptr)
      : lifecycle(lifecycle) {}

  flexUI::ApplicationCommandReserveResult
  reserve(const flexUI::ApplicationCommandBatch &batch) override {
    observed_scopes.emplace_back(batch.origin_scope());
    ++reserve_calls;
    if (lifecycle != nullptr) {
      lifecycle->push_back("reserve");
    }
    if (fail_reserve) {
      return {{}, {flexUI::ApplicationCommandErrorCode::QueueFull, 0,
                   "injected full command queue"}};
    }
    auto prepared = std::make_unique<ControllerPreparedCommands>(
        reserved, published, lifecycle);
    ++reserved;
    return {std::move(prepared), {}};
  }

  std::vector<std::string> observed_scopes;
  int reserve_calls = 0;
  int reserved = 0;
  int published = 0;
  bool fail_reserve = false;
  std::vector<std::string> *lifecycle = nullptr;
};

} // namespace

spec("FlexUI scoped script event retirement") {
  it("atomically drops exact scoped event routes and preserves other callbacks") {
    const auto program = scoped_controller_program();
    if (!program) return;
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "parent_handler", "on_frame",
                                 "on_service_completion"});
    auto *fake = module.get();
    flexUI::ScriptController controller;
    check(controller.load(std::move(module), program));
    check(controller.mount());
    check(controller.has_active_event_binding("sale::action", flexUI::UiEventKind::Click));
    check(controller.has_active_event_binding("sales::action", flexUI::UiEventKind::Click));
    check(controller.has_active_event_binding("sales::nested", flexUI::UiEventKind::KeyDown));
    check(controller.dispatch(scoped_event("sales::action")));
    check(controller.dispatch(scoped_event("sales::nested", flexUI::UiEventKind::KeyDown)));
    check_equal(fake->calls.size(), std::size_t{2});

    const auto unknown = controller.retire_scoped_event_handlers("other");
    check_false(static_cast<bool>(unknown));
    check(unknown.error.code == flexUI::ControllerErrorCode::InvalidArgument);
    check(unknown.error.stage == flexUI::ControllerStage::EventRetirement);
    for (const char *invalid : {"", "sales::action", "sales.", "2sales"}) {
      const auto rejected = controller.retire_scoped_event_handlers(invalid);
      check_false(static_cast<bool>(rejected));
      check(rejected.error.code == flexUI::ControllerErrorCode::InvalidArgument);
    }
    check(controller.has_active_event_binding("sales::action", flexUI::UiEventKind::Click));

    const auto retired = controller.retire_scoped_event_handlers("sales");
    check(static_cast<bool>(retired));
    check_equal(controller.program().get(), program.get());
    check_not_null(program->find_event_binding("sales::action", flexUI::UiEventKind::Click));
    check_false(controller.has_active_event_binding("sales::action", flexUI::UiEventKind::Click));
    check_false(controller.has_active_event_binding("sales::nested", flexUI::UiEventKind::KeyDown));
    check(controller.has_active_event_binding("sale::action", flexUI::UiEventKind::Click));
    check(controller.retire_scoped_event_handlers("sales"));
    check(controller.dispatch(scoped_event("sales::action")));
    check(controller.dispatch(scoped_event("sales::nested", flexUI::UiEventKind::KeyDown)));
    check_equal(fake->calls.size(), std::size_t{2});
    check(controller.dispatch(scoped_event("sale::action")));
    check_equal(fake->calls.size(), std::size_t{3});
    check_equal(fake->calls.back(), "save_document");
    check(controller.frame(0.016));
    check_equal(fake->calls.back(), "on_frame");
    check(controller.dispatch_service_completion(
        {11, flexUI::ApplicationCompletionStatus::Succeeded, "ok", {}, {}}));
    check_equal(fake->calls.back(), "on_service_completion");
    check(controller.state() == flexUI::ControllerState::Mounted);

    check(controller.unmount());
    auto replacement = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "parent_handler"});
    auto *fresh = replacement.get();
    check(controller.load(std::move(replacement), program));
    check(controller.mount());
    check(controller.has_active_event_binding("sales::action", flexUI::UiEventKind::Click));
    check(controller.dispatch(scoped_event("sales::action")));
    check_equal(fresh->calls.size(), std::size_t{1});
  }

  it("rejects wrong-thread and in-flight module event retirement without partial changes") {
    auto program = scoped_controller_program();
    if (!program) return;
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "parent_handler"});
    auto *fake = module.get();
    flexUI::ScriptController controller;
    check(controller.load(std::move(module), program));
    const auto compiled = controller.retire_scoped_event_handlers("sales");
    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == flexUI::ControllerErrorCode::InvalidState);
    check(controller.mount());

    flexUI::ControllerErrorCode other_thread = flexUI::ControllerErrorCode::None;
    std::thread worker([&] {
      other_thread = controller.retire_scoped_event_handlers("sales").error.code;
    });
    worker.join();
    check(other_thread == flexUI::ControllerErrorCode::WrongThread);
    check(controller.has_active_event_binding("sales::action", flexUI::UiEventKind::Click));

    bool called = false;
    flexUI::ControllerResult refused;
    fake->on_event_call = [&] {
      called = true;
      refused = controller.retire_scoped_event_handlers("sales");
    };
    check(controller.dispatch(scoped_event("sales::action")));
    check(called);
    check_false(static_cast<bool>(refused));
    check(refused.error.code == flexUI::ControllerErrorCode::InvalidState);
    check(controller.has_active_event_binding("sales::nested", flexUI::UiEventKind::KeyDown));
    check(controller.state() == flexUI::ControllerState::Mounted);
    fake->on_event_call = {};
    check(controller.retire_scoped_event_handlers("sales"));
    check(controller.unmount());
    const auto after_unmount = controller.retire_scoped_event_handlers("sale");
    check_false(static_cast<bool>(after_unmount));
    check(after_unmount.error.code == flexUI::ControllerErrorCode::InvalidState);
  }
}

spec("FlexUI trusted scoped command origin") {
  it("attributes commands to the compiled handler scope, never the event target") {
    const auto program = scoped_controller_program();
    if (!program) return;
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "parent_handler"});
    module->command_export = "save_document";
    module->command_payload = "data";
    ControllerCommandQueue queue;
    flexUI::ApplicationCommandEngine engine(queue);
    flexUI::ScriptController controller(flexUI::ControllerEffects{nullptr, &engine});
    check(controller.load(std::move(module), program));
    check(controller.mount());
    check(controller.dispatch(scoped_event("sales::action")));
    check(controller.dispatch(scoped_event("sale::action")));
    // A bubbled event's compiled current_target, not the original target,
    // determines ownership of the emitted service request.
    auto bubble = scoped_event("sale::action");
    bubble.current_target = {"sales::action", 1};
    check(controller.dispatch(bubble));
    check_equal(queue.published, 3);
    check_equal(queue.observed_scopes.size(), std::size_t{3});
    check_equal(queue.observed_scopes[0], std::string("sales"));
    check_equal(queue.observed_scopes[1], std::string("sale"));
    check_equal(queue.observed_scopes[2], std::string("sales"));
  }

  it("does not attribute a global service completion callback to a scope") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "on_service_completion"});
    module->command_export = "on_service_completion";
    ControllerCommandQueue queue;
    flexUI::ApplicationCommandEngine engine(queue);
    flexUI::ScriptController controller(flexUI::ControllerEffects{nullptr, &engine});
    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check(controller.dispatch_service_completion(
        {77, flexUI::ApplicationCompletionStatus::Succeeded, "ok", {}, {}}));
    check_equal(queue.observed_scopes.size(), std::size_t{1});
    check(queue.observed_scopes.front().empty());
  }
}

spec("FlexUI script controller lifecycle") {
  it("resolves exports before dispatch and preserves lifecycle order") {
    std::vector<std::string> calls;
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"on_mount", "save_document", "on_frame",
                                 "on_unmount"},
        &calls);
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.state() == flexUI::ControllerState::Compiled);
    check(controller.mount());
    check(controller.state() == flexUI::ControllerState::Mounted);
    check(controller.dispatch(click_event()));
    check(controller.frame(1.0 / 60.0));
    check(controller.unmount());
    check(controller.state() == flexUI::ControllerState::Empty);

    check_calls(calls, {"on_mount", "save_document", "on_frame",
                        "on_unmount", "destroy"});
  }

  it("does not call the module from a frame without an on_frame export") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    auto *fake = module.get();
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check(controller.frame(1.0 / 60.0));
    check_empty(fake->calls);
  }

  it("resolves the optional service completion export once and no-ops when absent") {
    auto module =
        std::make_unique<FakeScriptModule>(std::vector<std::string>{"save_document"});
    auto *fake = module.get();
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check_false(controller.has_service_completion_handler());
    check(controller.dispatch_service_completion(
        {11, flexUI::ApplicationCompletionStatus::Succeeded, "stored", {}, {}}));
    check(fake->calls.empty());

    std::size_t resolutions = 0;
    for (const auto &[name, callback] : fake->resolved) {
      if (name == "on_service_completion" &&
          callback == flexUI::ScriptCallbackKind::ServiceCompletion) {
        ++resolutions;
      }
    }
    check_equal(resolutions, std::size_t{1});
  }

  it("dispatches an immutable service completion to the pre-resolved export") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "on_service_completion"});
    auto *fake = module.get();
    flexUI::ScriptController controller;
    const flexUI::ApplicationServiceCompletion completion{
        41, flexUI::ApplicationCompletionStatus::Failed, {}, "write-failed", "disk full"};

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check_true(controller.has_service_completion_handler());
    check(controller.dispatch_service_completion(completion));
    check_equal(fake->calls.size(), std::size_t{1});
    check_equal(fake->calls.front(), std::string("on_service_completion"));
    check_equal(fake->service_completions.size(), std::size_t{1});
    if (!fake->service_completions.empty()) {
      const auto &received = fake->service_completions.front();
      check_equal(received.script_request_id, std::uint64_t{41});
      check(received.status == flexUI::ApplicationCompletionStatus::Failed);
      check_equal(received.error_code, std::string("write-failed"));
      check_equal(received.error_message, std::string("disk full"));
    }
    check_equal(completion.error_message, std::string("disk full"));
    check(controller.state() == flexUI::ControllerState::Mounted);
  }

  it("rejects invalid or oversized service completions before invoking script") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "on_service_completion"});
    auto *fake = module.get();
    flexUI::ControllerLimits limits;
    limits.max_service_payload_bytes = 3;
    flexUI::ScriptController controller(limits);

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto invalid = controller.dispatch_service_completion(
        {0, flexUI::ApplicationCompletionStatus::Succeeded, {}, {}, {}});
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == flexUI::ControllerErrorCode::InvalidArgument);
    check(invalid.error.stage == flexUI::ControllerStage::ServiceCompletion);

    const auto oversized = controller.dispatch_service_completion(
        {12, flexUI::ApplicationCompletionStatus::Succeeded, "four", {}, {}});
    check_false(static_cast<bool>(oversized));
    check(oversized.error.code == flexUI::ControllerErrorCode::CompletionLimitExceeded);
    check(oversized.error.stage == flexUI::ControllerStage::ServiceCompletion);
    check(fake->calls.empty());
    check(controller.state() == flexUI::ControllerState::Mounted);
  }

  it("faults after a service completion callback failure") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "on_service_completion"});
    module->failing_export = "on_service_completion";
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch_service_completion(
        {51, flexUI::ApplicationCompletionStatus::Cancelled, {}, {}, "window closed"});
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code == flexUI::ControllerErrorCode::ModuleCallFailed);
    check(dispatched.error.stage == flexUI::ControllerStage::ServiceCompletion);
    check_equal(dispatched.error.handler, std::string("on_service_completion"));
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("applies service completion effects through the existing transaction") {
    std::vector<std::string> lifecycle;
    lifecycle.reserve(8);
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "on_service_completion"}, &lifecycle);
    module->mutation_export = "on_service_completion";
    module->mutation_text = "completion committed";
    module->command_export = "on_service_completion";
    module->command_payload = "follow-up";
    ControllerMutationHost host(&lifecycle);
    flexUI::UiMutationEngine mutations(host);
    ControllerCommandQueue queue(&lifecycle);
    flexUI::ApplicationCommandEngine commands(queue);
    flexUI::ScriptController controller(flexUI::ControllerEffects{&mutations, &commands});

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check(controller.dispatch_service_completion(
        {61, flexUI::ApplicationCompletionStatus::Succeeded, "done", {}, {}}));
    check_calls(lifecycle,
                {"on_service_completion", "reserve", "ui_prepare", "ui_commit", "publish"});
    check_equal(host.text, std::string("completion committed"));
    check_equal(queue.published, 1);
  }

  it("does not call the module when the compiled program has no matching binding") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    auto *fake = module.get();
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    auto event = click_event();
    event.event = flexUI::UiEventKind::KeyDown;
    check(controller.dispatch(event));
    check_empty(fake->calls);
  }

  it("rejects oversized event snapshots before invoking script") {
    flexUI::ControllerLimits limits;
    limits.max_event_text_bytes = 3;
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    auto *fake = module.get();
    flexUI::ScriptController controller(limits);

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    auto event = click_event();
    event.text = "four";
    const auto dispatched = controller.dispatch(event);
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::EventLimitExceeded);
    check(controller.state() == flexUI::ControllerState::Mounted);
    check_empty(fake->calls);
  }

  it("rejects a missing required handler without publishing partial state") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"on_mount"});
    flexUI::ScriptController controller;

    const auto loaded =
        controller.load(std::move(module), controller_program());
    check_false(static_cast<bool>(loaded));
    check(controller.state() == flexUI::ControllerState::Empty);
    check(loaded.error.code ==
          flexUI::ControllerErrorCode::MissingHandlerExport);
    check_equal(loaded.error.element_id, "save");
    check_equal(loaded.error.handler, "save_document");
    check(loaded.error.event == flexUI::UiEventKind::Click);
    check_greater(loaded.error.source.line, std::size_t{0});
  }

  it("rejects a resolved export with an invalid opaque handle") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->zero_handle_export = "save_document";
    flexUI::ScriptController controller;

    const auto loaded =
        controller.load(std::move(module), controller_program());
    check_false(static_cast<bool>(loaded));
    check(loaded.error.code ==
          flexUI::ControllerErrorCode::ModuleResolutionFailed);
    check_equal(loaded.error.handler, "save_document");
    check(controller.state() == flexUI::ControllerState::Empty);
  }

  it("faults after a callback failure and recovers only through reload") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    auto *fake = module.get();
    fake->failing_export = "save_document";
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto failed = controller.dispatch(click_event());
    check_false(static_cast<bool>(failed));
    check(failed.error.code == flexUI::ControllerErrorCode::ModuleCallFailed);
    check(controller.state() == flexUI::ControllerState::Faulted);

    const auto rejected = controller.dispatch(click_event());
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code ==
          flexUI::ControllerErrorCode::ControllerFaulted);
    check_equal(fake->calls.size(), std::size_t{1});

    check(controller.unmount());
    auto replacement = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    check(controller.load(std::move(replacement), controller_program()));
    check(controller.mount());
    check(controller.dispatch(click_event()));
  }

  it("converts module exceptions and never remains dispatching") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    auto *fake = module.get();
    fake->throwing_export = "save_document";
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(click_event());
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::ModuleCallFailed);
    check(dispatched.error.module_error.code ==
          flexUI::ScriptModuleErrorCode::RuntimeFailure);
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("preserves real Box state when a callback times out") {
    flexUI::Box box(nullptr);
    auto *save = box.create("button", "save");
    box.set_root(save);
    save->set_text("before");
    box.bindings().inputs().set_number("count", 3.0);
    const auto handle = box.handle_for(*save);
    const auto revision = box.bindings().inputs().revision();
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->timeout_export = "save_document";
    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);
    auto event = click_event();
    event.target = handle;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(event);
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::ModuleCallFailed);
    check(dispatched.error.module_error.code ==
          flexUI::ScriptModuleErrorCode::Timeout);
    check(controller.state() == flexUI::ControllerState::Faulted);
    check(box.root() == save);
    check(box.resolve_handle(handle) == save);
    check_equal(save->text(), std::string("before"));
    check_equal(box.bindings().inputs().number("count"), 3.0);
    check_equal(box.bindings().inputs().revision(), revision);
  }

  it("preserves real Box state when handler resolution fails") {
    flexUI::Box box(nullptr);
    auto *save = box.create("button", "save");
    box.set_root(save);
    save->set_text("before");
    const auto handle = box.handle_for(*save);
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"on_mount"});
    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);

    const auto loaded = controller.load(std::move(module), controller_program());
    check_false(static_cast<bool>(loaded));
    check(loaded.error.code ==
          flexUI::ControllerErrorCode::MissingHandlerExport);
    check(controller.state() == flexUI::ControllerState::Empty);
    check(box.root() == save);
    check(box.resolve_handle(handle) == save);
    check_equal(save->text(), std::string("before"));
  }

  it("commits a callback mutation batch after module success") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->mutation_export = "save_document";
    module->mutation_text = "committed";
    ControllerMutationHost host;
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check(controller.dispatch(click_event()));
    check_equal(host.prepare_calls, 1);
    check_equal(host.commits, 1);
    check_equal(host.text, "committed");
    check(controller.state() == flexUI::ControllerState::Mounted);
  }

  it("commits an event mutation through the real Box host") {
    flexUI::Box box(nullptr);
    auto *save = box.create("button", "save");
    box.set_root(save);
    save->set_text("before");
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->mutation_export = "save_document";
    module->mutation_text = "committed by controller";
    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);
    auto event = click_event();
    event.target = box.handle_for(*save);

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check(controller.dispatch(event));
    check_equal(save->text(), std::string("committed by controller"));
    check(controller.state() == flexUI::ControllerState::Mounted);
  }

  it("reserves commands before UI commit and publishes them afterward") {
    std::vector<std::string> lifecycle;
    lifecycle.reserve(8);
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"}, &lifecycle);
    module->mutation_export = "save_document";
    module->mutation_text = "committed";
    module->command_export = "save_document";
    module->command_payload = "document";
    ControllerMutationHost host(&lifecycle);
    flexUI::UiMutationEngine mutations(host);
    ControllerCommandQueue queue(&lifecycle);
    flexUI::ApplicationCommandEngine commands(queue);
    flexUI::ScriptController controller(
        flexUI::ControllerEffects{&mutations, &commands});

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    check(controller.dispatch(click_event()));
    check_calls(lifecycle, {"save_document", "reserve", "ui_prepare",
                            "ui_commit", "publish"});
    check_equal(host.text, std::string("committed"));
    check_equal(queue.reserved, 0);
    check_equal(queue.published, 1);
  }

  it("does not prepare UI state when command reservation fails") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->mutation_export = "save_document";
    module->mutation_text = "must not commit";
    module->command_export = "save_document";
    ControllerMutationHost host;
    flexUI::UiMutationEngine mutations(host);
    ControllerCommandQueue queue;
    queue.fail_reserve = true;
    flexUI::ApplicationCommandEngine commands(queue);
    flexUI::ScriptController controller(
        flexUI::ControllerEffects{&mutations, &commands});

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(click_event());
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::CommandFailed);
    check(dispatched.error.command_error.code ==
          flexUI::ApplicationCommandErrorCode::QueueFull);
    check_equal(host.prepare_calls, 0);
    check_equal(host.commits, 0);
    check_equal(host.text, std::string("unchanged"));
    check_equal(queue.published, 0);
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("cancels reserved commands when UI mutation preparation fails") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->mutation_export = "save_document";
    module->mutation_text = "must not commit";
    module->command_export = "save_document";
    ControllerMutationHost host;
    host.fail_prepare = true;
    flexUI::UiMutationEngine mutations(host);
    ControllerCommandQueue queue;
    flexUI::ApplicationCommandEngine commands(queue);
    flexUI::ScriptController controller(
        flexUI::ControllerEffects{&mutations, &commands});

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(click_event());
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::MutationFailed);
    check_equal(host.commits, 0);
    check_equal(host.text, std::string("unchanged"));
    check_equal(queue.reserve_calls, 1);
    check_equal(queue.reserved, 0);
    check_equal(queue.published, 0);
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("faults when a callback emits commands without a command engine") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->command_export = "save_document";
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(click_event());
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::CommandFailed);
    check_equal(
        dispatched.error.message,
        std::string(
            "script emitted application commands without a command engine"));
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("rejects mount commands before reserving service queue slots") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"on_mount", "save_document"});
    module->command_export = "on_mount";
    ControllerCommandQueue queue;
    flexUI::ApplicationCommandEngine commands(queue);
    flexUI::ScriptController controller(
        flexUI::ControllerEffects{nullptr, &commands});

    check(controller.load(std::move(module), controller_program()));
    const auto mounted = controller.mount();
    check_false(static_cast<bool>(mounted));
    check(mounted.error.code == flexUI::ControllerErrorCode::CommandFailed);
    check_equal(queue.reserve_calls, 0);
    check_equal(queue.published, 0);
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("faults without a partial commit when the event handle becomes stale") {
    flexUI::Box box(nullptr);
    auto *save = box.create("button", "save");
    box.set_root(save);
    save->set_text("before");
    auto event = click_event();
    event.target = box.handle_for(*save);
    save->set_element_id("renamed");
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->mutation_export = "save_document";
    module->mutation_text = "must not commit";
    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(event);
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::MutationFailed);
    check(dispatched.error.mutation_error.code ==
          flexUI::MutationErrorCode::InvalidTarget);
    check_equal(save->text(), std::string("before"));
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("faults without committing when callback mutation preparation fails") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->mutation_export = "save_document";
    module->mutation_text = "must not commit";
    ControllerMutationHost host;
    host.fail_prepare = true;
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(click_event());
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::MutationFailed);
    check(dispatched.error.mutation_error.code ==
          flexUI::MutationErrorCode::HostPrepareFailed);
    check_equal(host.commits, 0);
    check_equal(host.text, "unchanged");
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("faults when a callback emits mutations without an installed engine") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document"});
    module->mutation_export = "save_document";
    module->mutation_text = "unroutable";
    flexUI::ScriptController controller;

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto dispatched = controller.dispatch(click_event());
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code ==
          flexUI::ControllerErrorCode::MutationFailed);
    check_equal(dispatched.error.message,
                "script emitted UI mutations without a mutation engine");
    check(controller.state() == flexUI::ControllerState::Faulted);
  }

  it("rejects unmount mutations while still releasing the module") {
    auto module = std::make_unique<FakeScriptModule>(
        std::vector<std::string>{"save_document", "on_unmount"});
    module->mutation_export = "on_unmount";
    module->mutation_text = "too late";
    ControllerMutationHost host;
    flexUI::UiMutationEngine mutations(host);
    flexUI::ScriptController controller(mutations);

    check(controller.load(std::move(module), controller_program()));
    check(controller.mount());
    const auto unmounted = controller.unmount();
    check_false(static_cast<bool>(unmounted));
    check(unmounted.error.code ==
          flexUI::ControllerErrorCode::MutationFailed);
    check_equal(host.prepare_calls, 0);
    check_equal(host.commits, 0);
    check(controller.state() == flexUI::ControllerState::Empty);
  }

  it("unmounts before destroying its owned module") {
    std::vector<std::string> lifecycle;
    {
      auto module = std::make_unique<FakeScriptModule>(
          std::vector<std::string>{"save_document", "on_unmount"},
          &lifecycle);
      flexUI::ScriptController controller;
      check(controller.load(std::move(module), controller_program()));
      check(controller.mount());
    }

    check_calls(lifecycle, {"on_unmount", "destroy"});
  }
}
