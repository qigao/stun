#pragma once

#include <flexUI/plugin_abi.h>

#include <cstdint>

enum class CreateUnwindMode { Reject, BadAllocation, StandardException, OtherException };

struct CreateUnwindStatistics {
  std::uint32_t created;
  std::uint32_t destroyed;
  std::uint32_t lifecycle_calls;
  std::uint32_t live_exceptions;
  std::uint32_t destroys_during_exception;
};

struct CreateUnwindFixtureApi {
  void (*reset)(CreateUnwindMode);
  CreateUnwindStatistics (*statistics)();
};

using CreateUnwindQuery = const CreateUnwindFixtureApi* (FLEXUI_PLUGIN_CALL*)();
inline constexpr const char* kCreateUnwindQuerySymbol = "flexui_test_create_unwind_query";
