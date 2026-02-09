%name SankeyParser
%token_prefix SANKEY_
%token_type {char*}
%default_type {void*}
%extra_argument { SankeyParserContext *ctx }

%include {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sankey/sankey_ast.h"
#include "sankey_parser_gen.h"

static double to_double(char* s) {
    if(!s) return 0.0;
    return atof(s);
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

start ::= SANKEY_KW EOL document.
// sankey rule removed as it was ambiguous with document lines

document ::= .
document ::= document line.

line ::= record EOL.
line ::= EOL.

record ::= FIELD(S) COMMA FIELD(T) COMMA FIELD(V). {
    sankey_add_link(ctx, S, T, to_double(V));
    free(S); free(T); free(V);
}
