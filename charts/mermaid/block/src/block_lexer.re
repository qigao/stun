#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "block/block_ast.h"
#include "block_parser_gen.h"

void BlockParser(void *parser, int token, void *value, BlockParserContext *ctx);

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

static char* copy_token(const char* start, const char* end) {
    size_t len = end - start;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start, len);
    res[len] = '\0';
    return res;
}

static char* copy_quoted(const char* start, const char* end) {
    if (end - start < 2) return strdup("");
    size_t len = (end - start) - 2;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start + 1, len);
    res[len] = '\0';
    return res;
}

void block_scan(Scanner *s, void *parser, BlockParserContext *ctx) {
    const char *token;

    loop:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        white = [ \t\r,]+;
        newline = [\n];
        comment = ("%%"| "#") [^\n]* newline;

        white { goto loop; }
        newline { s->line++; BlockParser(parser, BLOCK_NL, NULL, ctx); goto loop; }
        comment { s->line++; BlockParser(parser, BLOCK_NL, NULL, ctx); goto loop; }

        "block-beta"        { BlockParser(parser, BLOCK_START, strdup("block-beta"), ctx); goto loop; }
        "block"             { BlockParser(parser, BLOCK_START, strdup("block"), ctx); goto loop; }
        "block:"            { BlockParser(parser, BLOCK_KW_BLOCK, NULL, ctx); goto loop; }
        "columns"           { BlockParser(parser, BLOCK_KW_COLUMNS, NULL, ctx); goto loop; }
        "space"             { BlockParser(parser, BLOCK_KW_SPACE, NULL, ctx); goto loop; }
        "space:" [0-9]+     { BlockParser(parser, BLOCK_KW_SPACE_NUM, copy_token(token + 6, s->cursor), ctx); goto loop; }
        "auto"              { BlockParser(parser, BLOCK_AUTO, NULL, ctx); goto loop; }
        "end"               { BlockParser(parser, BLOCK_END, NULL, ctx); goto loop; }
        "style"             { BlockParser(parser, BLOCK_KW_STYLE, NULL, ctx); goto loop; }
        "classDef"          { BlockParser(parser, BLOCK_KW_CLASSDEF, NULL, ctx); goto loop; }
        "class"             { BlockParser(parser, BLOCK_KW_CLASS, NULL, ctx); goto loop; }

        // Edges
        "--"                { BlockParser(parser, BLOCK_EDGE, strdup("--"), ctx); goto loop; }
        "=="                { BlockParser(parser, BLOCK_EDGE, strdup("=="), ctx); goto loop; }
        "->"                { BlockParser(parser, BLOCK_EDGE, strdup("->"), ctx); goto loop; }
        "-->"               { BlockParser(parser, BLOCK_EDGE, strdup("-->"), ctx); goto loop; }
        "-.-"               { BlockParser(parser, BLOCK_EDGE, strdup("-.-"), ctx); goto loop; }
        
        // Node Shapes (Start/End)
        // Simplified mapping, could expand if needed
        "("                 { BlockParser(parser, BLOCK_SHAPE_START, strdup("("), ctx); goto loop; }
        ")"                 { BlockParser(parser, BLOCK_SHAPE_END, strdup(")"), ctx); goto loop; }
        "["                 { BlockParser(parser, BLOCK_SHAPE_START, strdup("["), ctx); goto loop; }
        "]"                 { BlockParser(parser, BLOCK_SHAPE_END, strdup("]"), ctx); goto loop; }
        "(("                { BlockParser(parser, BLOCK_SHAPE_START, strdup("(("), ctx); goto loop; }
        "))"                { BlockParser(parser, BLOCK_SHAPE_END, strdup("))"), ctx); goto loop; }
        ">"                 { BlockParser(parser, BLOCK_SHAPE_START, strdup(">"), ctx); goto loop; }
        
        // Arrow Block
        "<["                { BlockParser(parser, BLOCK_ARROW_START, NULL, ctx); goto loop; }
        "]>"                { BlockParser(parser, BLOCK_ARROW_END, NULL, ctx); goto loop; }
        
        // Directions
        "up"                { BlockParser(parser, BLOCK_DIR, strdup("up"), ctx); goto loop; }
        "down"              { BlockParser(parser, BLOCK_DIR, strdup("down"), ctx); goto loop; }
        "left"              { BlockParser(parser, BLOCK_DIR, strdup("left"), ctx); goto loop; }
        "right"             { BlockParser(parser, BLOCK_DIR, strdup("right"), ctx); goto loop; }
        
        // Quoted string
        "\"" [^\"]* "\"" {
            BlockParser(parser, BLOCK_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        // Identifiers (note: colon excluded to allow block:ID parsing)
        [a-zA-Z0-9_\-#,%&;.]+ {
            BlockParser(parser, BLOCK_ID, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        // Catch-all
        "\000" { return; }
        * { goto loop; } // Skip unrecognized chars for now
    */
}
