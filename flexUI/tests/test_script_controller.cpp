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
    return {};
  }

  std::vector<std::string> calls;
  std::string failing_export;
  std::string throwing_export;
  std::string zero_handle_export;

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
