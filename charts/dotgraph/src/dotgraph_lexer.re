#ifndef DOTGRAPH_LEXER_H
#define DOTGRAPH_LEXER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dotgraph_parser_gen.h"
#include "dotgraph/dotgraph_parser_wrapper.h"
#include "dotgraph_scanner.h"

void DotGraphParser(void* parser,
                    int token,
                    void* token_value,
                    DotGraphParserContext* context);

static char* dg_copy_token(const char* start, const char* end) {
    size_t len = end - start;
    char* s = (char*)malloc(len + 1);
    if (!s) return NULL;
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

/* Unescape a quoted string: remove surrounding quotes and handle backslash escapes */
static char* dg_unescape_string(const char* start, const char* end) {
    /* skip opening and closing quote */
    start++;
    end--;
    size_t max_len = end - start;
    char* s = (char*)malloc(max_len + 1);
    if (!s) return NULL;
    size_t j = 0;
    for (const char* p = start; p < end; p++) {
        if (*p == '\\' && p + 1 < end) {
            p++;
            switch (*p) {
                case 'n': s[j++] = '\n'; break;
                case 't': s[j++] = '\t'; break;
                case 'r': s[j++] = '\r'; break;
                case '\\': s[j++] = '\\'; break;
                case '"': s[j++] = '"'; break;
                default: s[j++] = '\\'; s[j++] = *p; break;
            }
        } else {
            s[j++] = *p;
        }
    }
    s[j] = '\0';
    return s;
}

static int dg_ascii_iequal(const char* lhs, const char* rhs) {
    while (*lhs && *rhs) {
        char a = *lhs++;
        char b = *rhs++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return *lhs == *rhs;
}

static void dg_record_lexer_error(DotGraphParserContext* ctx,
                                  int line,
                                  const char* message) {
    ctx->error_count++;
    if (ctx->error_message[0] == '\0') {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "DOT lexical error on line %d: %s", line, message);
    }
}

void dotgraph_scan(DotGraphScanner *s, void *parser, DotGraphParserContext *ctx) {
    char *token;
loop:
    token = (char*)s->cursor;
    ctx->line = s->line;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        white = [ \t\r]+;
        newline = [\n];
        ident = [a-zA-Z_][a-zA-Z0-9_]*;
        number = "-"? ("." [0-9]+ | [0-9]+ ("." [0-9]*)?);
        string = "\"" ([^"\\\000] | "\\" .)* "\"";

        white   { goto loop; }
        newline { s->line++; goto loop; }

        "//" [^\n\000]* { goto loop; }
        "#" [^\n\000]*  { goto loop; }
        "/*" ([^*\000] | "*" [^/\000])* "*/" {
            /* count newlines inside block comment */
            for (const char* p = token; p < s->cursor; p++) {
                if (*p == '\n') s->line++;
            }
            goto loop;
        }

        /* Operators */
        "->"  { DotGraphParser(parser, DG_ARROW, NULL, ctx); goto loop; }
        "--"  { DotGraphParser(parser, DG_DASHDASH, NULL, ctx); goto loop; }

        /* Delimiters */
        "{"   { DotGraphParser(parser, DG_LBRACE, NULL, ctx); goto loop; }
        "}"   { DotGraphParser(parser, DG_RBRACE, NULL, ctx); goto loop; }
        "["   { DotGraphParser(parser, DG_LBRACKET, NULL, ctx); goto loop; }
        "]"   { DotGraphParser(parser, DG_RBRACKET, NULL, ctx); goto loop; }
        ";"   { DotGraphParser(parser, DG_SEMI, NULL, ctx); goto loop; }
        ","   { DotGraphParser(parser, DG_COMMA, NULL, ctx); goto loop; }
        ":"   { DotGraphParser(parser, DG_COLON, NULL, ctx); goto loop; }
        "="   { DotGraphParser(parser, DG_EQUALS, NULL, ctx); goto loop; }

        /* HTML string: balanced angle brackets <...> */
        "<" {
            int depth = 1;
            const char* html_start = token;
            while (depth > 0 && s->cursor < s->limit) {
                char c = *s->cursor++;
                if (c == '<') depth++;
                else if (c == '>') depth--;
                else if (c == '\n') s->line++;
            }
            if (depth == 0) {
                /* strip outer < > */
                char* val = dg_copy_token(html_start + 1, s->cursor - 1);
                if (!val) {
                    dg_record_lexer_error(ctx, s->line, "out of memory");
                    return;
                }
                DotGraphParser(parser, DG_HTML_STRING, val, ctx);
            } else {
                dg_record_lexer_error(ctx, s->line, "unterminated HTML string");
            }
            goto loop;
        }

        /* Number literal */
        number {
            char* val = dg_copy_token(token, s->cursor);
            if (!val) { dg_record_lexer_error(ctx, s->line, "out of memory"); return; }
            DotGraphParser(parser, DG_NUMBER, val, ctx);
            goto loop;
        }

        /* Quoted string */
        string {
            char* val = dg_unescape_string(token, s->cursor);
            if (!val) { dg_record_lexer_error(ctx, s->line, "out of memory"); return; }
            DotGraphParser(parser, DG_STRING, val, ctx);
            goto loop;
        }

        /* Identifier */
        ident {
            char* val = dg_copy_token(token, s->cursor);
            if (!val) { dg_record_lexer_error(ctx, s->line, "out of memory"); return; }
            int token_id = DG_IDENTIFIER;
            if (dg_ascii_iequal(val, "digraph")) token_id = DG_DIGRAPH;
            else if (dg_ascii_iequal(val, "graph")) token_id = DG_GRAPH;
            else if (dg_ascii_iequal(val, "subgraph")) token_id = DG_SUBGRAPH;
            else if (dg_ascii_iequal(val, "node")) token_id = DG_NODE;
            else if (dg_ascii_iequal(val, "edge")) token_id = DG_EDGE;
            else if (dg_ascii_iequal(val, "strict")) token_id = DG_STRICT;
            if (token_id == DG_IDENTIFIER) {
                DotGraphParser(parser, token_id, val, ctx);
            } else {
                free(val);
                DotGraphParser(parser, token_id, NULL, ctx);
            }
            goto loop;
        }

        "\000" { DotGraphParser(parser, DG_EOF, NULL, ctx); return; }

        . {
            char message[48];
            snprintf(message, sizeof(message), "unexpected character 0x%02X",
                     (unsigned int)(unsigned char)*token);
            dg_record_lexer_error(ctx, s->line, message);
            goto loop;
        }
    */
}

#endif
