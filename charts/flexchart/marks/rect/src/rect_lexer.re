#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "rect_parser_gen.h"

void RectChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void rect_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "rect"             { RectChartParser(parser, RECT_RECT, NULL, ctx); goto loop; }
        "title"            { RectChartParser(parser, RECT_TITLE, NULL, ctx); goto loop; }
        "width"            { RectChartParser(parser, RECT_WIDTH, NULL, ctx); goto loop; }
        "height"           { RectChartParser(parser, RECT_HEIGHT, NULL, ctx); goto loop; }
        "data"             { RectChartParser(parser, RECT_DATA, NULL, ctx); goto loop; }
        "x"                { RectChartParser(parser, RECT_X, NULL, ctx); goto loop; }
        "y"                { RectChartParser(parser, RECT_Y, NULL, ctx); goto loop; }
        "color"            { RectChartParser(parser, RECT_COLOR, NULL, ctx); goto loop; }
        "cell-width"       { RectChartParser(parser, RECT_CELL_WIDTH, NULL, ctx); goto loop; }
        "cell-height"      { RectChartParser(parser, RECT_CELL_HEIGHT, NULL, ctx); goto loop; }
        "expr"             { RectChartParser(parser, RECT_EXPR, NULL, ctx); goto loop; }
        "encoding"         { RectChartParser(parser, RECT_ENCODING, NULL, ctx); goto loop; }
        "style"            { RectChartParser(parser, RECT_STYLE, NULL, ctx); goto loop; }
        "true"             { RectChartParser(parser, RECT_TRUE, NULL, ctx); goto loop; }
        "false"            { RectChartParser(parser, RECT_FALSE, NULL, ctx); goto loop; }

        "{"  { RectChartParser(parser, RECT_LBRACE, NULL, ctx); goto loop; }
        "}"  { RectChartParser(parser, RECT_RBRACE, NULL, ctx); goto loop; }
        "["  { RectChartParser(parser, RECT_LBRACKET, NULL, ctx); goto loop; }
        "]"  { RectChartParser(parser, RECT_RBRACKET, NULL, ctx); goto loop; }
        ":"  { RectChartParser(parser, RECT_COLON, NULL, ctx); goto loop; }
        ","  { RectChartParser(parser, RECT_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            RectChartParser(parser, RECT_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            RectChartParser(parser, RECT_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            RectChartParser(parser, RECT_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
