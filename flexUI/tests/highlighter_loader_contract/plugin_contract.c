#include <flexUI/ts_plugin.h>
#include <stdlib.h>
static TSHighlightResult* highlight(const char* data, uint32_t length) {
    TSHighlightResult* result = (TSHighlightResult*)calloc(1u, sizeof(*result));
    if (!result) return NULL;
    if (!data || !length) return result;
    result->tokens = (TSHighlightToken*)malloc(sizeof(TSHighlightToken));
    if (!result->tokens) return result;
    result->capacity = 1u;
    result->count = 1u;
    result->tokens[0].type = TS_HIGHLIGHT_KEYWORD;
    result->tokens[0].start = 0u;
    result->tokens[0].length = 1u;
    return result;
}
static void free_result(TSHighlightResult* result) {
    if (result) { free(result->tokens); free(result); }
}
static const TSPluginInfo plugin = {
    "contract", "1.0.0", highlight, free_result
};
#if defined(_WIN32)
#define TS_CONTRACT_EXPORT __declspec(dllexport)
#else
#define TS_CONTRACT_EXPORT __attribute__((visibility("default")))
#endif
TS_CONTRACT_EXPORT const TSPluginInfo* ts_plugin_info(void) { return &plugin; }
