#pragma once

#include "flexUI/plugin_abi.h"
#include "flexUI/plugin_host.h"

#include "native_plugin_library.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace flexUI::plugin_host_detail {

class PluginModule;

struct PluginModuleLoadResult {
  std::shared_ptr<PluginModule> module;
  PluginHostError error;

  explicit operator bool() const noexcept {
    return module != nullptr && !static_cast<bool>(error);
  }
};

class PluginModule final : public IApplicationServiceEndpoint {
public:
  static PluginModuleLoadResult load(const std::filesystem::path &absolute_path,
                                     const PluginHostLimits &limits);

  ~PluginModule() override;

  PluginModule(const PluginModule &) = delete;
  PluginModule &operator=(const PluginModule &) = delete;

  ApplicationServiceSubmitResult
  try_submit(const ApplicationServiceRequest &request,
             IApplicationServiceCompletionSink &completion_sink) override;

  PluginHostResult start();
  void disable_submissions() noexcept;
  PluginHostResult stop(std::chrono::milliseconds timeout);
  PluginHostStatistics statistics() const noexcept;
  const std::vector<ApplicationServiceDescriptor> &services() const noexcept;
  const std::string &plugin_id() const noexcept;
  const std::string &plugin_version() const noexcept;
  std::uint32_t required_host_minor() const noexcept;
  const std::filesystem::path &path() const noexcept;
  bool stopped() const noexcept;

private:
  enum class LifecycleState { Created, Started, Stopping, Stopped };
  enum class SlotState { Free, Active, Posting };

  struct Slot {
    SlotState state = SlotState::Free;
    ApplicationRequestToken token;
    std::uint64_t script_request_id = 0;
    IApplicationServiceCompletionSink *sink = nullptr;
  };

  PluginModule(NativePluginLibrary library, const flexui_plugin_api_v1 *api,
               flexui_plugin_instance *instance, std::string plugin_id,
               std::string plugin_version,
               std::uint32_t required_host_minor,
               std::vector<ApplicationServiceDescriptor> services,
               const PluginHostLimits &limits);

  static flexui_plugin_status FLEXUI_PLUGIN_CALL
  post_completion_callback(void *host_context,
                           const flexui_plugin_completion_v1 *completion,
                           flexui_plugin_error_buffer *error) noexcept;
  flexui_plugin_status post_completion(const flexui_plugin_completion_v1 *completion,
                                       flexui_plugin_error_buffer *error) noexcept;
  void destroy_created() noexcept;
  void abandon_live_library() noexcept;

  NativePluginLibrary library_;
  const flexui_plugin_api_v1 *api_ = nullptr;
  flexui_plugin_instance *instance_ = nullptr;
  flexui_host_api_v1 host_api_{};
  std::string plugin_id_;
  std::string plugin_version_;
  std::uint32_t required_host_minor_ = 0;
  std::vector<ApplicationServiceDescriptor> services_;
  PluginHostLimits limits_;

  mutable std::mutex lifecycle_mutex_;
  LifecycleState lifecycle_state_ = LifecycleState::Created;
  bool stop_succeeded_ = false;

  mutable std::mutex slots_mutex_;
  std::condition_variable submits_idle_;
  std::vector<Slot> slots_;
  bool accepting_ = false;
  std::size_t active_submit_calls_ = 0;
  PluginHostStatistics statistics_;
};

} // namespace flexUI::plugin_host_detail
