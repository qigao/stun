#define FLEXUI_PLUGIN_BUILD
#include <flexUI/plugin_abi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <limits>
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

struct WorkerRecord {
  std::string scope;
  std::uint64_t token_id = 0;
  std::uint64_t generation = 0;
  std::thread thread;
  bool finished = false;
};

struct TestInstance {
  flexui_host_api_v2 host{};
  std::atomic<bool> started{false};
  std::atomic<bool> stopping{false};
  std::atomic<std::uint32_t> stop_attempts{0};
  std::mutex mutex;
  std::condition_variable idle;
  std::size_t active_workers = 0;
  bool release_scoped_worker = false;
  std::vector<WorkerRecord> workers;
};

flexui_plugin_status post_completion(TestInstance &instance,
                                     std::uint64_t token_id,
                                     std::uint64_t generation,
                                     const std::string &payload) {
  flexui_plugin_completion_v2 completion{};
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

// A real plugin worker alone publishes this terminal status after observing
// the existing host token's cooperative stop signal.
flexui_plugin_status post_cancelled(TestInstance &instance,
                                    std::uint64_t token_id,
                                    std::uint64_t generation) {
  flexui_plugin_completion_v2 completion{};
  completion.struct_size = sizeof(completion);
  completion.status = FLEXUI_PLUGIN_COMPLETION_CANCELLED;
  completion.token_id = token_id;
  completion.token_generation = generation;
  completion.error_message = literal_view("cooperative plugin worker stopped");
  std::uint8_t error_storage[128]{};
  flexui_plugin_error_buffer error{};
  error.struct_size = sizeof(error);
  error.message_data = error_storage;
  error.message_capacity = sizeof(error_storage);
  return instance.host.post_completion(instance.host.host_context, &completion,
                                       &error);
}

void finish_worker(TestInstance &instance, std::uint64_t token_id,
                   std::uint64_t generation) {
  std::lock_guard<std::mutex> lock(instance.mutex);
  const auto found = std::find_if(instance.workers.begin(), instance.workers.end(),
      [token_id, generation](const WorkerRecord &item) {
        return item.token_id == token_id && item.generation == generation;
      });
  if (found != instance.workers.end()) found->finished = true;
  --instance.active_workers;
  instance.idle.notify_all();
}

flexui_plugin_status FLEXUI_PLUGIN_CALL
create_instance(const flexui_host_api_v2 *host,
                flexui_plugin_instance **out_instance,
                flexui_plugin_error_buffer *error) {
  if (host == nullptr || out_instance == nullptr ||
      host->struct_size < sizeof(flexui_host_api_v2) ||
      host->abi_major != FLEXUI_PLUGIN_ABI_MAJOR ||
      host->post_completion == nullptr || host->cancel_requested == nullptr) {
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
               const flexui_plugin_request_v2 *request,
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
  if (request->struct_size < sizeof(flexui_plugin_request_v2) ||
      request->origin_scope.size > 253 ||
      (request->origin_scope.size != 0 && request->origin_scope.data == nullptr)) {
    write_error(error, "invalid v2.1 request origin");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  const auto operation = copy_view(request->operation);
  const auto payload = copy_view(request->payload);
  const auto scope = request->origin_scope.size == 0
                         ? std::string{} : copy_view(request->origin_scope);
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
  if (operation == "release-scoped-worker") {
    {
      std::lock_guard<std::mutex> lock(instance.mutex);
      instance.release_scoped_worker = true;
      instance.idle.notify_all();
    }
    return post_completion(instance, request->token_id,
                           request->token_generation, payload);
  }
  if (operation == "cancel-watch") {
    std::uint32_t requested = 0;
    const auto status = instance.host.cancel_requested(
        instance.host.host_context, request->token_id,
        request->token_generation, &requested, error);
    if (status != FLEXUI_PLUGIN_STATUS_OK) {
      write_error(error, "cancel-watch requires an exact live native lease");
      return status;
    }
  }
  if (operation == "throw") {
    throw 7;
  }
  if (operation != "retry" && operation != "block" &&
      operation != "deferred-copy" && operation != "cancel-watch") {
    write_error(error, "operation is unsupported");
    return FLEXUI_PLUGIN_STATUS_REJECTED;
  }

  try {
    WorkerRecord record;
    record.scope = scope;
    record.token_id = request->token_id;
    record.generation = request->token_generation;
    {
      std::lock_guard<std::mutex> lock(instance.mutex);
      instance.workers.emplace_back(std::move(record));
      ++instance.active_workers;
      try {
        instance.workers.back().thread = std::thread(
        [&instance, operation, payload, token_id = request->token_id,
         generation = request->token_generation] {
          if (operation == "cancel-watch") {
            // The plugin must observe the exact host token before publishing
            // its own terminal completion; no script context is retained.
            while (!instance.stopping.load()) {
              std::uint32_t requested = 0;
              const auto status = instance.host.cancel_requested(
                  instance.host.host_context, token_id, generation,
                  &requested, nullptr);
              if (status != FLEXUI_PLUGIN_STATUS_OK) break;
              if (requested != 0) {
                (void)post_cancelled(instance, token_id, generation);
                // Deterministic proof: the terminal is ALREADY posted, but
                // the worker remains alive until an independent release call
                // or global stop. Queue drain must never impersonate join.
                std::unique_lock<std::mutex> lock(instance.mutex);
                instance.idle.wait(lock, [&instance] {
                  return instance.release_scoped_worker ||
                         instance.stopping.load();
                });
                break;
              }
              std::this_thread::sleep_for(kRetryDelay);
            }
          } else if (operation == "block") {
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
          finish_worker(instance, token_id, generation);
        });
      } catch (...) {
        --instance.active_workers;
        instance.workers.pop_back();
        throw;
      }
    }
  } catch (...) {
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
  instance.idle.notify_all();
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
    if (worker.thread.joinable()) {
      worker.thread.join();
    }
  }
  instance.workers.clear();
  return FLEXUI_PLUGIN_STATUS_OK;
}

flexui_plugin_status FLEXUI_PLUGIN_CALL
join_scope_instance(flexui_plugin_instance *opaque,
                    flexui_plugin_bytes_view source,
                    std::uint64_t timeout_ms,
                    flexui_plugin_error_buffer *error) {
  if (!opaque || source.size == 0 || source.size > 253 || !source.data) {
    write_error(error, "valid scoped join input required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  auto &instance = *reinterpret_cast<TestInstance *>(opaque);
  try {
    const auto scope = copy_view(source);
    std::unique_lock<std::mutex> lock(instance.mutex);
    const auto done = [&] {
      return std::none_of(instance.workers.begin(), instance.workers.end(),
          [&scope](const WorkerRecord &item) {
            return item.scope == scope && !item.finished;
          });
    };
    bool ready = false;
    if (timeout_ms == FLEXUI_PLUGIN_TIMEOUT_INFINITE) {
      instance.idle.wait(lock, done);
      ready = true;
    } else {
      const auto bounded = std::min(timeout_ms,
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
      ready = instance.idle.wait_for(lock,
          std::chrono::milliseconds(static_cast<std::int64_t>(bounded)), done);
    }
    if (!ready) {
      write_error(error, "worker still running after scoped terminal post");
      return FLEXUI_PLUGIN_STATUS_TIMED_OUT;
    }
    // Reserve before moving any joinable thread out of authoritative storage.
    std::vector<std::thread> retired;
    retired.reserve(instance.workers.size());
    for (auto it = instance.workers.begin(); it != instance.workers.end();) {
      if (it->scope == scope) {
        retired.push_back(std::move(it->thread));
        it = instance.workers.erase(it);
      } else {
        ++it;
      }
    }
    lock.unlock();
    for (auto &worker : retired)
      if (worker.joinable()) worker.join();
    return FLEXUI_PLUGIN_STATUS_OK;
  } catch (...) {
    write_error(error, "scoped join failed");
    return FLEXUI_PLUGIN_STATUS_INTERNAL;
  }
}

void FLEXUI_PLUGIN_CALL destroy_instance(flexui_plugin_instance *opaque) {
  delete reinterpret_cast<TestInstance *>(opaque);
}

const flexui_plugin_operation_descriptor_v2 operations[] = {
    {sizeof(flexui_plugin_operation_descriptor_v2), 0, literal_view("echo"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0, literal_view("reject"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0, literal_view("hold"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0,
     literal_view("release-scoped-worker"), 4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0,
     literal_view("cancel-watch"), 4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0, literal_view("retry"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0, literal_view("block"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0, literal_view("throw"),
     4096, {0}},
    {sizeof(flexui_plugin_operation_descriptor_v2), 0,
     literal_view("deferred-copy"), 4096, {0}},
};

const flexui_plugin_service_descriptor_v2 services[] = {
    {sizeof(flexui_plugin_service_descriptor_v2),
     static_cast<std::uint32_t>(sizeof(operations) / sizeof(operations[0])),
     literal_view(FLEXUI_TEST_PLUGIN_SERVICE), operations, {0}}};

#if defined(FLEXUI_TEST_OLD_MINOR)
constexpr std::uint32_t kPluginMinor = 0;
#else
constexpr std::uint32_t kPluginMinor = FLEXUI_PLUGIN_ABI_MINOR;
#endif

const flexui_plugin_descriptor_v2 descriptor = {
    sizeof(flexui_plugin_descriptor_v2), kPluginMinor,
    literal_view(FLEXUI_TEST_PLUGIN_ID),
    literal_view("1.0.0"), 1, 0, services, {0}};

const flexui_plugin_api_v2 api = {
    sizeof(flexui_plugin_api_v2), FLEXUI_PLUGIN_ABI_MAJOR,
    kPluginMinor,                0,
    &descriptor,                  &create_instance,
    &start_instance,             &submit_request,
    &stop_instance,              &join_instance,
    &destroy_instance,           &join_scope_instance, {0}};

} // namespace

extern "C" FLEXUI_PLUGIN_EXPORT const flexui_plugin_api_v2 *FLEXUI_PLUGIN_CALL
flexui_plugin_get_api_v2(void) {
  return &api;
}
