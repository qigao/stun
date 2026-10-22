/*
 * Tree-sitter C Language Plugin
 */

#include <tree_sitter/api.h>
#include "../ts_plugin.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

extern const TSLanguage* tree_sitter_c(void);

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
    if (strcmp(type, "system_lib_string") == 0) return TS_HIGHLIGHT_STRING;
    if (strcmp(type, "string_literal") == 0) return TS_HIGHLIGHT_STRING;
    if (strcmp(type, "char_literal") == 0) return TS_HIGHLIGHT_STRING;
    
    if (strcmp(type, "primitive_type") == 0) return TS_HIGHLIGHT_TYPE;
    if (strcmp(type, "type_identifier") == 0) return TS_HIGHLIGHT_TYPE;
    if (strcmp(type, "sized_type_specifier") == 0) return TS_HIGHLIGHT_TYPE;
    
    if (strcmp(type, "number_literal") == 0) return TS_HIGHLIGHT_NUMBER;
    if (strcmp(type, "comment") == 0) return TS_HIGHLIGHT_COMMENT;
    
    if (strcmp(type, "null") == 0) return TS_HIGHLIGHT_CONSTANT;
    if (strcmp(type, "true") == 0) return TS_HIGHLIGHT_CONSTANT;
    if (strcmp(type, "false") == 0) return TS_HIGHLIGHT_CONSTANT;
    
    if (strcmp(type, "identifier") == 0) return TS_HIGHLIGHT_VARIABLE;
    if (strcmp(type, "field_identifier") == 0) return TS_HIGHLIGHT_VARIABLE;
    
    return TS_HIGHLIGHT_PLAIN;
}

static int is_keyword_parent(const char* type) {
    return strcmp(type, "if_statement") == 0 ||
           strcmp(type, "else_clause") == 0 ||
           strcmp(type, "for_statement") == 0 ||
           strcmp(type, "while_statement") == 0 ||
           strcmp(type, "do_statement") == 0 ||
           strcmp(type, "switch_statement") == 0 ||
           strcmp(type, "case_statement") == 0 ||
           strcmp(type, "return_statement") == 0 ||
           strcmp(type, "break_statement") == 0 ||
           strcmp(type, "continue_statement") == 0 ||
           strcmp(type, "goto_statement") == 0 ||
           strcmp(type, "struct_specifier") == 0 ||
           strcmp(type, "union_specifier") == 0 ||
           strcmp(type, "enum_specifier") == 0 ||
           strcmp(type, "sizeof_expression") == 0 ||
           strcmp(type, "storage_class_specifier") == 0 ||
           strcmp(type, "type_qualifier") == 0 ||
           strcmp(type, "parameter_declaration") == 0 || // for 'const' in params
           strcmp(type, "declaration") == 0;           // for 'typedef', 'static' etc
}

static void traverse(TSNode node, TSRenderWriter* writer, uint32_t depth) {
    if (writer->stopped || depth >= TS_RENDER_MAX_DEPTH ||
        writer->result->count >= TS_RENDER_MAX_TOKENS) return;
    const char* type = ts_node_type(node);
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    uint32_t child_count = ts_node_child_count(node);

    // Specific highlight types
    TSHighlightType hl = node_type_to_highlight(type);
    if (hl != TS_HIGHLIGHT_PLAIN) {
        add_token(writer, hl, start, end - start);
        return;
    }

    // Function calls and declarations
    if (strcmp(type, "call_expression") == 0) {
        TSNode function = ts_node_child_by_field_name(node, "function", 8);
        if (!ts_node_is_null(function)) {
            add_token(writer, TS_HIGHLIGHT_FUNCTION, ts_node_start_byte(function), 
                      ts_node_end_byte(function) - ts_node_start_byte(function));
            // skip function identifier traversal, but traverse arguments
            uint32_t arguments_index = 0;
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                if (ts_node_eq(child, function)) continue;
                traverse(child, writer, depth + 1u);
            }
            return;
        }
    }

    if (strcmp(type, "function_declarator") == 0) {
        TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
        if (!ts_node_is_null(declarator)) {
            add_token(writer, TS_HIGHLIGHT_FUNCTION, ts_node_start_byte(declarator), 
                      ts_node_end_byte(declarator) - ts_node_start_byte(declarator));
            // traverse rest
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                if (ts_node_eq(child, declarator)) continue;
                traverse(child, writer, depth + 1u);
            }
            return;
        }
    }

    // Preprocessor directives
    if (strncmp(type, "preproc", 7) == 0) {
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            if (!ts_node_is_named(child)) {
                add_token(writer, TS_HIGHLIGHT_PREPROCESSOR, ts_node_start_byte(child), 
                          ts_node_end_byte(child) - ts_node_start_byte(child));
            } else if (strcmp(child_type, "preproc_directive") == 0) {
                add_token(writer, TS_HIGHLIGHT_PREPROCESSOR, ts_node_start_byte(child), 
                          ts_node_end_byte(child) - ts_node_start_byte(child));
            } else {
                traverse(child, writer, depth + 1u);
            }
        }
        return;
    }

    // Keywords (unnamed nodes in keyword-parent nodes)
    if (is_keyword_parent(type)) {
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_named(child)) {
                const char* child_type = ts_node_type(child);
                // Basic check for keywords (alphabetic tokens)
                if (child_type[0] >= 'a' && child_type[0] <= 'z') {
                    add_token(writer, TS_HIGHLIGHT_KEYWORD, ts_node_start_byte(child), 
                              ts_node_end_byte(child) - ts_node_start_byte(child));
                    continue;
                }
            }
            traverse(child, writer, depth + 1u);
        }
        return;
    }

    // Default: traverse children
    for (uint32_t i = 0; i < child_count; i++) {
        traverse(ts_node_child(node, i), writer, depth + 1u);
    }
}

static TSHighlightResult* c_highlight(const char* source, uint32_t length) {
    TSHighlightResult* result = (TSHighlightResult*)calloc(
        1u, sizeof(TSHighlightResult));
    if (!result || !source || !length || length > TS_RENDER_MAX_BYTES)
        return result;

    // Per-call parser ownership avoids sharing mutable TSParser across UI
    // threads and ensures error-path cleanup.
    TSParser* parser = ts_parser_new();
    if (!parser) return result;
    if (ts_parser_set_language(parser, tree_sitter_c())) {
        TSTree* tree = ts_parser_parse_string(parser, NULL, source, length);
        if (tree) {
            TSRenderWriter writer = {result, false};
            traverse(ts_tree_root_node(tree), &writer, 0u);
            ts_tree_delete(tree);
        }
    }
    ts_parser_delete(parser);
    return result;
}

static void c_free_result(TSHighlightResult* result) {
    if (result) {
        free(result->tokens);
        free(result);
    }
}

static TSPluginInfo plugin_info = {
    "c",
    "1.0.0",
    c_highlight,
    c_free_result
};

TS_PLUGIN_EXPORT const TSPluginInfo* ts_plugin_info(void) {
    return &plugin_info;
}
