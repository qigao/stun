/*
 * Tree-sitter JSON Language Plugin
 */

#include <tree_sitter/api.h>
#include "../ts_plugin.h"
#include <stdlib.h>
#include <string.h>

extern const TSLanguage* tree_sitter_json(void);

static TSParser* parser = NULL;

static void ensure_parser(void) {
    if (!parser) {
        parser = ts_parser_new();
        ts_parser_set_language(parser, tree_sitter_json());
    }
}

static void add_token(TSHighlightResult* result, TSHighlightType type, uint32_t start, uint32_t length) {
    if (result->count >= result->capacity) {
        result->capacity = result->capacity ? result->capacity * 2 : 64;
        result->tokens = realloc(result->tokens, result->capacity * sizeof(TSHighlightToken));
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

static void traverse(TSNode node, TSHighlightResult* result, int is_key) {
    const char* type = ts_node_type(node);
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    uint32_t child_count = ts_node_child_count(node);

    // JSON key (first string in a pair)
    if (is_key && strcmp(type, "string") == 0) {
        add_token(result, TS_HIGHLIGHT_VARIABLE, start, end - start);
        return;
    }

    TSHighlightType hl = node_type_to_highlight(type);
    if (hl != TS_HIGHLIGHT_PLAIN) {
        add_token(result, hl, start, end - start);
        return;
    }

    // Handle pair: first child is key, rest are values
    if (strcmp(type, "pair") == 0 && child_count >= 2) {
        traverse(ts_node_child(node, 0), result, 1);  // key
        for (uint32_t i = 1; i < child_count; i++) {
            traverse(ts_node_child(node, i), result, 0);  // values
        }
        return;
    }

    for (uint32_t i = 0; i < child_count; i++) {
        traverse(ts_node_child(node, i), result, 0);
    }
}

static TSHighlightResult* json_highlight(const char* source, uint32_t length) {
    ensure_parser();
    
    TSHighlightResult* result = calloc(1, sizeof(TSHighlightResult));
    if (!source || length == 0) return result;
    
    TSTree* tree = ts_parser_parse_string(parser, NULL, source, length);
    if (tree) {
        traverse(ts_tree_root_node(tree), result, 0);
        ts_tree_delete(tree);
    }
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
