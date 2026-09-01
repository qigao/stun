#define FLEXUI_PLUGIN_BUILD
#include <flexUI/plugin_abi.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

#ifndef FLEXUI_TEST_PLUGIN_ID
#if defined(FLEXUI_TEST_FAIL_START)
#define FLEXUI_TEST_PLUGIN_ID "test.fail-start"
#else
#define FLEXUI_TEST_PLUGIN_ID "test.echo"
#endif
#endif

#ifndef FLEXUI_TEST_PLUGIN_SERVICE
#if defined(FLEXUI_TEST_FAIL_START)
#define FLEXUI_TEST_PLUGIN_SERVICE "test.fail/1"
#else
#define FLEXUI_TEST_PLUGIN_SERVICE "test.echo/1"
#endif
#endif

constexpr std::chrono::milliseconds kRetryDelay{1};
constexpr std::chrono::milliseconds kBlockDelay{150};
constexpr std::chrono::milliseconds kDeferredCopyDelay{25};

flexui_plugin_bytes_view literal_view(const char *text) {
  return {reinterpret_cast<const std::uint8_t *>(text),
          static_cast<std::uint64_t>(std::strlen(text))};
}

flexui_plugin_bytes_view string_view(const std::string &text) {
  return {reinterpret_cast<const std::uint8_t *>(text.data()),
          static_cast<std::uint64_t>(text.size())};
}

std::string copy_view(flexui_plugin_bytes_view view) {
  return std::string(reinterpret_cast<const char *>(view.data),
                     static_cast<std::size_t>(view.size));
}

void write_error(flexui_plugin_error_buffer *error, const char *message) {
  if (error == nullptr || error->message_data == nullptr) {
    return;
  }
  const auto length = static_cast<std::uint64_t>(std::strlen(message));
  const auto count = length < error->message_capacity ? length
                                                       : error->message_capacity;
  std::memcpy(error->message_data, message, static_cast<std::size_t>(count));
  error->message_size = count;
}

struct TestInstance {
  flexui_host_api_v1 host{};
  std::atomic<bool> started{false};
  std::atomic<bool> stopping{false};
  std::atomic<std::uint32_t> stop_attempts{0};
  std::mutex mutex;
  std::condition_variable idle;
  std::size_t active_workers = 0;
  std::vector<std::thread> workers;
};

flexui_plugin_status post_completion(TestInstance &instance,
                                     std::uint64_t token_id,
                                     std::uint64_t generation,
                                     const std::string &payload) {
  flexui_plugin_completion_v1 completion{};
  completion.struct_size = sizeof(completion);
  completion.status = FLEXUI_PLUGIN_COMPLETION_SUCCEEDED;
  completion.token_id = token_id;
  completion.token_generation = generation;
  completion.payload = string_view(payload);
  std::uint8_t error_storage[128]{};
  flexui_plugin_error_buffer error{};
  error.struct_size = sizeof(error);
  error.message_data = error_storage;
  error.message_capacity = sizeof(error_storage);
  return instance.host.post_completion(instance.host.host_context, &completion,
                                       &error);
}

void finish_worker(TestInstance &instance) {
  std::lock_guard<std::mutex> lock(instance.mutex);
  --instance.active_workers;
  instance.idle.notify_all();
}

flexui_plugin_status FLEXUI_PLUGIN_CALL
create_instance(const flexui_host_api_v1 *host,
                flexui_plugin_instance **out_instance,
                flexui_plugin_error_buffer *error) {
  if (host == nullptr || out_instance == nullptr ||
      host->struct_size < sizeof(flexui_host_api_v1) ||
      host->abi_major != FLEXUI_PLUGIN_ABI_MAJOR ||
      host->post_completion == nullptr) {
    write_error(error, "invalid host API");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
#if defined(FLEXUI_TEST_FAIL_CREATE)
  write_error(error, "create rejected by test plugin");
  return FLEXUI_PLUGIN_STATUS_REJECTED;
#else
  auto *instance = new (std::nothrow) TestInstance;
  if (instance == nullptr) {
    write_error(error, "instance allocation failed");
    return FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
  }
  instance->host = *host;
  *out_instance = reinterpret_cast<flexui_plugin_instance *>(instance);
  return FLEXUI_PLUGIN_STATUS_OK;
#endif
}

flexui_plugin_status FLEXUI_PLUGIN_CALL
start_instance(flexui_plugin_instance *opaque,
               flexui_plugin_error_buffer *error) {
  if (opaque == nullptr) {
    write_error(error, "instance is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
#if defined(FLEXUI_TEST_FAIL_START)
  write_error(error, "start rejected by test plugin");
  return FLEXUI_PLUGIN_STATUS_REJECTED;
#else
  auto &instance = *reinterpret_cast<TestInstance *>(opaque);
  if (instance.started.exchange(true)) {
    write_error(error, "instance is already started");
    return FLEXUI_PLUGIN_STATUS_INVALID_STATE;
  }
  return FLEXUI_PLUGIN_STATUS_OK;
#endif
}

flexui_plugin_status FLEXUI_PLUGIN_CALL
submit_request(flexui_plugin_instance *opaque,
               const flexui_plugin_request_v1 *request,
               flexui_plugin_error_buffer *error) {
  if (opaque == nullptr || request == nullptr) {
    write_error(error, "request is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  auto &instance = *reinterpret_cast<TestInstance *>(opaque);
  if (!instance.started.load() || instance.stopping.load()) {
    write_error(error, "instance is stopping");
    return FLEXUI_PLUGIN_STATUS_CLOSED;
  }
  const auto operation = copy_view(request->operation);
  const auto payload = copy_view(request->payload);
  if (operation == "echo") {
    return post_completion(instance, request->token_id,
                           request->token_generation, payload);
  }
  if (operation == "reject") {
    write_error(error, "request rejected by test plugin");
    return FLEXUI_PLUGIN_STATUS_REJECTED;
  }
  if (operation == "hold") {
    return FLEXUI_PLUGIN_STATUS_OK;
  }
  if (operation == "throw") {
    throw 7;
  }
  if (operation != "retry" && operation != "block" &&
      operation != "deferred-copy") {
    write_error(error, "operation is unsupported");
    return FLEXUI_PLUGIN_STATUS_REJECTED;
  }

  try {
    {
      std::lock_guard<std::mutex> lock(instance.mutex);
      ++instance.active_workers;
    }
    instance.workers.emplace_back(
        [&instance, operation, payload, token_id = request->token_id,
         generation = request->token_generation] {
          if (operation == "block") {
            std::this_thread::sleep_for(kBlockDelay);
            if (!instance.stopping.load()) {
              (void)post_completion(instance, token_id, generation, payload);
            }
          } else if (operation == "deferred-copy") {
            std::this_thread::sleep_for(kDeferredCopyDelay);
            if (!instance.stopping.load()) {
              (void)post_completion(instance, token_id, generation, payload);
            }
          } else {
            while (!instance.stopping.load()) {
              const auto status =
                  post_completion(instance, token_id, generation, payload);
              if (status == FLEXUI_PLUGIN_STATUS_OK ||
                  (status != FLEXUI_PLUGIN_STATUS_QUEUE_FULL &&
                   status != FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT)) {
                break;
              }
              std::this_thread::sleep_for(kRetryDelay);
            }
          }
          finish_worker(instance);
        });
  } catch (...) {
    finish_worker(instance);
    write_error(error, "worker creation failed");
    return FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
  }
  return FLEXUI_PLUGIN_STATUS_OK;
}

flexui_plugin_status FLEXUI_PLUGIN_CALL
stop_instance(flexui_plugin_instance *opaque,
              flexui_plugin_error_buffer *error) {
  if (opaque == nullptr) {
    write_error(error, "instance is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  auto &instance = *reinterpret_cast<TestInstance *>(opaque);
#if defined(FLEXUI_TEST_FAIL_STOP_ONCE)
  if (instance.stop_attempts.fetch_add(1) == 0) {
    write_error(error, "stop rejected once by test plugin");
    return FLEXUI_PLUGIN_STATUS_REJECTED;
  }
#endif
  instance.stopping.store(true);
  return FLEXUI_PLUGIN_STATUS_OK;
}

flexui_plugin_status FLEXUI_PLUGIN_CALL
join_instance(flexui_plugin_instance *opaque, std::uint64_t timeout_ms,
              flexui_plugin_error_buffer *error) {
  if (opaque == nullptr) {
    write_error(error, "instance is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  auto &instance = *reinterpret_cast<TestInstance *>(opaque);
  std::unique_lock<std::mutex> lock(instance.mutex);
  const auto idle = [&instance] { return instance.active_workers == 0; };
  bool ready = false;
  if (timeout_ms == FLEXUI_PLUGIN_TIMEOUT_INFINITE) {
    instance.idle.wait(lock, idle);
    ready = true;
  } else {
    ready = instance.idle.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                   idle);
  }
  if (!ready) {
    write_error(error, "test worker is still active");
    return FLEXUI_PLUGIN_STATUS_TIMED_OUT;
  }
  lock.unlock();
  for (auto &worker : instance.workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  instance.workers.clear();
  return FLEXUI_PLUGIN_STATUS_OK;
}

void FLEXUI_PLUGIN_CALL destroy_instance(flexui_plugin_instance *opaque) {
  delete reinterpret_cast<TestInstance *>(opaque);
}

const flexui_plugin_operation_descriptor_v1 operations[] = {
    {sizeof(flexui_plugin_operation_descriptor_v1), 0, literal_view("echo"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v1), 0, literal_view("reject"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v1), 0, literal_view("hold"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v1), 0, literal_view("retry"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v1), 0, literal_view("block"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v1), 0, literal_view("throw"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v1), 0,
     literal_view("deferred-copy"), 4096, {0}},
};

const flexui_plugin_service_descriptor_v1 services[] = {
    {sizeof(flexui_plugin_service_descriptor_v1),
     static_cast<std::uint32_t>(sizeof(operations) / sizeof(operations[0])),
     literal_view(FLEXUI_TEST_PLUGIN_SERVICE), operations, {0}}};

const flexui_plugin_descriptor_v1 descriptor = {
    sizeof(flexui_plugin_descriptor_v1), 0,
    literal_view(FLEXUI_TEST_PLUGIN_ID),
    literal_view("1.0.0"), 1, 0, services, {0}};

const flexui_plugin_api_v1 api = {
    sizeof(flexui_plugin_api_v1), FLEXUI_PLUGIN_ABI_MAJOR,
    FLEXUI_PLUGIN_ABI_MINOR,      0,
    &descriptor,                  &create_instance,
    &start_instance,             &submit_request,
    &stop_instance,              &join_instance,
    &destroy_instance,           {0}};

} // namespace

extern "C" FLEXUI_PLUGIN_EXPORT const flexui_plugin_api_v1 *FLEXUI_PLUGIN_CALL
flexui_plugin_get_api_v1(void) {
  return &api;
}
