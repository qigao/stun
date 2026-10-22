/*
 * Tree-sitter JSON Language Plugin
 */

#include <tree_sitter/api.h>
#include "../ts_plugin.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

extern const TSLanguage* tree_sitter_json(void);

// Presentation-only Tree-sitter providers have fixed admission limits.
// A discarded highlight leaves Markdown text unchanged; these are NOT a
// secondary Markdown grammar.
enum { TS_RENDER_MAX_BYTES = 64u * 1024u,
       TS_RENDER_MAX_TOKENS = 1024u,
       TS_RENDER_MAX_DEPTH = 128u };
typedef struct {
    TSHighlightResult* result;
    bool stopped;
} TSRenderWriter;

static void add_token(TSRenderWriter* writer, TSHighlightType type,
                      uint32_t start, uint32_t length) {
    if (writer->stopped || !length) return;
    TSHighlightResult* result = writer->result;
    if (result->count >= TS_RENDER_MAX_TOKENS) {
        writer->stopped = true;
        return;
    }
    if (result->count == result->capacity) {
        const uint32_t next = result->capacity
            ? (result->capacity < TS_RENDER_MAX_TOKENS / 2u
                 ? result->capacity * 2u : TS_RENDER_MAX_TOKENS)
            : 64u;
        TSHighlightToken* grown = (TSHighlightToken*)realloc(
            result->tokens, (size_t)next * sizeof(TSHighlightToken));
        if (!grown) {
            writer->stopped = true;
            return;
        }
        result->tokens = grown;
        result->capacity = next;
    }
    result->tokens[result->count++] = (TSHighlightToken){type, start, length};
}

static TSHighlightType node_type_to_highlight(const char* type) {
    if (strcmp(type, "string") == 0) return TS_HIGHLIGHT_STRING;
    if (strcmp(type, "string_content") == 0) return TS_HIGHLIGHT_STRING;
    if (strcmp(type, "number") == 0) return TS_HIGHLIGHT_NUMBER;
    if (strcmp(type, "true") == 0) return TS_HIGHLIGHT_CONSTANT;
    if (strcmp(type, "false") == 0) return TS_HIGHLIGHT_CONSTANT;
    if (strcmp(type, "null") == 0) return TS_HIGHLIGHT_CONSTANT;
    if (strcmp(type, "pair") == 0) return TS_HIGHLIGHT_PLAIN;  // traverse children
    return TS_HIGHLIGHT_PLAIN;
}

static void traverse(TSNode node, TSRenderWriter* writer,
                     int is_key, uint32_t depth) {
    if (writer->stopped || depth >= TS_RENDER_MAX_DEPTH ||
        writer->result->count >= TS_RENDER_MAX_TOKENS) return;
    const char* type = ts_node_type(node);
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    uint32_t child_count = ts_node_child_count(node);

    // JSON key (first string in a pair)
    if (is_key && strcmp(type, "string") == 0) {
        add_token(writer, TS_HIGHLIGHT_VARIABLE, start, end - start);
        return;
    }

    TSHighlightType hl = node_type_to_highlight(type);
    if (hl != TS_HIGHLIGHT_PLAIN) {
        add_token(writer, hl, start, end - start);
        return;
    }

    // Handle pair: first child is key, rest are values
    if (strcmp(type, "pair") == 0 && child_count >= 2) {
        traverse(ts_node_child(node, 0), writer, 1, depth + 1u);  // key
        for (uint32_t i = 1; i < child_count; i++) {
            traverse(ts_node_child(node, i), writer, 0, depth + 1u);  // values
        }
        return;
    }

    for (uint32_t i = 0; i < child_count; i++) {
        traverse(ts_node_child(node, i), writer, 0, depth + 1u);
    }
}

static TSHighlightResult* json_highlight(const char* source, uint32_t length) {
    TSHighlightResult* result = (TSHighlightResult*)calloc(
        1u, sizeof(TSHighlightResult));
    if (!result || !source || !length || length > TS_RENDER_MAX_BYTES)
        return result;
    TSParser* parser = ts_parser_new();
    if (!parser) return result;
    if (ts_parser_set_language(parser, tree_sitter_json())) {
        TSTree* tree = ts_parser_parse_string(parser, NULL, source, length);
        if (tree) {
            TSRenderWriter writer = {result, false};
            traverse(ts_tree_root_node(tree), &writer, 0, 0u);
            ts_tree_delete(tree);
        }
    }
    ts_parser_delete(parser);
    return result;
}

static void json_free_result(TSHighlightResult* result) {
    if (result) {
        free(result->tokens);
        free(result);
    }
}

static TSPluginInfo plugin_info = {
    "json",
    "1.0.0",
    json_highlight,
    json_free_result
};

TS_PLUGIN_EXPORT const TSPluginInfo* ts_plugin_info(void) {
    return &plugin_info;
}
