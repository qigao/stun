#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "errorband_parser_gen.h"

void ErrorbandChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void errorband_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "errorband"        { ErrorbandChartParser(parser, ERRORBAND_ERRORBAND, NULL, ctx); goto loop; }
        "title"            { ErrorbandChartParser(parser, ERRORBAND_TITLE, NULL, ctx); goto loop; }
        "width"            { ErrorbandChartParser(parser, ERRORBAND_WIDTH, NULL, ctx); goto loop; }
        "height"           { ErrorbandChartParser(parser, ERRORBAND_HEIGHT, NULL, ctx); goto loop; }
        "data"             { ErrorbandChartParser(parser, ERRORBAND_DATA, NULL, ctx); goto loop; }
        "x"                { ErrorbandChartParser(parser, ERRORBAND_X, NULL, ctx); goto loop; }
        "y"                { ErrorbandChartParser(parser, ERRORBAND_Y, NULL, ctx); goto loop; }
        "y-error"          { ErrorbandChartParser(parser, ERRORBAND_Y_ERROR, NULL, ctx); goto loop; }
        "color"            { ErrorbandChartParser(parser, ERRORBAND_COLOR, NULL, ctx); goto loop; }
        "expr"             { ErrorbandChartParser(parser, ERRORBAND_EXPR, NULL, ctx); goto loop; }
        "encoding"         { ErrorbandChartParser(parser, ERRORBAND_ENCODING, NULL, ctx); goto loop; }
        "style"            { ErrorbandChartParser(parser, ERRORBAND_STYLE, NULL, ctx); goto loop; }
        "true"             { ErrorbandChartParser(parser, ERRORBAND_TRUE, NULL, ctx); goto loop; }
        "false"            { ErrorbandChartParser(parser, ERRORBAND_FALSE, NULL, ctx); goto loop; }

        "{"  { ErrorbandChartParser(parser, ERRORBAND_LBRACE, NULL, ctx); goto loop; }
        "}"  { ErrorbandChartParser(parser, ERRORBAND_RBRACE, NULL, ctx); goto loop; }
        "["  { ErrorbandChartParser(parser, ERRORBAND_LBRACKET, NULL, ctx); goto loop; }
        "]"  { ErrorbandChartParser(parser, ERRORBAND_RBRACKET, NULL, ctx); goto loop; }
        ":"  { ErrorbandChartParser(parser, ERRORBAND_COLON, NULL, ctx); goto loop; }
        ","  { ErrorbandChartParser(parser, ERRORBAND_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            ErrorbandChartParser(parser, ERRORBAND_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            ErrorbandChartParser(parser, ERRORBAND_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            ErrorbandChartParser(parser, ERRORBAND_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
