#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "line_parser_gen.h"

void LineChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void line_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "line"             { LineChartParser(parser, LINE_LINE, NULL, ctx); goto loop; }
        "title"            { LineChartParser(parser, LINE_TITLE, NULL, ctx); goto loop; }
        "width"            { LineChartParser(parser, LINE_WIDTH, NULL, ctx); goto loop; }
        "height"           { LineChartParser(parser, LINE_HEIGHT, NULL, ctx); goto loop; }
        "data"             { LineChartParser(parser, LINE_DATA, NULL, ctx); goto loop; }
        "x"                { LineChartParser(parser, LINE_X, NULL, ctx); goto loop; }
        "y"                { LineChartParser(parser, LINE_Y, NULL, ctx); goto loop; }
        "stroke-width"     { LineChartParser(parser, LINE_STROKE_WIDTH, NULL, ctx); goto loop; }
        "interpolate"      { LineChartParser(parser, LINE_INTERPOLATE, NULL, ctx); goto loop; }
        "point"            { LineChartParser(parser, LINE_POINT, NULL, ctx); goto loop; }
        "dash"             { LineChartParser(parser, LINE_DASH, NULL, ctx); goto loop; }
        "expr"             { LineChartParser(parser, LINE_EXPR, NULL, ctx); goto loop; }
        "encoding"         { LineChartParser(parser, LINE_ENCODING, NULL, ctx); goto loop; }
        "style"            { LineChartParser(parser, LINE_STYLE, NULL, ctx); goto loop; }
        "true"             { LineChartParser(parser, LINE_TRUE, NULL, ctx); goto loop; }
        "false"            { LineChartParser(parser, LINE_FALSE, NULL, ctx); goto loop; }

        "{"  { LineChartParser(parser, LINE_LBRACE, NULL, ctx); goto loop; }
        "}"  { LineChartParser(parser, LINE_RBRACE, NULL, ctx); goto loop; }
        "["  { LineChartParser(parser, LINE_LBRACKET, NULL, ctx); goto loop; }
        "]"  { LineChartParser(parser, LINE_RBRACKET, NULL, ctx); goto loop; }
        ":"  { LineChartParser(parser, LINE_COLON, NULL, ctx); goto loop; }
        ","  { LineChartParser(parser, LINE_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            LineChartParser(parser, LINE_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            LineChartParser(parser, LINE_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            LineChartParser(parser, LINE_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
