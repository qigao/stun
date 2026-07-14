%name FlowchartParser
%token_prefix FC_
%extra_argument { FlowchartParserContext *ctx }

%token NEWLINE GRAPH FLOWCHART SUBGRAPH END DIR ARROW 
       DOUBLE_CIRCLE_START DOUBLE_CIRCLE_END CPS CPE STADIUM_START STADIUM_END
       SUBROUTINE_START SUBROUTINE_END HEXAGON_START HEXAGON_END
       RHOMBUS_START RHOMBUS_END ELLIPSE_START ELLIPSE_END
       CYLINDER_START CYLINDER_END TRAP_START INV_TRAP_START TRAP_END
       SQS SQE PS PE PIPE SEMI COLON DASH HTML IDENTIFIER STRING CLASS EOF.

%token_type { char* }
%token_destructor { free($$); }
%type node { FlowchartNode* }
%type label_text { char* }
%type node_label { char* }
%type edge_chain { FlowchartNode* }
%destructor label_text { free($$); }
%destructor node_label { free($$); }

%include {
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flowchart/flowchart_ast.h"

static void push_subgraph(FlowchartParserContext *ctx, FlowchartSubGraph* sg) {
    FlowchartSubGraphStack* s = (FlowchartSubGraphStack*)malloc(sizeof(FlowchartSubGraphStack));
    if (!s) {
        ctx->error_count++;
        return;
    }
    s->sg = sg;
    s->next = (FlowchartSubGraphStack*)ctx->active_subgraphs;
    ctx->active_subgraphs = s;
}

static FlowchartSubGraph* pop_subgraph(FlowchartParserContext *ctx) {
    FlowchartSubGraphStack* s = (FlowchartSubGraphStack*)ctx->active_subgraphs;
    if (!s) return NULL;
    FlowchartSubGraph* sg = s->sg;
    ctx->active_subgraphs = s->next;
    free(s);
    return sg;
}

static FlowchartSubGraph* current_subgraph(FlowchartParserContext *ctx) {
    FlowchartSubGraphStack* s = (FlowchartSubGraphStack*)ctx->active_subgraphs;
    return s ? s->sg : NULL;
}

static void add_node_ref(FlowchartParserContext* ctx, FlowchartSubGraph* sg, const char* id) {
    FlowchartNodeRef* ref = (FlowchartNodeRef*)calloc(1, sizeof(FlowchartNodeRef));
    if (!ref) {
        ctx->error_count++;
        return;
    }
    ref->id = strdup(id);
    if (!ref->id) {
        free(ref);
        ctx->error_count++;
        return;
    }
    ref->next = sg->node_refs;
    sg->node_refs = ref;
}

static FlowchartNode* find_or_create_node(FlowchartParserContext *ctx, const char* id, const char* label) {
    if (!id) return NULL;
    FlowchartNode* curr = ctx->diagram->nodes;
    while (curr) {
        if (strcmp(curr->id, id) == 0) {
            if (label && (!curr->label || strcmp(curr->label, id) == 0)) {
                char* replacement = strdup(label);
                if (!replacement) {
                    ctx->error_count++;
                    return NULL;
                }
                free(curr->label);
                curr->label = replacement;
            }
            return curr;
        }
        curr = curr->next;
    }
    FlowchartNode* n = (FlowchartNode*)calloc(1, sizeof(FlowchartNode));
    if (!n) {
        ctx->error_count++;
        return NULL;
    }
    n->id = strdup(id);
    n->label = label ? strdup(label) : strdup(id);
    if (!n->id || !n->label) {
        free(n->id);
        free(n->label);
        free(n);
        ctx->error_count++;
        return NULL;
    }
    n->shape = FC_SHAPE_RECT;
    n->next = ctx->diagram->nodes;
    ctx->diagram->nodes = n;
    
    FlowchartSubGraph* sg = current_subgraph(ctx);
    if (sg) add_node_ref(ctx, sg, id);

    return n;
}

static void add_edge(FlowchartParserContext *ctx, const char* from, const char* to, const char* arrow, const char* label) {
    if (!from || !to) return;
    FlowchartEdge* e = (FlowchartEdge*)calloc(1, sizeof(FlowchartEdge));
    if (!e) {
        ctx->error_count++;
        return;
    }
    e->from = strdup(from);
    e->to = strdup(to);
    e->label = label ? strdup(label) : NULL;
    e->arrow_type = arrow ? strdup(arrow) : strdup("-->");
    if (!e->from || !e->to || !e->arrow_type || (label && !e->label)) {
        free(e->from);
        free(e->to);
        free(e->label);
        free(e->arrow_type);
        free(e);
        ctx->error_count++;
        return;
    }
    e->next = ctx->diagram->edges;
    ctx->diagram->edges = e;
}

static char* concat(FlowchartParserContext* ctx, char* a, char* b, const char* sep) {
    if (!a && !b) return NULL;
    if (!a) return b;
    if (!b) return a;
    size_t len = strlen(a) + strlen(b) + strlen(sep) + 1;
    char* r = (char*)malloc(len);
    if (!r) {
        free(a); free(b);
        ctx->error_count++;
        return NULL;
    }
    sprintf(r, "%s%s%s", a, sep, b);
    free(a); free(b);
    return r;
}

static void start_subgraph(FlowchartParserContext* ctx,
                           const char* id, const char* label) {
    FlowchartSubGraph* sg = (FlowchartSubGraph*)calloc(1, sizeof(FlowchartSubGraph));
    if (!sg) {
        ctx->error_count++;
        return;
    }
    sg->id = strdup(id);
    sg->label = strdup(label);
    if (!sg->id || !sg->label) {
        free(sg->id);
        free(sg->label);
        free(sg);
        ctx->error_count++;
        return;
    }

    FlowchartSubGraph* parent = current_subgraph(ctx);
    if (parent) {
        sg->next = parent->children;
        parent->children = sg;
    } else {
        sg->next = ctx->diagram->subgraphs;
        ctx->diagram->subgraphs = sg;
    }
    push_subgraph(ctx, sg);
}
}

%syntax_error {
    // fprintf(stderr, "Syntax error!\n");
    ctx->error_count++;
}

start ::= flowchart_type direction_opt statements EOF.

flowchart_type ::= FLOWCHART.
flowchart_type ::= GRAPH.

direction_opt ::= .
direction_opt ::= DIR(A). {
    ctx->diagram->direction = strdup(A);
    if (!ctx->diagram->direction) ctx->error_count++;
    free(A);
}

statements ::= statements statement.
statements ::= .

statement ::= node_definition.
statement ::= edge_statement.
statement ::= subgraph_statement.
statement ::= class_statement.
statement ::= NEWLINE.
statement ::= SEMI.

class_statement ::= CLASS class_idents SEMI.
class_statement ::= CLASS class_idents NEWLINE.
class_idents ::= class_idents label_text(A). { free(A); }
class_idents ::= label_text(A). { free(A); }

node_definition ::= node(A). { (void)A; }

// 基础值可以是标识符或字符串
label_text(R) ::= IDENTIFIER(A). { R = A; }
label_text(R) ::= STRING(A).     { R = A; }

// 形状和边标签允许由多个未加引号的单词组成；lexer 会丢弃词间空白。
node_label(R) ::= label_text(A). { R = A; }
node_label(R) ::= node_label(A) label_text(B). {
    R = concat(ctx, A, B, (A && A[strlen(A) - 1] == '>') ? "" : " ");
}
node_label(R) ::= node_label(A) COLON label_text(B). { R = concat(ctx, A, B, ":"); }
node_label(R) ::= node_label(A) DASH label_text(B). { R = concat(ctx, A, B, "-"); }
node_label(R) ::= node_label(A) HTML(B). { R = concat(ctx, A, B, ""); }

node(R) ::= label_text(A). {
    R = find_or_create_node(ctx, A, NULL);
    free(A);
}
node(R) ::= label_text(A) SQS node_label(B) SQE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_RECT;
    free(A); free(B);
}
node(R) ::= label_text(A) PS node_label(B) PE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_ROUND;
    free(A); free(B);
}
node(R) ::= label_text(A) CPS node_label(B) CPE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_CIRCLE;
    free(A); free(B);
}
node(R) ::= label_text(A) DOUBLE_CIRCLE_START node_label(B) DOUBLE_CIRCLE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_DOUBLECIRCLE;
    free(A); free(B);
}
node(R) ::= label_text(A) RHOMBUS_START node_label(B) RHOMBUS_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_RHOMBUS;
    free(A); free(B);
}
node(R) ::= label_text(A) STADIUM_START node_label(B) STADIUM_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_STADIUM;
    free(A); free(B);
}
node(R) ::= label_text(A) SUBROUTINE_START node_label(B) SUBROUTINE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_SUBROUTINE;
    free(A); free(B);
}
node(R) ::= label_text(A) HEXAGON_START node_label(B) HEXAGON_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_HEXAGON;
    free(A); free(B);
}
node(R) ::= label_text(A) ELLIPSE_START node_label(B) ELLIPSE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_ELLIPSE;
    free(A); free(B);
}
node(R) ::= label_text(A) CYLINDER_START node_label(B) CYLINDER_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_CYLINDER;
    free(A); free(B);
}
node(R) ::= label_text(A) TRAP_START node_label(B) TRAP_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_TRAPEZOID;
    free(A); free(B);
}
node(R) ::= label_text(A) INV_TRAP_START node_label(B) TRAP_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_INV_TRAPEZOID;
    free(A); free(B);
}

edge_statement ::= edge_chain(A). { (void)A; }

edge_chain(R) ::= node(A) ARROW(L) node(B). {
    if (A && B) add_edge(ctx, A->id, B->id, L, NULL);
    free(L);
    R = B;
}
edge_chain(R) ::= edge_chain(A) ARROW(L) node(B). {
    if (A && B) add_edge(ctx, A->id, B->id, L, NULL);
    free(L);
    R = B;
}
edge_chain(R) ::= node(A) ARROW(L) PIPE node_label(C) PIPE node(B). {
    if (A && B) add_edge(ctx, A->id, B->id, L, C);
    free(L); free(C);
    R = B;
}
edge_chain(R) ::= edge_chain(A) ARROW(L) PIPE node_label(C) PIPE node(B). {
    if (A && B) add_edge(ctx, A->id, B->id, L, C);
    free(L); free(C);
    R = B;
}

subgraph_statement ::= subgraph_start statements END. {
    pop_subgraph(ctx);
}

subgraph_start ::= SUBGRAPH label_text(A). {
    start_subgraph(ctx, A, A);
    free(A);
}

subgraph_start ::= SUBGRAPH label_text(A) SQS node_label(B) SQE. {
    start_subgraph(ctx, A, B);
    free(A); free(B);
}
