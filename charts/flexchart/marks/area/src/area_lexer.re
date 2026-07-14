#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "area_parser_gen.h"

void AreaChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void area_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "area"           { AreaChartParser(parser, AREA_AREA, NULL, ctx); goto loop; }
        "title"          { AreaChartParser(parser, AREA_TITLE, NULL, ctx); goto loop; }
        "width"          { AreaChartParser(parser, AREA_WIDTH, NULL, ctx); goto loop; }
        "height"         { AreaChartParser(parser, AREA_HEIGHT, NULL, ctx); goto loop; }
        "data"           { AreaChartParser(parser, AREA_DATA, NULL, ctx); goto loop; }
        "x"              { AreaChartParser(parser, AREA_X, NULL, ctx); goto loop; }
        "y"              { AreaChartParser(parser, AREA_Y, NULL, ctx); goto loop; }
        "opacity"        { AreaChartParser(parser, AREA_OPACITY, NULL, ctx); goto loop; }
        "interpolate"    { AreaChartParser(parser, AREA_INTERPOLATE, NULL, ctx); goto loop; }
        "stroke-width"   { AreaChartParser(parser, AREA_STROKE_WIDTH, NULL, ctx); goto loop; }
        "expr"           { AreaChartParser(parser, AREA_EXPR, NULL, ctx); goto loop; }
        "encoding"       { AreaChartParser(parser, AREA_ENCODING, NULL, ctx); goto loop; }
        "style"          { AreaChartParser(parser, AREA_STYLE, NULL, ctx); goto loop; }
        "true"           { AreaChartParser(parser, AREA_TRUE, NULL, ctx); goto loop; }
        "false"          { AreaChartParser(parser, AREA_FALSE, NULL, ctx); goto loop; }

        "{"  { AreaChartParser(parser, AREA_LBRACE, NULL, ctx); goto loop; }
        "}"  { AreaChartParser(parser, AREA_RBRACE, NULL, ctx); goto loop; }
        "["  { AreaChartParser(parser, AREA_LBRACKET, NULL, ctx); goto loop; }
        "]"  { AreaChartParser(parser, AREA_RBRACKET, NULL, ctx); goto loop; }
        ":"  { AreaChartParser(parser, AREA_COLON, NULL, ctx); goto loop; }
        ","  { AreaChartParser(parser, AREA_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            AreaChartParser(parser, AREA_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            AreaChartParser(parser, AREA_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            AreaChartParser(parser, AREA_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
