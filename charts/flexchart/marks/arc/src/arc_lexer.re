#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "arc_parser_gen.h"

void ArcChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void arc_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "arc"              { ArcChartParser(parser, ARC_ARC, NULL, ctx); goto loop; }
        "title"            { ArcChartParser(parser, ARC_TITLE, NULL, ctx); goto loop; }
        "width"            { ArcChartParser(parser, ARC_WIDTH, NULL, ctx); goto loop; }
        "height"           { ArcChartParser(parser, ARC_HEIGHT, NULL, ctx); goto loop; }
        "data"             { ArcChartParser(parser, ARC_DATA, NULL, ctx); goto loop; }
        "theta"            { ArcChartParser(parser, ARC_THETA, NULL, ctx); goto loop; }
        "color"            { ArcChartParser(parser, ARC_COLOR, NULL, ctx); goto loop; }
        "inner-radius"     { ArcChartParser(parser, ARC_INNER_RADIUS, NULL, ctx); goto loop; }
        "outer-radius"     { ArcChartParser(parser, ARC_OUTER_RADIUS, NULL, ctx); goto loop; }
        "pad-angle"        { ArcChartParser(parser, ARC_PAD_ANGLE, NULL, ctx); goto loop; }
        "expr"             { ArcChartParser(parser, ARC_EXPR, NULL, ctx); goto loop; }
        "encoding"         { ArcChartParser(parser, ARC_ENCODING, NULL, ctx); goto loop; }
        "style"            { ArcChartParser(parser, ARC_STYLE, NULL, ctx); goto loop; }
        "true"             { ArcChartParser(parser, ARC_TRUE, NULL, ctx); goto loop; }
        "false"            { ArcChartParser(parser, ARC_FALSE, NULL, ctx); goto loop; }

        "{"  { ArcChartParser(parser, ARC_LBRACE, NULL, ctx); goto loop; }
        "}"  { ArcChartParser(parser, ARC_RBRACE, NULL, ctx); goto loop; }
        "["  { ArcChartParser(parser, ARC_LBRACKET, NULL, ctx); goto loop; }
        "]"  { ArcChartParser(parser, ARC_RBRACKET, NULL, ctx); goto loop; }
        ":"  { ArcChartParser(parser, ARC_COLON, NULL, ctx); goto loop; }
        ","  { ArcChartParser(parser, ARC_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            ArcChartParser(parser, ARC_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            ArcChartParser(parser, ARC_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            ArcChartParser(parser, ARC_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
