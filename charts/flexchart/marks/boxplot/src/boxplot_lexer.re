#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "boxplot_parser_gen.h"

void BoxplotChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void boxplot_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "boxplot"          { BoxplotChartParser(parser, BOXPLOT_BOXPLOT, NULL, ctx); goto loop; }
        "title"            { BoxplotChartParser(parser, BOXPLOT_TITLE, NULL, ctx); goto loop; }
        "width"            { BoxplotChartParser(parser, BOXPLOT_WIDTH, NULL, ctx); goto loop; }
        "height"           { BoxplotChartParser(parser, BOXPLOT_HEIGHT, NULL, ctx); goto loop; }
        "data"             { BoxplotChartParser(parser, BOXPLOT_DATA, NULL, ctx); goto loop; }
        "x"                { BoxplotChartParser(parser, BOXPLOT_X, NULL, ctx); goto loop; }
        "min"              { BoxplotChartParser(parser, BOXPLOT_MIN, NULL, ctx); goto loop; }
        "q1"               { BoxplotChartParser(parser, BOXPLOT_Q1, NULL, ctx); goto loop; }
        "median"           { BoxplotChartParser(parser, BOXPLOT_MEDIAN, NULL, ctx); goto loop; }
        "q3"               { BoxplotChartParser(parser, BOXPLOT_Q3, NULL, ctx); goto loop; }
        "max"              { BoxplotChartParser(parser, BOXPLOT_MAX, NULL, ctx); goto loop; }
        "color"            { BoxplotChartParser(parser, BOXPLOT_COLOR, NULL, ctx); goto loop; }
        "expr"             { BoxplotChartParser(parser, BOXPLOT_EXPR, NULL, ctx); goto loop; }
        "encoding"         { BoxplotChartParser(parser, BOXPLOT_ENCODING, NULL, ctx); goto loop; }
        "style"            { BoxplotChartParser(parser, BOXPLOT_STYLE, NULL, ctx); goto loop; }
        "true"             { BoxplotChartParser(parser, BOXPLOT_TRUE, NULL, ctx); goto loop; }
        "false"            { BoxplotChartParser(parser, BOXPLOT_FALSE, NULL, ctx); goto loop; }

        "{"  { BoxplotChartParser(parser, BOXPLOT_LBRACE, NULL, ctx); goto loop; }
        "}"  { BoxplotChartParser(parser, BOXPLOT_RBRACE, NULL, ctx); goto loop; }
        "["  { BoxplotChartParser(parser, BOXPLOT_LBRACKET, NULL, ctx); goto loop; }
        "]"  { BoxplotChartParser(parser, BOXPLOT_RBRACKET, NULL, ctx); goto loop; }
        ":"  { BoxplotChartParser(parser, BOXPLOT_COLON, NULL, ctx); goto loop; }
        ","  { BoxplotChartParser(parser, BOXPLOT_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            BoxplotChartParser(parser, BOXPLOT_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            BoxplotChartParser(parser, BOXPLOT_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            BoxplotChartParser(parser, BOXPLOT_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
