#include <flexUI/plugin_abi.h>

#include <stddef.h>
#include <stdio.h>

_Static_assert(FLEXUI_PLUGIN_ABI_MAJOR == 1, "the consumer uses ABI v1");
_Static_assert(sizeof(flexui_plugin_status) == sizeof(uint32_t),
               "plugin status is a 32-bit ABI value");
_Static_assert(offsetof(flexui_host_api_v1, struct_size) == 0,
               "host API begins with its struct size");
_Static_assert(offsetof(flexui_plugin_api_v1, struct_size) == 0,
               "plugin API begins with its struct size");

int main(void) {
  puts("PluginSDK installed C header consumer passed");
  return FLEXUI_PLUGIN_STATUS_OK;
}
