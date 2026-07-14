#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "bar_parser_gen.h"

void BarChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void bar_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "bar"              { BarChartParser(parser, BAR_BAR, NULL, ctx); goto loop; }
        "title"            { BarChartParser(parser, BAR_TITLE, NULL, ctx); goto loop; }
        "width"            { BarChartParser(parser, BAR_WIDTH, NULL, ctx); goto loop; }
        "height"           { BarChartParser(parser, BAR_HEIGHT, NULL, ctx); goto loop; }
        "data"             { BarChartParser(parser, BAR_DATA, NULL, ctx); goto loop; }
        "x"                { BarChartParser(parser, BAR_X, NULL, ctx); goto loop; }
        "y"                { BarChartParser(parser, BAR_Y, NULL, ctx); goto loop; }
        "color"            { BarChartParser(parser, BAR_COLOR, NULL, ctx); goto loop; }
        "stack"            { BarChartParser(parser, BAR_STACK, NULL, ctx); goto loop; }
        "corner-radius"    { BarChartParser(parser, BAR_CORNER_RADIUS, NULL, ctx); goto loop; }
        "orientation"      { BarChartParser(parser, BAR_ORIENTATION, NULL, ctx); goto loop; }
        "expr"             { BarChartParser(parser, BAR_EXPR, NULL, ctx); goto loop; }
        "encoding"         { BarChartParser(parser, BAR_ENCODING, NULL, ctx); goto loop; }
        "style"            { BarChartParser(parser, BAR_STYLE, NULL, ctx); goto loop; }
        "true"             { BarChartParser(parser, BAR_TRUE, NULL, ctx); goto loop; }
        "false"            { BarChartParser(parser, BAR_FALSE, NULL, ctx); goto loop; }

        "{"  { BarChartParser(parser, BAR_LBRACE, NULL, ctx); goto loop; }
        "}"  { BarChartParser(parser, BAR_RBRACE, NULL, ctx); goto loop; }
        "["  { BarChartParser(parser, BAR_LBRACKET, NULL, ctx); goto loop; }
        "]"  { BarChartParser(parser, BAR_RBRACKET, NULL, ctx); goto loop; }
        ":"  { BarChartParser(parser, BAR_COLON, NULL, ctx); goto loop; }
        ","  { BarChartParser(parser, BAR_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            BarChartParser(parser, BAR_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            BarChartParser(parser, BAR_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            BarChartParser(parser, BAR_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
