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
#include <json_parser.h>

static json_value_t* serialize_axis(XYAxis* axis) {
    json_value_t* obj = json_create_object();
    if (axis->title) {
        json_object_set_string(obj, "title", axis->title);
    }
    json_object_set_bool(obj, "hasRange", axis->has_range);
    if (axis->has_range) {
        json_object_set_number(obj, "rangeMin", axis->range_min);
        json_object_set_number(obj, "rangeMax", axis->range_max);
    }
    if (axis->categories.count > 0) {
        json_value_t* arr = json_create_array();
        for (int i = 0; i < axis->categories.count; i++) {
            json_array_add(arr, json_create_string(axis->categories.items[i]));
        }
        json_object_add(obj, "categories", arr);
    }
    return obj;
}

static json_value_t* serialize_series(XYSeries* series) {
    json_value_t* arr = json_create_array();
    while (series) {
        json_value_t* obj = json_create_object();
        json_object_set_string(obj, "type", series->type == XY_SERIES_LINE ? "line" : "bar");
        if (series->name) {
            json_object_set_string(obj, "name", series->name);
        }
        json_value_t* data_arr = json_create_array();
        for (int i = 0; i < series->data.count; i++) {
            json_array_add(data_arr, json_create_number(series->data.items[i]));
        }
        json_object_add(obj, "data", data_arr);
        json_array_add(arr, obj);
        series = series->next;
    }
    return arr;
}

char* xychart_to_json(XYDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = json_create_object();
    json_object_set_string(root, "type", "xychart");

    if (diagram->title) json_object_set_string(root, "title", diagram->title);
    if (diagram->accTitle) json_object_set_string(root, "accTitle", diagram->accTitle);
    if (diagram->accDescr) json_object_set_string(root, "accDescr", diagram->accDescr);

    json_object_set_string(root, "orientation", 
        diagram->orientation == XY_ORIENTATION_VERTICAL ? "vertical" : "horizontal");

    json_object_add(root, "xAxis", serialize_axis(&diagram->xAxis));
    json_object_add(root, "yAxis", serialize_axis(&diagram->yAxis));

    if (diagram->series) {
        json_object_add(root, "series", serialize_series(diagram->series));
    }

    size_t len;
    return json_serialize_pretty_crlf(root, &len);
}
