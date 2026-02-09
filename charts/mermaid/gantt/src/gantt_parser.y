%name GanttParser
%token_prefix GANTT_
%extra_argument { GanttParserContext *ctx }

%token_type { char* }
%type any_text { char* }
%type any_word { char* }
%type task_name { char* }

%include {
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gantt/gantt_ast.h"

static char* concat(const char* a, const char* b, const char* sep) {
    if (!a && !b) return NULL;
    if (!a) return strdup(b);
    if (!b) return strdup(a);
    size_t la = strlen(a);
    size_t lb = strlen(b);
    size_t ls = strlen(sep);
    char* res = (char*)malloc(la + lb + ls + 1);
    memcpy(res, a, la);
    memcpy(res + la, sep, ls);
    memcpy(res + la + ls, b, lb);
    res[la + lb + ls] = '\0';
    free((void*)a);
    free((void*)b);
    return res;
}

static void add_section(GanttParserContext *ctx, const char* name) {
    GanttSection* s = (GanttSection*)malloc(sizeof(GanttSection));
    memset(s, 0, sizeof(GanttSection));
    s->name = strdup(name);
    if (!ctx->diagram->sections) {
        ctx->diagram->sections = s;
    } else {
        GanttSection* curr = ctx->diagram->sections;
        while (curr->next) curr = curr->next;
        curr->next = s;
    }
    ctx->current_section = s;
}

static void add_task(GanttParserContext *ctx, const char* name) {
    if (!ctx->current_section) {
        add_section(ctx, "Default");
    }
    GanttTask* t = (GanttTask*)malloc(sizeof(GanttTask));
    memset(t, 0, sizeof(GanttTask));
    t->name = strdup(name);
    if (!ctx->current_section->tasks) {
        ctx->current_section->tasks = t;
    } else {
        GanttTask* curr = ctx->current_section->tasks;
        while (curr->next) curr = curr->next;
        curr->next = t;
    }
}

static GanttTask* last_task(GanttParserContext *ctx) {
    if (!ctx->current_section || !ctx->current_section->tasks) return NULL;
    GanttTask* curr = ctx->current_section->tasks;
    while (curr->next) curr = curr->next;
    return curr;
}
}

%syntax_error {
    fprintf(stderr, "Gantt Syntax error!\n");
    ctx->error_count++;
}

start ::= GANTT statements.

statements ::= .
statements ::= statements statement.

statement ::= NL.
statement ::= TITLE_KW any_text(T) NL. { ctx->diagram->title = T; }
statement ::= DATE_FORMAT_KW any_text(T) NL. { ctx->diagram->date_format = T; }
statement ::= AXIS_FORMAT_KW any_text(T) NL. { ctx->diagram->axis_format = T; }
statement ::= SECTION_KW any_text(T) NL. { add_section(ctx, T); free(T); }
statement ::= INCLUSIVE_END_DATES NL. { ctx->diagram->inclusive_end_dates = 1; }
statement ::= TOP_AXIS NL. { ctx->diagram->top_axis = 1; }
statement ::= EXCLUDES_KW any_text(T) NL. { ctx->diagram->excludes = T; }
statement ::= task_statement.

task_statement ::= task_entry data_list NL.
task_statement ::= task_entry NL.

task_entry ::= task_name(T) COLON. { add_task(ctx, T); free(T); }

task_name(R) ::= TEXT(A). { R = A; }
task_name(R) ::= STRING(A). { R = A; }
task_name(R) ::= task_name(A) any_word(B). { R = concat(A, B, " "); }

data_list ::= data_item.
data_list ::= data_list COMMA data_item.

data_item ::= any_text(T). { 
    GanttTask* t = last_task(ctx); 
    if(t) {
        if (strcmp(T, "done") == 0) t->status |= GANTT_STATUS_DONE;
        else if (strcmp(T, "active") == 0) t->status |= GANTT_STATUS_ACTIVE;
        else if (strcmp(T, "crit") == 0) t->status |= GANTT_STATUS_CRIT;
        else if (strcmp(T, "milestone") == 0) t->status |= GANTT_STATUS_MILESTONE;
        else {
            if (!t->id) t->id = strdup(T);
            else if (!t->start) t->start = strdup(T);
            else if (!t->end) t->end = strdup(T);
        }
    }
    free(T);
}

any_text(R) ::= any_word(A). { R = A; }
any_text(R) ::= any_text(A) any_word(B). { R = concat(A, B, " "); }

any_word(R) ::= TEXT(A). { R = A; }
any_word(R) ::= STRING(A). { R = A; }
any_word(R) ::= GANTT. { R = strdup("gantt"); }
any_word(R) ::= TITLE_KW. { R = strdup("title"); }
any_word(R) ::= DATE_FORMAT_KW. { R = strdup("dateFormat"); }
any_word(R) ::= AXIS_FORMAT_KW. { R = strdup("axisFormat"); }
any_word(R) ::= SECTION_KW. { R = strdup("section"); }
any_word(R) ::= STATUS_DONE_KW. { R = strdup("done"); }
any_word(R) ::= STATUS_ACTIVE_KW. { R = strdup("active"); }
any_word(R) ::= STATUS_CRIT_KW. { R = strdup("crit"); }
any_word(R) ::= STATUS_MILESTONE_KW. { R = strdup("milestone"); }
any_word(R) ::= INCLUSIVE_END_DATES. { R = strdup("inclusiveEndDates"); }
any_word(R) ::= TOP_AXIS. { R = strdup("topAxis"); }
any_word(R) ::= EXCLUDES_KW. { R = strdup("excludes"); }
