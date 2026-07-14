#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "geoshape_parser_gen.h"

void GeoshapeChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void geoshape_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "geoshape"         { GeoshapeChartParser(parser, GEOSHAPE_GEOSHAPE, NULL, ctx); goto loop; }
        "title"            { GeoshapeChartParser(parser, GEOSHAPE_TITLE, NULL, ctx); goto loop; }
        "width"            { GeoshapeChartParser(parser, GEOSHAPE_WIDTH, NULL, ctx); goto loop; }
        "height"           { GeoshapeChartParser(parser, GEOSHAPE_HEIGHT, NULL, ctx); goto loop; }
        "data"             { GeoshapeChartParser(parser, GEOSHAPE_DATA, NULL, ctx); goto loop; }
        "shape"            { GeoshapeChartParser(parser, GEOSHAPE_SHAPE, NULL, ctx); goto loop; }
        "color"            { GeoshapeChartParser(parser, GEOSHAPE_COLOR, NULL, ctx); goto loop; }
        "projection"       { GeoshapeChartParser(parser, GEOSHAPE_PROJECTION, NULL, ctx); goto loop; }
        "expr"             { GeoshapeChartParser(parser, GEOSHAPE_EXPR, NULL, ctx); goto loop; }
        "encoding"         { GeoshapeChartParser(parser, GEOSHAPE_ENCODING, NULL, ctx); goto loop; }
        "style"            { GeoshapeChartParser(parser, GEOSHAPE_STYLE, NULL, ctx); goto loop; }
        "true"             { GeoshapeChartParser(parser, GEOSHAPE_TRUE, NULL, ctx); goto loop; }
        "false"            { GeoshapeChartParser(parser, GEOSHAPE_FALSE, NULL, ctx); goto loop; }

        "{"  { GeoshapeChartParser(parser, GEOSHAPE_LBRACE, NULL, ctx); goto loop; }
        "}"  { GeoshapeChartParser(parser, GEOSHAPE_RBRACE, NULL, ctx); goto loop; }
        "["  { GeoshapeChartParser(parser, GEOSHAPE_LBRACKET, NULL, ctx); goto loop; }
        "]"  { GeoshapeChartParser(parser, GEOSHAPE_RBRACKET, NULL, ctx); goto loop; }
        ":"  { GeoshapeChartParser(parser, GEOSHAPE_COLON, NULL, ctx); goto loop; }
        ","  { GeoshapeChartParser(parser, GEOSHAPE_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            GeoshapeChartParser(parser, GEOSHAPE_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            GeoshapeChartParser(parser, GEOSHAPE_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            GeoshapeChartParser(parser, GEOSHAPE_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
