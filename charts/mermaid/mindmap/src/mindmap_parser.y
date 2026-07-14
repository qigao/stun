%name MindmapParser
%token_prefix MINDMAP_
%extra_argument { MindmapParserContext *ctx }

%token_type { char* }
%type id { char* }
%type indent { char* }
%type node { MindmapNode* }

%include {
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flex/core/expr_c.h"
#include "mindmap/mindmap_ast.h"

static int parse_indent_val(const char* s) {
    if (!s) return 0;
    int value = 0;
    return flex_expr_eval_i32(s, &value) ? value : 0;
}
}

%syntax_error {
    fprintf(stderr, "Mindmap Syntax error! TokenID: %d\n", yymajor);
    ctx->error_count++;
}

start ::= START(H) statements. { 
    free(H);
}

statements ::= .
statements ::= statements statement.

statement ::= NL.
statement ::= node_stmt.
statement ::= icon_stmt.
statement ::= class_stmt.

// Node statement variations
node_stmt ::= INDENT(I) node(N) NL. {
    N->level = parse_indent_val(I);
    mindmap_add_node(ctx, N);
    free(I);
}
node_stmt ::= node(N) NL. {
    N->level = 0;
    mindmap_add_node(ctx, N);
}

// Helper for text inside shapes
description(D) ::= ID(A). { D = A; }
description(D) ::= STRING(S). { D = S; }

// Basic Node definitions (restored)
node(R) ::= ID(A). {
    R = mindmap_create_node(A, A, MINDMAP_NODE_DEFAULT, 0); 
}
node(R) ::= STRING(S). {
    R = mindmap_create_node(NULL, S, MINDMAP_NODE_DEFAULT, 0);
}

// Shaped nodes with ID
node(R) ::= ID(A) SHAPE_SQUARE_START description(S) SHAPE_SQUARE_END. {
    R = mindmap_create_node(A, S, MINDMAP_NODE_SQUARE, 0);
}
node(R) ::= ID(A) SHAPE_ROUNDED_START description(S) SHAPE_ROUNDED_END. {
    R = mindmap_create_node(A, S, MINDMAP_NODE_ROUNDED, 0);
}
node(R) ::= ID(A) SHAPE_CIRCLE_START description(S) SHAPE_CIRCLE_END. {
    R = mindmap_create_node(A, S, MINDMAP_NODE_CIRCLE, 0);
}
node(R) ::= ID(A) SHAPE_CLOUD_START description(S) SHAPE_CLOUD_END. {
    R = mindmap_create_node(A, S, MINDMAP_NODE_CLOUD, 0);
}
node(R) ::= ID(A) SHAPE_BANG_START description(S) SHAPE_BANG_END. {
    R = mindmap_create_node(A, S, MINDMAP_NODE_BANG, 0);
}
node(R) ::= ID(A) SHAPE_HEXAGON_START description(S) SHAPE_HEXAGON_END. {
    R = mindmap_create_node(A, S, MINDMAP_NODE_HEXAGON, 0);
}

// Shaped nodes without ID (description only)
node(R) ::= SHAPE_SQUARE_START description(S) SHAPE_SQUARE_END. {
    R = mindmap_create_node(NULL, S, MINDMAP_NODE_SQUARE, 0);
}
node(R) ::= SHAPE_ROUNDED_START description(S) SHAPE_ROUNDED_END. {
    R = mindmap_create_node(NULL, S, MINDMAP_NODE_ROUNDED, 0);
}
node(R) ::= SHAPE_CIRCLE_START description(S) SHAPE_CIRCLE_END. {
    R = mindmap_create_node(NULL, S, MINDMAP_NODE_CIRCLE, 0);
}
node(R) ::= SHAPE_CLOUD_START description(S) SHAPE_CLOUD_END. {
    R = mindmap_create_node(NULL, S, MINDMAP_NODE_CLOUD, 0);
}
node(R) ::= SHAPE_BANG_START description(S) SHAPE_BANG_END. {
    R = mindmap_create_node(NULL, S, MINDMAP_NODE_BANG, 0);
}
node(R) ::= SHAPE_HEXAGON_START description(S) SHAPE_HEXAGON_END. {
    R = mindmap_create_node(NULL, S, MINDMAP_NODE_HEXAGON, 0);
}

// Icon statement
icon_stmt ::= INDENT(I) ICON(IC) NL. {
    mindmap_add_icon(ctx, IC);
    free(I); 
    // IC ownership transferred
}
icon_stmt ::= ICON(IC) NL. {
    mindmap_add_icon(ctx, IC);
    // IC ownership transferred
}

// Class statement
class_stmt ::= INDENT(I) CLASS(C) NL. {
    mindmap_add_class(ctx, C);
    free(I); 
    // C ownership transferred
}
class_stmt ::= CLASS(C) NL. {
    mindmap_add_class(ctx, C);
    // C ownership transferred
}
