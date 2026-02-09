#include <stdlib.h>
#include <string.h>
#include "er/er_ast.h"
#include "er_parser_gen.h"

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

void er_scan(Scanner *s, void *parser, ERParserContext *ctx) {
    const char *token;

    loop:
    if (s->cursor >= s->limit || *s->cursor == '\0') {
        ERParser(parser, ER_EOF, NULL, ctx);
        return;
    }
    
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        white = [ \t\r]+;
        newline = [\n];
        comment = ("%%"| "#") [^\n\000]*;

        white { goto loop; }
        newline { s->line++; ERParser(parser, ER_NL, NULL, ctx); goto loop; }
        comment { goto loop; }

        "erDiagram"         { ERParser(parser, ER_START, NULL, ctx); goto loop; }
        
        "{"                 { ERParser(parser, ER_LBRACE, NULL, ctx); goto loop; }
        "}"                 { ERParser(parser, ER_RBRACE, NULL, ctx); goto loop; }
        ":"                 { ERParser(parser, ER_COLON, NULL, ctx); goto loop; }

        "|o" | "o|" | "zero or one" | "one or zero" { ERParser(parser, ER_ZERO_OR_ONE, NULL, ctx); goto loop; }
        "}o" | "o{" | "zero or more" | "zero or many" | "0+" { ERParser(parser, ER_ZERO_OR_MORE, NULL, ctx); goto loop; }
        "}|" | "|{" | "one or more" | "one or many" | "1+" { ERParser(parser, ER_ONE_OR_MORE, NULL, ctx); goto loop; }
        "||" | "only one" | "1" { ERParser(parser, ER_ONLY_ONE, NULL, ctx); goto loop; }

        "--" | "to"         { ERParser(parser, ER_REL_IDENTIFYING, NULL, ctx); goto loop; }
        ".."                { ERParser(parser, ER_REL_NON_IDENTIFYING, NULL, ctx); goto loop; }

        "PK"                { ERParser(parser, ER_PK, NULL, ctx); goto loop; }
        "FK"                { ERParser(parser, ER_FK, NULL, ctx); goto loop; }
        "UK"                { ERParser(parser, ER_UK, NULL, ctx); goto loop; }

        "\"" [^\"]* "\"" {
            ERParser(parser, ER_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_]* ("-" [a-zA-Z0-9_]+)* {
            ERParser(parser, ER_ID, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { ERParser(parser, ER_EOF, NULL, ctx); return; }
        * { s->cursor++; goto loop; }
    */
}
