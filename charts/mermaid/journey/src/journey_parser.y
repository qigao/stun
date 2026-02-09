%name JourneyParser
%token_prefix JOURNEY_
%token_type {char*}
%default_type {void*}
%extra_argument { JourneyParserContext *ctx }

%include {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "journey/journey_ast.h"
#include "journey_parser_gen.h"

// Helper to strip prefix
static char* strip_prefix(char* s, int len) {
    if (!s) return NULL;
    char* res = strdup(s + len);
    return res;
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

start ::= journey document.

journey ::= JOURNEY.

document ::= .
document ::= document line.

line ::= statement.
line ::= NEWLINE.

statement ::= TITLE(T). {
    // T starts with "title "
    char* val = strip_prefix(T, 6);
    journey_set_title(ctx->diagram, val);
    free(val);
    free(T);
}

statement ::= ACC_TITLE ACC_TITLE_VALUE(V). {
    journey_set_acc_title(ctx->diagram, V); 
    free(V);
}

statement ::= ACC_DESCR ACC_DESCR_VALUE(V). {
    journey_set_acc_descr(ctx->diagram, V); 
    free(V);
}

statement ::= ACC_DESCR_MULTILINE_VALUE(V). {
    journey_set_acc_descr(ctx->diagram, V);
    free(V);
}

statement ::= SECTION(S). {
    // S starts with "section "
    char* val = strip_prefix(S, 8);
    journey_add_section(ctx, val);
    free(val);
    free(S);
}

statement ::= TASK_NAME(N) TASK_DATA(D). {
    journey_add_task(ctx, N, D); 
    free(N); 
    free(D);
}
