#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "quadrant/quadrant_ast.h"

static char* copy_string(const char* s) {
    if(!s) return NULL;
    return strdup(s);
}

static void set_string(char** dest, const char* src) {
    if(*dest) free(*dest);
    *dest = copy_string(src);
}

QuadrantDiagram* quadrant_create_diagram() {
    QuadrantDiagram* d = (QuadrantDiagram*)malloc(sizeof(QuadrantDiagram));
    if(d) {
        memset(d, 0, sizeof(QuadrantDiagram));
    }
    return d;
}

void quadrant_free_diagram(QuadrantDiagram* d) {
    if(!d) return;
    free(d->title);
    free(d->accTitle);
    free(d->accDescr);
    free(d->xAxisLeft);
    free(d->xAxisRight);
    free(d->yAxisBottom);
    free(d->yAxisTop);
    free(d->quadrant1Text);
    free(d->quadrant2Text);
    free(d->quadrant3Text);
    free(d->quadrant4Text);
    
    QuadrantPoint* p = d->points;
    while(p) {
        QuadrantPoint* next = p->next;
        free(p->text);
        free(p->className);
        free(p);
        p = next;
    }
    free(d);
}

void quadrant_set_title(QuadrantParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->title, title);
}

void quadrant_set_acc_title(QuadrantParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->accTitle, title);
}

void quadrant_set_acc_descr(QuadrantParserContext* ctx, const char* descr) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->accDescr, descr);
}

void quadrant_set_x_axis_left(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->xAxisLeft, text);
}

void quadrant_set_x_axis_right(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->xAxisRight, text);
}

void quadrant_set_y_axis_bottom(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->yAxisBottom, text);
}

void quadrant_set_y_axis_top(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->yAxisTop, text);
}

void quadrant_set_quadrant_1_text(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->quadrant1Text, text);
}

void quadrant_set_quadrant_2_text(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->quadrant2Text, text);
}

void quadrant_set_quadrant_3_text(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->quadrant3Text, text);
}

void quadrant_set_quadrant_4_text(QuadrantParserContext* ctx, const char* text) {
    if(ctx && ctx->diagram) set_string(&ctx->diagram->quadrant4Text, text);
}

void quadrant_add_point(QuadrantParserContext* ctx, const char* text, const char* className, double x, double y) {
    if(!ctx || !ctx->diagram) return;
    
    QuadrantPoint* p = (QuadrantPoint*)malloc(sizeof(QuadrantPoint));
    if(p) {
        memset(p, 0, sizeof(QuadrantPoint));
        p->text = copy_string(text);
        p->className = copy_string(className);
        p->x = x;
        p->y = y;
        p->next = NULL;
        
        if(!ctx->diagram->points) {
            ctx->diagram->points = p;
        } else {
            QuadrantPoint* last = ctx->diagram->points;
            while(last->next) last = last->next;
            last->next = p;
        }
    }
}
