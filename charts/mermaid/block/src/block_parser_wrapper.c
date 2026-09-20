#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "block/block_ast.h"
#include "block_parser_gen.h"
#include "json_parser.h"

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
    json_value_t* arr = json_create_array();
    while (s) {
        json_value_t* obj = json_create_object();
        switch (s->type) {
            case BLOCK_STMT_NODE:
                json_object_set_string(obj, "type", "node");
                json_object_set_string(obj, "id", s->data.node.id ? s->data.node.id : "");
                json_object_set_string(obj, "label", s->data.node.label ? s->data.node.label : "");
                json_object_set_string(obj, "shape", s->data.node.shape ? s->data.node.shape : "");
                json_object_set_number(obj, "width", s->data.node.width);
                if (s->data.node.children) {
                    json_object_add(obj, "children", serialize_statements(s->data.node.children));
                }
                if (s->data.node.direction_count > 0) {
                    json_value_t* dir_arr = json_create_array();
                    for (int i = 0; i < s->data.node.direction_count; i++) {
                        json_array_add(dir_arr, json_create_string(s->data.node.directions[i]));
                    }
                    json_object_add(obj, "directions", dir_arr);
                }
                break;
            case BLOCK_STMT_EDGE:
                json_object_set_string(obj, "type", "edge");
                json_object_set_string(obj, "id1", s->data.edge.id1 ? s->data.edge.id1 : "");
                json_object_set_string(obj, "id2", s->data.edge.id2 ? s->data.edge.id2 : "");
                json_object_set_string(obj, "label", s->data.edge.label ? s->data.edge.label : "");
                json_object_set_string(obj, "edgeType", s->data.edge.edgeType ? s->data.edge.edgeType : "");
                break;
            case BLOCK_STMT_COLUMNS:
                json_object_set_string(obj, "type", "columns");
                json_object_set_number(obj, "count", s->data.columns.count);
                break;
            case BLOCK_STMT_SPACE:
                json_object_set_string(obj, "type", "space");
                json_object_set_number(obj, "width", s->data.space.width);
                break;
            case BLOCK_STMT_CLASSDEF:
                json_object_set_string(obj, "type", "classDef");
                json_object_set_string(obj, "id", s->data.classDef.id ? s->data.classDef.id : "");
                json_object_set_string(obj, "styles", s->data.classDef.styles ? s->data.classDef.styles : "");
                break;
            case BLOCK_STMT_APPLYCLASS:
                json_object_set_string(obj, "type", "applyClass");
                json_object_set_string(obj, "id", s->data.applyClass.id ? s->data.applyClass.id : "");
                json_object_set_string(obj, "className", s->data.applyClass.className ? s->data.applyClass.className : "");
                break;
            case BLOCK_STMT_STYLE:
                json_object_set_string(obj, "type", "style");
                json_object_set_string(obj, "id", s->data.style.id ? s->data.style.id : "");
                json_object_set_string(obj, "styles", s->data.style.styles ? s->data.style.styles : "");
                break;
        }
        json_array_add(arr, obj);
        s = s->next;
    }
    return arr;
}

char* block_to_json(BlockDiagram* diagram) {
    if (!diagram) return NULL;
    json_value_t* root = json_create_object();
    json_object_set_string(root, "hierarchy", diagram->hierarchy ? diagram->hierarchy : "block");
    json_object_add(root, "statements", serialize_statements(diagram->statements));

    size_t len;
    char* str = json_serialize_pretty(root, &len);
    json_free(&root);
    return str;
}

BlockDiagram* block_parse(const char* input) {
    if (!input) return NULL;
    BlockParserContext ctx;
    ctx.diagram = (BlockDiagram*)malloc(sizeof(BlockDiagram));
    if (!ctx.diagram) return NULL;
    memset(ctx.diagram, 0, sizeof(BlockDiagram));
    ctx.current_container = NULL;
    ctx.container_stack = NULL;
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = BlockParserAlloc(malloc);
    if (!parser) {
        block_free(ctx.diagram);
        return NULL;
    }

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
