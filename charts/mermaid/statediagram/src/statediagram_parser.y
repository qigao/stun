%name StateParser
%token_prefix STATE_
%extra_argument { StateParserContext *ctx }

%token_type { char* }
%token TXT DIRECTION.
%type id { char* }
%type label_text { char* }

%include {
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "statediagram/statediagram_ast.h"

// Helper to add nodes/transitions to current doc
static void add_node(StateParserContext *ctx, StateNode* n) {
    if (!ctx->current_doc->nodes) {
        ctx->current_doc->nodes = n;
    } else {
        StateNode* curr = ctx->current_doc->nodes;
        while (curr->next) curr = curr->next;
        curr->next = n;
    }
}

static void add_transition(StateParserContext *ctx, StateTransition* t) {
    if (!ctx->current_doc->transitions) {
        ctx->current_doc->transitions = t;
    } else {
        StateTransition* curr = ctx->current_doc->transitions;
        while (curr->next) curr = curr->next;
        curr->next = t;
    }
}

static StateNode* ensure_node(StateParserContext *ctx, const char* id) {
    // Search in current doc
    StateNode* curr = ctx->current_doc->nodes;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    // Create new
    StateNode* n = (StateNode*)malloc(sizeof(StateNode));
    memset(n, 0, sizeof(StateNode));
    n->id = strdup(id);
    n->type = STATE_TYPE_DEFAULT;
    add_node(ctx, n);
    return n;
}

// Stack management for nested docs
typedef struct DocStackItem {
    StateDoc* doc;
    struct DocStackItem* next;
} DocStackItem;

static void push_doc(StateParserContext *ctx) {
    DocStackItem* item = (DocStackItem*)malloc(sizeof(DocStackItem));
    item->doc = ctx->current_doc;
    item->next = (DocStackItem*)ctx->doc_stack;
    ctx->doc_stack = item;
    
    // Create new doc
    StateDoc* new_doc = (StateDoc*)malloc(sizeof(StateDoc));
    memset(new_doc, 0, sizeof(StateDoc));
    ctx->current_doc = new_doc;
}

static StateDoc* pop_doc(StateParserContext *ctx) {
    StateDoc* finished_doc = ctx->current_doc;
    DocStackItem* item = (DocStackItem*)ctx->doc_stack;
    if (item) {
        ctx->current_doc = item->doc;
        ctx->doc_stack = item->next;
        free(item);
    }
    return finished_doc;
}

}

%syntax_error {
    ctx->error_count++;
}

start ::= SD statements.

statements ::= .
statements ::= statements statement.

statement ::= NL.
statement ::= decl_statement.
statement ::= rel_statement.
statement ::= composite_statement.
statement ::= note_statement.
statement ::= direction_statement.
statement ::= hide_empty_statement.
statement ::= CONCURRENT NL.

direction_statement ::= DIRECTION id(D) NL. { ctx->diagram->direction = D; }

decl_statement ::= id(I) NL. { ensure_node(ctx, I); free(I); }
decl_statement ::= id(I) COLON label_text(S) NL. {
    StateNode* n = ensure_node(ctx, I);
    n->description = S;
    free(I);
}

// Descriptions like: state "Description" as ID
decl_statement ::= KW label_text(D) AS id(I) NL. {
    StateNode* n = ensure_node(ctx, I);
    n->description = D;
    free(I);
}

decl_statement ::= KW id(I) CHOICE NL. {
    StateNode* n = ensure_node(ctx, I);
    n->type = STATE_TYPE_CHOICE;
    free(I);
}

decl_statement ::= KW id(I) FORK NL. {
    StateNode* n = ensure_node(ctx, I);
    n->type = STATE_TYPE_FORK;
    free(I);
}

decl_statement ::= KW id(I) JOIN NL. {
    StateNode* n = ensure_node(ctx, I);
    n->type = STATE_TYPE_JOIN;
    free(I);
}

// Composite state
composite_statement ::= KW id(I) LBRACE push_action statements RBRACE NL. {
    StateDoc* doc = pop_doc(ctx);
    StateNode* n = ensure_node(ctx, I);
    n->type = STATE_TYPE_COMPOSITE;
    n->doc = doc;
    free(I);
}

// Composite with "state ... as ..." syntax
composite_statement ::= KW label_text(D) AS id(I) LBRACE push_action statements RBRACE NL. {
    StateDoc* doc = pop_doc(ctx);
    StateNode* n = ensure_node(ctx, I);
    n->description = D;
    n->type = STATE_TYPE_COMPOSITE;
    n->doc = doc;
    free(I);
}

push_action ::= . { push_doc(ctx); }

// Relationships
rel_statement ::= id(A) ARROW id(B) NL. {
    StateTransition* t = (StateTransition*)malloc(sizeof(StateTransition));
    memset(t, 0, sizeof(StateTransition));
    t->id1 = A;
    t->id2 = B;
    add_transition(ctx, t);
    ensure_node(ctx, A);
    ensure_node(ctx, B);
}

rel_statement ::= id(A) ARROW id(B) COLON label_text(D) NL. {
    StateTransition* t = (StateTransition*)malloc(sizeof(StateTransition));
    memset(t, 0, sizeof(StateTransition));
    t->id1 = A;
    t->id2 = B;
    t->description = D;
    add_transition(ctx, t);
    ensure_node(ctx, A);
    ensure_node(ctx, B);
}

note_statement ::= NOTE LEFT_OF id(I) COLON label_text(T) NL. { free(I); if(T) free(T); }
note_statement ::= NOTE RIGHT_OF id(I) COLON label_text(T) NL. { free(I); if(T) free(T); }
note_statement ::= NOTE LEFT_OF id(I) NL. { free(I); }
note_statement ::= NOTE RIGHT_OF id(I) NL. { free(I); }

label_text(R) ::= STRING_LITERAL(A). { R = A; }
label_text(R) ::= TXT(A). { R = A; }
label_text(R) ::= . { R = NULL; }

hide_empty_statement ::= HIDE_EMPTY NL.

id(R) ::= ID(A). { R = A; }
id(R) ::= START_END(A). { R = A; }
id(R) ::= FORK. { R = strdup("fork_node"); } // Handle fork IDs properly?
id(R) ::= JOIN. { R = strdup("join_node"); }
id(R) ::= CHOICE. { R = strdup("choice_node"); }
