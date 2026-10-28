#define FLEXUI_PLUGIN_BUILD
#include <flexUI/plugin_abi.h>

static const flexui_plugin_api_v2 bad_api = {
    sizeof(flexui_plugin_api_v2),
#if defined(FLEXUI_TEST_OLD_MINOR)
    FLEXUI_PLUGIN_ABI_MAJOR,
    0u, /* v2.0 deliberately rejected: no join_scope proof. */
#else
    99u,
    FLEXUI_PLUGIN_ABI_MINOR,
#endif
    0u,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    {0}};

FLEXUI_PLUGIN_EXPORT const flexui_plugin_api_v2 *FLEXUI_PLUGIN_CALL flexui_plugin_get_api_v2(void) {
  return &bad_api;
}
