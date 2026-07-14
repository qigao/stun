#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kanban/kanban_ast.h"
#include "kanban_parser_gen.h"
#include "turbo_parser.h"

void *KanbanParserAlloc(void *(*mallocProc)(size_t));
void KanbanParser(void *yyp, int yymajor, void* yyminor, KanbanParserContext *ctx);
void KanbanParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
    int at_bol;
} Scanner;

void kanban_scan(Scanner *s, void *parser, KanbanParserContext *ctx);

KanbanDiagram* kanban_parse(const char* input) {
    if(!input) return NULL;

    KanbanParserContext ctx;
    ctx.diagram = kanban_create_diagram();
    if (!ctx.diagram) return NULL;
    ctx.error_count = 0;
    ctx.error_message = NULL;
    // Initialize tree state
    memset(ctx.last_node_at_level, 0, sizeof(ctx.last_node_at_level));
    ctx.last_added_node = NULL;

    void* parser = KanbanParserAlloc(malloc);
    if (!parser) {
        kanban_free_diagram(ctx.diagram);
        return NULL;
    }
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input) + 1;
    s.marker = NULL;
    s.line = 1;
    s.at_bol = 1;

    kanban_scan(&s, parser, &ctx);
    
    // Ensure the stream ends with a newline if it doesn't already
    if (!s.at_bol) {
        KanbanParser(parser, KANBAN_NL, NULL, &ctx);
    }
    
    KanbanParser(parser, 0, NULL, &ctx);
    KanbanParserFree(parser, free);

    if (ctx.error_count > 0) {
        if(ctx.error_message) {
             printf("Kanban Parser Error: %s\n", ctx.error_message);
             free(ctx.error_message);
        }
        kanban_free_diagram(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

static json_value_t* kanban_node_to_json_recursive(KanbanNode* node) {
    if(!node) return NULL;
    
    json_value_t* obj = turbo_json_create_object();
    
    // Add children first (alphabetically first)
    json_value_t* children_arr = turbo_json_create_array();
    KanbanNode* child = node->children;
    while(child) {
        json_value_t* child_obj = kanban_node_to_json_recursive(child);
        if(child_obj) {
            turbo_json_array_add(children_arr, child_obj);
        }
        child = child->next;
    }
    turbo_json_object_add(obj, "children", children_arr);
    
    // Then add other keys in alphabetical order: children, class, descr, icon, id, indent, type
    if(node->classes && node->classes[0]) {
        turbo_json_object_set_string(obj, "class", node->classes);
    }
    turbo_json_object_set_string(obj, "descr", node->label ? node->label : "");
    if(node->icon && node->icon[0]) {
        turbo_json_object_set_string(obj, "icon", node->icon);
    }
    turbo_json_object_set_string(obj, "id", node->id ? node->id : "");
    turbo_json_object_set_number(obj, "indent", (double)node->depth);
    turbo_json_object_set_number(obj, "type", (double)node->type);
    
    return obj;
}

char* kanban_to_json(KanbanDiagram* d) {
    if(!d) {
        json_value_t *null_val = turbo_json_create_null();
        size_t len;
        char *s = turbo_json_serialize_pretty_crlf(null_val, &len);
        turbo_free_json(&null_val);
        return s;
    }
    
    json_value_t *root = turbo_json_create_object();
    
    // Add keys in alphabetical order: nodes, type
    json_value_t* nodes_arr = turbo_json_create_array();
    
    // Add all top-level nodes (children of root)
    if(d->root && d->root->children) {
        KanbanNode* node = d->root->children;
        while(node) {
            json_value_t* node_obj = kanban_node_to_json_recursive(node);
            if(node_obj) {
                turbo_json_array_add(nodes_arr, node_obj);
            }
            node = node->next;
        }
    }
    
    turbo_json_object_add(root, "nodes", nodes_arr);
    turbo_json_object_set_string(root, "type", "kanban");
    
    size_t len;
    char *s = turbo_json_serialize_pretty_crlf(root, &len);
    turbo_free_json(&root);
    return s;
}
