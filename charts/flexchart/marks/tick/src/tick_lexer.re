#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "tick_parser_gen.h"

void TickChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void tick_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
    const char *token;

    loop:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        ws = [ \t\r\n]+;
        digit = [0-9];
        number = "-"? digit+ ("." digit+)?;

        ws { if (*token == '\n') s->line++; goto loop; }

        "tick"             { TickChartParser(parser, TICK_TICK, NULL, ctx); goto loop; }
        "title"            { TickChartParser(parser, TICK_TITLE, NULL, ctx); goto loop; }
        "width"            { TickChartParser(parser, TICK_WIDTH, NULL, ctx); goto loop; }
        "height"           { TickChartParser(parser, TICK_HEIGHT, NULL, ctx); goto loop; }
        "data"             { TickChartParser(parser, TICK_DATA, NULL, ctx); goto loop; }
        "x"                { TickChartParser(parser, TICK_X, NULL, ctx); goto loop; }
        "y"                { TickChartParser(parser, TICK_Y, NULL, ctx); goto loop; }
        "color"            { TickChartParser(parser, TICK_COLOR, NULL, ctx); goto loop; }
        "size"             { TickChartParser(parser, TICK_SIZE, NULL, ctx); goto loop; }
        "orient"           { TickChartParser(parser, TICK_ORIENT, NULL, ctx); goto loop; }
        "expr"             { TickChartParser(parser, TICK_EXPR, NULL, ctx); goto loop; }
        "encoding"         { TickChartParser(parser, TICK_ENCODING, NULL, ctx); goto loop; }
        "style"            { TickChartParser(parser, TICK_STYLE, NULL, ctx); goto loop; }
        "true"             { TickChartParser(parser, TICK_TRUE, NULL, ctx); goto loop; }
        "false"            { TickChartParser(parser, TICK_FALSE, NULL, ctx); goto loop; }

        "{"  { TickChartParser(parser, TICK_LBRACE, NULL, ctx); goto loop; }
        "}"  { TickChartParser(parser, TICK_RBRACE, NULL, ctx); goto loop; }
        "["  { TickChartParser(parser, TICK_LBRACKET, NULL, ctx); goto loop; }
        "]"  { TickChartParser(parser, TICK_RBRACKET, NULL, ctx); goto loop; }
        ":"  { TickChartParser(parser, TICK_COLON, NULL, ctx); goto loop; }
        ","  { TickChartParser(parser, TICK_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            TickChartParser(parser, TICK_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            TickChartParser(parser, TICK_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            TickChartParser(parser, TICK_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
