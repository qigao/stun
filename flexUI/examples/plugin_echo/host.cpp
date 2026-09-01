#include <flexUI/plugin_host.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#ifndef FLEXUI_EXAMPLE_PLUGIN_PATH
#error FLEXUI_EXAMPLE_PLUGIN_PATH must name the example plugin library
#endif

namespace {

class CompletionSink final
    : public flexUI::IApplicationServiceCompletionSink {
public:
  flexUI::ApplicationCompletionPostResult
  try_post(const flexUI::ApplicationCompletion &value) override {
    completion_ = value;
    return {};
  }

  const std::optional<flexUI::ApplicationCompletion> &completion() const {
    return completion_;
  }

private:
  std::optional<flexUI::ApplicationCompletion> completion_;
};

int fail(const char *stage, const std::string &message) {
  std::cerr << "FlexUI plugin example failed during " << stage << ": "
            << message << '\n';
  return 1;
}

} // namespace

int main() {
  flexUI::PluginHostBuilder builder;
  const auto loaded = builder.load_plugin(
      std::filesystem::path(FLEXUI_EXAMPLE_PLUGIN_PATH));
  if (!loaded) {
    return fail(loaded.error.stage.c_str(), loaded.error.message);
  }

  auto built = builder.build();
  if (!built) {
    return fail(built.error.stage.c_str(), built.error.message);
  }

  flexUI::ApplicationServiceRequest request{
      {1, 1}, 1, "example.echo/1", "echo", "installed-package"};
  flexUI::ApplicationCapabilityManifest manifest;
  manifest.allowed = {request.capability};
  manifest.required = {request.capability};
  auto resolved = built.registry->resolve(request, manifest);
  if (!resolved) {
    (void)built.host->stop(std::chrono::seconds(1));
    return fail("resolve", resolved.error.message);
  }

  CompletionSink sink;
  const auto submitted = resolved.endpoint->try_submit(request, sink);
  if (!submitted) {
    (void)built.host->stop(std::chrono::seconds(1));
    return fail("submit", submitted.error.message);
  }
  if (!sink.completion() ||
      sink.completion()->status !=
          flexUI::ApplicationCompletionStatus::Succeeded ||
      sink.completion()->payload != request.payload) {
    (void)built.host->stop(std::chrono::seconds(1));
    return fail("completion", "echo completion did not preserve the payload");
  }

  const auto stopped = built.host->stop(std::chrono::seconds(1));
  if (!stopped) {
    return fail(stopped.error.stage.c_str(), stopped.error.message);
  }
  return 0;
}
