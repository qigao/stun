#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "point_parser_gen.h"

void PointChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void point_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "point"            { PointChartParser(parser, POINT_POINT, NULL, ctx); goto loop; }
        "title"            { PointChartParser(parser, POINT_TITLE, NULL, ctx); goto loop; }
        "width"            { PointChartParser(parser, POINT_WIDTH, NULL, ctx); goto loop; }
        "height"           { PointChartParser(parser, POINT_HEIGHT, NULL, ctx); goto loop; }
        "data"             { PointChartParser(parser, POINT_DATA, NULL, ctx); goto loop; }
        "x"                { PointChartParser(parser, POINT_X, NULL, ctx); goto loop; }
        "y"                { PointChartParser(parser, POINT_Y, NULL, ctx); goto loop; }
        "size"             { PointChartParser(parser, POINT_SIZE, NULL, ctx); goto loop; }
        "color"            { PointChartParser(parser, POINT_COLOR, NULL, ctx); goto loop; }
        "shape"            { PointChartParser(parser, POINT_SHAPE, NULL, ctx); goto loop; }
        "expr"             { PointChartParser(parser, POINT_EXPR, NULL, ctx); goto loop; }
        "encoding"         { PointChartParser(parser, POINT_ENCODING, NULL, ctx); goto loop; }
        "style"            { PointChartParser(parser, POINT_STYLE, NULL, ctx); goto loop; }
        "true"             { PointChartParser(parser, POINT_TRUE, NULL, ctx); goto loop; }
        "false"            { PointChartParser(parser, POINT_FALSE, NULL, ctx); goto loop; }

        "{"  { PointChartParser(parser, POINT_LBRACE, NULL, ctx); goto loop; }
        "}"  { PointChartParser(parser, POINT_RBRACE, NULL, ctx); goto loop; }
        "["  { PointChartParser(parser, POINT_LBRACKET, NULL, ctx); goto loop; }
        "]"  { PointChartParser(parser, POINT_RBRACKET, NULL, ctx); goto loop; }
        ":"  { PointChartParser(parser, POINT_COLON, NULL, ctx); goto loop; }
        ","  { PointChartParser(parser, POINT_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            PointChartParser(parser, POINT_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            PointChartParser(parser, POINT_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            PointChartParser(parser, POINT_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
