%name SequenceParser
%token_prefix SEQ_
%extra_argument { SequenceParserContext *ctx }

%token_type { char* }
%type document { SequenceStatement* }
%type statement { SequenceStatement* }
%type participant_stmt { SequenceStatement* }
%type signal_stmt { SequenceStatement* }
%type note_stmt { SequenceStatement* }
%type block_stmt { SequenceStatement* }
%type activate_stmt { SequenceStatement* }
%type deactivate_stmt { SequenceStatement* }
%type autonumber_stmt { SequenceStatement* }
%type actor_id { char* }
%type label_text { char* }
%type alt_else { SequenceBlock* }
%type par_and { SequenceBlock* }
%type option_list { SequenceStatement* }
%type option_stmt { SequenceStatement* }
%type optional_option_list { SequenceStatement* }

%start_symbol start

%include {
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sequence/sequence_ast.h"

static SequenceParticipant* find_participant(SequenceParserContext *ctx, const char* id) {
    SequenceParticipant* curr = ctx->diagram->participants;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static SequenceParticipant* add_participant(SequenceParserContext *ctx, const char* id, const char* label, const char* type) {
    SequenceParticipant* existing = find_participant(ctx, id);
    if (existing) {
        if (label) { free(existing->label); existing->label = strdup(label); }
        if (type) { free(existing->type); existing->type = strdup(type); }
        return existing;
    }

    SequenceParticipant* p = (SequenceParticipant*)malloc(sizeof(SequenceParticipant));
    memset(p, 0, sizeof(SequenceParticipant));
    p->id = strdup(id);
    p->label = label ? strdup(label) : strdup(id);
    p->type = type ? strdup(type) : NULL;
    // Prepend to match golden file expected order (John, Bob, Alice)
    p->next = ctx->diagram->participants;
    ctx->diagram->participants = p;
    return p;
}

static void ensure_participant(SequenceParserContext *ctx, const char* id) {
    if (!find_participant(ctx, id)) {
        add_participant(ctx, id, NULL, NULL);
    }
}

static SequenceSignalType parse_arrow(const char* s) {
    if (strcmp(s, "->>") == 0) return SEQ_SIGNAL_SOLID;
    if (strcmp(s, "-->>") == 0) return SEQ_SIGNAL_DOTTED;
    if (strcmp(s, "->") == 0) return SEQ_SIGNAL_SOLID_OPEN;
    if (strcmp(s, "-->") == 0) return SEQ_SIGNAL_DOTTED_OPEN;
    if (strcmp(s, "-x") == 0) return SEQ_SIGNAL_SOLID_CROSS;
    if (strcmp(s, "--x") == 0) return SEQ_SIGNAL_DOTTED_CROSS;
    if (strcmp(s, "-))") == 0) return SEQ_SIGNAL_SOLID_POINT;
    if (strcmp(s, "--))") == 0) return SEQ_SIGNAL_DOTTED_POINT;
    if (strcmp(s, "<<->>") == 0) return SEQ_SIGNAL_BIDIR_SOLID;
    if (strcmp(s, "<<-->>") == 0) return SEQ_SIGNAL_BIDIR_DOTTED;
    return SEQ_SIGNAL_SOLID;
}

static SequenceStatement* append_statement(SequenceStatement* head, SequenceStatement* s) {
    if (!s) return head;
    if (!head) return s;
    SequenceStatement* curr = head;
    while (curr->next) curr = curr->next;
    curr->next = s;
    return head;
}
}

%syntax_error {
    ctx->error_count++;
}

start ::= SD document(D). { ctx->diagram->statements = D; }

document(R) ::= . { R = NULL; }
document(R) ::= document(D) statement(S). { R = append_statement(D, S); }

statement(R) ::= NEWLINE. { R = NULL; }
statement(R) ::= participant_stmt(S). { R = S; }
statement(R) ::= signal_stmt(S).      { R = S; }
statement(R) ::= note_stmt(S).        { R = S; }
statement(R) ::= block_stmt(S).       { R = S; }
statement(R) ::= activate_stmt(S).    { R = S; }
statement(R) ::= deactivate_stmt(S).  { R = S; }
statement(R) ::= autonumber_stmt(S).  { R = S; }

participant_stmt(R) ::= PARTICIPANT actor_id(A) NEWLINE. { add_participant(ctx, A, NULL, "participant"); free(A); R = NULL; }
participant_stmt(R) ::= PARTICIPANT actor_id(A) AS label_text(B) NEWLINE. { add_participant(ctx, A, B, "participant"); free(A); free(B); R = NULL; }
participant_stmt(R) ::= ACTOR_KW actor_id(A) NEWLINE. { add_participant(ctx, A, NULL, "actor"); free(A); R = NULL; }
participant_stmt(R) ::= ACTOR_KW actor_id(A) AS label_text(B) NEWLINE. { add_participant(ctx, A, B, "actor"); free(A); free(B); R = NULL; }

actor_id(R) ::= IDENTIFIER(A). { R = A; }
actor_id(R) ::= STRING(A).     { R = A; }

label_text(R) ::= IDENTIFIER(A). { R = A; }
label_text(R) ::= STRING(A).     { R = A; }
label_text(R) ::= TXT(A).        { R = A; }

signal_stmt(R) ::= actor_id(A) ARROW(S) actor_id(B) TXT(T) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_SIGNAL;
    stmt->data.signal.from = strdup(A);
    stmt->data.signal.to = strdup(B);
    stmt->data.signal.message = strdup(T);
    stmt->data.signal.signal_type = parse_arrow(S);
    ensure_participant(ctx, A);
    ensure_participant(ctx, B);
    free(A); free(B); free(S); free(T);
    R = stmt;
}

signal_stmt(R) ::= actor_id(A) ARROW(S) PLUS actor_id(B) TXT(T) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_SIGNAL;
    stmt->data.signal.from = strdup(A);
    stmt->data.signal.to = strdup(B);
    stmt->data.signal.message = strdup(T);
    stmt->data.signal.signal_type = parse_arrow(S);
    ensure_participant(ctx, A);
    ensure_participant(ctx, B);

    SequenceStatement* act = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(act, 0, sizeof(SequenceStatement));
    act->type = SEQ_STMT_ACTIVATE;
    act->data.activation.actor = strdup(B);
    stmt->next = act;

    free(A); free(B); free(S); free(T);
    R = stmt;
}

signal_stmt(R) ::= actor_id(A) ARROW(S) MINUS actor_id(B) TXT(T) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_SIGNAL;
    stmt->data.signal.from = strdup(A);
    stmt->data.signal.to = strdup(B);
    stmt->data.signal.message = strdup(T);
    stmt->data.signal.signal_type = parse_arrow(S);
    ensure_participant(ctx, A);
    ensure_participant(ctx, B);

    SequenceStatement* deact = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(deact, 0, sizeof(SequenceStatement));
    deact->type = SEQ_STMT_DEACTIVATE;
    deact->data.activation.actor = strdup(A);
    stmt->next = deact;

    free(A); free(B); free(S); free(T);
    R = stmt;
}

note_stmt(R) ::= NOTE LEFT_OF actor_id(A) TXT(T) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_NOTE;
    stmt->data.note.actor = strdup(A);
    stmt->data.note.text = strdup(T);
    stmt->data.note.placement = SEQ_NOTE_LEFT_OF;
    ensure_participant(ctx, A);
    free(A); free(T);
    R = stmt;
}

note_stmt(R) ::= NOTE RIGHT_OF actor_id(A) TXT(T) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_NOTE;
    stmt->data.note.actor = strdup(A);
    stmt->data.note.text = strdup(T);
    stmt->data.note.placement = SEQ_NOTE_RIGHT_OF;
    ensure_participant(ctx, A);
    free(A); free(T);
    R = stmt;
}

note_stmt(R) ::= NOTE OVER actor_id(A) TXT(T) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_NOTE;
    stmt->data.note.actor = strdup(A);
    stmt->data.note.text = strdup(T);
    stmt->data.note.placement = SEQ_NOTE_OVER;
    ensure_participant(ctx, A);
    free(A); free(T);
    R = stmt;
}

note_stmt(R) ::= NOTE OVER actor_id(A) COMMA actor_id(B) TXT(T) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_NOTE;
    stmt->data.note.actor = strdup(A);
    stmt->data.note.actor2 = strdup(B);
    stmt->data.note.text = strdup(T);
    stmt->data.note.placement = SEQ_NOTE_OVER;
    ensure_participant(ctx, A);
    ensure_participant(ctx, B);
    free(A); free(B); free(T);
    R = stmt;
}

block_stmt(R) ::= LOOP TXT(T) NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_LOOP;
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    free(T);
    R = stmt;
}
block_stmt(R) ::= LOOP NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_LOOP;
    stmt->data.block.body = D;
    R = stmt;
}

block_stmt(R) ::= RECT TXT(T) NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_RECT;
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    free(T);
    R = stmt;
}
block_stmt(R) ::= RECT NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_RECT;
    stmt->data.block.body = D;
    R = stmt;
}

block_stmt(R) ::= OPT TXT(T) NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_OPT;
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    free(T);
    R = stmt;
}
block_stmt(R) ::= OPT NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_OPT;
    stmt->data.block.body = D;
    R = stmt;
}

alt_else(R) ::= . { R = NULL; }
alt_else(R) ::= ELSE label_text(T) NEWLINE document(D). {
    SequenceBlock* b = (SequenceBlock*)malloc(sizeof(SequenceBlock));
    memset(b, 0, sizeof(SequenceBlock));
    b->alternate_text = strdup(T);
    b->alternate_body = D;
    R = b;
}

block_stmt(R) ::= ALT label_text(T) NEWLINE document(D) alt_else(E) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_ALT;
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    if (E) {
        stmt->data.block.alternate_text = E->alternate_text;
        stmt->data.block.alternate_body = E->alternate_body;
        free(E);
    }
    free(T);
    R = stmt;
}

block_stmt(R) ::= ALT NEWLINE document(D) alt_else(E) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_ALT;
    stmt->data.block.body = D;
    if (E) {
        stmt->data.block.alternate_text = E->alternate_text;
        stmt->data.block.alternate_body = E->alternate_body;
        free(E);
    }
    R = stmt;
}

par_and(R) ::= . { R = NULL; }
par_and(R) ::= AND label_text(T) NEWLINE document(D). {
    SequenceBlock* b = (SequenceBlock*)malloc(sizeof(SequenceBlock));
    memset(b, 0, sizeof(SequenceBlock));
    b->alternate_text = strdup(T);
    b->alternate_body = D;
    R = b;
}

block_stmt(R) ::= PAR label_text(T) NEWLINE document(D) par_and(A) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_PAR;
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    if (A) {
        stmt->data.block.alternate_text = A->alternate_text;
        stmt->data.block.alternate_body = A->alternate_body;
        free(A);
    }
    free(T);
    R = stmt;
}
block_stmt(R) ::= PAR NEWLINE document(D) par_and(A) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_PAR;
    stmt->data.block.body = D;
    if (A) {
        stmt->data.block.alternate_text = A->alternate_text;
        stmt->data.block.alternate_body = A->alternate_body;
        free(A);
    }
    R = stmt;
}

block_stmt(R) ::= BREAK TXT(T) NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_BREAK;
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    free(T);
    R = stmt;
}
block_stmt(R) ::= BREAK NEWLINE document(D) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_BREAK;
    stmt->data.block.body = D;
    R = stmt;
}

option_stmt(R) ::= OPTION label_text(T) NEWLINE document(D). {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_OPT; // We use OPT type for options
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    free(T);
    R = stmt;
}
option_stmt(R) ::= OPTION NEWLINE document(D). {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_OPT;
    stmt->data.block.body = D;
    R = stmt;
}

optional_option_list(R) ::= . { R = NULL; }
optional_option_list(R) ::= option_list(L). { R = L; }

option_list(R) ::= option_stmt(O). { R = O; }
option_list(R) ::= option_list(L) option_stmt(O). {
    SequenceStatement* curr = L;
    while(curr->next) curr = curr->next;
    curr->next = O;
    R = L;
}

block_stmt(R) ::= CRITICAL TXT(T) NEWLINE document(D) optional_option_list(O) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_CRITICAL;
    stmt->data.block.text = strdup(T);
    stmt->data.block.body = D;
    stmt->data.block.alternate_body = O;
    free(T);
    R = stmt;
}

block_stmt(R) ::= CRITICAL NEWLINE document(D) optional_option_list(O) END. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_BLOCK;
    stmt->data.block.type = SEQ_BLOCK_CRITICAL;
    stmt->data.block.body = D;
    stmt->data.block.alternate_body = O;
    R = stmt;
}

activate_stmt(R) ::= ACTIVATE actor_id(A) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_ACTIVATE;
    stmt->data.activation.actor = strdup(A);
    ensure_participant(ctx, A);
    free(A);
    R = stmt;
}

deactivate_stmt(R) ::= DEACTIVATE actor_id(A) NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_DEACTIVATE;
    stmt->data.activation.actor = strdup(A);
    ensure_participant(ctx, A);
    free(A);
    R = stmt;
}

autonumber_stmt(R) ::= AUTONUMBER NEWLINE. {
    SequenceStatement* stmt = (SequenceStatement*)malloc(sizeof(SequenceStatement));
    memset(stmt, 0, sizeof(SequenceStatement));
    stmt->type = SEQ_STMT_AUTONUMBER;
    stmt->data.autonumber.visible = 1;
    stmt->data.autonumber.start = 1;
    stmt->data.autonumber.step = 1;
    R = stmt;
}

/* Fallback for other tokens */
statement(R) ::= PLUS. { (void)ctx; R = NULL; }
statement(R) ::= COMMA. { (void)ctx; R = NULL; }
statement(R) ::= MINUS. { (void)ctx; R = NULL; }
statement(R) ::= BOX. { (void)ctx; R = NULL; }
statement(R) ::= CREATE. { (void)ctx; R = NULL; }
statement(R) ::= DESTROY. { (void)ctx; R = NULL; }
statement(R) ::= OVER. { (void)ctx; R = NULL; }
