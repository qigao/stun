%name DotGraphParser
%token_prefix DG_
%extra_argument { DotGraphParserContext *ctx }

%token DIGRAPH GRAPH SUBGRAPH NODE EDGE STRICT
       ARROW DASHDASH
       LBRACE RBRACE LBRACKET RBRACKET
       SEMI COMMA COLON EQUALS
       IDENTIFIER NUMBER STRING HTML_STRING
       EOF.

%token_type { char* }
%token_destructor { free($$); }
%type id { char* }
%destructor id { free($$); }
%type edge_op { int }
%type endpoint { DgEndpoint }
%destructor endpoint { dg_free_endpoint(&$$); }

%include {
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dotgraph/dotgraph_ast.h"

/* ── Subgraph stack ── */
typedef struct SubGraphStack {
    DotGraphSubGraph* sg;
    struct SubGraphStack* next;
} SubGraphStack;

static void dg_record_oom(DotGraphParserContext *ctx) {
    if (!ctx) return;
    ctx->error_count++;
    if (ctx->error_message[0] == '\0') {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "DOT parser ran out of memory on line %d", ctx->line);
    }
}

static char* dg_strdup(DotGraphParserContext *ctx, const char* value) {
    if (!value) return NULL;
    char* copy = strdup(value);
    if (!copy) dg_record_oom(ctx);
    return copy;
}

static int dg_replace_string(DotGraphParserContext *ctx, char** target,
                             const char* value) {
    char* replacement = dg_strdup(ctx, value);
    if (!replacement) return 0;
    free(*target);
    *target = replacement;
    return 1;
}

static int dg_push_subgraph(DotGraphParserContext *ctx, DotGraphSubGraph* sg) {
    SubGraphStack* s = (SubGraphStack*)malloc(sizeof(SubGraphStack));
    if (!s) {
        dg_record_oom(ctx);
        return 0;
    }
    s->sg = sg;
    s->next = (SubGraphStack*)ctx->active_subgraphs;
    ctx->active_subgraphs = s;
    return 1;
}

static DotGraphSubGraph* dg_pop_subgraph(DotGraphParserContext *ctx) {
    SubGraphStack* s = (SubGraphStack*)ctx->active_subgraphs;
    if (!s) return NULL;
    DotGraphSubGraph* sg = s->sg;
    ctx->active_subgraphs = s->next;
    free(s);
    return sg;
}

static DotGraphSubGraph* dg_current_subgraph(DotGraphParserContext *ctx) {
    SubGraphStack* s = (SubGraphStack*)ctx->active_subgraphs;
    return s ? s->sg : NULL;
}

static void dg_clear_subgraph_stack(DotGraphParserContext *ctx) {
    while (ctx->active_subgraphs) dg_pop_subgraph(ctx);
}

static void dg_add_node_ref(DotGraphParserContext* ctx,
                            DotGraphSubGraph* sg, const char* id) {
    if (!sg || !id) return;
    DotGraphNodeRef* ref = (DotGraphNodeRef*)malloc(sizeof(DotGraphNodeRef));
    if (!ref) {
        dg_record_oom(ctx);
        return;
    }
    ref->id = dg_strdup(ctx, id);
    if (!ref->id) {
        free(ref);
        return;
    }
    ref->next = sg->node_refs;
    sg->node_refs = ref;
}

/* ── Attribute helpers ── */
static DotGraphAttr* dg_create_attr(DotGraphParserContext* ctx,
                                    const char* key, const char* value) {
    DotGraphAttr* a = (DotGraphAttr*)malloc(sizeof(DotGraphAttr));
    if (!a) {
        dg_record_oom(ctx);
        return NULL;
    }
    a->key = dg_strdup(ctx, key);
    a->value = dg_strdup(ctx, value);
    if ((key && !a->key) || (value && !a->value)) {
        free(a->key);
        free(a->value);
        free(a);
        return NULL;
    }
    a->next = NULL;
    return a;
}

static DotGraphAttr* dg_append_attr(DotGraphAttr* list, DotGraphAttr* item) {
    if (!item) return list;
    item->next = list;
    return item;
}

static const char* dg_find_attr_value(DotGraphAttr* list, const char* key) {
    while (list) {
        if (list->key && key && strcmp(list->key, key) == 0) return list->value;
        list = list->next;
    }
    return NULL;
}

static void dg_free_attrs(DotGraphAttr* list) {
    while (list) {
        DotGraphAttr* next = list->next;
        free(list->key);
        free(list->value);
        free(list);
        list = next;
    }
}

/* ── Shape resolution ── */
static DotGraphShape dg_resolve_shape(const char* name) {
    if (!name) return DG_SHAPE_ELLIPSE;
    if (strcmp(name, "box") == 0 || strcmp(name, "rect") == 0 || strcmp(name, "rectangle") == 0) return DG_SHAPE_BOX;
    if (strcmp(name, "circle") == 0) return DG_SHAPE_CIRCLE;
    if (strcmp(name, "diamond") == 0) return DG_SHAPE_DIAMOND;
    if (strcmp(name, "record") == 0 || strcmp(name, "Mrecord") == 0) return DG_SHAPE_RECORD;
    if (strcmp(name, "plaintext") == 0 || strcmp(name, "plain") == 0 || strcmp(name, "none") == 0) return DG_SHAPE_PLAINTEXT;
    if (strcmp(name, "doublecircle") == 0) return DG_SHAPE_DOUBLECIRCLE;
    if (strcmp(name, "triangle") == 0) return DG_SHAPE_TRIANGLE;
    if (strcmp(name, "hexagon") == 0) return DG_SHAPE_HEXAGON;
    if (strcmp(name, "parallelogram") == 0) return DG_SHAPE_PARALLELOGRAM;
    if (strcmp(name, "cylinder") == 0) return DG_SHAPE_CYLINDER;
    if (strcmp(name, "note") == 0) return DG_SHAPE_NOTE;
    if (strcmp(name, "component") == 0) return DG_SHAPE_COMPONENT;
    if (strcmp(name, "folder") == 0) return DG_SHAPE_FOLDER;
    return DG_SHAPE_ELLIPSE;
}

/* ── Compass resolution ── */
static DotGraphCompass dg_resolve_compass(const char* name) {
    if (!name) return DG_COMPASS_NONE;
    if (strcmp(name, "n") == 0) return DG_COMPASS_N;
    if (strcmp(name, "ne") == 0) return DG_COMPASS_NE;
    if (strcmp(name, "e") == 0) return DG_COMPASS_E;
    if (strcmp(name, "se") == 0) return DG_COMPASS_SE;
    if (strcmp(name, "s") == 0) return DG_COMPASS_S;
    if (strcmp(name, "sw") == 0) return DG_COMPASS_SW;
    if (strcmp(name, "w") == 0) return DG_COMPASS_W;
    if (strcmp(name, "nw") == 0) return DG_COMPASS_NW;
    if (strcmp(name, "c") == 0) return DG_COMPASS_C;
    return DG_COMPASS_NONE;
}

/* ── Rankdir resolution ── */
static DotGraphRankdir dg_resolve_rankdir(const char* name) {
    if (!name) return DG_RANKDIR_TB;
    if (strcmp(name, "TB") == 0 || strcmp(name, "td") == 0) return DG_RANKDIR_TB;
    if (strcmp(name, "BT") == 0 || strcmp(name, "bt") == 0) return DG_RANKDIR_BT;
    if (strcmp(name, "LR") == 0 || strcmp(name, "lr") == 0) return DG_RANKDIR_LR;
    if (strcmp(name, "RL") == 0 || strcmp(name, "rl") == 0) return DG_RANKDIR_RL;
    return DG_RANKDIR_TB;
}

/* ── Node find/create ── */
static DotGraphNode* dg_find_or_create_node(DotGraphParserContext *ctx, const char* id) {
    if (!ctx || !ctx->diagram || !id) return NULL;
    DotGraphNode* curr = ctx->diagram->nodes;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    DotGraphNode* n = (DotGraphNode*)malloc(sizeof(DotGraphNode));
    if (!n) {
        dg_record_oom(ctx);
        return NULL;
    }
    memset(n, 0, sizeof(DotGraphNode));
    n->id = dg_strdup(ctx, id);
    n->label = dg_strdup(ctx, id);
    if (!n->id || !n->label) {
        free(n->id);
        free(n->label);
        free(n);
        return NULL;
    }
    n->shape = DG_SHAPE_ELLIPSE;
    n->next = ctx->diagram->nodes;
    ctx->diagram->nodes = n;

    DotGraphSubGraph* sg = dg_current_subgraph(ctx);
    if (sg) dg_add_node_ref(ctx, sg, id);

    return n;
}

/* ── Apply attributes to node ── */
static void dg_apply_node_attrs(DotGraphParserContext* ctx,
                                DotGraphNode* node, DotGraphAttr* attrs) {
    if (!node || !attrs) return;
    const char* v;
    v = dg_find_attr_value(attrs, "label");
    if (v) dg_replace_string(ctx, &node->label, v);
    v = dg_find_attr_value(attrs, "shape");
    if (v) node->shape = dg_resolve_shape(v);
    v = dg_find_attr_value(attrs, "color");
    if (v) dg_replace_string(ctx, &node->color, v);
    v = dg_find_attr_value(attrs, "fillcolor");
    if (v) dg_replace_string(ctx, &node->fillcolor, v);
    v = dg_find_attr_value(attrs, "fontcolor");
    if (v) dg_replace_string(ctx, &node->fontcolor, v);
    v = dg_find_attr_value(attrs, "style");
    if (v) dg_replace_string(ctx, &node->style, v);
    v = dg_find_attr_value(attrs, "width");
    if (v) node->layout_width = atof(v);
    v = dg_find_attr_value(attrs, "height");
    if (v) node->layout_height = atof(v);
    /* Store full attr list on node */
    dg_free_attrs(node->attrs);
    node->attrs = attrs;
}

static void dg_apply_edge_attrs(DotGraphParserContext* ctx,
                                DotGraphEdge* edge, DotGraphAttr* attrs) {
    if (!edge || !attrs) return;
    const char* value = dg_find_attr_value(attrs, "label");
    if (value) dg_replace_string(ctx, &edge->label, value);
    value = dg_find_attr_value(attrs, "color");
    if (value) dg_replace_string(ctx, &edge->color, value);
    value = dg_find_attr_value(attrs, "style");
    if (value) dg_replace_string(ctx, &edge->style, value);

    DotGraphAttr* tail = attrs;
    while (tail->next) tail = tail->next;
    tail->next = edge->attrs;
    edge->attrs = attrs;
}

/* ── Add edge ── */
static void dg_add_edge(DotGraphParserContext *ctx,
                         const char* from, const char* from_port, DotGraphCompass from_compass,
                         const char* to, const char* to_port, DotGraphCompass to_compass,
                         DotGraphAttr* attrs) {
    if (!from || !to) { dg_free_attrs(attrs); return; }
    if (!dg_find_or_create_node(ctx, from) ||
        !dg_find_or_create_node(ctx, to)) {
        dg_free_attrs(attrs);
        return;
    }

    if (ctx->diagram->is_strict) {
        for (DotGraphEdge* existing = ctx->diagram->edges;
             existing; existing = existing->next) {
            const int same_direction = strcmp(existing->from, from) == 0 &&
                                       strcmp(existing->to, to) == 0;
            const int reverse_direction = !ctx->diagram->is_directed &&
                                          strcmp(existing->from, to) == 0 &&
                                          strcmp(existing->to, from) == 0;
            if (same_direction || reverse_direction) {
                dg_apply_edge_attrs(ctx, existing, attrs);
                return;
            }
        }
    }

    DotGraphEdge* e = (DotGraphEdge*)malloc(sizeof(DotGraphEdge));
    if (!e) {
        dg_record_oom(ctx);
        dg_free_attrs(attrs);
        return;
    }
    memset(e, 0, sizeof(DotGraphEdge));
    e->from = dg_strdup(ctx, from);
    e->from_port = from_port ? dg_strdup(ctx, from_port) : NULL;
    e->from_compass = from_compass;
    e->to = dg_strdup(ctx, to);
    e->to_port = to_port ? dg_strdup(ctx, to_port) : NULL;
    e->to_compass = to_compass;

    if (!e->from || !e->to || (from_port && !e->from_port) ||
        (to_port && !e->to_port)) {
        free(e->from);
        free(e->from_port);
        free(e->to);
        free(e->to_port);
        free(e);
        dg_free_attrs(attrs);
        return;
    }

    dg_apply_edge_attrs(ctx, e, attrs);

    e->next = ctx->diagram->edges;
    ctx->diagram->edges = e;
}

/* ── Temporary edge endpoint holder ── */
typedef struct {
    char* id;
    char* port;
    DotGraphCompass compass;
} DgEndpoint;

static DgEndpoint dg_make_endpoint(char* id, char* port, DotGraphCompass compass) {
    DgEndpoint ep;
    ep.id = id;
    ep.port = port;
    ep.compass = compass;
    return ep;
}

static void dg_free_endpoint(DgEndpoint* ep) {
    if (ep) { free(ep->id); free(ep->port); }
}
}

%syntax_error {
    ctx->error_count++;
    if (ctx->error_message[0] == '\0') {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "DOT syntax error on line %d", ctx->line);
    }
}

%parse_failure {
    dg_clear_subgraph_stack(ctx);
}

%parse_accept {
    dg_clear_subgraph_stack(ctx);
}

%stack_overflow {
    ctx->error_count++;
    if (ctx->error_message[0] == '\0') {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "DOT parser stack overflow on line %d", ctx->line);
    }
    dg_clear_subgraph_stack(ctx);
}

/* ── Grammar ── */

start ::= graph EOF.

graph ::= strict_opt graph_type id_opt LBRACE stmt_list RBRACE.

strict_opt ::= .
strict_opt ::= STRICT. { ctx->diagram->is_strict = 1; }

graph_type ::= DIGRAPH. { ctx->diagram->is_directed = 1; }
graph_type ::= GRAPH.   { ctx->diagram->is_directed = 0; }

id_opt ::= .
id_opt ::= id(A). { free(ctx->diagram->graph_id); ctx->diagram->graph_id = A; }

stmt_list ::= .
stmt_list ::= stmt_list stmt semi_opt.

semi_opt ::= .
semi_opt ::= SEMI.

stmt ::= node_stmt.
stmt ::= edge_stmt.
stmt ::= attr_stmt.
stmt ::= graph_attr_assign.
stmt ::= subgraph_def.

/* ── id production ── */
id(R) ::= IDENTIFIER(A). { R = A; }
id(R) ::= NUMBER(A).     { R = A; }
id(R) ::= STRING(A).     { R = A; }
id(R) ::= HTML_STRING(A). { R = A; }

/* ── Node statement ── */
node_stmt ::= id(A) attr_list_opt(B). {
    DotGraphNode* n = dg_find_or_create_node(ctx, A);
    if (n && B) dg_apply_node_attrs(ctx, n, B);
    else dg_free_attrs(B);
    free(A);
}

node_stmt ::= id(A) COLON id(P) attr_list_opt(B). {
    DotGraphNode* n = dg_find_or_create_node(ctx, A);
    if (n && B) dg_apply_node_attrs(ctx, n, B);
    else dg_free_attrs(B);
    free(A); free(P);
}

/* ── Edge statement ── */
edge_stmt ::= endpoint(A) edge_op(OP) endpoint(B) attr_list_opt(C). {
    const int expected_directed = ctx->diagram->is_directed ? 1 : 0;
    if (OP != expected_directed) {
        ctx->error_count++;
        if (ctx->error_message[0] == '\0') {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "edge operator does not match graph type on line %d", ctx->line);
        }
        dg_free_attrs(C);
    } else {
        dg_add_edge(ctx, A.id, A.port, A.compass,
                    B.id, B.port, B.compass, C);
    }
    dg_free_endpoint(&A);
    dg_free_endpoint(&B);
}

edge_op(R) ::= ARROW.    { R = 1; }
edge_op(R) ::= DASHDASH. { R = 0; }

endpoint(R) ::= id(A). {
    R = dg_make_endpoint(A, NULL, DG_COMPASS_NONE);
}
endpoint(R) ::= id(A) COLON id(P). {
    DotGraphCompass compass = dg_resolve_compass(P);
    if (compass == DG_COMPASS_NONE) {
        R = dg_make_endpoint(A, P, DG_COMPASS_NONE);
    } else {
        free(P);
        R = dg_make_endpoint(A, NULL, compass);
    }
}
endpoint(R) ::= id(A) COLON id(P) COLON id(C). {
    R = dg_make_endpoint(A, P, dg_resolve_compass(C));
    free(C);
}
endpoint(R) ::= id(A) COLON COLON id(C). {
    R = dg_make_endpoint(A, NULL, dg_resolve_compass(C));
    free(C);
}

/* ── Attribute statement (defaults) ── */
attr_stmt ::= GRAPH LBRACKET a_list(A) RBRACKET. {
    dg_free_attrs(ctx->diagram->graph_defaults);
    ctx->diagram->graph_defaults = A;
    /* Apply rankdir if present */
    const char* rd = dg_find_attr_value(A, "rankdir");
    if (rd) ctx->diagram->rankdir = dg_resolve_rankdir(rd);
}

attr_stmt ::= NODE LBRACKET a_list(A) RBRACKET. {
    DotGraphSubGraph* sg = dg_current_subgraph(ctx);
    if (sg) {
        dg_free_attrs(sg->node_defaults);
        sg->node_defaults = A;
    } else {
        dg_free_attrs(ctx->diagram->node_defaults);
        ctx->diagram->node_defaults = A;
    }
}

attr_stmt ::= EDGE LBRACKET a_list(A) RBRACKET. {
    DotGraphSubGraph* sg = dg_current_subgraph(ctx);
    if (sg) {
        dg_free_attrs(sg->edge_defaults);
        sg->edge_defaults = A;
    } else {
        dg_free_attrs(ctx->diagram->edge_defaults);
        ctx->diagram->edge_defaults = A;
    }
}

/* ── Graph-level key=value ── */
graph_attr_assign ::= id(K) EQUALS id(V). {
    if (strcmp(K, "rankdir") == 0) {
        ctx->diagram->rankdir = dg_resolve_rankdir(V);
    } else {
        DotGraphAttr* a = dg_create_attr(ctx, K, V);
        if (a) {
            a->next = ctx->diagram->graph_defaults;
            ctx->diagram->graph_defaults = a;
        }
    }
    free(K); free(V);
}

/* ── Subgraph ── */
subgraph_def ::= subgraph_start stmt_list RBRACE. {
    dg_pop_subgraph(ctx);
}

subgraph_start ::= SUBGRAPH id(A) LBRACE. {
    DotGraphSubGraph* sg = (DotGraphSubGraph*)malloc(sizeof(DotGraphSubGraph));
    if (!sg) {
        dg_record_oom(ctx);
        free(A);
    } else {
        memset(sg, 0, sizeof(DotGraphSubGraph));
        sg->id = A;
        sg->is_cluster = (strncmp(A, "cluster", 7) == 0) ? 1 : 0;

        DotGraphSubGraph* parent = dg_current_subgraph(ctx);
        if (parent) {
            sg->next = parent->children;
            parent->children = sg;
        } else {
            sg->next = ctx->diagram->subgraphs;
            ctx->diagram->subgraphs = sg;
        }
        dg_push_subgraph(ctx, sg);
    }
}

subgraph_start ::= SUBGRAPH LBRACE. {
    DotGraphSubGraph* sg = (DotGraphSubGraph*)malloc(sizeof(DotGraphSubGraph));
    if (!sg) {
        dg_record_oom(ctx);
    } else {
        memset(sg, 0, sizeof(DotGraphSubGraph));
        sg->id = dg_strdup(ctx, "");
        if (sg->id) {
            DotGraphSubGraph* parent = dg_current_subgraph(ctx);
            if (parent) {
                sg->next = parent->children;
                parent->children = sg;
            } else {
                sg->next = ctx->diagram->subgraphs;
                ctx->diagram->subgraphs = sg;
            }
            dg_push_subgraph(ctx, sg);
        } else {
            free(sg);
        }
    }
}

/* ── Attribute list ── */
%type attr_list_opt { DotGraphAttr* }
%type a_list { DotGraphAttr* }
%destructor attr_list_opt { dg_free_attrs($$); }
%destructor a_list { dg_free_attrs($$); }

attr_list_opt(R) ::= . { R = NULL; }
attr_list_opt(R) ::= LBRACKET a_list(A) RBRACKET. { R = A; }
attr_list_opt(R) ::= LBRACKET RBRACKET. { R = NULL; }

a_list(R) ::= id(K) EQUALS id(V). {
    R = dg_create_attr(ctx, K, V);
    free(K); free(V);
}
a_list(R) ::= a_list(L) COMMA id(K) EQUALS id(V). {
    DotGraphAttr* a = dg_create_attr(ctx, K, V);
    R = dg_append_attr(L, a);
    free(K); free(V);
}
a_list(R) ::= a_list(L) SEMI id(K) EQUALS id(V). {
    DotGraphAttr* a = dg_create_attr(ctx, K, V);
    R = dg_append_attr(L, a);
    free(K); free(V);
}
a_list(R) ::= a_list(L) id(K) EQUALS id(V). {
    DotGraphAttr* a = dg_create_attr(ctx, K, V);
    R = dg_append_attr(L, a);
    free(K); free(V);
}
