#include "plugin_module.hpp"

#include "plugin_validation.hpp"

#include <turbo_vstr.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <string_view>
#include <utility>

namespace flexUI::plugin_host_detail {
namespace {

constexpr std::size_t kAbiErrorCapacity =
    PluginHostLimits::kDefaultMaxErrorMessageBytes;

template <typename Struct, typename Member>
constexpr std::size_t field_end(Member Struct::*member) noexcept {
  Struct value{};
  const auto *base = reinterpret_cast<const unsigned char *>(&value);
  const auto *field = reinterpret_cast<const unsigned char *>(&(value.*member));
  return static_cast<std::size_t>(field - base) + sizeof(Member);
}

struct AbiErrorBuffer {
  std::array<std::uint8_t, kAbiErrorCapacity> storage{};
  flexui_plugin_error_buffer abi{};

  explicit AbiErrorBuffer(std::size_t configured_capacity) noexcept {
    abi.struct_size = sizeof(abi);
    abi.message_data = storage.data();
    abi.message_capacity = std::min(configured_capacity, storage.size());
  }

  std::string message() const {
    if (abi.message_size > abi.message_capacity || abi.message_data == nullptr) {
      return "plugin returned an invalid error buffer length";
    }
    const auto length = static_cast<std::size_t>(abi.message_size);
    const auto *data = reinterpret_cast<const char *>(abi.message_data);
    if (length != 0 && !vstr_utf8_valid(vstr_from_buf(data, length))) {
      return "plugin returned a non-UTF-8 error message";
    }
    return std::string(data, length);
  }
};

PluginHostError host_error(PluginHostErrorCode code, std::string stage,
                           std::filesystem::path path, std::string message,
                           std::string plugin_id = {},
                           flexui_plugin_status status = FLEXUI_PLUGIN_STATUS_OK) {
  PluginHostError result;
  result.code = code;
  result.stage = std::move(stage);
  result.plugin_id = std::move(plugin_id);
  result.path = std::move(path);
  result.message = std::move(message);
  result.plugin_status = status;
  return result;
}

void write_abi_error(flexui_plugin_error_buffer *error,
                     std::string_view message) noexcept {
  if (error == nullptr ||
      error->struct_size < field_end(&flexui_plugin_error_buffer::message_size)) {
    return;
  }
  error->message_size = 0;
  if (error->message_data == nullptr || error->message_capacity == 0) {
    return;
  }
  const auto count = static_cast<std::size_t>(
      std::min<std::uint64_t>(error->message_capacity, message.size()));
  std::memcpy(error->message_data, message.data(), count);
  error->message_size = count;
}

bool contains_nul(std::string_view value) noexcept {
  return value.find('\0') != std::string_view::npos;
}

bool copy_utf8_view(flexui_plugin_bytes_view view, std::size_t limit,
                    bool require_nonempty, std::string &output,
                    std::string &message) {
  if (view.size > static_cast<std::uint64_t>(limit) ||
      view.size > static_cast<std::uint64_t>(
                      std::numeric_limits<std::size_t>::max())) {
    message = "plugin descriptor string exceeds its configured limit";
    return false;
  }
  if (view.size != 0 && view.data == nullptr) {
    message = "plugin descriptor string has a null data pointer";
    return false;
  }
  if (require_nonempty && view.size == 0) {
    message = "plugin descriptor string must not be empty";
    return false;
  }
  const auto size = static_cast<std::size_t>(view.size);
  const auto *data = reinterpret_cast<const char *>(view.data);
  if (size != 0 && !vstr_utf8_valid(vstr_from_buf(data, size))) {
    message = "plugin descriptor string must be valid UTF-8";
    return false;
  }
  std::string_view borrowed(data != nullptr ? data : "", size);
  if (contains_nul(borrowed)) {
    message = "plugin descriptor string must not contain NUL bytes";
    return false;
  }
  output.assign(borrowed);
  return true;
}

const ApplicationServiceOperationDescriptor *
find_operation(const std::vector<ApplicationServiceDescriptor> &services,
               const ApplicationServiceRequest &request) noexcept {
  const auto service = std::find_if(
      services.begin(), services.end(), [&request](const auto &candidate) {
        return candidate.capability == request.capability;
      });
  if (service == services.end()) {
    return nullptr;
  }
  const auto operation = std::find_if(
      service->operations.begin(), service->operations.end(),
      [&request](const auto &candidate) {
        return candidate.name == request.operation;
      });
  return operation == service->operations.end() ? nullptr : &*operation;
}

flexui_plugin_bytes_view view_of(std::string_view value) noexcept {
  return {reinterpret_cast<const std::uint8_t *>(value.data()),
          static_cast<std::uint64_t>(value.size())};
}

ApplicationServiceSubmitError submit_error(
    ApplicationServiceSubmitErrorCode code, std::string message) {
  return {code, std::move(message)};
}

ApplicationServiceSubmitErrorCode
map_submit_status(flexui_plugin_status status) noexcept {
  switch (status) {
  case FLEXUI_PLUGIN_STATUS_BUSY:
  case FLEXUI_PLUGIN_STATUS_QUEUE_FULL:
  case FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT:
    return ApplicationServiceSubmitErrorCode::Busy;
  case FLEXUI_PLUGIN_STATUS_CLOSED:
    return ApplicationServiceSubmitErrorCode::Closed;
  case FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT:
    return ApplicationServiceSubmitErrorCode::InvalidRequest;
  case FLEXUI_PLUGIN_STATUS_REJECTED:
    return ApplicationServiceSubmitErrorCode::Rejected;
  default:
    return ApplicationServiceSubmitErrorCode::InternalFailure;
  }
}

bool valid_completion_view(flexui_plugin_bytes_view view,
                           std::size_t limit) noexcept {
  if (view.size > static_cast<std::uint64_t>(limit) ||
      view.size > static_cast<std::uint64_t>(
                      std::numeric_limits<std::size_t>::max()) ||
      (view.size != 0 && view.data == nullptr)) {
    return false;
  }
  if (view.size == 0) {
    return true;
  }
  return vstr_utf8_valid(vstr_from_buf(
             reinterpret_cast<const char *>(view.data),
             static_cast<std::size_t>(view.size))) != 0;
}

std::string copy_completion_view(flexui_plugin_bytes_view view) {
  if (view.size == 0) {
    return {};
  }
  return std::string(reinterpret_cast<const char *>(view.data),
                     static_cast<std::size_t>(view.size));
}

bool completion_strings_fit(const flexui_plugin_completion_v1 &completion,
                            std::size_t limit) noexcept {
  const auto bounded_limit = static_cast<std::uint64_t>(limit);
  if (completion.payload.size > bounded_limit ||
      completion.error_code.size > bounded_limit - completion.payload.size) {
    return false;
  }
  const auto first_total = completion.payload.size + completion.error_code.size;
  return completion.error_message.size <= bounded_limit - first_total;
}

bool valid_completion_shape(const flexui_plugin_completion_v1 &completion) noexcept {
  switch (completion.status) {
  case FLEXUI_PLUGIN_COMPLETION_SUCCEEDED:
    return completion.error_code.size == 0 && completion.error_message.size == 0;
  case FLEXUI_PLUGIN_COMPLETION_FAILED:
    return completion.error_code.size != 0;
  case FLEXUI_PLUGIN_COMPLETION_CANCELLED:
    return completion.payload.size == 0 && completion.error_code.size == 0;
  default:
    return false;
  }
}

} // namespace

PluginModule::PluginModule(NativePluginLibrary library,
                           const flexui_plugin_api_v1 *api,
                           flexui_plugin_instance *instance,
                           std::string plugin_id, std::string plugin_version,
                           std::uint32_t required_host_minor,
                           std::vector<ApplicationServiceDescriptor> services,
                           const PluginHostLimits &limits)
    : library_(std::move(library)), api_(api), instance_(instance),
      plugin_id_(std::move(plugin_id)),
      plugin_version_(std::move(plugin_version)),
      required_host_minor_(required_host_minor),
      services_(std::move(services)),
      limits_(limits), slots_(limits.max_in_flight_per_plugin) {
  host_api_.struct_size = sizeof(host_api_);
  host_api_.abi_major = FLEXUI_PLUGIN_ABI_MAJOR;
  host_api_.abi_minor = FLEXUI_PLUGIN_ABI_MINOR;
  host_api_.host_context = this;
  host_api_.post_completion = &PluginModule::post_completion_callback;
  statistics_.plugin_count = 1;
}

PluginModuleLoadResult
PluginModule::load(const std::filesystem::path &absolute_path,
                   const PluginHostLimits &limits) {
  if (!valid_plugin_host_limits(limits)) {
    return {{}, host_error(PluginHostErrorCode::InvalidLimits, "validate",
                           absolute_path, "plugin host limits are invalid")};
  }

  try {
    NativePluginLibrary library;
    std::string message;
    if (!library.open(absolute_path, message)) {
      const auto code = absolute_path.is_absolute()
                            ? PluginHostErrorCode::LoadFailed
                            : PluginHostErrorCode::InvalidPath;
      return {{}, host_error(code, "load", absolute_path, std::move(message))};
    }

    void *entry_address =
        library.symbol(FLEXUI_PLUGIN_ENTRY_SYMBOL_V1, message);
    if (entry_address == nullptr) {
      return {{}, host_error(PluginHostErrorCode::EntryPointMissing, "resolve",
                             library.path(), std::move(message))};
    }
    auto entry = reinterpret_cast<flexui_plugin_get_api_v1_fn>(entry_address);
    const flexui_plugin_api_v1 *api = entry();
    if (api == nullptr ||
        api->struct_size < field_end(&flexui_plugin_api_v1::destroy) ||
        api->abi_major != FLEXUI_PLUGIN_ABI_MAJOR ||
        api->descriptor == nullptr || api->create == nullptr ||
        api->start == nullptr || api->submit == nullptr || api->stop == nullptr ||
        api->join == nullptr || api->destroy == nullptr) {
      return {{}, host_error(PluginHostErrorCode::UnsupportedAbi, "validate_abi",
                             library.path(),
                             "plugin ABI table is missing, truncated, or incompatible")};
    }

    const auto *descriptor = api->descriptor;
    if (descriptor->struct_size <
            field_end(&flexui_plugin_descriptor_v1::services) ||
        descriptor->required_host_minor > FLEXUI_PLUGIN_ABI_MINOR ||
        descriptor->service_count == 0 ||
        descriptor->service_count > limits.registry.max_services ||
        descriptor->services == nullptr) {
      return {{}, host_error(PluginHostErrorCode::InvalidDescriptor,
                             "validate_descriptor", library.path(),
                             "plugin descriptor is truncated or exceeds host limits")};
    }

    std::string plugin_id;
    std::string plugin_version;
    if (!copy_utf8_view(descriptor->plugin_id,
                        limits.max_plugin_identifier_bytes, true, plugin_id,
                        message) ||
        !valid_plugin_identifier(plugin_id) ||
        !copy_utf8_view(descriptor->plugin_version,
                        limits.max_plugin_version_bytes, true, plugin_version,
                        message)) {
      if (!valid_plugin_identifier(plugin_id)) {
        message = "plugin id must use canonical lowercase identifier syntax";
      }
      return {{}, host_error(PluginHostErrorCode::InvalidDescriptor,
                             "validate_descriptor", library.path(),
                             std::move(message), std::move(plugin_id))};
    }

    std::vector<ApplicationServiceDescriptor> services;
    services.reserve(descriptor->service_count);
    for (std::uint32_t service_index = 0;
         service_index < descriptor->service_count; ++service_index) {
      const auto &source_service = descriptor->services[service_index];
      if (source_service.struct_size <
              field_end(&flexui_plugin_service_descriptor_v1::operations) ||
          source_service.operation_count == 0 ||
          source_service.operation_count >
              limits.registry.max_operations_per_service ||
          source_service.operations == nullptr) {
        return {{}, host_error(PluginHostErrorCode::InvalidDescriptor,
                               "validate_descriptor", library.path(),
                               "plugin service descriptor is truncated or invalid",
                               plugin_id)};
      }

      ApplicationServiceDescriptor service;
      if (!copy_utf8_view(source_service.capability,
                          limits.registry.max_identifier_bytes, true,
                          service.capability, message)) {
        return {{}, host_error(PluginHostErrorCode::InvalidDescriptor,
                               "validate_descriptor", library.path(),
                               std::move(message), plugin_id)};
      }
      service.operations.reserve(source_service.operation_count);
      for (std::uint32_t operation_index = 0;
           operation_index < source_service.operation_count; ++operation_index) {
        const auto &source_operation = source_service.operations[operation_index];
        if (source_operation.struct_size <
                field_end(&flexui_plugin_operation_descriptor_v1::max_payload_bytes) ||
            source_operation.max_payload_bytes == 0 ||
            source_operation.max_payload_bytes > limits.registry.max_payload_bytes ||
            source_operation.max_payload_bytes >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max())) {
          return {{}, host_error(PluginHostErrorCode::InvalidDescriptor,
                                 "validate_descriptor", library.path(),
                                 "plugin operation descriptor is invalid", plugin_id)};
        }
        ApplicationServiceOperationDescriptor operation;
        if (!copy_utf8_view(source_operation.name,
                            limits.registry.max_identifier_bytes, true,
                            operation.name, message)) {
          return {{}, host_error(PluginHostErrorCode::InvalidDescriptor,
                                 "validate_descriptor", library.path(),
                                 std::move(message), plugin_id)};
        }
        operation.max_payload_bytes =
            static_cast<std::size_t>(source_operation.max_payload_bytes);
        service.operations.push_back(std::move(operation));
      }
      services.push_back(std::move(service));
    }

    auto module = std::shared_ptr<PluginModule>(new PluginModule(
        std::move(library), api, nullptr, plugin_id, std::move(plugin_version),
        descriptor->required_host_minor, std::move(services), limits));
    AbiErrorBuffer error_buffer(limits.max_error_message_bytes);
    flexui_plugin_instance *instance = nullptr;
    const auto status = api->create(&module->host_api_, &instance,
                                    &error_buffer.abi);
    if (status != FLEXUI_PLUGIN_STATUS_OK || instance == nullptr) {
      if (instance != nullptr) {
        api->destroy(instance);
      }
      return {{}, host_error(PluginHostErrorCode::CreateFailed, "create",
                             module->library_.path(), error_buffer.message(),
                             module->plugin_id_, status)};
    }
    module->instance_ = instance;
    return {std::move(module), {}};
  } catch (const std::bad_alloc &) {
    return {{}, host_error(PluginHostErrorCode::AllocationFailed, "load",
                           absolute_path, "plugin load allocation failed")};
  } catch (const std::exception &error) {
    return {{}, host_error(PluginHostErrorCode::InternalInvariant, "load",
                           absolute_path, error.what())};
  } catch (...) {
    return {{}, host_error(PluginHostErrorCode::InternalInvariant, "load",
                           absolute_path, "plugin load failed unexpectedly")};
  }
}

PluginModule::~PluginModule() {
  auto result = stop(std::chrono::milliseconds::max());
  if (!result) {
    abandon_live_library();
  }
}

ApplicationServiceSubmitResult PluginModule::try_submit(
    const ApplicationServiceRequest &request,
    IApplicationServiceCompletionSink &completion_sink) {
  if (!request.token || request.script_request_id == 0) {
    std::lock_guard<std::mutex> lock(slots_mutex_);
    ++statistics_.rejected;
    return {submit_error(ApplicationServiceSubmitErrorCode::InvalidRequest,
                         "plugin request identity is invalid")};
  }
  const auto *operation = find_operation(services_, request);
  if (operation == nullptr || request.payload.size() > operation->max_payload_bytes) {
    std::lock_guard<std::mutex> lock(slots_mutex_);
    ++statistics_.rejected;
    return {submit_error(ApplicationServiceSubmitErrorCode::InvalidRequest,
                         "plugin request is not declared or exceeds its payload limit")};
  }

  std::size_t slot_index = slots_.size();
  {
    std::lock_guard<std::mutex> lock(slots_mutex_);
    if (!accepting_) {
      ++statistics_.rejected;
      return {submit_error(ApplicationServiceSubmitErrorCode::Closed,
                           "plugin is not accepting requests")};
    }
    const auto duplicate = std::find_if(
        slots_.begin(), slots_.end(), [&request](const Slot &slot) {
          return slot.state != SlotState::Free &&
                 slot.token.id == request.token.id &&
                 slot.token.generation == request.token.generation;
        });
    if (duplicate != slots_.end()) {
      ++statistics_.rejected;
      return {submit_error(ApplicationServiceSubmitErrorCode::InvalidRequest,
                           "plugin request token is already active")};
    }
    const auto free_slot = std::find_if(slots_.begin(), slots_.end(),
                                        [](const Slot &slot) {
                                          return slot.state == SlotState::Free;
                                        });
    if (free_slot == slots_.end()) {
      ++statistics_.rejected;
      return {submit_error(ApplicationServiceSubmitErrorCode::Busy,
                           "plugin request capacity is full")};
    }
    slot_index = static_cast<std::size_t>(free_slot - slots_.begin());
    free_slot->state = SlotState::Active;
    free_slot->token = request.token;
    free_slot->script_request_id = request.script_request_id;
    free_slot->sink = &completion_sink;
    ++active_submit_calls_;
    ++statistics_.submitted;
    ++statistics_.current_in_flight;
    statistics_.peak_in_flight =
        std::max(statistics_.peak_in_flight,
                 statistics_.current_in_flight);
  }

  flexui_plugin_request_v1 abi_request{};
  abi_request.struct_size = sizeof(abi_request);
  abi_request.token_id = request.token.id;
  abi_request.token_generation = request.token.generation;
  abi_request.script_request_id = request.script_request_id;
  abi_request.capability = view_of(request.capability);
  abi_request.operation = view_of(request.operation);
  abi_request.payload = view_of(request.payload);
  AbiErrorBuffer error_buffer(limits_.max_error_message_bytes);
  flexui_plugin_status status = FLEXUI_PLUGIN_STATUS_INTERNAL;
  const char *boundary_error = nullptr;
  try {
    status = api_->submit(instance_, &abi_request, &error_buffer.abi);
  } catch (const std::bad_alloc &) {
    status = FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
    boundary_error = "plugin submit threw an allocation exception across the C ABI";
  } catch (...) {
    status = FLEXUI_PLUGIN_STATUS_INTERNAL;
    boundary_error = "plugin submit threw an exception across the C ABI";
  }

  bool slot_was_released = false;
  {
    std::lock_guard<std::mutex> lock(slots_mutex_);
    --active_submit_calls_;
    submits_idle_.notify_all();
    auto &slot = slots_[slot_index];
    if (status != FLEXUI_PLUGIN_STATUS_OK &&
        slot.state == SlotState::Active && slot.token.id == request.token.id &&
        slot.token.generation == request.token.generation) {
      slot = {};
      --statistics_.current_in_flight;
      ++statistics_.rejected;
      slot_was_released = true;
    }
  }

  if (status == FLEXUI_PLUGIN_STATUS_OK) {
    return {};
  }
  if (!slot_was_released) {
    return {submit_error(ApplicationServiceSubmitErrorCode::InternalFailure,
                         "plugin completed a request after reporting submit failure")};
  }
  auto message = boundary_error == nullptr ? error_buffer.message()
                                           : std::string(boundary_error);
  if (message.empty()) {
    message = "plugin rejected the request";
  }
  return {submit_error(map_submit_status(status), std::move(message))};
}

PluginHostResult PluginModule::start() {
  std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
  if (lifecycle_state_ != LifecycleState::Created || instance_ == nullptr ||
      api_ == nullptr) {
    return {host_error(PluginHostErrorCode::InvalidState, "start", path(),
                       "plugin is not in the created state", plugin_id_)};
  }
  AbiErrorBuffer error_buffer(limits_.max_error_message_bytes);
  flexui_plugin_status status = FLEXUI_PLUGIN_STATUS_INTERNAL;
  const char *boundary_error = nullptr;
  try {
    status = api_->start(instance_, &error_buffer.abi);
  } catch (const std::bad_alloc &) {
    status = FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
    boundary_error = "plugin start threw an allocation exception across the C ABI";
  } catch (...) {
    boundary_error = "plugin start threw an exception across the C ABI";
  }
  if (status != FLEXUI_PLUGIN_STATUS_OK) {
    return {host_error(PluginHostErrorCode::StartFailed, "start", path(),
                       boundary_error == nullptr ? error_buffer.message()
                                                 : std::string(boundary_error),
                       plugin_id_, status)};
  }
  lifecycle_state_ = LifecycleState::Started;
  {
    std::lock_guard<std::mutex> slots_lock(slots_mutex_);
    accepting_ = true;
  }
  return {};
}

void PluginModule::disable_submissions() noexcept {
  std::lock_guard<std::mutex> lock(slots_mutex_);
  accepting_ = false;
}

PluginHostResult PluginModule::stop(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lifecycle_lock(lifecycle_mutex_);
  if (lifecycle_state_ == LifecycleState::Stopped) {
    return {};
  }
  if (lifecycle_state_ == LifecycleState::Created) {
    destroy_created();
    lifecycle_state_ = LifecycleState::Stopped;
    return {};
  }

  lifecycle_state_ = LifecycleState::Stopping;
  disable_submissions();
  const bool infinite = timeout == std::chrono::milliseconds::max();
  const auto deadline = infinite ? std::chrono::steady_clock::time_point::max()
                                 : std::chrono::steady_clock::now() + timeout;
  {
    std::unique_lock<std::mutex> slots_lock(slots_mutex_);
    const auto idle = [this] { return active_submit_calls_ == 0; };
    if (infinite) {
      submits_idle_.wait(slots_lock, idle);
    } else if (!submits_idle_.wait_until(slots_lock, deadline, idle)) {
      return {host_error(PluginHostErrorCode::JoinTimedOut, "quiesce", path(),
                         "timed out waiting for active submit calls", plugin_id_,
                         FLEXUI_PLUGIN_STATUS_TIMED_OUT)};
    }
  }

  if (!stop_succeeded_) {
    AbiErrorBuffer error_buffer(limits_.max_error_message_bytes);
    flexui_plugin_status status = FLEXUI_PLUGIN_STATUS_INTERNAL;
    const char *boundary_error = nullptr;
    try {
      status = api_->stop(instance_, &error_buffer.abi);
    } catch (const std::bad_alloc &) {
      status = FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
      boundary_error = "plugin stop threw an allocation exception across the C ABI";
    } catch (...) {
      boundary_error = "plugin stop threw an exception across the C ABI";
    }
    if (status != FLEXUI_PLUGIN_STATUS_OK) {
      return {host_error(PluginHostErrorCode::StopFailed, "stop", path(),
                         boundary_error == nullptr ? error_buffer.message()
                                                   : std::string(boundary_error),
                         plugin_id_, status)};
    }
    stop_succeeded_ = true;
  }

  std::uint64_t remaining_ms = FLEXUI_PLUGIN_TIMEOUT_INFINITE;
  if (!infinite) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      remaining_ms = 0;
    } else {
      remaining_ms = static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
              .count());
    }
  }
  AbiErrorBuffer error_buffer(limits_.max_error_message_bytes);
  flexui_plugin_status status = FLEXUI_PLUGIN_STATUS_INTERNAL;
  const char *boundary_error = nullptr;
  try {
    status = api_->join(instance_, remaining_ms, &error_buffer.abi);
  } catch (const std::bad_alloc &) {
    status = FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
    boundary_error = "plugin join threw an allocation exception across the C ABI";
  } catch (...) {
    boundary_error = "plugin join threw an exception across the C ABI";
  }
  if (status == FLEXUI_PLUGIN_STATUS_TIMED_OUT) {
    return {host_error(PluginHostErrorCode::JoinTimedOut, "join", path(),
                       boundary_error == nullptr ? error_buffer.message()
                                                 : std::string(boundary_error),
                       plugin_id_, status)};
  }
  if (status != FLEXUI_PLUGIN_STATUS_OK) {
    return {host_error(PluginHostErrorCode::JoinFailed, "join", path(),
                       boundary_error == nullptr ? error_buffer.message()
                                                 : std::string(boundary_error),
                       plugin_id_, status)};
  }

  {
    std::lock_guard<std::mutex> slots_lock(slots_mutex_);
    for (auto &slot : slots_) {
      if (slot.state != SlotState::Free) {
        ++statistics_.abandoned;
        --statistics_.current_in_flight;
        slot = {};
      }
    }
  }
  api_->destroy(instance_);
  instance_ = nullptr;
  api_ = nullptr;
  library_.close();
  lifecycle_state_ = LifecycleState::Stopped;
  return {};
}

PluginHostStatistics PluginModule::statistics() const noexcept {
  std::lock_guard<std::mutex> lock(slots_mutex_);
  return statistics_;
}

const std::vector<ApplicationServiceDescriptor> &
PluginModule::services() const noexcept {
  return services_;
}

const std::string &PluginModule::plugin_id() const noexcept { return plugin_id_; }

const std::string &PluginModule::plugin_version() const noexcept {
  return plugin_version_;
}

std::uint32_t PluginModule::required_host_minor() const noexcept {
  return required_host_minor_;
}

const std::filesystem::path &PluginModule::path() const noexcept {
  return library_.path();
}

bool PluginModule::stopped() const noexcept {
  std::lock_guard<std::mutex> lock(lifecycle_mutex_);
  return lifecycle_state_ == LifecycleState::Stopped;
}

flexui_plugin_status FLEXUI_PLUGIN_CALL PluginModule::post_completion_callback(
    void *host_context, const flexui_plugin_completion_v1 *completion,
    flexui_plugin_error_buffer *error) noexcept {
  if (host_context == nullptr) {
    write_abi_error(error, "host context is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  return static_cast<PluginModule *>(host_context)->post_completion(completion,
                                                                    error);
}

flexui_plugin_status PluginModule::post_completion(
    const flexui_plugin_completion_v1 *completion,
    flexui_plugin_error_buffer *error) noexcept {
  try {
    if (completion == nullptr ||
        completion->struct_size <
            field_end(&flexui_plugin_completion_v1::error_message) ||
        completion->token_id == 0 || completion->token_generation == 0 ||
        !valid_completion_shape(*completion) ||
        !valid_completion_view(completion->payload,
                               limits_.completion.max_payload_bytes) ||
        !valid_completion_view(completion->error_code,
                               limits_.completion.max_error_code_bytes) ||
        !valid_completion_view(completion->error_message,
                               limits_.completion.max_error_message_bytes) ||
        !completion_strings_fit(*completion,
                                limits_.completion.max_total_string_bytes)) {
      write_abi_error(error, "completion is invalid or exceeds host limits");
      return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
    }

    ApplicationCompletion value;
    value.token = {completion->token_id, completion->token_generation};
    switch (completion->status) {
    case FLEXUI_PLUGIN_COMPLETION_SUCCEEDED:
      value.status = ApplicationCompletionStatus::Succeeded;
      break;
    case FLEXUI_PLUGIN_COMPLETION_FAILED:
      value.status = ApplicationCompletionStatus::Failed;
      break;
    case FLEXUI_PLUGIN_COMPLETION_CANCELLED:
      value.status = ApplicationCompletionStatus::Cancelled;
      break;
    default:
      write_abi_error(error, "completion status is invalid");
      return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    value.payload = copy_completion_view(completion->payload);
    value.error_code = copy_completion_view(completion->error_code);
    value.error_message = copy_completion_view(completion->error_message);

    Slot *selected = nullptr;
    IApplicationServiceCompletionSink *sink = nullptr;
    {
      std::lock_guard<std::mutex> lock(slots_mutex_);
      const auto found = std::find_if(
          slots_.begin(), slots_.end(), [&value](const Slot &slot) {
            return slot.state != SlotState::Free &&
                   slot.token.id == value.token.id &&
                   slot.token.generation == value.token.generation;
          });
      if (found == slots_.end()) {
        write_abi_error(error, "completion token is stale or unknown");
        return FLEXUI_PLUGIN_STATUS_STALE;
      }
      if (found->state == SlotState::Posting) {
        write_abi_error(error, "completion token is already being posted");
        return FLEXUI_PLUGIN_STATUS_BUSY;
      }
      found->state = SlotState::Posting;
      value.token = found->token;
      sink = found->sink;
      selected = &*found;
    }

    ApplicationCompletionPostResult posted;
    try {
      posted = sink->try_post(value);
    } catch (const std::bad_alloc &) {
      std::lock_guard<std::mutex> lock(slots_mutex_);
      selected->state = SlotState::Active;
      ++statistics_.completion_retries;
      write_abi_error(error, "completion sink allocation failed");
      return FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
    } catch (...) {
      std::lock_guard<std::mutex> lock(slots_mutex_);
      *selected = {};
      --statistics_.current_in_flight;
      ++statistics_.abandoned;
      write_abi_error(error, "completion sink threw unexpectedly");
      return FLEXUI_PLUGIN_STATUS_INTERNAL;
    }
    std::lock_guard<std::mutex> lock(slots_mutex_);
    if (posted) {
      *selected = {};
      --statistics_.current_in_flight;
      ++statistics_.completed;
      return FLEXUI_PLUGIN_STATUS_OK;
    }
    if (posted.error.code == ApplicationCompletionErrorCode::QueueFull ||
        posted.error.code == ApplicationCompletionErrorCode::AllocationFailed) {
      selected->state = SlotState::Active;
      ++statistics_.completion_retries;
      write_abi_error(error, posted.error.message);
      return posted.error.code == ApplicationCompletionErrorCode::QueueFull
                 ? FLEXUI_PLUGIN_STATUS_QUEUE_FULL
                 : FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
    }

    *selected = {};
    --statistics_.current_in_flight;
    ++statistics_.abandoned;
    write_abi_error(error, posted.error.message);
    if (posted.error.code == ApplicationCompletionErrorCode::Closed) {
      return FLEXUI_PLUGIN_STATUS_CLOSED;
    }
    if (posted.error.code == ApplicationCompletionErrorCode::StaleGeneration) {
      return FLEXUI_PLUGIN_STATUS_STALE;
    }
    return FLEXUI_PLUGIN_STATUS_INTERNAL;
  } catch (const std::bad_alloc &) {
    write_abi_error(error, "completion allocation failed");
    return FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
  } catch (...) {
    write_abi_error(error, "completion dispatch failed unexpectedly");
    return FLEXUI_PLUGIN_STATUS_INTERNAL;
  }
}

void PluginModule::destroy_created() noexcept {
  if (instance_ != nullptr && api_ != nullptr) {
    api_->destroy(instance_);
  }
  instance_ = nullptr;
  api_ = nullptr;
  library_.close();
}

void PluginModule::abandon_live_library() noexcept {
  instance_ = nullptr;
  api_ = nullptr;
  library_.abandon();
}

} // namespace flexUI::plugin_host_detail
