%include {
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "c4/c4_ast.h"
#include "c4_parser_wrapper.h"

// Helper macros/functions
static void free_attribute_list(C4Attribute* list) {
    while(list) {
        C4Attribute* next = list->next;
        if(list->value) free(list->value);
        if(list->key) free(list->key);
        free(list);
        list = next;
    }
}
}

%name C4Parser
%token_prefix C4_
%extra_argument { C4ParserContext *ctx }

%token NL C4_CONTEXT C4_CONTAINER C4_COMPONENT C4_DYNAMIC C4_DEPLOYMENT 
       PERSON_EXT PERSON SYSTEM_EXT_DB SYSTEM_EXT_QUEUE SYSTEM_EXT SYSTEM_DB SYSTEM_QUEUE SYSTEM 
       ENTERPRISE_BOUNDARY SYSTEM_BOUNDARY CONTAINER_BOUNDARY BOUNDARY
       CONTAINER_EXT_DB CONTAINER_EXT_QUEUE CONTAINER_EXT CONTAINER_DB CONTAINER_QUEUE CONTAINER
       COMPONENT_EXT_DB COMPONENT_EXT_QUEUE COMPONENT_EXT COMPONENT_DB COMPONENT_QUEUE COMPONENT
       REL_U REL_D REL_L REL_R REL_B BIREL REL
       UPDATE_EL_STYLE UPDATE_REL_STYLE UPDATE_LAYOUT_CONFIG
       ACC_TITLE ACC_DESCR ACC_VALUE TITLE_VALUE
       LBRACE RBRACE LPAREN RPAREN COMMA STRING STR_UNQUOTED EQUALS
       EOF.

%syntax_error {
    fprintf(stderr, "C4 Syntax error at token %d\n", yymajor);
    ctx->error_count++;
}

%token_type { char* }
%type attribute { char* }
%type attribute_list { C4Attribute* }
%type attributes { C4Attribute* }
%type element { C4Element* }
%type rel { C4Rel* }

// Start
start ::= mermaidDoc EOF.

mermaidDoc ::= graphConfig.

// Diagram Types
graphConfig ::= C4_CONTEXT statements. { ctx->diagram->type = C4_DT_CONTEXT; }
graphConfig ::= C4_CONTAINER statements. { ctx->diagram->type = C4_DT_CONTAINER; }
graphConfig ::= C4_COMPONENT statements. { ctx->diagram->type = C4_DT_COMPONENT; }
graphConfig ::= C4_DYNAMIC statements. { ctx->diagram->type = C4_DT_DYNAMIC; }
graphConfig ::= C4_DEPLOYMENT statements. { ctx->diagram->type = C4_DT_DEPLOYMENT; }

statements ::= .
statements ::= statements statement.

statement ::= NL.
statement ::= element_stmt.
statement ::= rel_stmt.
statement ::= boundary_stmt.
statement ::= update_stmt.
statement ::= title_stmt.

// Elements - Context
element_stmt ::= PERSON attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_PERSON, A)); }
element_stmt ::= PERSON_EXT attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_PERSON_EXT, A)); }
element_stmt ::= SYSTEM attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_SYSTEM, A)); }
element_stmt ::= SYSTEM_EXT attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_SYSTEM_EXT, A)); }
element_stmt ::= SYSTEM_DB attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_SYSTEM_DB, A)); }
element_stmt ::= SYSTEM_QUEUE attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_SYSTEM_QUEUE, A)); }
element_stmt ::= SYSTEM_EXT_DB attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_SYSTEM_EXT_DB, A)); }
element_stmt ::= SYSTEM_EXT_QUEUE attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_SYSTEM_EXT_QUEUE, A)); }

// Elements - Container
element_stmt ::= CONTAINER attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_CONTAINER, A)); }
element_stmt ::= CONTAINER_DB attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_CONTAINER_DB, A)); }
element_stmt ::= CONTAINER_QUEUE attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_CONTAINER_QUEUE, A)); }
element_stmt ::= CONTAINER_EXT attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_CONTAINER_EXT, A)); }
element_stmt ::= CONTAINER_EXT_DB attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_CONTAINER_EXT_DB, A)); }
element_stmt ::= CONTAINER_EXT_QUEUE attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_CONTAINER_EXT_QUEUE, A)); }

// Elements - Component
element_stmt ::= COMPONENT attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_COMPONENT, A)); }
element_stmt ::= COMPONENT_DB attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_COMPONENT_DB, A)); }
element_stmt ::= COMPONENT_QUEUE attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_COMPONENT_QUEUE, A)); }
element_stmt ::= COMPONENT_EXT attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_COMPONENT_EXT, A)); }
element_stmt ::= COMPONENT_EXT_DB attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_COMPONENT_EXT_DB, A)); }
element_stmt ::= COMPONENT_EXT_QUEUE attributes(A). { c4_add_element(ctx, c4_create_element(C4_EL_COMPONENT_EXT_QUEUE, A)); }

// Relationships
rel_stmt ::= REL attributes(A). { c4_add_rel(ctx, c4_create_rel(C4_RT_REL, A)); }
rel_stmt ::= BIREL attributes(A). { c4_add_rel(ctx, c4_create_rel(C4_RT_BIREL, A)); }
rel_stmt ::= REL_U attributes(A). { c4_add_rel(ctx, c4_create_rel(C4_RT_REL_U, A)); }
rel_stmt ::= REL_D attributes(A). { c4_add_rel(ctx, c4_create_rel(C4_RT_REL_D, A)); }
rel_stmt ::= REL_L attributes(A). { c4_add_rel(ctx, c4_create_rel(C4_RT_REL_L, A)); }
rel_stmt ::= REL_R attributes(A). { c4_add_rel(ctx, c4_create_rel(C4_RT_REL_R, A)); }
rel_stmt ::= REL_B attributes(A). { c4_add_rel(ctx, c4_create_rel(C4_RT_REL_B, A)); }

// Boundaries
boundary_stmt ::= boundary_head statements RBRACE. {
    // Pop boundary context
    ctx->current_boundary = ctx->current_boundary ? ctx->current_boundary->parent : NULL;
}

boundary_head ::= ENTERPRISE_BOUNDARY attributes(A) LBRACE. {
    C4Element* el = c4_create_element(C4_EL_ENTERPRISE_BOUNDARY, A);
    c4_add_element(ctx, el);
    // Push boundary context
    el->parent = ctx->current_boundary;
    ctx->current_boundary = el;
}
boundary_head ::= SYSTEM_BOUNDARY attributes(A) LBRACE. {
    C4Element* el = c4_create_element(C4_EL_SYSTEM_BOUNDARY, A);
    c4_add_element(ctx, el);
    el->parent = ctx->current_boundary;
    ctx->current_boundary = el;
}
boundary_head ::= CONTAINER_BOUNDARY attributes(A) LBRACE. {
    C4Element* el = c4_create_element(C4_EL_CONTAINER_BOUNDARY, A);
    c4_add_element(ctx, el);
    el->parent = ctx->current_boundary;
    ctx->current_boundary = el;
}
boundary_head ::= BOUNDARY attributes(A) LBRACE. {
    C4Element* el = c4_create_element(C4_EL_BOUNDARY, A);
    c4_add_element(ctx, el);
    el->parent = ctx->current_boundary;
    ctx->current_boundary = el;
}

// Attributes Parsing
attributes(L) ::= LPAREN attribute_list(A) RPAREN. { L = A; }
attributes(L) ::= LPAREN RPAREN. { L = NULL; }

attribute_list(L) ::= attribute(A). {
    L = (C4Attribute*)calloc(1, sizeof(C4Attribute));
    L->value = A;
}
attribute_list(L) ::= attribute(A) COMMA attribute_list(N). {
    L = (C4Attribute*)calloc(1, sizeof(C4Attribute));
    L->value = A;
    L->next = N;
}

attribute(V) ::= STRING(S). { V = S; }
attribute(V) ::= STR_UNQUOTED(S). { V = S; }

// Title and Acc parts
title_stmt ::= TITLE_VALUE(T). {
    if(ctx->diagram->title) free(ctx->diagram->title);
    ctx->diagram->title = T;
}

statement ::= acc_title_stmt.
statement ::= acc_descr_stmt.

acc_title_stmt ::= ACC_TITLE ACC_VALUE(V). { 
    if(ctx->diagram->title) free(ctx->diagram->title);
    ctx->diagram->title = V;
}
acc_descr_stmt ::= ACC_DESCR ACC_VALUE(V). { 
    free(V);
}

// Attributes Key-Value
attribute(V) ::= STR_UNQUOTED(K) EQUALS STRING(val). { 
    V = val; 
    free(K);
}

// Updates
update_stmt ::= UPDATE_EL_STYLE attributes(A). { free_attribute_list(A); }
update_stmt ::= UPDATE_REL_STYLE attributes(A). { free_attribute_list(A); }
update_stmt ::= UPDATE_LAYOUT_CONFIG attributes(A). { free_attribute_list(A); }
