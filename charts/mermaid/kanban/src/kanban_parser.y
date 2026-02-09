%name KanbanParser
%token_prefix KANBAN_
%token_type {char*}
%default_type {void*}
%type node {KanbanNode*}
%extra_argument { KanbanParserContext *ctx }

%include {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kanban/kanban_ast.h"
#include "kanban_parser_gen.h"

static int to_int(char* s) {
    if(!s) return 0;
    return atoi(s);
}
}

%syntax_error {
    if(!ctx->error_message) {
        char buf[128];
        snprintf(buf, 128, "Syntax error at token type %d", yymajor);
        ctx->error_message = strdup(buf);
    }
    ctx->error_count++;
}

start ::= kanban document.

// kanban rule
kanban ::= KANBAN_KW.

// document structure
document ::= .
document ::= document line.

line ::= statement NL.
line ::= NL.

statement ::= SPACELIST(L) node(N). {
    int level = to_int(L);
    N->level = level;
    kanban_add_node(ctx, N);
    free(L);
}

statement ::= node(N). {
    N->level = 0;
    kanban_add_node(ctx, N);
}

statement ::= SPACELIST(L) ICON(I). {
    kanban_add_icon(ctx, I);
    free(L);
    free(I);
}

statement ::= ICON(I). {
    kanban_add_icon(ctx, I);
    free(I);
}

statement ::= SPACELIST(L) CLASS(C). {
    kanban_add_class(ctx, C);
    free(L);
    free(C);
}

statement ::= CLASS(C). {
    kanban_add_class(ctx, C);
    free(C);
}

statement ::= SPACELIST(L) node(N) SHAPE_DATA(D). {
    int level = to_int(L);
    N->level = level;
    kanban_add_node(ctx, N);
    kanban_add_shape_data(ctx, D);
    free(L);
    free(D);
}

statement ::= node(N) SHAPE_DATA(D). {
    N->level = 0;
    kanban_add_node(ctx, N);
    kanban_add_shape_data(ctx, D);
    free(D);
}

// Node Rules
// node ::= nodeWithId | nodeWithoutId
// nodeWithId ::= NODE_ID
// nodeWithId ::= NODE_ID NODE_DSTART NODE_DESCR NODE_DEND

node(N) ::= NODE_ID(I). {
    N = kanban_create_node(I, I, KANBAN_NODE_DEFAULT, 0); 
    free(I);
}

node(N) ::= NODE_ID(I) NODE_DSTART(S) NODE_DESCR(D) NODE_DEND(E). {
    // Determine shape from S/E?
    // S contains start char e.g. "("
    KanbanNodeShape type = KANBAN_NODE_DEFAULT;
    if(strcmp(S, "(") == 0) type = KANBAN_NODE_ROUNDED;
    else if(strcmp(S, "[") == 0) type = KANBAN_NODE_SQUARE;
    else if(strcmp(S, "((") == 0) type = KANBAN_NODE_CIRCLE;
    else if(strcmp(S, "{{") == 0) type = KANBAN_NODE_HEXAGON;
    else if(strcmp(S, "(-") == 0) type = KANBAN_NODE_CLOUD;
    else if(strcmp(S, "-)") == 0) type = KANBAN_NODE_BANG; // Explosion?
    
    // We ignore E check for simplicity or double check it matches?
    N = kanban_create_node(I, D, type, 0);
    free(I); free(S); free(D); free(E);
}

node(N) ::= NODE_DSTART(S) NODE_DESCR(D) NODE_DEND(E). {
    // Without ID -> ID is D?
    // Grammar says: $$ = { id: $2, descr: $2, type: ... }
    KanbanNodeShape type = KANBAN_NODE_DEFAULT;
    if(strcmp(S, "(") == 0) type = KANBAN_NODE_ROUNDED;
    else if(strcmp(S, "[") == 0) type = KANBAN_NODE_SQUARE;
    else if(strcmp(S, "((") == 0) type = KANBAN_NODE_CIRCLE;
    else if(strcmp(S, "{{") == 0) type = KANBAN_NODE_HEXAGON;
    else if(strcmp(S, "(-") == 0) type = KANBAN_NODE_CLOUD;
    else if(strcmp(S, "-)") == 0) type = KANBAN_NODE_BANG;
    
    N = kanban_create_node(D, D, type, 0);
    free(S); free(D); free(E);
}
