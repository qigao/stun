#include "tinytest.h"

#include <flexUI/plugin_abi.h>

#include <stddef.h>

_Static_assert(FLEXUI_PLUGIN_ABI_MAJOR == 1u, "ABI major must remain one");
_Static_assert(FLEXUI_PLUGIN_ABI_MINOR == 0u, "ABI minor must remain zero");
_Static_assert(offsetof(flexui_plugin_bytes_view, size) >
                   offsetof(flexui_plugin_bytes_view, data),
               "byte view layout must preserve field order");

suite("FlexUI plugin C ABI") {
  it("can initialize the versioned plugin table from C11") {
    flexui_plugin_api_v1 api = {0};
    api.struct_size = (uint32_t)sizeof(api);
    api.abi_major = FLEXUI_PLUGIN_ABI_MAJOR;
    api.abi_minor = FLEXUI_PLUGIN_ABI_MINOR;

    check_equal(api.struct_size, (uint32_t)sizeof(flexui_plugin_api_v1));
    check_equal(api.abi_major, (uint32_t)1u);
    check_equal(api.abi_minor, (uint32_t)0u);
  }
}
