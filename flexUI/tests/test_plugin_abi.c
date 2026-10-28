#include "tinytest.h"

#include <flexUI/plugin_abi.h>

#include <stddef.h>

_Static_assert(FLEXUI_PLUGIN_ABI_MAJOR == 2u, "ABI major must remain two");
_Static_assert(FLEXUI_PLUGIN_ABI_MINOR == 0u, "ABI minor must remain zero");
_Static_assert(offsetof(flexui_plugin_bytes_view, size) >
                   offsetof(flexui_plugin_bytes_view, data),
               "byte view layout must preserve field order");

suite("FlexUI plugin C ABI") {
  it("can initialize the versioned plugin table from C11") {
    flexui_plugin_api_v2 api = {0};
    api.struct_size = (uint32_t)sizeof(api);
    api.abi_major = FLEXUI_PLUGIN_ABI_MAJOR;
    api.abi_minor = FLEXUI_PLUGIN_ABI_MINOR;

    check_equal(api.struct_size, (uint32_t)sizeof(flexui_plugin_api_v2));
    check_equal(api.abi_major, (uint32_t)2u);
    check_equal(api.abi_minor, (uint32_t)0u);
    check(offsetof(flexui_host_api_v2, cancel_requested) >
               offsetof(flexui_host_api_v2, post_completion));
    check(sizeof(flexui_plugin_cancel_requested_v2_fn) ==
               sizeof(flexui_plugin_post_completion_v2_fn));
  }
}
