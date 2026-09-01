#ifndef FLEXUI_PLUGIN_ABI_H
#define FLEXUI_PLUGIN_ABI_H

#include <stdint.h>

#ifdef _WIN32
#define FLEXUI_PLUGIN_CALL __cdecl
#if defined(FLEXUI_PLUGIN_BUILD)
#define FLEXUI_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FLEXUI_PLUGIN_EXPORT
#endif
#else
#define FLEXUI_PLUGIN_CALL
#if defined(FLEXUI_PLUGIN_BUILD)
#define FLEXUI_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define FLEXUI_PLUGIN_EXPORT
#endif
#endif

#define FLEXUI_PLUGIN_ABI_MAJOR UINT32_C(1)
#define FLEXUI_PLUGIN_ABI_MINOR UINT32_C(0)
#define FLEXUI_PLUGIN_ENTRY_SYMBOL_V1 "flexui_plugin_get_api_v1"
#define FLEXUI_PLUGIN_TIMEOUT_INFINITE UINT64_MAX

typedef uint32_t flexui_plugin_status;

#define FLEXUI_PLUGIN_STATUS_OK UINT32_C(0)
#define FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT UINT32_C(1)
#define FLEXUI_PLUGIN_STATUS_INVALID_STATE UINT32_C(2)
#define FLEXUI_PLUGIN_STATUS_UNSUPPORTED_ABI UINT32_C(3)
#define FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT UINT32_C(4)
#define FLEXUI_PLUGIN_STATUS_BUSY UINT32_C(5)
#define FLEXUI_PLUGIN_STATUS_CLOSED UINT32_C(6)
#define FLEXUI_PLUGIN_STATUS_REJECTED UINT32_C(7)
#define FLEXUI_PLUGIN_STATUS_TIMED_OUT UINT32_C(8)
#define FLEXUI_PLUGIN_STATUS_INTERNAL UINT32_C(9)
#define FLEXUI_PLUGIN_STATUS_QUEUE_FULL UINT32_C(10)
#define FLEXUI_PLUGIN_STATUS_STALE UINT32_C(11)

typedef uint32_t flexui_plugin_completion_status;

#define FLEXUI_PLUGIN_COMPLETION_SUCCEEDED UINT32_C(0)
#define FLEXUI_PLUGIN_COMPLETION_FAILED UINT32_C(1)
#define FLEXUI_PLUGIN_COMPLETION_CANCELLED UINT32_C(2)

/*
 * Every view is borrowed and immutable for the duration of the ABI call that
 * receives it. A callee must copy bytes that it retains after returning.
 */
typedef struct flexui_plugin_bytes_view {
  const uint8_t *data;
  uint64_t size;
} flexui_plugin_bytes_view;

/*
 * The caller owns message_data and its capacity. The callee sets message_size
 * to the number of UTF-8 bytes written, excluding a terminator. It never frees
 * or retains the buffer. A zero-capacity buffer is valid for status-only use.
 */
typedef struct flexui_plugin_error_buffer {
  uint32_t struct_size;
  uint32_t reserved0;
  uint8_t *message_data;
  uint64_t message_capacity;
  uint64_t message_size;
  uint64_t reserved[4];
} flexui_plugin_error_buffer;

typedef struct flexui_plugin_operation_descriptor_v1 {
  uint32_t struct_size;
  uint32_t reserved0;
  flexui_plugin_bytes_view name;
  uint64_t max_payload_bytes;
  uint64_t reserved[4];
} flexui_plugin_operation_descriptor_v1;

typedef struct flexui_plugin_service_descriptor_v1 {
  uint32_t struct_size;
  uint32_t operation_count;
  flexui_plugin_bytes_view capability;
  const flexui_plugin_operation_descriptor_v1 *operations;
  uint64_t reserved[4];
} flexui_plugin_service_descriptor_v1;

/* Descriptor storage must remain valid until the plugin library is unloaded. */
typedef struct flexui_plugin_descriptor_v1 {
  uint32_t struct_size;
  uint32_t required_host_minor;
  flexui_plugin_bytes_view plugin_id;
  flexui_plugin_bytes_view plugin_version;
  uint32_t service_count;
  uint32_t reserved0;
  const flexui_plugin_service_descriptor_v1 *services;
  uint64_t reserved[8];
} flexui_plugin_descriptor_v1;

typedef struct flexui_plugin_request_v1 {
  uint32_t struct_size;
  uint32_t reserved0;
  uint64_t token_id;
  uint64_t token_generation;
  uint64_t script_request_id;
  flexui_plugin_bytes_view capability;
  flexui_plugin_bytes_view operation;
  flexui_plugin_bytes_view payload;
  uint64_t reserved[4];
} flexui_plugin_request_v1;

typedef struct flexui_plugin_completion_v1 {
  uint32_t struct_size;
  flexui_plugin_completion_status status;
  uint64_t token_id;
  uint64_t token_generation;
  flexui_plugin_bytes_view payload;
  flexui_plugin_bytes_view error_code;
  flexui_plugin_bytes_view error_message;
  uint64_t reserved[4];
} flexui_plugin_completion_v1;

typedef void flexui_plugin_instance;

/*
 * post_completion may be called concurrently from plugin-owned threads after
 * start succeeds and before join succeeds. On QUEUE_FULL the plugin may retry
 * the same token. All completion views are borrowed only for this call.
 */
typedef flexui_plugin_status(FLEXUI_PLUGIN_CALL *
                                flexui_plugin_post_completion_v1_fn)(
    void *host_context, const flexui_plugin_completion_v1 *completion,
    flexui_plugin_error_buffer *error);

typedef struct flexui_host_api_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t reserved0;
  void *host_context;
  flexui_plugin_post_completion_v1_fn post_completion;
  uint64_t reserved[8];
} flexui_host_api_v1;

typedef flexui_plugin_status(FLEXUI_PLUGIN_CALL *flexui_plugin_create_v1_fn)(
    const flexui_host_api_v1 *host, flexui_plugin_instance **out_instance,
    flexui_plugin_error_buffer *error);
typedef flexui_plugin_status(FLEXUI_PLUGIN_CALL *flexui_plugin_start_v1_fn)(
    flexui_plugin_instance *instance, flexui_plugin_error_buffer *error);
/* A successful submit means the plugin copied every request byte it retains. */
typedef flexui_plugin_status(FLEXUI_PLUGIN_CALL *flexui_plugin_submit_v1_fn)(
    flexui_plugin_instance *instance, const flexui_plugin_request_v1 *request,
    flexui_plugin_error_buffer *error);
typedef flexui_plugin_status(FLEXUI_PLUGIN_CALL *flexui_plugin_stop_v1_fn)(
    flexui_plugin_instance *instance, flexui_plugin_error_buffer *error);
typedef flexui_plugin_status(FLEXUI_PLUGIN_CALL *flexui_plugin_join_v1_fn)(
    flexui_plugin_instance *instance, uint64_t timeout_ms,
    flexui_plugin_error_buffer *error);
/* destroy is no-fail and is valid only after join succeeds or create unwind. */
typedef void(FLEXUI_PLUGIN_CALL *flexui_plugin_destroy_v1_fn)(
    flexui_plugin_instance *instance);

typedef struct flexui_plugin_api_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t reserved0;
  const flexui_plugin_descriptor_v1 *descriptor;
  flexui_plugin_create_v1_fn create;
  flexui_plugin_start_v1_fn start;
  flexui_plugin_submit_v1_fn submit;
  flexui_plugin_stop_v1_fn stop;
  flexui_plugin_join_v1_fn join;
  flexui_plugin_destroy_v1_fn destroy;
  uint64_t reserved[8];
} flexui_plugin_api_v1;

typedef const flexui_plugin_api_v1 *(FLEXUI_PLUGIN_CALL *
                                         flexui_plugin_get_api_v1_fn)(void);

#ifdef __cplusplus
extern "C" {
#endif

FLEXUI_PLUGIN_EXPORT const flexui_plugin_api_v1 *FLEXUI_PLUGIN_CALL
flexui_plugin_get_api_v1(void);

#ifdef __cplusplus
}
#endif

#endif
