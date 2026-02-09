/*
 * flexUI - Tree-sitter Plugin Interface
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TS_HIGHLIGHT_PLAIN,
    TS_HIGHLIGHT_KEYWORD,
    TS_HIGHLIGHT_TYPE,
    TS_HIGHLIGHT_FUNCTION,
    TS_HIGHLIGHT_VARIABLE,
    TS_HIGHLIGHT_STRING,
    TS_HIGHLIGHT_NUMBER,
    TS_HIGHLIGHT_COMMENT,
    TS_HIGHLIGHT_PREPROCESSOR,
    TS_HIGHLIGHT_OPERATOR,
    TS_HIGHLIGHT_PUNCTUATION,
    TS_HIGHLIGHT_CONSTANT
} TSHighlightType;

typedef struct {
    TSHighlightType type;
    uint32_t start;
    uint32_t length;
} TSHighlightToken;

typedef struct {
    uint32_t count;
    TSHighlightToken* tokens;
} TSHighlightResult;

/*
 * Function signature for highlighting source code.
 * The plugin is responsible for parsing and returning a list of tokens.
 */
typedef TSHighlightResult* (*TSHighlightFunc)(const char* source, uint32_t len);

/*
 * Function signature for freeing the result.
 */
typedef void (*TSFreeResultFunc)(TSHighlightResult* result);

/*
 * Structure exported by dynamic libraries.
 */
typedef struct {
    const char* name;
    TSHighlightFunc highlight;
    TSFreeResultFunc free_result;
} TSPluginInfo;

/*
 * Symbol name expected: "ts_plugin_info"
 */
typedef const TSPluginInfo* (*TSPluginInfoFunc)();

#ifdef __cplusplus
}
#endif
