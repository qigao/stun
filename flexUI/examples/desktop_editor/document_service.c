#include <flexUI/plugin_abi.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FLEXUI_LITERAL_VIEW(value) {(const uint8_t *)(value), (uint64_t)(sizeof(value) - 1U)}

enum { FLEXUI_DOCUMENT_MAX_PAYLOAD_BYTES = 4096 };

typedef struct document_service_instance {
  flexui_host_api_v1 host;
  int started;
  int stopping;
} document_service_instance;

static const char kCapability[] = "document.save/1";
static const char kOperation[] = "save";

static void write_error(flexui_plugin_error_buffer *error, const char *message) {
  uint64_t copy_length;
  if (error == NULL || error->message_data == NULL || message == NULL) {
    return;
  }
  copy_length = (uint64_t)strlen(message);
  if (copy_length > error->message_capacity) {
    copy_length = error->message_capacity;
  }
  if (copy_length != 0U) {
    memcpy(error->message_data, message, (size_t)copy_length);
  }
  error->message_size = copy_length;
}

static int bytes_equal(flexui_plugin_bytes_view value, const char *literal, size_t literal_size) {
  return value.size == (uint64_t)literal_size &&
         (literal_size == 0U ||
          (value.data != NULL && memcmp(value.data, literal, literal_size) == 0));
}

static flexui_plugin_status FLEXUI_PLUGIN_CALL
create_instance(const flexui_host_api_v1 *host, flexui_plugin_instance **out_instance,
                flexui_plugin_error_buffer *error) {
  document_service_instance *instance;
  if (host == NULL || out_instance == NULL || host->struct_size < sizeof(flexui_host_api_v1) ||
      host->abi_major != FLEXUI_PLUGIN_ABI_MAJOR || host->post_completion == NULL) {
    write_error(error, "invalid host API");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  instance = (document_service_instance *)calloc(1U, sizeof(document_service_instance));
  if (instance == NULL) {
    write_error(error, "document service allocation failed");
    return FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
  }
  instance->host = *host;
  *out_instance = (flexui_plugin_instance *)instance;
  return FLEXUI_PLUGIN_STATUS_OK;
}

static flexui_plugin_status FLEXUI_PLUGIN_CALL start_instance(flexui_plugin_instance *opaque,
                                                              flexui_plugin_error_buffer *error) {
  document_service_instance *instance = (document_service_instance *)opaque;
  if (instance == NULL) {
    write_error(error, "document service instance is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (instance->started != 0 || instance->stopping != 0) {
    write_error(error, "document service cannot be started");
    return FLEXUI_PLUGIN_STATUS_INVALID_STATE;
  }
  instance->started = 1;
  return FLEXUI_PLUGIN_STATUS_OK;
}

static flexui_plugin_status FLEXUI_PLUGIN_CALL
submit_request(flexui_plugin_instance *opaque, const flexui_plugin_request_v1 *request,
               flexui_plugin_error_buffer *error) {
  document_service_instance *instance = (document_service_instance *)opaque;
  flexui_plugin_completion_v1 completion;
  if (instance == NULL || request == NULL ||
      request->struct_size < sizeof(flexui_plugin_request_v1)) {
    write_error(error, "valid document request is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (instance->started == 0 || instance->stopping != 0) {
    write_error(error, "document service is not accepting requests");
    return FLEXUI_PLUGIN_STATUS_CLOSED;
  }
  if (!bytes_equal(request->capability, kCapability, sizeof(kCapability) - 1U) ||
      !bytes_equal(request->operation, kOperation, sizeof(kOperation) - 1U)) {
    write_error(error, "unsupported document operation");
    return FLEXUI_PLUGIN_STATUS_REJECTED;
  }
  if (request->payload.size > FLEXUI_DOCUMENT_MAX_PAYLOAD_BYTES ||
      (request->payload.size != 0U && request->payload.data == NULL)) {
    write_error(error, "document payload exceeds the service limit");
    return FLEXUI_PLUGIN_STATUS_RESOURCE_LIMIT;
  }

  memset(&completion, 0, sizeof(completion));
  completion.struct_size = sizeof(completion);
  completion.status = FLEXUI_PLUGIN_COMPLETION_SUCCEEDED;
  completion.token_id = request->token_id;
  completion.token_generation = request->token_generation;
  completion.payload = request->payload;
  return instance->host.post_completion(instance->host.host_context, &completion, error);
}

static flexui_plugin_status FLEXUI_PLUGIN_CALL stop_instance(flexui_plugin_instance *opaque,
                                                             flexui_plugin_error_buffer *error) {
  document_service_instance *instance = (document_service_instance *)opaque;
  if (instance == NULL) {
    write_error(error, "document service instance is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  instance->stopping = 1;
  return FLEXUI_PLUGIN_STATUS_OK;
}

static flexui_plugin_status FLEXUI_PLUGIN_CALL join_instance(flexui_plugin_instance *opaque,
                                                             uint64_t timeout_ms,
                                                             flexui_plugin_error_buffer *error) {
  document_service_instance *instance = (document_service_instance *)opaque;
  (void)timeout_ms;
  if (instance == NULL) {
    write_error(error, "document service instance is required");
    return FLEXUI_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (instance->stopping == 0) {
    write_error(error, "document service stop must precede join");
    return FLEXUI_PLUGIN_STATUS_INVALID_STATE;
  }
  return FLEXUI_PLUGIN_STATUS_OK;
}

static void FLEXUI_PLUGIN_CALL destroy_instance(flexui_plugin_instance *opaque) { free(opaque); }

static const flexui_plugin_operation_descriptor_v1 operations[] = {
    {sizeof(flexui_plugin_operation_descriptor_v1),
     0,
     FLEXUI_LITERAL_VIEW("save"),
     FLEXUI_DOCUMENT_MAX_PAYLOAD_BYTES,
     {0}}};

static const flexui_plugin_service_descriptor_v1 services[] = {
    {sizeof(flexui_plugin_service_descriptor_v1),
     1,
     FLEXUI_LITERAL_VIEW("document.save/1"),
     operations,
     {0}}};

static const flexui_plugin_descriptor_v1 descriptor = {
    sizeof(flexui_plugin_descriptor_v1),
    0,
    FLEXUI_LITERAL_VIEW("example.document-service"),
    FLEXUI_LITERAL_VIEW("1.0.0"),
    1,
    0,
    services,
    {0}};

static const flexui_plugin_api_v1 api = {sizeof(flexui_plugin_api_v1),
                                         FLEXUI_PLUGIN_ABI_MAJOR,
                                         FLEXUI_PLUGIN_ABI_MINOR,
                                         0,
                                         &descriptor,
                                         create_instance,
                                         start_instance,
                                         submit_request,
                                         stop_instance,
                                         join_instance,
                                         destroy_instance,
                                         {0}};

FLEXUI_PLUGIN_EXPORT const flexui_plugin_api_v1 *FLEXUI_PLUGIN_CALL flexui_plugin_get_api_v1(void) {
  return &api;
}
