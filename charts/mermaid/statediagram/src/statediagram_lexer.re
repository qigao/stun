#include <stdlib.h>
#include <string.h>
#include "statediagram/statediagram_ast.h"
#include "statediagram_parser_gen.h"

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

static char* copy_quoted_state(const char* start, const char* end) {
    // "desc" as id -> need to extract desc
    // Assuming this handles simple "quoted"
    if (end - start < 2) return strdup("");
    size_t len = (end - start) - 2;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start + 1, len);
    res[len] = '\0';
    return res;
}

void statediagram_scan(Scanner *s, void *parser, StateParserContext *ctx) {
    const char *token;

    loop:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        white = [ \t\r]+;
        newline = [\n];
        comment = ("%%"| "#") [^\n]* newline;

        white { goto loop; }
        newline { s->line++; StateParser(parser, STATE_NL, NULL, ctx); goto loop; }
        comment { s->line++; StateParser(parser, STATE_NL, NULL, ctx); goto loop; }

        "stateDiagram-v2"   { StateParser(parser, STATE_SD, NULL, ctx); goto loop; }
        "stateDiagram"      { StateParser(parser, STATE_SD, NULL, ctx); goto loop; }
        
        "state"             { StateParser(parser, STATE_KW, NULL, ctx); goto loop; }
        "as"                { StateParser(parser, STATE_AS, NULL, ctx); goto loop; }
        "note"              { StateParser(parser, STATE_NOTE, NULL, ctx); goto loop; }
        "left of"           { StateParser(parser, STATE_LEFT_OF, NULL, ctx); goto loop; }
        "right of"          { StateParser(parser, STATE_RIGHT_OF, NULL, ctx); goto loop; }
        
        "direction"         { StateParser(parser, STATE_DIRECTION, NULL, ctx); goto loop; }
        "hide empty description" { StateParser(parser, STATE_HIDE_EMPTY, NULL, ctx); goto loop; }
        
        "[*]"               { StateParser(parser, STATE_START_END, strdup("[*]"), ctx); goto loop; }
        "-->"               { StateParser(parser, STATE_ARROW, NULL, ctx); goto loop; }
        "--"                { StateParser(parser, STATE_CONCURRENT, NULL, ctx); goto loop; }
        
        ":" [ \t]* {
            // Capture everything until newline as TXT
            const char* start = s->cursor;
            while (s->cursor < s->limit && *s->cursor != '\n' && *s->cursor != '\r') {
                s->cursor++;
            }
            StateParser(parser, STATE_COLON, NULL, ctx);
            StateParser(parser, STATE_TXT, copy_token(start, s->cursor), ctx);
            goto loop;
        }
        "{"                 { StateParser(parser, STATE_LBRACE, NULL, ctx); goto loop; }
        "}"                 { StateParser(parser, STATE_RBRACE, NULL, ctx); goto loop; }

        "<<fork>>"          { StateParser(parser, STATE_FORK, NULL, ctx); goto loop; }
        "<<join>>"          { StateParser(parser, STATE_JOIN, NULL, ctx); goto loop; }
        "<<choice>>"        { StateParser(parser, STATE_CHOICE, NULL, ctx); goto loop; }
        "[[fork]]"          { StateParser(parser, STATE_FORK, NULL, ctx); goto loop; }
        "[[join]]"          { StateParser(parser, STATE_JOIN, NULL, ctx); goto loop; }
        "[[choice]]"        { StateParser(parser, STATE_CHOICE, NULL, ctx); goto loop; }

        // Quoted string
        "\"" [^\"]* "\"" {
            StateParser(parser, STATE_STRING_LITERAL, copy_quoted_state(token, s->cursor), ctx);
            goto loop;
        }

        // Identifiers
        [a-zA-Z0-9_\-]+ {
            StateParser(parser, STATE_ID, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { goto loop; }
    */
}
