#include <stdlib.h>
#include <string.h>
#include "flexchart/common/mark_parser_context.h"
#include "image_parser_gen.h"

void ImageChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);

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

void image_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx) {
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

        "image"            { ImageChartParser(parser, IMAGE_IMAGE, NULL, ctx); goto loop; }
        "title"            { ImageChartParser(parser, IMAGE_TITLE, NULL, ctx); goto loop; }
        "width"            { ImageChartParser(parser, IMAGE_WIDTH, NULL, ctx); goto loop; }
        "height"           { ImageChartParser(parser, IMAGE_HEIGHT, NULL, ctx); goto loop; }
        "data"             { ImageChartParser(parser, IMAGE_DATA, NULL, ctx); goto loop; }
        "x"                { ImageChartParser(parser, IMAGE_X, NULL, ctx); goto loop; }
        "y"                { ImageChartParser(parser, IMAGE_Y, NULL, ctx); goto loop; }
        "url"              { ImageChartParser(parser, IMAGE_URL, NULL, ctx); goto loop; }
        "img-width"        { ImageChartParser(parser, IMAGE_IMG_WIDTH, NULL, ctx); goto loop; }
        "img-height"       { ImageChartParser(parser, IMAGE_IMG_HEIGHT, NULL, ctx); goto loop; }
        "expr"             { ImageChartParser(parser, IMAGE_EXPR, NULL, ctx); goto loop; }
        "encoding"         { ImageChartParser(parser, IMAGE_ENCODING, NULL, ctx); goto loop; }
        "style"            { ImageChartParser(parser, IMAGE_STYLE, NULL, ctx); goto loop; }
        "true"             { ImageChartParser(parser, IMAGE_TRUE, NULL, ctx); goto loop; }
        "false"            { ImageChartParser(parser, IMAGE_FALSE, NULL, ctx); goto loop; }

        "{"  { ImageChartParser(parser, IMAGE_LBRACE, NULL, ctx); goto loop; }
        "}"  { ImageChartParser(parser, IMAGE_RBRACE, NULL, ctx); goto loop; }
        "["  { ImageChartParser(parser, IMAGE_LBRACKET, NULL, ctx); goto loop; }
        "]"  { ImageChartParser(parser, IMAGE_RBRACKET, NULL, ctx); goto loop; }
        ":"  { ImageChartParser(parser, IMAGE_COLON, NULL, ctx); goto loop; }
        ","  { ImageChartParser(parser, IMAGE_COMMA, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            ImageChartParser(parser, IMAGE_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        number {
            ImageChartParser(parser, IMAGE_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_\-]* {
            ImageChartParser(parser, IMAGE_IDENT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { ctx->report_unexpected_character((unsigned char)*token, s->line); return; }
    */
}
