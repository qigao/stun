#include <tinytest.hpp>

#include <flexUI/plugin_host.h>

#include "native_plugin_library.hpp"
#include "plugin_create_unwind_fixture.h"

#include <filesystem>
#include <string>

#ifndef FLEXUI_TEST_CREATE_UNWIND_PLUGIN
#error FLEXUI_TEST_CREATE_UNWIND_PLUGIN must name the create-unwind test library
#endif

namespace {

flexUI::PluginHostError load_failure(const CreateUnwindFixtureApi& fixture,
                                    CreateUnwindMode mode) {
  fixture.reset(mode);
  flexUI::PluginHostError error;
  {
    flexUI::PluginHostBuilder builder;
    const auto result = builder.load_plugin(FLEXUI_TEST_CREATE_UNWIND_PLUGIN);
    check_false(static_cast<bool>(result));
    error = result.error;
    check_true(fixture.statistics().destroyed == std::uint32_t{1});
  }
  check_true(fixture.statistics().created == std::uint32_t{1});
  check_true(fixture.statistics().destroyed == std::uint32_t{1});
  check_true(fixture.statistics().lifecycle_calls == std::uint32_t{0});
  check_true(fixture.statistics().live_exceptions == std::uint32_t{0});
  check_true(fixture.statistics().destroys_during_exception == std::uint32_t{0});
  return error;
}

} // namespace

// TinyTest's variadic check_equal sentinel does not expand under the pinned
// current MSVC preprocessor for this fixture. Keep EXACT equality assertions
// using its already-required check_true primitive on every platform.
suite("FlexUI PluginHost create ownership") {
  // An explicit test-owned library keeps the observation API alive after the
  // host releases its own library. API pointers alone never retain a module.
  static flexUI::plugin_host_detail::NativePluginLibrary library;
  static const CreateUnwindFixtureApi* fixture;

  before_each() {
    std::string message;
    check_true(library.open(std::filesystem::path(FLEXUI_TEST_CREATE_UNWIND_PLUGIN),
                            message));
    const auto query = reinterpret_cast<CreateUnwindQuery>(
        library.symbol(kCreateUnwindQuerySymbol, message));
    check_not_null(query);
    fixture = query();
    check_not_null(fixture);
  }

  after_each() {
    fixture = nullptr;
    library.close();
  }

  it("releases an owned instance returned with create rejection") {
    const auto error = load_failure(*fixture, CreateUnwindMode::Reject);
    check_true(error.code == flexUI::PluginHostErrorCode::CreateFailed);
  }

  it("releases an owned instance when create throws bad_alloc") {
    const auto error = load_failure(*fixture, CreateUnwindMode::BadAllocation);
    check_true(error.code == flexUI::PluginHostErrorCode::AllocationFailed);
  }

  it("releases an owned instance when create throws a standard exception") {
    const auto error = load_failure(*fixture, CreateUnwindMode::StandardException);
    check_true(error.code == flexUI::PluginHostErrorCode::InternalInvariant);
    check_true(error.message ==
               std::string("test create threw after returning ownership"));
  }

  it("releases an owned instance when create throws a nonstandard exception") {
    const auto error = load_failure(*fixture, CreateUnwindMode::OtherException);
    check_true(error.code == flexUI::PluginHostErrorCode::InternalInvariant);
  }
};
