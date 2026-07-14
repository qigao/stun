%name QuadrantParser
%token_prefix QUADRANT_
%token_type {char*}
%default_type {void*}
%extra_argument { QuadrantParserContext *ctx }

%include {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flex/core/expr_c.h"
#include "quadrant/quadrant_ast.h"
#include "quadrant_parser_gen.h"

static double to_double(char* s) {
    if(!s) return 0.0;
    double value = 0.0;
    return flex_expr_eval_f64(s, &value) ? value : 0.0;
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

start ::= quadrant document.

quadrant ::= QUADRANT_KW.

document ::= .
document ::= document line.

line ::= statement NL.
line ::= NL.

statement ::= TITLE(T). {
    quadrant_set_title(ctx, T);
    free(T);
}

statement ::= ACC_TITLE(T). {
    quadrant_set_acc_title(ctx, T);
    free(T);
}

statement ::= ACC_DESCR(D). {
    quadrant_set_acc_descr(ctx, D);
    free(D);
}

// Text sequence to handle spaces
text_seq(S) ::= TEXT(T). { S = T; }
text_seq(S) ::= text_seq(H) TEXT(T). {
    size_t len = strlen(H) + strlen(T) + 2;
    S = (char*)malloc(len);
    snprintf(S, len, "%s %s", H, T);
    free(H); free(T);
}

// Axis Details
statement ::= X_AXIS x_axis_def.
statement ::= Y_AXIS y_axis_def.

x_axis_def ::= text_seq(L). [LOW] {
    quadrant_set_x_axis_left(ctx, L);
    free(L);
}
x_axis_def ::= text_seq(L) AXIS_DELIM text_seq(R). {
    quadrant_set_x_axis_left(ctx, L);
    quadrant_set_x_axis_right(ctx, R);
    free(L); free(R);
}

y_axis_def ::= text_seq(B). [LOW] {
    quadrant_set_y_axis_bottom(ctx, B);
    free(B);
}
y_axis_def ::= text_seq(B) AXIS_DELIM text_seq(T). {
    quadrant_set_y_axis_bottom(ctx, B);
    quadrant_set_y_axis_top(ctx, T);
    free(B); free(T);
}

// Quadrant Details
statement ::= QUADRANT_1 text_seq(T). { quadrant_set_quadrant_1_text(ctx, T); free(T); }
statement ::= QUADRANT_2 text_seq(T). { quadrant_set_quadrant_2_text(ctx, T); free(T); }
statement ::= QUADRANT_3 text_seq(T). { quadrant_set_quadrant_3_text(ctx, T); free(T); }
statement ::= QUADRANT_4 text_seq(T). { quadrant_set_quadrant_4_text(ctx, T); free(T); }

// Points
statement ::= text_seq(T) POINT_START POINT_X(X) POINT_Y(Y) POINT_END. {
    quadrant_add_point(ctx, T, NULL, to_double(X), to_double(Y));
    free(T); free(X); free(Y);
}

statement ::= text_seq(T) CLASS_NAME(C) POINT_START POINT_X(X) POINT_Y(Y) POINT_END. {
    quadrant_add_point(ctx, T, C, to_double(X), to_double(Y));
    free(T); free(C); free(X); free(Y);
}
