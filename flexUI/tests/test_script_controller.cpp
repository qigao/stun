#include <flexUI/box.h>
#include <flexUI/box_mutation_host.h>
#include <flexUI/controller.h>

#include <tinytest.hpp>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
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
                 flexUI::ScriptCallbackKind) override {
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
    if (lifecycle_ != nullptr) {
      lifecycle_->push_back(found->second);
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
  std::string failing_export;
  std::string timeout_export;
  std::string throwing_export;
  std::string zero_handle_export;
  std::string mutation_export;
  std::string mutation_text;
  std::string command_export;
  std::string command_payload;

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
  reserve(const flexUI::ApplicationCommandBatch &) override {
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

  int reserve_calls = 0;
  int reserved = 0;
  int published = 0;
  bool fail_reserve = false;
  std::vector<std::string> *lifecycle = nullptr;
};

} // namespace

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
