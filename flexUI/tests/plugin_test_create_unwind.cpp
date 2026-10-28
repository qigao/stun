#define FLEXUI_PLUGIN_BUILD
#include "plugin_create_unwind_fixture.h"

#include <cstdint>
#include <new>
#include <exception>

namespace {

CreateUnwindMode mode = CreateUnwindMode::Reject;
CreateUnwindStatistics counts{};

struct TestInstance {};

class TestCreateException final : public std::exception {
public:
  TestCreateException() { ++counts.live_exceptions; }
  TestCreateException(const TestCreateException&) { ++counts.live_exceptions; }
  ~TestCreateException() override { --counts.live_exceptions; }
  const char* what() const noexcept override {
    return "test create threw after returning ownership";
  }
};

flexui_plugin_status FLEXUI_PLUGIN_CALL create_instance(
    const flexui_host_api_v2*, flexui_plugin_instance** out_instance,
    flexui_plugin_error_buffer*) {
  *out_instance = reinterpret_cast<flexui_plugin_instance*>(new TestInstance);
  ++counts.created;
  switch (mode) {
  case CreateUnwindMode::Reject:
    return FLEXUI_PLUGIN_STATUS_REJECTED;
  case CreateUnwindMode::BadAllocation:
    throw std::bad_alloc{};
  case CreateUnwindMode::StandardException:
    throw TestCreateException{};
  case CreateUnwindMode::OtherException:
    throw mode;
  }
  return FLEXUI_PLUGIN_STATUS_INTERNAL;
}

flexui_plugin_status FLEXUI_PLUGIN_CALL start_instance(
    flexui_plugin_instance*, flexui_plugin_error_buffer*) {
  ++counts.lifecycle_calls;
  return FLEXUI_PLUGIN_STATUS_INVALID_STATE;
}

flexui_plugin_status FLEXUI_PLUGIN_CALL submit_request(
    flexui_plugin_instance*, const flexui_plugin_request_v2*,
    flexui_plugin_error_buffer*) {
  ++counts.lifecycle_calls;
  return FLEXUI_PLUGIN_STATUS_INVALID_STATE;
}

flexui_plugin_status FLEXUI_PLUGIN_CALL join_instance(
    flexui_plugin_instance*, std::uint64_t, flexui_plugin_error_buffer*) {
  ++counts.lifecycle_calls;
  return FLEXUI_PLUGIN_STATUS_INVALID_STATE;
}
flexui_plugin_status FLEXUI_PLUGIN_CALL join_scope_instance(
    flexui_plugin_instance*, flexui_plugin_bytes_view, std::uint64_t,
    flexui_plugin_error_buffer*) {
  ++counts.lifecycle_calls;
  return FLEXUI_PLUGIN_STATUS_INVALID_STATE;
}

void FLEXUI_PLUGIN_CALL destroy_instance(flexui_plugin_instance* instance) {
  if (counts.live_exceptions != 0) ++counts.destroys_during_exception;
  ++counts.destroyed;
  delete reinterpret_cast<TestInstance*>(instance);
}

template <std::size_t N>
flexui_plugin_bytes_view literal_view(const char (&text)[N]) {
  return {reinterpret_cast<const std::uint8_t*>(text), N - 1u};
}

constexpr std::uint64_t kPayloadLimit = 1;
const flexui_plugin_operation_descriptor_v2 operation = {
    sizeof(operation), 0, literal_view("echo"), kPayloadLimit, {0}};
const flexui_plugin_service_descriptor_v2 service = {
    sizeof(service), 1, literal_view("test.create-unwind/1"), &operation, {0}};
const flexui_plugin_descriptor_v2 descriptor = {
    sizeof(descriptor), FLEXUI_PLUGIN_ABI_MINOR,
    literal_view("test.create-unwind"),
    literal_view("1.0.0"), 1, 0, &service, {0}};
const flexui_plugin_api_v2 plugin_api = {
    sizeof(plugin_api), FLEXUI_PLUGIN_ABI_MAJOR, FLEXUI_PLUGIN_ABI_MINOR, 0,
    &descriptor, create_instance, start_instance, submit_request, start_instance,
    join_instance, destroy_instance, join_scope_instance, {0}};

void reset(CreateUnwindMode requested_mode) {
  mode = requested_mode;
  counts = {};
}

CreateUnwindStatistics statistics() { return counts; }
const CreateUnwindFixtureApi fixture_api = {reset, statistics};

} // namespace

extern "C" FLEXUI_PLUGIN_EXPORT const flexui_plugin_api_v2* FLEXUI_PLUGIN_CALL
flexui_plugin_get_api_v2() { return &plugin_api; }

extern "C" FLEXUI_PLUGIN_EXPORT const CreateUnwindFixtureApi* FLEXUI_PLUGIN_CALL
flexui_test_create_unwind_query() { return &fixture_api; }
