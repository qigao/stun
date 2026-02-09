#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xychart/xychart_ast.h"

static char* copy_string(const char* s) {
    if(!s) return NULL;
    return strdup(s);
}

static void free_string_list(XYStringList* list) {
    if(!list) return;
    for(int i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

XYDiagram* xychart_create_diagram() {
    XYDiagram* d = (XYDiagram*)malloc(sizeof(XYDiagram));
    if(d) {
        memset(d, 0, sizeof(XYDiagram));
        d->orientation = XY_ORIENTATION_VERTICAL;
    }
    return d;
}

void xychart_free_diagram(XYDiagram* d) {
    if(!d) return;
    free(d->title);
    free(d->accTitle);
    free(d->accDescr);
    free(d->xAxis.title);
    free_string_list(&d->xAxis.categories);
    free(d->yAxis.title);
    
    XYSeries* s = d->series;
    while(s) {
        XYSeries* next = s->next;
        free(s->name);
        free(s->data.items);
        free(s);
        s = next;
    }
    free(d);
}

void xychart_set_orientation(XYParserContext* ctx, XYOrientation orientation) {
    if(ctx && ctx->diagram) ctx->diagram->orientation = orientation;
}

void xychart_set_title(XYParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->title) free(ctx->diagram->title);
        ctx->diagram->title = copy_string(title);
    }
}

void xychart_set_acc_title(XYParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->accTitle) free(ctx->diagram->accTitle);
        ctx->diagram->accTitle = copy_string(title);
    }
}

void xychart_set_acc_descr(XYParserContext* ctx, const char* descr) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->accDescr) free(ctx->diagram->accDescr);
        ctx->diagram->accDescr = copy_string(descr);
    }
}

void xychart_set_x_axis_title(XYParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->xAxis.title) free(ctx->diagram->xAxis.title);
        ctx->diagram->xAxis.title = copy_string(title);
    }
}

void xychart_set_x_axis_range(XYParserContext* ctx, double min, double max) {
    if(ctx && ctx->diagram) {
        ctx->diagram->xAxis.has_range = true;
        ctx->diagram->xAxis.range_min = min;
        ctx->diagram->xAxis.range_max = max;
    }
}

void xychart_set_x_axis_categories(XYParserContext* ctx, XYStringList categories) {
    if(ctx && ctx->diagram) {
        free_string_list(&ctx->diagram->xAxis.categories);
        ctx->diagram->xAxis.categories = categories;
    }
}

void xychart_set_y_axis_title(XYParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->yAxis.title) free(ctx->diagram->yAxis.title);
        ctx->diagram->yAxis.title = copy_string(title);
    }
}

void xychart_set_y_axis_range(XYParserContext* ctx, double min, double max) {
    if(ctx && ctx->diagram) {
        ctx->diagram->yAxis.has_range = true;
        ctx->diagram->yAxis.range_min = min;
        ctx->diagram->yAxis.range_max = max;
    }
}

void xychart_add_series(XYParserContext* ctx, XYSeriesType type, const char* name, XYNumberList data) {
    if(!ctx || !ctx->diagram) return;
    
    XYSeries* s = (XYSeries*)malloc(sizeof(XYSeries));
    if(s) {
        s->type = type;
        s->name = copy_string(name);
        s->data = data;
        s->next = NULL;
        
        if(!ctx->diagram->series) {
            ctx->diagram->series = s;
        } else {
            XYSeries* last = ctx->diagram->series;
            while(last->next) last = last->next;
            last->next = s;
        }
    }
}
