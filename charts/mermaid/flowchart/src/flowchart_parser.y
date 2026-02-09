%name FlowchartParser
%token_prefix FC_
%extra_argument { FlowchartParserContext *ctx }

%token NEWLINE GRAPH FLOWCHART SUBGRAPH END DIR ARROW 
       DOUBLE_CIRCLE_START DOUBLE_CIRCLE_END CPS CPE STADIUM_START STADIUM_END
       SUBROUTINE_START SUBROUTINE_END HEXAGON_START HEXAGON_END
       RHOMBUS_START RHOMBUS_END ELLIPSE_START ELLIPSE_END
       CYLINDER_START CYLINDER_END TRAP_START INV_TRAP_START TRAP_END
       SQS SQE PS PE PIPE SEMI IDENTIFIER STRING CLASS EOF.

%token_type { char* }
%type node { FlowchartNode* }
%type label_text { char* }

%include {
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flowchart/flowchart_ast.h"

typedef struct SubGraphStack {
    FlowchartSubGraph* sg;
    struct SubGraphStack* next;
} SubGraphStack;

static void push_subgraph(FlowchartParserContext *ctx, FlowchartSubGraph* sg) {
    SubGraphStack* s = (SubGraphStack*)malloc(sizeof(SubGraphStack));
    s->sg = sg;
    s->next = (SubGraphStack*)ctx->active_subgraphs;
    ctx->active_subgraphs = s;
}

static FlowchartSubGraph* pop_subgraph(FlowchartParserContext *ctx) {
    SubGraphStack* s = (SubGraphStack*)ctx->active_subgraphs;
    if (!s) return NULL;
    FlowchartSubGraph* sg = s->sg;
    ctx->active_subgraphs = s->next;
    free(s);
    return sg;
}

static FlowchartSubGraph* current_subgraph(FlowchartParserContext *ctx) {
    SubGraphStack* s = (SubGraphStack*)ctx->active_subgraphs;
    return s ? s->sg : NULL;
}

static void add_node_ref(FlowchartSubGraph* sg, const char* id) {
    FlowchartNodeRef* ref = (FlowchartNodeRef*)malloc(sizeof(FlowchartNodeRef));
    ref->id = strdup(id);
    ref->next = sg->node_refs;
    sg->node_refs = ref;
}

static FlowchartNode* find_or_create_node(FlowchartParserContext *ctx, const char* id, const char* label) {
    if (!id) return NULL;
    FlowchartNode* curr = ctx->diagram->nodes;
    while (curr) {
        if (strcmp(curr->id, id) == 0) {
            if (label && (!curr->label || strcmp(curr->label, id) == 0)) {
                free(curr->label);
                curr->label = strdup(label);
            }
            return curr;
        }
        curr = curr->next;
    }
    FlowchartNode* n = (FlowchartNode*)malloc(sizeof(FlowchartNode));
    memset(n, 0, sizeof(FlowchartNode));
    n->id = strdup(id);
    n->label = label ? strdup(label) : strdup(id);
    n->shape = FC_SHAPE_RECT;
    n->next = ctx->diagram->nodes;
    ctx->diagram->nodes = n;
    
    FlowchartSubGraph* sg = current_subgraph(ctx);
    if (sg) add_node_ref(sg, id);

    return n;
}

static void add_edge(FlowchartParserContext *ctx, const char* from, const char* to, const char* arrow, const char* label) {
    if (!from || !to) return;
    FlowchartEdge* e = (FlowchartEdge*)malloc(sizeof(FlowchartEdge));
    memset(e, 0, sizeof(FlowchartEdge));
    e->from = strdup(from);
    e->to = strdup(to);
    e->label = label ? strdup(label) : NULL;
    e->arrow_type = arrow ? strdup(arrow) : strdup("-->");
    e->next = ctx->diagram->edges;
    ctx->diagram->edges = e;
}

static char* concat(char* a, char* b, const char* sep) {
    if (!a && !b) return NULL;
    if (!a) return b;
    if (!b) return a;
    size_t len = strlen(a) + strlen(b) + strlen(sep) + 1;
    char* r = (char*)malloc(len);
    sprintf(r, "%s%s%s", a, sep, b);
    free(a); free(b);
    return r;
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
direction_opt ::= DIR(A). { ctx->diagram->direction = strdup(A); free(A); }

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

node(R) ::= label_text(A). {
    R = find_or_create_node(ctx, A, NULL);
    free(A);
}
node(R) ::= label_text(A) SQS label_text(B) SQE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_RECT;
    free(A); free(B);
}
node(R) ::= label_text(A) PS label_text(B) PE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_ROUND;
    free(A); free(B);
}
node(R) ::= label_text(A) CPS label_text(B) CPE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_CIRCLE;
    free(A); free(B);
}
node(R) ::= label_text(A) DOUBLE_CIRCLE_START label_text(B) DOUBLE_CIRCLE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_DOUBLECIRCLE;
    free(A); free(B);
}
node(R) ::= label_text(A) RHOMBUS_START label_text(B) RHOMBUS_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_RHOMBUS;
    free(A); free(B);
}
node(R) ::= label_text(A) STADIUM_START label_text(B) STADIUM_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_STADIUM;
    free(A); free(B);
}
node(R) ::= label_text(A) SUBROUTINE_START label_text(B) SUBROUTINE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_SUBROUTINE;
    free(A); free(B);
}
node(R) ::= label_text(A) HEXAGON_START label_text(B) HEXAGON_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_HEXAGON;
    free(A); free(B);
}
node(R) ::= label_text(A) ELLIPSE_START label_text(B) ELLIPSE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_ELLIPSE;
    free(A); free(B);
}
node(R) ::= label_text(A) CYLINDER_START label_text(B) CYLINDER_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_CYLINDER;
    free(A); free(B);
}
node(R) ::= label_text(A) TRAP_START label_text(B) TRAP_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_TRAPEZOID;
    free(A); free(B);
}
node(R) ::= label_text(A) INV_TRAP_START label_text(B) TRAP_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_INV_TRAPEZOID;
    free(A); free(B);
}

edge_statement ::= node(A) ARROW(L) node(B). {
    if (A && B) add_edge(ctx, A->id, B->id, L, NULL);
    free(L);
}
edge_statement ::= node(A) ARROW(L) PIPE label_text(C) PIPE node(B). {
    if (A && B) add_edge(ctx, A->id, B->id, L, C);
    free(L); free(C);
}

subgraph_statement ::= subgraph_start statements END. {
    pop_subgraph(ctx);
}

subgraph_start ::= SUBGRAPH label_text(A). {
    FlowchartSubGraph* sg = (FlowchartSubGraph*)malloc(sizeof(FlowchartSubGraph));
    memset(sg, 0, sizeof(FlowchartSubGraph));
    sg->id = strdup(A);
    sg->label = strdup(A);
    
    FlowchartSubGraph* parent = current_subgraph(ctx);
    if (parent) {
        sg->next = parent->children;
        parent->children = sg;
    } else {
        sg->next = ctx->diagram->subgraphs;
        ctx->diagram->subgraphs = sg;
    }
    push_subgraph(ctx, sg);
    free(A);
}

subgraph_start ::= SUBGRAPH label_text(A) SQS label_text(B) SQE. {
    FlowchartSubGraph* sg = (FlowchartSubGraph*)malloc(sizeof(FlowchartSubGraph));
    memset(sg, 0, sizeof(FlowchartSubGraph));
    sg->id = strdup(A);
    sg->label = strdup(B);
    
    FlowchartSubGraph* parent = current_subgraph(ctx);
    if (parent) {
        sg->next = parent->children;
        parent->children = sg;
    } else {
        sg->next = ctx->diagram->subgraphs;
        ctx->diagram->subgraphs = sg;
    }
    push_subgraph(ctx, sg);
    free(A); free(B);
}
