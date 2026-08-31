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
  resolve_export(std::string_view name) override {
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
       const flexUI::ScriptCallContext &) override {
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
    if (found->second == throwing_export) {
      throw std::runtime_error("injected script exception");
    }
    flexUI::ScriptCallResult result;
    if (found->second == mutation_export) {
      const auto appended = result.mutations.append(
          flexUI::SetTextMutation{{"save", 1}, mutation_text});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                 appended.error.message}};
      }
    }
    return result;
  }

  std::vector<std::string> calls;
  std::string failing_export;
  std::string throwing_export;
  std::string zero_handle_export;
  std::string mutation_export;
  std::string mutation_text;

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
                             std::string next_text)
      : commits_(commits), text_(text), next_text_(std::move(next_text)) {}

  void commit() noexcept override {
    text_.swap(next_text_);
    ++commits_;
  }

private:
  int &commits_;
  std::string &text_;
  std::string next_text_;
};

class ControllerMutationHost final : public flexUI::IUiMutationHost {
public:
  flexUI::MutationPrepareResult
  prepare(const flexUI::UiMutationBatch &batch) override {
    ++prepare_calls;
    if (fail_prepare) {
      return {{}, {flexUI::MutationErrorCode::HostPrepareFailed, 0,
                   "injected controller prepare failure"}};
    }
    const auto &mutation =
        std::get<flexUI::SetTextMutation>(batch.mutations().front());
    return {std::make_unique<ControllerPreparedMutation>(
                commits, text, mutation.text),
            {}};
  }

  int prepare_calls = 0;
  int commits = 0;
  std::string text = "unchanged";
  bool fail_prepare = false;
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
