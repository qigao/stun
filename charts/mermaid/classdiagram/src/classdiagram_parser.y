%name ClassParser
%token_prefix CLASS_
%extra_argument { ClassParserContext *ctx }

%token NL START KW TITLE_KW DIRECTION NAMESPACE NOTE FOR END
       REL_INHERIT REL_REALIZE REL_COMP REL_AGGR REL_ASSOC REL_DEP REL_LINK_TOKEN
       REL_INHERIT_REV REL_REALIZE_REV REL_COMP_REV REL_AGGR_REV REL_ASSOC_REV REL_DEP_REV
       PLUS MINUS HASH TILDE COLON LBRACE RBRACE LPAREN RPAREN STRING ANNOTATION IDENTIFIER EOF.

%token_type { char* }
%type rel_type { int }
%type visibility { int }

%include {
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "classdiagram/classdiagram_ast.h"

static ClassNode* find_class(ClassParserContext *ctx, const char* name) {
    ClassNode* curr = ctx->diagram->classes;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static ClassNode* ensure_class(ClassParserContext *ctx, const char* name) {
    if (!name) return NULL;
    ClassNode* existing = find_class(ctx, name);
    if (existing) return existing;
    ClassNode* n = (ClassNode*)malloc(sizeof(ClassNode));
    memset(n, 0, sizeof(ClassNode));
    n->name = strdup(name);
    if (!ctx->diagram->classes) {
        ctx->diagram->classes = n;
    } else {
        ClassNode* curr = ctx->diagram->classes;
        while (curr->next) curr = curr->next;
        curr->next = n;
    }
    return n;
}

static void add_member(ClassNode* node, ClassMember* m) {
    if (!node || !m) return;
    if (!node->members) node->members = m;
    else {
        ClassMember* curr = node->members;
        while (curr->next) curr = curr->next;
        curr->next = m;
    }
}

static void add_relationship(ClassParserContext *ctx, ClassRelationship* r) {
    if (!ctx || !r) return;
    if (!ctx->diagram->relationships) ctx->diagram->relationships = r;
    else {
        ClassRelationship* curr = ctx->diagram->relationships;
        while (curr->next) curr = curr->next;
        curr->next = r;
    }
}
}

%syntax_error {
    fprintf(stderr, "ClassDiagram Syntax error: token type %d\n", yymajor);
    ctx->error_count++;
}

start ::= START statements.

statements ::= .
statements ::= statements line.

line ::= NL.
line ::= EOF.
line ::= stmt NL.
line ::= stmt EOF.
line ::= class_block_start.
line ::= class_block_end NL.
line ::= class_block_end EOF.

stmt ::= direction_stmt.
stmt ::= title_stmt.
stmt ::= class_decl.
stmt ::= rel_stmt.
stmt ::= member_stmt.
stmt ::= annotation_stmt.
stmt ::= note_stmt.
stmt ::= namespace_start.
stmt ::= namespace_end.

direction_stmt ::= DIRECTION IDENTIFIER(D). { free(D); }

title_stmt ::= TITLE_KW id_val(T). { ctx->diagram->title = T; }

// class ClassName
class_decl ::= KW id_val(N). { ctx->current_class = ensure_class(ctx, N); free(N); }
// class ClassName~Generic~
class_decl ::= KW id_val(N) TILDE id_val(G) TILDE. { ctx->current_class = ensure_class(ctx, N); free(N); free(G); }

// class ClassName { - starts a block
class_block_start ::= KW id_val(N) LBRACE NL. { ctx->current_class = ensure_class(ctx, N); free(N); }
class_block_start ::= KW id_val(N) LBRACE EOF. { ctx->current_class = ensure_class(ctx, N); free(N); }
// class ClassName~Generic~ { - generic class with block
class_block_start ::= KW id_val(N) TILDE id_val(G) TILDE LBRACE NL. { ctx->current_class = ensure_class(ctx, N); free(N); free(G); }
class_block_start ::= KW id_val(N) TILDE id_val(G) TILDE LBRACE EOF. { ctx->current_class = ensure_class(ctx, N); free(N); free(G); }
// class ClassName~K,V~ { - multiple generic params
class_block_start ::= KW id_val(N) TILDE id_val(G1) id_val(G2) TILDE LBRACE NL. { ctx->current_class = ensure_class(ctx, N); free(N); free(G1); free(G2); }
class_block_start ::= KW id_val(N) TILDE id_val(G1) id_val(G2) TILDE LBRACE EOF. { ctx->current_class = ensure_class(ctx, N); free(N); free(G1); free(G2); }

// } - ends a class block
class_block_end ::= RBRACE. { ctx->current_class = NULL; }

// Namespace - just skip the tokens
namespace_start ::= NAMESPACE id_val(N) LBRACE. { free(N); }
namespace_end ::= END. { }

// Relationships: A rel B or A rel B : label
rel_stmt ::= id_val(A) rel_type(T) id_val(B). {
    ClassRelationship* r = (ClassRelationship*)calloc(1, sizeof(ClassRelationship));
    r->from = A; r->to = B; r->type = (ClassRelationshipType)T;
    ensure_class(ctx, A); ensure_class(ctx, B); add_relationship(ctx, r);
}
rel_stmt ::= id_val(A) rel_type(T) id_val(B) COLON id_val(L). {
    ClassRelationship* r = (ClassRelationship*)calloc(1, sizeof(ClassRelationship));
    r->from = A; r->to = B; r->type = (ClassRelationshipType)T; r->label = L;
    ensure_class(ctx, A); ensure_class(ctx, B); add_relationship(ctx, r);
}
// With cardinality: A "1" rel "*" B
rel_stmt ::= id_val(A) STRING(C1) rel_type(T) STRING(C2) id_val(B). {
    ClassRelationship* r = (ClassRelationship*)calloc(1, sizeof(ClassRelationship));
    r->from = A; r->to = B; r->from_cardinality = C1; r->to_cardinality = C2;
    r->type = (ClassRelationshipType)T;
    ensure_class(ctx, A); ensure_class(ctx, B); add_relationship(ctx, r);
}
rel_stmt ::= id_val(A) STRING(C1) rel_type(T) STRING(C2) id_val(B) COLON id_val(L). {
    ClassRelationship* r = (ClassRelationship*)calloc(1, sizeof(ClassRelationship));
    r->from = A; r->to = B; r->from_cardinality = C1; r->to_cardinality = C2;
    r->type = (ClassRelationshipType)T; r->label = L;
    ensure_class(ctx, A); ensure_class(ctx, B); add_relationship(ctx, r);
}

// Member definition: ClassName : +memberName Type
member_stmt ::= id_val(C) COLON visibility(V) id_val(N). {
    ClassNode* cls = ensure_class(ctx, C);
    ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
    m->name = N; m->visibility = (ClassVisibility)V;
    add_member(cls, m); free(C);
}
member_stmt ::= id_val(C) COLON visibility(V) id_val(N) id_val(T). {
    ClassNode* cls = ensure_class(ctx, C);
    ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
    m->name = N; m->type = T; m->visibility = (ClassVisibility)V;
    add_member(cls, m); free(C);
}
member_stmt ::= id_val(C) COLON visibility(V) id_val(N) LPAREN RPAREN. {
    ClassNode* cls = ensure_class(ctx, C);
    ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
    m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
    add_member(cls, m); free(C);
}
member_stmt ::= id_val(C) COLON visibility(V) id_val(N) LPAREN RPAREN id_val(T). {
    ClassNode* cls = ensure_class(ctx, C);
    ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
    m->name = N; m->return_type = T; m->visibility = (ClassVisibility)V; m->is_method = 1;
    add_member(cls, m); free(C);
}

// Inside class block: +Type name or +name() or just +name
member_stmt ::= visibility(V) id_val(N). {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V;
        add_member(ctx->current_class, m);
    } else { free(N); }
}
member_stmt ::= visibility(V) id_val(T) id_val(N). {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->type = T; m->visibility = (ClassVisibility)V;
        add_member(ctx->current_class, m);
    } else { free(T); free(N); }
}
member_stmt ::= visibility(V) id_val(N) LPAREN RPAREN. {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
        add_member(ctx->current_class, m);
    } else { free(N); }
}
member_stmt ::= visibility(V) id_val(N) LPAREN id_val(P) RPAREN. {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
        add_member(ctx->current_class, m);
    } else { free(N); }
    free(P);
}
// Method with typed parameter: +move(int distance)
member_stmt ::= visibility(V) id_val(N) LPAREN id_val(PT) id_val(PN) RPAREN. {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
        add_member(ctx->current_class, m);
    } else { free(N); }
    free(PT); free(PN);
}
// Method with two typed parameters: +put(K key, V value)
member_stmt ::= visibility(V) id_val(N) LPAREN id_val(PT1) id_val(PN1) id_val(PT2) id_val(PN2) RPAREN. {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
        add_member(ctx->current_class, m);
    } else { free(N); }
    free(PT1); free(PN1); free(PT2); free(PN2);
}
// Method with return type: +get(int index) T
member_stmt ::= visibility(V) id_val(N) LPAREN RPAREN id_val(RT). {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
        m->return_type = RT;
        add_member(ctx->current_class, m);
    } else { free(N); free(RT); }
}
member_stmt ::= visibility(V) id_val(N) LPAREN id_val(P) RPAREN id_val(RT). {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
        m->return_type = RT;
        add_member(ctx->current_class, m);
    } else { free(N); free(RT); }
    free(P);
}
member_stmt ::= visibility(V) id_val(N) LPAREN id_val(PT) id_val(PN) RPAREN id_val(RT). {
    if (ctx->current_class) {
        ClassMember* m = (ClassMember*)calloc(1, sizeof(ClassMember));
        m->name = N; m->visibility = (ClassVisibility)V; m->is_method = 1;
        m->return_type = RT;
        add_member(ctx->current_class, m);
    } else { free(N); free(RT); }
    free(PT); free(PN);
}

// Annotation inside class block
annotation_stmt ::= ANNOTATION(A). {
    if (ctx->current_class) ctx->current_class->annotation = A;
    else free(A);
}
// Annotation on class: ClassName <<annotation>>
annotation_stmt ::= id_val(C) ANNOTATION(A). {
    ClassNode* cls = ensure_class(ctx, C);
    if (cls) cls->annotation = A;
    free(C);
}

// Note statement
note_stmt ::= NOTE STRING(T). { free(T); }
note_stmt ::= NOTE FOR id_val(C) STRING(T). { free(C); free(T); }

rel_type(R) ::= REL_INHERIT. { R = CLASS_REL_INHERITANCE; }
rel_type(R) ::= REL_REALIZE. { R = CLASS_REL_REALIZATION; }
rel_type(R) ::= REL_COMP. { R = CLASS_REL_COMPOSITION; }
rel_type(R) ::= REL_AGGR. { R = CLASS_REL_AGGREGATION; }
rel_type(R) ::= REL_ASSOC. { R = CLASS_REL_ASSOCIATION; }
rel_type(R) ::= REL_DEP. { R = CLASS_REL_DEPENDENCY; }
rel_type(R) ::= REL_LINK_TOKEN. { R = CLASS_REL_LINK; }
rel_type(R) ::= REL_INHERIT_REV. { R = CLASS_REL_INHERITANCE; }
rel_type(R) ::= REL_REALIZE_REV. { R = CLASS_REL_REALIZATION; }
rel_type(R) ::= REL_COMP_REV. { R = CLASS_REL_COMPOSITION; }
rel_type(R) ::= REL_AGGR_REV. { R = CLASS_REL_AGGREGATION; }
rel_type(R) ::= REL_ASSOC_REV. { R = CLASS_REL_ASSOCIATION; }
rel_type(R) ::= REL_DEP_REV. { R = CLASS_REL_DEPENDENCY; }

visibility(R) ::= PLUS. { R = CLASS_VISIBILITY_PUBLIC; }
visibility(R) ::= MINUS. { R = CLASS_VISIBILITY_PRIVATE; }
visibility(R) ::= HASH. { R = CLASS_VISIBILITY_PROTECTED; }
visibility(R) ::= TILDE. { R = CLASS_VISIBILITY_INTERNAL; }

id_val(R) ::= IDENTIFIER(A). { R = A; }
id_val(R) ::= STRING(A). { R = A; }
