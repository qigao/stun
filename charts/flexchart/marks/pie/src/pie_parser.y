%name PieChartParser
%token_prefix PIE_
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
        ctx->error_message = "pie: syntax error";
    }
}

start ::= PIE LBRACE body RBRACE.
start ::= PIE LBRACE RBRACE.

body ::= stmt.
body ::= body stmt.

// --- Common properties ---
stmt ::= TITLE COLON STRING(V).        { ctx->set_title(V); free(V); }
stmt ::= WIDTH COLON NUMBER(V).        { ctx->set_width(V); free(V); }
stmt ::= HEIGHT COLON NUMBER(V).       { ctx->set_height(V); free(V); }

// --- Simple encodings (backward compat) ---
stmt ::= THETA COLON STRING(V).        { ctx->add_simple_encoding("theta", V); free(V); }
stmt ::= COLOR COLON STRING(V).        { ctx->add_simple_encoding("color", V); free(V); }

// --- Range encoding: theta: [start, end, step] ---
stmt ::= THETA COLON LBRACKET NUMBER(A) COMMA NUMBER(B) COMMA NUMBER(C) RBRACKET. {
    ctx->add_range("theta", atof(A), atof(B), atof(C)); free(A); free(B); free(C);
}

// --- Expression ---
stmt ::= EXPR COLON STRING(V).         { ctx->set_expr(V); free(V); }

// --- Mark-specific style properties (backward compat inline) ---
stmt ::= INNER_RADIUS COLON NUMBER(V). { ctx->add_style_num("inner-radius", atof(V)); free(V); }
stmt ::= OUTER_RADIUS COLON NUMBER(V). { ctx->add_style_num("outer-radius", atof(V)); free(V); }
stmt ::= PAD_ANGLE COLON NUMBER(V).    { ctx->add_style_num("pad-angle", atof(V)); free(V); }

// --- Encoding block ---
stmt ::= ENCODING LBRACE enc_stmts RBRACE.
stmt ::= ENCODING LBRACE RBRACE.

enc_stmts ::= enc_stmt.
enc_stmts ::= enc_stmts enc_stmt.

enc_stmt ::= IDENT(K) COLON STRING(V). { ctx->add_simple_encoding(K, V); free(K); free(V); }
enc_stmt ::= THETA COLON STRING(V).   { ctx->add_simple_encoding("theta", V); free(V); }
enc_stmt ::= COLOR COLON STRING(V).   { ctx->add_simple_encoding("color", V); free(V); }

// --- Style block ---
stmt ::= STYLE LBRACE style_stmts RBRACE.
stmt ::= STYLE LBRACE RBRACE.

style_stmts ::= style_stmt.
style_stmts ::= style_stmts style_stmt.

style_stmt ::= IDENT(K) COLON NUMBER(V).  { ctx->add_style_num(K, atof(V)); free(K); free(V); }
style_stmt ::= IDENT(K) COLON STRING(V).  { ctx->add_style(K, V); free(K); free(V); }
style_stmt ::= IDENT(K) COLON TRUE.       { ctx->add_style_bool(K, 1); free(K); }
style_stmt ::= IDENT(K) COLON FALSE.      { ctx->add_style_bool(K, 0); free(K); }
style_stmt ::= IDENT(K) COLON IDENT(V).   { ctx->add_style(K, V); free(K); free(V); }
style_stmt ::= INNER_RADIUS COLON NUMBER(V). { ctx->add_style_num("inner-radius", atof(V)); free(V); }
style_stmt ::= OUTER_RADIUS COLON NUMBER(V). { ctx->add_style_num("outer-radius", atof(V)); free(V); }
style_stmt ::= PAD_ANGLE COLON NUMBER(V).    { ctx->add_style_num("pad-angle", atof(V)); free(V); }

// --- Data block ---
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
