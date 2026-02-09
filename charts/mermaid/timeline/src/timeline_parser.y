%name TimelineParser
%token_prefix TL_
%token_type {char*}
%extra_argument { TimelineParserContext *ctx }

%token NL TIMELINE_KW TITLE_VAL ACC_TITLE_VAL ACC_DESCR_VAL SECTION_VAL PERIOD_VAL EVENT_VAL EOF.

%include {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timeline/timeline_ast.h"
}

%syntax_error {
    if(!ctx->error_message) {
        char buf[128];
        snprintf(buf, 128, "Syntax error at token type %d", yymajor);
        ctx->error_message = strdup(buf);
    }
    ctx->error_count++;
}

start ::= TIMELINE_KW items.

items ::= .
items ::= items item.

item ::= NL.
item ::= EOF. 
item ::= statement NL.
item ::= statement EOF.

statement ::= TITLE_VAL(V). { timeline_set_title(ctx, V); free(V); }
statement ::= ACC_TITLE_VAL(V). { timeline_set_acc_title(ctx, V); free(V); }
statement ::= ACC_DESCR_VAL(V). { timeline_set_acc_descr(ctx, V); free(V); }
statement ::= SECTION_VAL(V). { timeline_add_section(ctx, V); free(V); }
statement ::= EVENT_VAL(V). { timeline_add_event(ctx, V); free(V); }

statement ::= PERIOD_VAL(V). { timeline_add_period(ctx, V); free(V); }
statement ::= PERIOD_VAL(P) EVENT_VAL(E). { 
    timeline_add_period(ctx, P); free(P); 
    timeline_add_event(ctx, E); free(E); 
}
