%name LineChartParser
%token_prefix LINE_
%extra_argument { flex::chart::MarkParserContext *ctx }

%token_type { char* }

%include {
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
}

%syntax_error {
    ctx->error_count++;
    if (ctx->error_message.empty()) {
        ctx->error_message = "line: syntax error";
    }
}

start ::= LINE LBRACE body RBRACE.
start ::= LINE LBRACE RBRACE.

body ::= stmt.
body ::= body stmt.

stmt ::= TITLE COLON STRING(V).        { ctx->set_title(V); free(V); }
stmt ::= WIDTH COLON NUMBER(V).        { ctx->set_width(V); free(V); }
stmt ::= HEIGHT COLON NUMBER(V).       { ctx->set_height(V); free(V); }

// Simple encodings
stmt ::= X COLON STRING(V).            { ctx->add_simple_encoding("x", V); free(V); }
stmt ::= Y COLON STRING(V).            { ctx->add_simple_encoding("y", V); free(V); }

// Range
stmt ::= X COLON LBRACKET NUMBER(A) COMMA NUMBER(B) COMMA NUMBER(C) RBRACKET. {
    ctx->add_range("x", atof(A), atof(B), atof(C)); free(A); free(B); free(C);
}
stmt ::= Y COLON LBRACKET NUMBER(A) COMMA NUMBER(B) COMMA NUMBER(C) RBRACKET. {
    ctx->add_range("y", atof(A), atof(B), atof(C)); free(A); free(B); free(C);
}

// Expression
stmt ::= EXPR COLON STRING(V).         { ctx->set_expr(V); free(V); }

// Mark-specific styles (backward compat inline)
stmt ::= STROKE_WIDTH COLON NUMBER(V). { ctx->add_style_num("stroke-width", atof(V)); free(V); }
stmt ::= INTERPOLATE COLON IDENT(V).   { ctx->add_style("interpolate", V); free(V); }
stmt ::= POINT COLON TRUE.             { ctx->add_style_bool("point", 1); }
stmt ::= POINT COLON FALSE.            { ctx->add_style_bool("point", 0); }
stmt ::= POINT COLON IDENT(V).         { ctx->add_style("point", V); free(V); }
stmt ::= DASH COLON STRING(V).         { ctx->add_style("dash", V); free(V); }

// Encoding block
stmt ::= ENCODING LBRACE enc_stmts RBRACE.
stmt ::= ENCODING LBRACE RBRACE.

enc_stmts ::= enc_stmt.
enc_stmts ::= enc_stmts enc_stmt.

enc_stmt ::= IDENT(K) COLON STRING(V). { ctx->add_simple_encoding(K, V); free(K); free(V); }
enc_stmt ::= X COLON STRING(V).       { ctx->add_simple_encoding("x", V); free(V); }
enc_stmt ::= Y COLON STRING(V).       { ctx->add_simple_encoding("y", V); free(V); }

// Style block
stmt ::= STYLE LBRACE style_stmts RBRACE.
stmt ::= STYLE LBRACE RBRACE.

style_stmts ::= style_stmt.
style_stmts ::= style_stmts style_stmt.

style_stmt ::= IDENT(K) COLON NUMBER(V).  { ctx->add_style_num(K, atof(V)); free(K); free(V); }
style_stmt ::= IDENT(K) COLON STRING(V).  { ctx->add_style(K, V); free(K); free(V); }
style_stmt ::= IDENT(K) COLON TRUE.       { ctx->add_style_bool(K, 1); free(K); }
style_stmt ::= IDENT(K) COLON FALSE.      { ctx->add_style_bool(K, 0); free(K); }
style_stmt ::= IDENT(K) COLON IDENT(V).   { ctx->add_style(K, V); free(K); free(V); }
style_stmt ::= STROKE_WIDTH COLON NUMBER(V). { ctx->add_style_num("stroke-width", atof(V)); free(V); }
style_stmt ::= INTERPOLATE COLON IDENT(V).   { ctx->add_style("interpolate", V); free(V); }
style_stmt ::= POINT COLON TRUE.             { ctx->add_style_bool("point", 1); }
style_stmt ::= POINT COLON FALSE.            { ctx->add_style_bool("point", 0); }
style_stmt ::= POINT COLON IDENT(V).         { ctx->add_style("point", V); free(V); }
style_stmt ::= DASH COLON STRING(V).         { ctx->add_style("dash", V); free(V); }

// Data block
stmt ::= data_block.

data_block ::= DATA LBRACE rows RBRACE.   { ctx->finish_data_block(); }
data_block ::= DATA LBRACE RBRACE.
data_block ::= DATA COLON STRING(V).      { ctx->set_data_source(V); free(V); }

rows ::= row.
rows ::= rows row.

row ::= LBRACE pairs RBRACE. { ctx->finish_data_row(); }

pairs ::= pair.
pairs ::= pairs COMMA pair.

pair ::= STRING(K) COLON STRING(V). { ctx->add_data_row_kv(K, V); free(K); free(V); }
pair ::= STRING(K) COLON NUMBER(V). { ctx->add_data_row_kv(K, V); free(K); free(V); }
pair ::= STRING(K) COLON IDENT(V).  { ctx->add_data_row_kv(K, V); free(K); free(V); }
