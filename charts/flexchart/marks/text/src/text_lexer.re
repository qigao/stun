#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "text_parser_gen.h"

void TextChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void text_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "text"             { TextChartParser(parser, TEXT_TEXT_KW, NULL, ctx); goto loop; }
        "title"            { TextChartParser(parser, TEXT_TITLE, NULL, ctx); goto loop; }
        "width"            { TextChartParser(parser, TEXT_WIDTH, NULL, ctx); goto loop; }
        "height"           { TextChartParser(parser, TEXT_HEIGHT, NULL, ctx); goto loop; }
        "data"             { TextChartParser(parser, TEXT_DATA, NULL, ctx); goto loop; }
        "x"                { TextChartParser(parser, TEXT_X, NULL, ctx); goto loop; }
        "y"                { TextChartParser(parser, TEXT_Y, NULL, ctx); goto loop; }
        "text-field"       { TextChartParser(parser, TEXT_TEXT, NULL, ctx); goto loop; }
        "color"            { TextChartParser(parser, TEXT_COLOR, NULL, ctx); goto loop; }
        "font-size"        { TextChartParser(parser, TEXT_FONT_SIZE, NULL, ctx); goto loop; }
        "expr"             { TextChartParser(parser, TEXT_EXPR, NULL, ctx); goto loop; }
        "encoding"         { TextChartParser(parser, TEXT_ENCODING, NULL, ctx); goto loop; }
        "style"            { TextChartParser(parser, TEXT_STYLE, NULL, ctx); goto loop; }
        "true"             { TextChartParser(parser, TEXT_TRUE, NULL, ctx); goto loop; }
        "false"            { TextChartParser(parser, TEXT_FALSE, NULL, ctx); goto loop; }

        "{"  { TextChartParser(parser, TEXT_LBRACE, NULL, ctx); goto loop; }
        "}"  { TextChartParser(parser, TEXT_RBRACE, NULL, ctx); goto loop; }
        "["  { TextChartParser(parser, TEXT_LBRACKET, NULL, ctx); goto loop; }
        "]"  { TextChartParser(parser, TEXT_RBRACKET, NULL, ctx); goto loop; }
        ":"  { TextChartParser(parser, TEXT_COLON, NULL, ctx); goto loop; }
        ","  { TextChartParser(parser, TEXT_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            TextChartParser(parser, TEXT_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            TextChartParser(parser, TEXT_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            TextChartParser(parser, TEXT_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
