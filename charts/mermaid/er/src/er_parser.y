%name ERParser
%token_prefix ER_
%extra_argument { ERParserContext *ctx }

%token NL START LBRACE RBRACE COLON ZERO_OR_ONE ZERO_OR_MORE ONE_OR_MORE ONLY_ONE 
       REL_IDENTIFYING REL_NON_IDENTIFYING PK FK UK STRING ID EOF.

%token_type { char* }
%type id { char* }
%type cardinality { int }
%type rel_type { int }
%type attribute { ERAttribute* }
%type attributes { ERAttribute* }
%type opt_keys { char* }
%type opt_comment { char* }

%include {
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "er/er_ast.h"

static EREntity* ensure_entity(ERParserContext* ctx, const char* name) {
    EREntity* curr = ctx->diagram->entities;
    while(curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    EREntity* n = (EREntity*)malloc(sizeof(EREntity));
    memset(n, 0, sizeof(EREntity));
    n->name = strdup(name);
    if (!ctx->diagram->entities) ctx->diagram->entities = n;
    else {
        EREntity* last = ctx->diagram->entities;
        while(last->next) last = last->next;
        last->next = n;
    }
    return n;
}

static void add_relationship(ERParserContext* ctx, const char* e1, const char* e2, int c1, int c2, int type, const char* label) {
    ERRelationship* r = (ERRelationship*)malloc(sizeof(ERRelationship));
    memset(r, 0, sizeof(ERRelationship));
    r->entity1 = strdup(e1);
    r->entity2 = strdup(e2);
    r->role = label ? strdup(label) : NULL;
    r->card1 = (ERCardinality)c1;
    r->card2 = (ERCardinality)c2;
    r->type = (ERRelType)type;
    if (!ctx->diagram->relationships) ctx->diagram->relationships = r;
    else {
        ERRelationship* last = ctx->diagram->relationships;
        while(last->next) last = last->next;
        last->next = r;
    }
}
}

%syntax_error {
    ctx->error_count++;
}

start ::= START statements EOF.

statements ::= .
statements ::= statements statement.

statement ::= NL.
statement ::= relationship_def.
statement ::= block_def.
statement ::= entity_def.

entity_def ::= id(N) NL. { ensure_entity(ctx, N); free(N); }
entity_def ::= id(N) EOF. { ensure_entity(ctx, N); free(N); }

block_def ::= id(N) LBRACE attributes(L) RBRACE. {
    EREntity* e = ensure_entity(ctx, N); e->attributes = L; free(N);
}

relationship_def ::= id(A) cardinality(C1) rel_type(T) cardinality(C2) id(B). {
    ensure_entity(ctx, A); ensure_entity(ctx, B);
    add_relationship(ctx, A, B, C1, C2, T, NULL);
    free(A); free(B);
}
relationship_def ::= id(A) cardinality(C1) rel_type(T) cardinality(C2) id(B) COLON id(L). {
    ensure_entity(ctx, A); ensure_entity(ctx, B);
    add_relationship(ctx, A, B, C1, C2, T, L);
    free(A); free(B); free(L);
}

attributes(R) ::= . { R = NULL; }
attributes(R) ::= attributes(L) attribute(A). {
    if (!L) { R = A; }
    else {
        ERAttribute* last = L;
        while(last->next) last = last->next;
        last->next = A;
        R = L;
    }
}
attributes(R) ::= attributes(L) NL. { R = L; }

// Conflict resolved: The first field of an attribute (the type) must be an ID, not a STRING.
// This distinguishes it from an optional preceding comment which is a STRING.
attribute(R) ::= ID(T) id(N) opt_keys(K) opt_comment(C). {
    ERAttribute* a = (ERAttribute*)malloc(sizeof(ERAttribute));
    memset(a, 0, sizeof(ERAttribute));
    a->type = T; a->name = N; a->keys = K; a->comment = C; R = a;
}

opt_keys(R) ::= . { R = NULL; }
opt_keys(R) ::= PK. { R = strdup("PK"); }
opt_keys(R) ::= FK. { R = strdup("FK"); }
opt_keys(R) ::= UK. { R = strdup("UK"); }

opt_comment(R) ::= . { R = NULL; }
opt_comment(R) ::= STRING(C). { R = C; }

cardinality(R) ::= ZERO_OR_ONE. { R = ER_CARDINALITY_ZERO_OR_ONE; }
cardinality(R) ::= ZERO_OR_MORE. { R = ER_CARDINALITY_ZERO_OR_MORE; }
cardinality(R) ::= ONE_OR_MORE. { R = ER_CARDINALITY_ONE_OR_MORE; }
cardinality(R) ::= ONLY_ONE. { R = ER_CARDINALITY_ONLY_ONE; }

rel_type(R) ::= REL_IDENTIFYING. { R = ER_REL_IDENTIFYING; }
rel_type(R) ::= REL_NON_IDENTIFYING. { R = ER_REL_NON_IDENTIFYING; }

id(R) ::= ID(A). { R = A; }
id(R) ::= STRING(A). { R = A; }
