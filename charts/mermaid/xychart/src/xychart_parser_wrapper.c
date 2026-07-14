#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xychart/xychart_ast.h"
#include "xychart_parser_gen.h"

void *XYParserAlloc(void *(*mallocProc)(size_t));
void XYParser(void *yyp, int yymajor, void* yyminor, XYParserContext *ctx);
void XYParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void xychart_scan(Scanner *s, void *parser, XYParserContext *ctx);

XYDiagram* xychart_parse(const char* input) {
    if(!input) return NULL;

    XYParserContext ctx;
    ctx.diagram = xychart_create_diagram();
    if (!ctx.diagram) return NULL;
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = XYParserAlloc(malloc);
    if (!parser) {
        xychart_free_diagram(ctx.diagram);
        return NULL;
    }
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    xychart_scan(&s, parser, &ctx);
    
    XYParser(parser, XY_NL, NULL, &ctx);
    XYParser(parser, 0, NULL, &ctx);
    XYParserFree(parser, free);

    if (ctx.error_count > 0) {
        if(ctx.error_message) {
             printf("XYChart Parser Error: %s\n", ctx.error_message);
             free(ctx.error_message);
        }
        xychart_free_diagram(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

// --- JSON Serialization ---
#include "turbo_parser.h"

static json_value_t* serialize_axis(XYAxis* axis) {
    json_value_t* obj = turbo_json_create_object();
    if (axis->title) {
        turbo_json_object_set_string(obj, "title", axis->title);
    }
    turbo_json_object_set_bool(obj, "hasRange", axis->has_range);
    if (axis->has_range) {
        turbo_json_object_set_number(obj, "rangeMin", axis->range_min);
        turbo_json_object_set_number(obj, "rangeMax", axis->range_max);
    }
    if (axis->categories.count > 0) {
        json_value_t* arr = turbo_json_create_array();
        for (int i = 0; i < axis->categories.count; i++) {
            turbo_json_array_add(arr, turbo_json_create_string(axis->categories.items[i]));
        }
        turbo_json_object_add(obj, "categories", arr);
    }
    return obj;
}

static json_value_t* serialize_series(XYSeries* series) {
    json_value_t* arr = turbo_json_create_array();
    while (series) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "type", series->type == XY_SERIES_LINE ? "line" : "bar");
        if (series->name) {
            turbo_json_object_set_string(obj, "name", series->name);
        }
        json_value_t* data_arr = turbo_json_create_array();
        for (int i = 0; i < series->data.count; i++) {
            turbo_json_array_add(data_arr, turbo_json_create_number(series->data.items[i]));
        }
        turbo_json_object_add(obj, "data", data_arr);
        turbo_json_array_add(arr, obj);
        series = series->next;
    }
    return arr;
}

char* xychart_to_json(XYDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = turbo_json_create_object();
    turbo_json_object_set_string(root, "type", "xychart");

    if (diagram->title) turbo_json_object_set_string(root, "title", diagram->title);
    if (diagram->accTitle) turbo_json_object_set_string(root, "accTitle", diagram->accTitle);
    if (diagram->accDescr) turbo_json_object_set_string(root, "accDescr", diagram->accDescr);

    turbo_json_object_set_string(root, "orientation", 
        diagram->orientation == XY_ORIENTATION_VERTICAL ? "vertical" : "horizontal");

    turbo_json_object_add(root, "xAxis", serialize_axis(&diagram->xAxis));
    turbo_json_object_add(root, "yAxis", serialize_axis(&diagram->yAxis));

    if (diagram->series) {
        turbo_json_object_add(root, "series", serialize_series(diagram->series));
    }

    size_t len;
    return turbo_json_serialize_pretty_crlf(root, &len);
}
