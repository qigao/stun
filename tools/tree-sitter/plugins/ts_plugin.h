/*
 * flexUI - Tree-sitter Language Plugin Interface (C ABI)
 * 
 * Each plugin exports: ts_plugin_info()
 * Plugin filename convention: ts-lang-{name}.dll/.so
 */

#ifndef FLEXUI_TS_PLUGIN_H
#define FLEXUI_TS_PLUGIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TS_HIGHLIGHT_PLAIN = 0,
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
    TS_HIGHLIGHT_CONSTANT,
} TSHighlightType;

typedef struct {
    TSHighlightType type;
    uint32_t start;
    uint32_t length;
} TSHighlightToken;

typedef struct {
    TSHighlightToken* tokens;
    uint32_t count;
    uint32_t capacity;
} TSHighlightResult;

typedef struct {
    const char* name;
    const char* version;
    TSHighlightResult* (*highlight)(const char* source, uint32_t length);
    void (*free_result)(TSHighlightResult* result);
} TSPluginInfo;

#ifdef _WIN32
#define TS_PLUGIN_EXPORT __declspec(dllexport)
#else
#define TS_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

TS_PLUGIN_EXPORT const TSPluginInfo* ts_plugin_info(void);

#ifdef __cplusplus
}
#endif

#endif
