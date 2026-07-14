#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "radar_parser_gen.h"

void RadarChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void radar_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "radar"            { RadarChartParser(parser, RADAR_RADAR, NULL, ctx); goto loop; }
        "title"            { RadarChartParser(parser, RADAR_TITLE, NULL, ctx); goto loop; }
        "width"            { RadarChartParser(parser, RADAR_WIDTH, NULL, ctx); goto loop; }
        "height"           { RadarChartParser(parser, RADAR_HEIGHT, NULL, ctx); goto loop; }
        "data"             { RadarChartParser(parser, RADAR_DATA, NULL, ctx); goto loop; }
        "category"         { RadarChartParser(parser, RADAR_CATEGORY, NULL, ctx); goto loop; }
        "value"            { RadarChartParser(parser, RADAR_VALUE, NULL, ctx); goto loop; }
        "max"              { RadarChartParser(parser, RADAR_MAX, NULL, ctx); goto loop; }
        "fill-opacity"     { RadarChartParser(parser, RADAR_FILL_OPACITY, NULL, ctx); goto loop; }
        "grid-lines"       { RadarChartParser(parser, RADAR_GRID_LINES, NULL, ctx); goto loop; }
        "expr"             { RadarChartParser(parser, RADAR_EXPR, NULL, ctx); goto loop; }
        "encoding"         { RadarChartParser(parser, RADAR_ENCODING, NULL, ctx); goto loop; }
        "style"            { RadarChartParser(parser, RADAR_STYLE, NULL, ctx); goto loop; }
        "true"             { RadarChartParser(parser, RADAR_TRUE, NULL, ctx); goto loop; }
        "false"            { RadarChartParser(parser, RADAR_FALSE, NULL, ctx); goto loop; }

        "{"  { RadarChartParser(parser, RADAR_LBRACE, NULL, ctx); goto loop; }
        "}"  { RadarChartParser(parser, RADAR_RBRACE, NULL, ctx); goto loop; }
        "["  { RadarChartParser(parser, RADAR_LBRACKET, NULL, ctx); goto loop; }
        "]"  { RadarChartParser(parser, RADAR_RBRACKET, NULL, ctx); goto loop; }
        ":"  { RadarChartParser(parser, RADAR_COLON, NULL, ctx); goto loop; }
        ","  { RadarChartParser(parser, RADAR_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            RadarChartParser(parser, RADAR_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            RadarChartParser(parser, RADAR_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            RadarChartParser(parser, RADAR_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
