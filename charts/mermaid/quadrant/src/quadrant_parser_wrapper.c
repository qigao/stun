#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "quadrant/quadrant_ast.h"
#include "quadrant_parser_gen.h"
#include "turbo_parser.h"

void *QuadrantParserAlloc(void *(*mallocProc)(size_t));
void QuadrantParser(void *yyp, int yymajor, void* yyminor, QuadrantParserContext *ctx);
void QuadrantParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void quadrant_scan(Scanner *s, void *parser, QuadrantParserContext *ctx);

QuadrantDiagram* quadrant_parse(const char* input) {
    if(!input) return NULL;

    QuadrantParserContext ctx;
    ctx.diagram = quadrant_create_diagram();
    if (!ctx.diagram) return NULL;
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = QuadrantParserAlloc(malloc);
    if (!parser) {
        quadrant_free_diagram(ctx.diagram);
        return NULL;
    }
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    quadrant_scan(&s, parser, &ctx);
    
    // Ensure we send a newline to flush any pending statements
    QuadrantParser(parser, QUADRANT_NL, NULL, &ctx);
    
    QuadrantParser(parser, 0, NULL, &ctx);
    QuadrantParserFree(parser, free);

    if (ctx.error_count > 0) {
        if(ctx.error_message) {
             printf("Quadrant Parser Error: %s\n", ctx.error_message);
             free(ctx.error_message);
        }
        quadrant_free_diagram(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

// --- JSON Serialization ---

static json_value_t* serialize_points(QuadrantPoint* p) {
    json_value_t* arr = turbo_json_create_array();
    while (p) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "text", p->text ? p->text : "");
        if (p->className) {
            turbo_json_object_set_string(obj, "className", p->className);
        }
        turbo_json_object_set_number(obj, "x", p->x);
        turbo_json_object_set_number(obj, "y", p->y);
        turbo_json_array_add(arr, obj);
        p = p->next;
    }
    return arr;
}

char* quadrant_to_json(QuadrantDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = turbo_json_create_object();
    turbo_json_object_set_string(root, "type", "quadrantChart");

    if (diagram->title) {
        turbo_json_object_set_string(root, "title", diagram->title);
    }
    if (diagram->accTitle) {
        turbo_json_object_set_string(root, "accTitle", diagram->accTitle);
    }
    if (diagram->accDescr) {
        turbo_json_object_set_string(root, "accDescr", diagram->accDescr);
    }

    // X-axis
    if (diagram->xAxisLeft || diagram->xAxisRight) {
        json_value_t* xAxis = turbo_json_create_object();
        if (diagram->xAxisLeft) turbo_json_object_set_string(xAxis, "left", diagram->xAxisLeft);
        if (diagram->xAxisRight) turbo_json_object_set_string(xAxis, "right", diagram->xAxisRight);
        turbo_json_object_add(root, "xAxis", xAxis);
    }

    // Y-axis
    if (diagram->yAxisBottom || diagram->yAxisTop) {
        json_value_t* yAxis = turbo_json_create_object();
        if (diagram->yAxisBottom) turbo_json_object_set_string(yAxis, "bottom", diagram->yAxisBottom);
        if (diagram->yAxisTop) turbo_json_object_set_string(yAxis, "top", diagram->yAxisTop);
        turbo_json_object_add(root, "yAxis", yAxis);
    }

    // Quadrant labels
    json_value_t* quadrants = turbo_json_create_object();
    if (diagram->quadrant1Text) turbo_json_object_set_string(quadrants, "q1", diagram->quadrant1Text);
    if (diagram->quadrant2Text) turbo_json_object_set_string(quadrants, "q2", diagram->quadrant2Text);
    if (diagram->quadrant3Text) turbo_json_object_set_string(quadrants, "q3", diagram->quadrant3Text);
    if (diagram->quadrant4Text) turbo_json_object_set_string(quadrants, "q4", diagram->quadrant4Text);
    if (diagram->quadrant1Text || diagram->quadrant2Text || diagram->quadrant3Text || diagram->quadrant4Text) {
        turbo_json_object_add(root, "quadrants", quadrants);
    } else {
        turbo_free_json(&quadrants);
    }

    // Points
    turbo_json_object_add(root, "points", serialize_points(diagram->points));

    size_t len;
    char* str = turbo_json_serialize_pretty(root, &len);
    turbo_free_json(&root);
    return str;
}
