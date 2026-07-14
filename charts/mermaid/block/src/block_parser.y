%name BlockParser
%token_prefix BLOCK_
%extra_argument { BlockParserContext *ctx }

%type id { char* }
%type node_def { char* }

// Precedence declarations to resolve shift/reduce conflicts
// Lower precedence listed first
%nonassoc ID STRING DIR.
%left EDGE.
%nonassoc SHAPE_START ARROW_START.

%include {
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flex/core/expr_c.h"
#include "block/block_ast.h"

static int to_int(const char* s) {
    if (!s) return 0;
    int value = 0;
    return flex_expr_eval_i32(s, &value) ? value : 0;
}

static void add_statement(BlockParserContext *ctx, BlockStatement* stmt) {
    if (!stmt) return;
    BlockStatement** head_scan;
    if (ctx->current_container) {
        head_scan = &ctx->current_container->data.node.children;
    } else {
        head_scan = &ctx->diagram->statements;
    }

    if (!*head_scan) {
        *head_scan = stmt;
    } else {
        BlockStatement* last = *head_scan;
        while(last->next) last = last->next;
        last->next = stmt;
    }
}

static void create_node(BlockParserContext *ctx, char* id, char* label, char* shape) {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_NODE;
    s->data.node.id = id;
    s->data.node.label = label;
    s->data.node.shape = shape;
    s->data.node.width = 1;
    add_statement(ctx, s);
}

typedef struct ContainerStackItem {
    BlockStatement* container;
    struct ContainerStackItem* next;
} ContainerStackItem;

static void push_container(BlockParserContext *ctx, BlockStatement* container) {
    ContainerStackItem* item = (ContainerStackItem*)malloc(sizeof(ContainerStackItem));
    item->container = ctx->current_container;
    item->next = (ContainerStackItem*)ctx->container_stack;
    ctx->container_stack = item;
    ctx->current_container = container;
}

static void pop_container(BlockParserContext *ctx) {
    ContainerStackItem* item = (ContainerStackItem*)ctx->container_stack;
    if (item) {
        ctx->current_container = item->container;
        ctx->container_stack = item->next;
        free(item);
    } else {
        ctx->current_container = NULL;
    }
}
}

%syntax_error {
    ctx->error_count++;
}

start ::= START(H) statements. {
    if (ctx->diagram->hierarchy) free(ctx->diagram->hierarchy);
    ctx->diagram->hierarchy = H;
}

statements ::= .
statements ::= statements statement.

statement ::= NL.
statement ::= line_content NL.
statement ::= composite_block.

// A line can have multiple items
line_content ::= line_item.
line_content ::= line_content line_item.

line_item ::= node_def.
line_item ::= edge_def.
line_item ::= columns_stmt.
line_item ::= space_stmt.
line_item ::= apply_class_stmt.
line_item ::= style_stmt.
line_item ::= classdef_stmt.

// Node definitions
node_def ::= id(N). [ID] {
    create_node(ctx, N, NULL, NULL);
}

node_def ::= id(N) SHAPE_START(S1) id(L) SHAPE_END(S2). {
    create_node(ctx, N, L, S1);
    free(S2);
}

node_def ::= id(N) ARROW_START id(L) ARROW_END SHAPE_START id(D) SHAPE_END. {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_NODE;
    s->data.node.id = N;
    s->data.node.label = L;
    s->data.node.shape = strdup("arrow");
    s->data.node.directions = (char**)malloc(sizeof(char*));
    s->data.node.directions[0] = D;
    s->data.node.direction_count = 1;
    add_statement(ctx, s);
}

// Edge definitions
edge_def ::= id(A) EDGE(E) id(B). {
    create_node(ctx, strdup(A), NULL, NULL);
    create_node(ctx, strdup(B), NULL, NULL);

    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_EDGE;
    s->data.edge.id1 = A;
    s->data.edge.id2 = B;
    s->data.edge.edgeType = E;
    add_statement(ctx, s);
}

edge_def ::= id(A) SHAPE_START(S1) id(L1) SHAPE_END EDGE(E) id(B). {
    create_node(ctx, strdup(A), L1, S1);
    create_node(ctx, strdup(B), NULL, NULL);

    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_EDGE;
    s->data.edge.id1 = A;
    s->data.edge.id2 = B;
    s->data.edge.edgeType = E;
    add_statement(ctx, s);
}

edge_def ::= id(A) EDGE(E) id(B) SHAPE_START(S2) id(L2) SHAPE_END. {
    create_node(ctx, strdup(A), NULL, NULL);
    create_node(ctx, strdup(B), L2, S2);

    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_EDGE;
    s->data.edge.id1 = A;
    s->data.edge.id2 = B;
    s->data.edge.edgeType = E;
    add_statement(ctx, s);
}

edge_def ::= id(A) SHAPE_START(S1) id(L1) SHAPE_END EDGE(E) id(B) SHAPE_START(S2) id(L2) SHAPE_END. {
    create_node(ctx, strdup(A), L1, S1);
    create_node(ctx, strdup(B), L2, S2);

    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_EDGE;
    s->data.edge.id1 = A;
    s->data.edge.id2 = B;
    s->data.edge.edgeType = E;
    add_statement(ctx, s);
}

columns_stmt ::= KW_COLUMNS id(C). {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_COLUMNS;
    if (strcmp(C, "auto") == 0) s->data.columns.count = -1;
    else s->data.columns.count = to_int(C);
    add_statement(ctx, s);
    free(C);
}

columns_stmt ::= KW_COLUMNS AUTO. {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_COLUMNS;
    s->data.columns.count = -1;
    add_statement(ctx, s);
}

space_stmt ::= KW_SPACE. {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_SPACE;
    s->data.space.width = 1;
    add_statement(ctx, s);
}

space_stmt ::= KW_SPACE_NUM(N). {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_SPACE;
    s->data.space.width = to_int(N);
    add_statement(ctx, s);
    free(N);
}

apply_class_stmt ::= KW_CLASS id(N) id(C). {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_APPLYCLASS;
    s->data.applyClass.id = N;
    s->data.applyClass.className = C;
    add_statement(ctx, s);
}

// Style uses id to capture the style definition (lexer captures it as one token)
style_stmt ::= KW_STYLE id(N) id(L). {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_STYLE;
    s->data.style.id = N;
    s->data.style.styles = L;
    add_statement(ctx, s);
}

classdef_stmt ::= KW_CLASSDEF id(N) id(L). {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_CLASSDEF;
    s->data.classDef.id = N;
    s->data.classDef.styles = L;
    add_statement(ctx, s);
}

composite_block ::= block_head statements END. {
    pop_container(ctx);
}

block_head ::= KW_BLOCK id(N) NL. {
    BlockStatement* s = (BlockStatement*)malloc(sizeof(BlockStatement));
    memset(s, 0, sizeof(BlockStatement));
    s->type = BLOCK_STMT_NODE;
    s->data.node.id = N;
    add_statement(ctx, s);
    push_container(ctx, s);
}

id(R) ::= ID(A). { R = A; }
id(R) ::= STRING(A). { R = A; }
id(R) ::= DIR(A). { R = A; }
