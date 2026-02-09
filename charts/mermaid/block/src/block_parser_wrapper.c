#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "block/block_ast.h"
#include "block_parser_gen.h"
#include "turbo_parser.h"

void *BlockParserAlloc(void *(*mallocProc)(size_t));
void BlockParser(void *yyp, int yymajor, void* yyminor, BlockParserContext *ctx);
void BlockParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void block_scan(Scanner *s, void *parser, BlockParserContext *ctx);

static void free_stmt(BlockStatement* s);

static void free_stmts(BlockStatement* s) {
    while (s) {
        BlockStatement* next = s->next;
        free_stmt(s);
        s = next;
    }
}

static void free_stmt(BlockStatement* s) {
    if (!s) return;
    switch(s->type) {
        case BLOCK_STMT_NODE:
            free(s->data.node.id);
            free(s->data.node.label);
            free(s->data.node.shape);
            if (s->data.node.children) free_stmts(s->data.node.children);
            // Directions
            break;
        case BLOCK_STMT_EDGE:
            free(s->data.edge.id1);
            free(s->data.edge.id2);
            free(s->data.edge.label);
            free(s->data.edge.edgeType);
            break;
        case BLOCK_STMT_COLUMNS:
            // int
            break;
        case BLOCK_STMT_SPACE:
            // int
            break;
        case BLOCK_STMT_CLASSDEF:
            free(s->data.classDef.id);
            free(s->data.classDef.styles);
            break;
        case BLOCK_STMT_APPLYCLASS:
            free(s->data.applyClass.id);
            free(s->data.applyClass.className);
            break;
        case BLOCK_STMT_STYLE:
            free(s->data.style.id);
            free(s->data.style.styles);
            break;
    }
    free(s);
}

void block_free(BlockDiagram* diagram) {
    if (!diagram) return;
    free(diagram->hierarchy);
    free_stmts(diagram->statements);
    free(diagram);
}

static json_value_t* serialize_statements(BlockStatement* s) {
    json_value_t* arr = turbo_json_create_array();
    while (s) {
        json_value_t* obj = turbo_json_create_object();
        switch (s->type) {
            case BLOCK_STMT_NODE:
                turbo_json_object_set_string(obj, "type", "node");
                turbo_json_object_set_string(obj, "id", s->data.node.id ? s->data.node.id : "");
                turbo_json_object_set_string(obj, "label", s->data.node.label ? s->data.node.label : "");
                turbo_json_object_set_string(obj, "shape", s->data.node.shape ? s->data.node.shape : "");
                turbo_json_object_set_number(obj, "width", s->data.node.width);
                if (s->data.node.children) {
                    turbo_json_object_add(obj, "children", serialize_statements(s->data.node.children));
                }
                if (s->data.node.direction_count > 0) {
                    json_value_t* dir_arr = turbo_json_create_array();
                    for (int i = 0; i < s->data.node.direction_count; i++) {
                        turbo_json_array_add(dir_arr, turbo_json_create_string(s->data.node.directions[i]));
                    }
                    turbo_json_object_add(obj, "directions", dir_arr);
                }
                break;
            case BLOCK_STMT_EDGE:
                turbo_json_object_set_string(obj, "type", "edge");
                turbo_json_object_set_string(obj, "id1", s->data.edge.id1 ? s->data.edge.id1 : "");
                turbo_json_object_set_string(obj, "id2", s->data.edge.id2 ? s->data.edge.id2 : "");
                turbo_json_object_set_string(obj, "label", s->data.edge.label ? s->data.edge.label : "");
                turbo_json_object_set_string(obj, "edgeType", s->data.edge.edgeType ? s->data.edge.edgeType : "");
                break;
            case BLOCK_STMT_COLUMNS:
                turbo_json_object_set_string(obj, "type", "columns");
                turbo_json_object_set_number(obj, "count", s->data.columns.count);
                break;
            case BLOCK_STMT_SPACE:
                turbo_json_object_set_string(obj, "type", "space");
                turbo_json_object_set_number(obj, "width", s->data.space.width);
                break;
            case BLOCK_STMT_CLASSDEF:
                turbo_json_object_set_string(obj, "type", "classDef");
                turbo_json_object_set_string(obj, "id", s->data.classDef.id ? s->data.classDef.id : "");
                turbo_json_object_set_string(obj, "styles", s->data.classDef.styles ? s->data.classDef.styles : "");
                break;
            case BLOCK_STMT_APPLYCLASS:
                turbo_json_object_set_string(obj, "type", "applyClass");
                turbo_json_object_set_string(obj, "id", s->data.applyClass.id ? s->data.applyClass.id : "");
                turbo_json_object_set_string(obj, "className", s->data.applyClass.className ? s->data.applyClass.className : "");
                break;
            case BLOCK_STMT_STYLE:
                turbo_json_object_set_string(obj, "type", "style");
                turbo_json_object_set_string(obj, "id", s->data.style.id ? s->data.style.id : "");
                turbo_json_object_set_string(obj, "styles", s->data.style.styles ? s->data.style.styles : "");
                break;
        }
        turbo_json_array_add(arr, obj);
        s = s->next;
    }
    return arr;
}

char* block_to_json(BlockDiagram* diagram) {
    if (!diagram) return NULL;
    json_value_t* root = turbo_json_create_object();
    turbo_json_object_set_string(root, "hierarchy", diagram->hierarchy ? diagram->hierarchy : "block");
    turbo_json_object_add(root, "statements", serialize_statements(diagram->statements));

    size_t len;
    char* str = turbo_json_serialize_pretty(root, &len);
    turbo_free_json(&root);
    return str;
}

BlockDiagram* block_parse(const char* input) {
    BlockParserContext ctx;
    ctx.diagram = (BlockDiagram*)malloc(sizeof(BlockDiagram));
    memset(ctx.diagram, 0, sizeof(BlockDiagram));
    ctx.current_container = NULL;
    ctx.container_stack = NULL;
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = BlockParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    block_scan(&s, parser, &ctx);

    BlockParser(parser, BLOCK_NL, NULL, &ctx);
    BlockParser(parser, 0, NULL, &ctx);
    BlockParserFree(parser, free);

    if (ctx.error_count > 0) {
        block_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}
