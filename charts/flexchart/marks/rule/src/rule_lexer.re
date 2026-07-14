#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "rule_parser_gen.h"

void RuleChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void rule_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "rule"              { RuleChartParser(parser, RULE_RULE, NULL, ctx); goto loop; }
        "title"             { RuleChartParser(parser, RULE_TITLE, NULL, ctx); goto loop; }
        "width"             { RuleChartParser(parser, RULE_WIDTH, NULL, ctx); goto loop; }
        "height"            { RuleChartParser(parser, RULE_HEIGHT, NULL, ctx); goto loop; }
        "data"              { RuleChartParser(parser, RULE_DATA, NULL, ctx); goto loop; }
        "x2"                { RuleChartParser(parser, RULE_X2, NULL, ctx); goto loop; }
        "y2"                { RuleChartParser(parser, RULE_Y2, NULL, ctx); goto loop; }
        "x"                 { RuleChartParser(parser, RULE_X, NULL, ctx); goto loop; }
        "y"                 { RuleChartParser(parser, RULE_Y, NULL, ctx); goto loop; }
        "color"             { RuleChartParser(parser, RULE_COLOR, NULL, ctx); goto loop; }
        "stroke-width"      { RuleChartParser(parser, RULE_STROKE_WIDTH, NULL, ctx); goto loop; }
        "expr"              { RuleChartParser(parser, RULE_EXPR, NULL, ctx); goto loop; }
        "encoding"          { RuleChartParser(parser, RULE_ENCODING, NULL, ctx); goto loop; }
        "style"             { RuleChartParser(parser, RULE_STYLE, NULL, ctx); goto loop; }
        "true"              { RuleChartParser(parser, RULE_TRUE, NULL, ctx); goto loop; }
        "false"             { RuleChartParser(parser, RULE_FALSE, NULL, ctx); goto loop; }

        "{"  { RuleChartParser(parser, RULE_LBRACE, NULL, ctx); goto loop; }
        "}"  { RuleChartParser(parser, RULE_RBRACE, NULL, ctx); goto loop; }
        "["  { RuleChartParser(parser, RULE_LBRACKET, NULL, ctx); goto loop; }
        "]"  { RuleChartParser(parser, RULE_RBRACKET, NULL, ctx); goto loop; }
        ":"  { RuleChartParser(parser, RULE_COLON, NULL, ctx); goto loop; }
        ","  { RuleChartParser(parser, RULE_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            RuleChartParser(parser, RULE_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            RuleChartParser(parser, RULE_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            RuleChartParser(parser, RULE_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
