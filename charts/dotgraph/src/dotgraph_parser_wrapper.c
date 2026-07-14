#include "dotgraph/dotgraph_parser_wrapper.h"
#include "dotgraph_parser_gen.h"
#include "dotgraph_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *DotGraphParserAlloc(void *(*mallocProc)(size_t));
void DotGraphParser(void *yyp, int yymajor, void *yyminor, DotGraphParserContext *ctx);
void DotGraphParserFree(void *p, void (*freeProc)(void*));

#if defined(_MSC_VER)
#define DG_THREAD_LOCAL __declspec(thread)
#else
#define DG_THREAD_LOCAL _Thread_local
#endif

static DG_THREAD_LOCAL char g_dotgraph_last_error[256] = {0};

const char* dotgraph_get_last_error(void) {
    return g_dotgraph_last_error;
}

/* ── Free helpers ── */

static void free_attrs(DotGraphAttr* a) {
    while (a) {
        DotGraphAttr* next = a->next;
        free(a->key);
        free(a->value);
        free(a);
        a = next;
    }
}

static void free_node_refs(DotGraphNodeRef* nr) {
    while (nr) {
        DotGraphNodeRef* next = nr->next;
        free(nr->id);
        free(nr);
        nr = next;
    }
}

static void free_subgraph(DotGraphSubGraph* sg) {
    if (!sg) return;
    free(sg->id);
    free(sg->label);
    free(sg->color);
    free(sg->style);
    free_attrs(sg->node_defaults);
    free_attrs(sg->edge_defaults);
    free_node_refs(sg->node_refs);
    DotGraphSubGraph* child = sg->children;
    while (child) {
        DotGraphSubGraph* next = child->next;
        free_subgraph(child);
        child = next;
    }
    free(sg);
}

void dotgraph_diagram_free(DotGraphDiagram* diagram) {
    if (!diagram) return;
    free(diagram->graph_id);
    free_attrs(diagram->graph_defaults);
    free_attrs(diagram->node_defaults);
    free_attrs(diagram->edge_defaults);

    DotGraphNode* n = diagram->nodes;
    while (n) {
        DotGraphNode* next = n->next;
        free(n->id);
        free(n->label);
        free(n->color);
        free(n->fillcolor);
        free(n->fontcolor);
        free(n->style);
        free_attrs(n->attrs);
        free(n);
        n = next;
    }

    DotGraphEdge* e = diagram->edges;
    while (e) {
        DotGraphEdge* next = e->next;
        free(e->from);
        free(e->from_port);
        free(e->to);
        free(e->to_port);
        free(e->label);
        free(e->color);
        free(e->style);
        free_attrs(e->attrs);
        free(e);
        e = next;
    }

    DotGraphSubGraph* sg = diagram->subgraphs;
    while (sg) {
        DotGraphSubGraph* next = sg->next;
        free_subgraph(sg);
        sg = next;
    }

    free(diagram);
}

/* ── Parse ── */

DotGraphDiagram* dotgraph_parse(const char* input) {
    g_dotgraph_last_error[0] = '\0';
    if (!input) {
        snprintf(g_dotgraph_last_error, sizeof(g_dotgraph_last_error),
                 "DOT input is NULL");
        return NULL;
    }

    const size_t input_size = strlen(input);
    if (input_size > DOTGRAPH_MAX_INPUT_BYTES) {
        snprintf(g_dotgraph_last_error, sizeof(g_dotgraph_last_error),
                 "DOT input exceeds %d bytes", DOTGRAPH_MAX_INPUT_BYTES);
        return NULL;
    }

    DotGraphParserContext ctx;
    memset(&ctx, 0, sizeof(DotGraphParserContext));
    ctx.line = 1;
    ctx.diagram = (DotGraphDiagram*)malloc(sizeof(DotGraphDiagram));
    if (!ctx.diagram) {
        snprintf(g_dotgraph_last_error, sizeof(g_dotgraph_last_error),
                 "failed to allocate DOT diagram");
        return NULL;
    }
    memset(ctx.diagram, 0, sizeof(DotGraphDiagram));

    ctx.diagram->rankdir = DG_RANKDIR_TB;
    ctx.diagram->routing_mode = DG_ROUTE_ORTHOGONAL;
    ctx.diagram->routing_shape_buffer = -1.0;
    ctx.diagram->routing_nudging_distance = -1.0;
    ctx.diagram->routing_segment_penalty = -1.0;
    ctx.diagram->routing_angle_penalty = -1.0;
    ctx.diagram->routing_crossing_penalty = -1.0;
    ctx.diagram->routing_nudge_orthogonal_ends = -1;
    ctx.diagram->routing_nudge_shared_paths = -1;

    void* parser = DotGraphParserAlloc(malloc);
    if (!parser) {
        free(ctx.diagram);
        snprintf(g_dotgraph_last_error, sizeof(g_dotgraph_last_error),
                 "failed to allocate DOT parser");
        return NULL;
    }

    DotGraphScanner s;
    s.cursor = input;
    s.limit = input + input_size;
    s.marker = NULL;
    s.line = 1;

    dotgraph_scan(&s, parser, &ctx);
    DotGraphParser(parser, 0, NULL, &ctx);
    DotGraphParserFree(parser, free);

    if (ctx.error_count > 0) {
        if (ctx.error_message[0] != '\0') {
            snprintf(g_dotgraph_last_error, sizeof(g_dotgraph_last_error),
                     "%s", ctx.error_message);
        } else {
            snprintf(g_dotgraph_last_error, sizeof(g_dotgraph_last_error),
                     "DOT parse error (%d error%s)", ctx.error_count,
                     ctx.error_count > 1 ? "s" : "");
        }
        dotgraph_diagram_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

/* ── Node size setter ── */

void dotgraph_set_node_size(DotGraphDiagram* diagram, const char* id, double width, double height) {
    if (!diagram || !id) return;
    DotGraphNode* n = diagram->nodes;
    while (n) {
        if (n->id && strcmp(n->id, id) == 0) {
            n->layout_width = width;
            n->layout_height = height;
            return;
        }
        n = n->next;
    }
}
