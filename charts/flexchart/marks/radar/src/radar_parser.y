%name RadarChartParser
%token_prefix RADAR_
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
        ctx->error_message = "radar: syntax error";
    }
}

start ::= RADAR LBRACE body RBRACE.
start ::= RADAR LBRACE RBRACE.

body ::= stmt.
body ::= body stmt.

// --- Common properties ---
stmt ::= TITLE COLON STRING(V).        { ctx->set_title(V); free(V); }
stmt ::= WIDTH COLON NUMBER(V).        { ctx->set_width(V); free(V); }
stmt ::= HEIGHT COLON NUMBER(V).       { ctx->set_height(V); free(V); }

// --- Simple encodings ---
stmt ::= CATEGORY COLON STRING(V).     { ctx->add_simple_encoding("category", V); free(V); }
stmt ::= VALUE COLON STRING(V).        { ctx->add_simple_encoding("value", V); free(V); }

// --- Range encoding: category/value: [start, end, step] ---
stmt ::= CATEGORY COLON LBRACKET NUMBER(A) COMMA NUMBER(B) COMMA NUMBER(C) RBRACKET. {
    ctx->add_range("category", atof(A), atof(B), atof(C)); free(A); free(B); free(C);
}
stmt ::= VALUE COLON LBRACKET NUMBER(A) COMMA NUMBER(B) COMMA NUMBER(C) RBRACKET. {
    ctx->add_range("value", atof(A), atof(B), atof(C)); free(A); free(B); free(C);
}

// --- Expression ---
stmt ::= EXPR COLON STRING(V).         { ctx->set_expr(V); free(V); }

// --- Mark-specific style properties (backward compat inline) ---
stmt ::= MAX COLON NUMBER(V).          { ctx->add_style_num("max", atof(V)); free(V); }
stmt ::= FILL_OPACITY COLON NUMBER(V). { ctx->add_style_num("fill-opacity", atof(V)); free(V); }
stmt ::= GRID_LINES COLON NUMBER(V).   { ctx->add_style_num("grid-lines", atof(V)); free(V); }

// --- Encoding block ---
stmt ::= ENCODING LBRACE enc_stmts RBRACE.
stmt ::= ENCODING LBRACE RBRACE.

enc_stmts ::= enc_stmt.
enc_stmts ::= enc_stmts enc_stmt.

enc_stmt ::= IDENT(K) COLON STRING(V). { ctx->add_simple_encoding(K, V); free(K); free(V); }
enc_stmt ::= CATEGORY COLON STRING(V). { ctx->add_simple_encoding("category", V); free(V); }
enc_stmt ::= VALUE COLON STRING(V).    { ctx->add_simple_encoding("value", V); free(V); }

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
style_stmt ::= MAX COLON NUMBER(V).          { ctx->add_style_num("max", atof(V)); free(V); }
style_stmt ::= FILL_OPACITY COLON NUMBER(V). { ctx->add_style_num("fill-opacity", atof(V)); free(V); }
style_stmt ::= GRID_LINES COLON NUMBER(V).   { ctx->add_style_num("grid-lines", atof(V)); free(V); }

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
