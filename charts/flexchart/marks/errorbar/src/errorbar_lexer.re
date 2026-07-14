#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "errorbar_parser_gen.h"

void ErrorbarChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void errorbar_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "errorbar"         { ErrorbarChartParser(parser, ERRORBAR_ERRORBAR, NULL, ctx); goto loop; }
        "title"            { ErrorbarChartParser(parser, ERRORBAR_TITLE, NULL, ctx); goto loop; }
        "width"            { ErrorbarChartParser(parser, ERRORBAR_WIDTH, NULL, ctx); goto loop; }
        "height"           { ErrorbarChartParser(parser, ERRORBAR_HEIGHT, NULL, ctx); goto loop; }
        "data"             { ErrorbarChartParser(parser, ERRORBAR_DATA, NULL, ctx); goto loop; }
        "x"                { ErrorbarChartParser(parser, ERRORBAR_X, NULL, ctx); goto loop; }
        "y-error-min"      { ErrorbarChartParser(parser, ERRORBAR_Y_ERROR_MIN, NULL, ctx); goto loop; }
        "y-error-max"      { ErrorbarChartParser(parser, ERRORBAR_Y_ERROR_MAX, NULL, ctx); goto loop; }
        "y-error"          { ErrorbarChartParser(parser, ERRORBAR_Y_ERROR, NULL, ctx); goto loop; }
        "y"                { ErrorbarChartParser(parser, ERRORBAR_Y, NULL, ctx); goto loop; }
        "color"            { ErrorbarChartParser(parser, ERRORBAR_COLOR, NULL, ctx); goto loop; }
        "cap-size"         { ErrorbarChartParser(parser, ERRORBAR_CAP_SIZE, NULL, ctx); goto loop; }
        "expr"             { ErrorbarChartParser(parser, ERRORBAR_EXPR, NULL, ctx); goto loop; }
        "encoding"         { ErrorbarChartParser(parser, ERRORBAR_ENCODING, NULL, ctx); goto loop; }
        "style"            { ErrorbarChartParser(parser, ERRORBAR_STYLE, NULL, ctx); goto loop; }
        "true"             { ErrorbarChartParser(parser, ERRORBAR_TRUE, NULL, ctx); goto loop; }
        "false"            { ErrorbarChartParser(parser, ERRORBAR_FALSE, NULL, ctx); goto loop; }

        "{"  { ErrorbarChartParser(parser, ERRORBAR_LBRACE, NULL, ctx); goto loop; }
        "}"  { ErrorbarChartParser(parser, ERRORBAR_RBRACE, NULL, ctx); goto loop; }
        "["  { ErrorbarChartParser(parser, ERRORBAR_LBRACKET, NULL, ctx); goto loop; }
        "]"  { ErrorbarChartParser(parser, ERRORBAR_RBRACKET, NULL, ctx); goto loop; }
        ":"  { ErrorbarChartParser(parser, ERRORBAR_COLON, NULL, ctx); goto loop; }
        ","  { ErrorbarChartParser(parser, ERRORBAR_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            ErrorbarChartParser(parser, ERRORBAR_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            ErrorbarChartParser(parser, ERRORBAR_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            ErrorbarChartParser(parser, ERRORBAR_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
