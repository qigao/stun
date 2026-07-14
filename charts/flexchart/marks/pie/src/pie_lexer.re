#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "pie_parser_gen.h"

void PieChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void pie_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "pie"              { PieChartParser(parser, PIE_PIE, NULL, ctx); goto loop; }
        "title"            { PieChartParser(parser, PIE_TITLE, NULL, ctx); goto loop; }
        "width"            { PieChartParser(parser, PIE_WIDTH, NULL, ctx); goto loop; }
        "height"           { PieChartParser(parser, PIE_HEIGHT, NULL, ctx); goto loop; }
        "data"             { PieChartParser(parser, PIE_DATA, NULL, ctx); goto loop; }
        "theta"            { PieChartParser(parser, PIE_THETA, NULL, ctx); goto loop; }
        "color"            { PieChartParser(parser, PIE_COLOR, NULL, ctx); goto loop; }
        "inner-radius"     { PieChartParser(parser, PIE_INNER_RADIUS, NULL, ctx); goto loop; }
        "outer-radius"     { PieChartParser(parser, PIE_OUTER_RADIUS, NULL, ctx); goto loop; }
        "pad-angle"        { PieChartParser(parser, PIE_PAD_ANGLE, NULL, ctx); goto loop; }
        "expr"             { PieChartParser(parser, PIE_EXPR, NULL, ctx); goto loop; }
        "encoding"         { PieChartParser(parser, PIE_ENCODING, NULL, ctx); goto loop; }
        "style"            { PieChartParser(parser, PIE_STYLE, NULL, ctx); goto loop; }
        "true"             { PieChartParser(parser, PIE_TRUE, NULL, ctx); goto loop; }
        "false"            { PieChartParser(parser, PIE_FALSE, NULL, ctx); goto loop; }

        "{"  { PieChartParser(parser, PIE_LBRACE, NULL, ctx); goto loop; }
        "}"  { PieChartParser(parser, PIE_RBRACE, NULL, ctx); goto loop; }
        "["  { PieChartParser(parser, PIE_LBRACKET, NULL, ctx); goto loop; }
        "]"  { PieChartParser(parser, PIE_RBRACKET, NULL, ctx); goto loop; }
        ":"  { PieChartParser(parser, PIE_COLON, NULL, ctx); goto loop; }
        ","  { PieChartParser(parser, PIE_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            PieChartParser(parser, PIE_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            PieChartParser(parser, PIE_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            PieChartParser(parser, PIE_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
