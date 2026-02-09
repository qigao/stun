#include <stdlib.h>
#include <string.h>
#include "classdiagram/classdiagram_ast.h"
#include "classdiagram_parser_gen.h"

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

void classdiagram_scan(Scanner *s, void *parser, ClassParserContext *ctx) {
    const char *token;

    loop:
    if (s->cursor >= s->limit || *s->cursor == '\0') {
        ClassParser(parser, CLASS_EOF, NULL, ctx);
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
        newline { s->line++; ClassParser(parser, CLASS_NL, NULL, ctx); goto loop; }
        comment { goto loop; }

        "classDiagram"      { ClassParser(parser, CLASS_START, NULL, ctx); goto loop; }
        "class"             { ClassParser(parser, CLASS_KW, NULL, ctx); goto loop; }
        "title"             { ClassParser(parser, CLASS_TITLE_KW, NULL, ctx); goto loop; }
        "direction"         { ClassParser(parser, CLASS_DIRECTION, NULL, ctx); goto loop; }
        "namespace"         { ClassParser(parser, CLASS_NAMESPACE, NULL, ctx); goto loop; }
        "note"              { ClassParser(parser, CLASS_NOTE, NULL, ctx); goto loop; }
        "for"               { ClassParser(parser, CLASS_FOR, NULL, ctx); goto loop; }
        "end"               { ClassParser(parser, CLASS_END, NULL, ctx); goto loop; }

        // Reverse relationships (arrow points left)
        "<|--" { ClassParser(parser, CLASS_REL_INHERIT_REV, NULL, ctx); goto loop; }
        "<|.." { ClassParser(parser, CLASS_REL_REALIZE_REV, NULL, ctx); goto loop; }
        "*--"  { ClassParser(parser, CLASS_REL_COMP_REV, NULL, ctx); goto loop; }
        "o--"  { ClassParser(parser, CLASS_REL_AGGR_REV, NULL, ctx); goto loop; }
        "<--"  { ClassParser(parser, CLASS_REL_ASSOC_REV, NULL, ctx); goto loop; }
        "<.."  { ClassParser(parser, CLASS_REL_DEP_REV, NULL, ctx); goto loop; }

        // Forward relationships
        "--|>" { ClassParser(parser, CLASS_REL_INHERIT, NULL, ctx); goto loop; }
        "..|>" { ClassParser(parser, CLASS_REL_REALIZE, NULL, ctx); goto loop; }
        "--*"  { ClassParser(parser, CLASS_REL_COMP, NULL, ctx); goto loop; }
        "--o"  { ClassParser(parser, CLASS_REL_AGGR, NULL, ctx); goto loop; }
        "-->"  { ClassParser(parser, CLASS_REL_ASSOC, NULL, ctx); goto loop; }
        "..>"  { ClassParser(parser, CLASS_REL_DEP, NULL, ctx); goto loop; }
        "--"   { ClassParser(parser, CLASS_REL_LINK, NULL, ctx); goto loop; }
        ".."   { ClassParser(parser, CLASS_REL_LINK, NULL, ctx); goto loop; }

        "+" { ClassParser(parser, CLASS_PLUS, NULL, ctx); goto loop; }
        "-" { ClassParser(parser, CLASS_MINUS, NULL, ctx); goto loop; }
        "#" { ClassParser(parser, CLASS_HASH, NULL, ctx); goto loop; }
        "~" { ClassParser(parser, CLASS_TILDE, NULL, ctx); goto loop; }

        ":" { ClassParser(parser, CLASS_COLON, NULL, ctx); goto loop; }
        "{" { ClassParser(parser, CLASS_LBRACE, NULL, ctx); goto loop; }
        "}" { ClassParser(parser, CLASS_RBRACE, NULL, ctx); goto loop; }
        "(" { ClassParser(parser, CLASS_LPAREN, NULL, ctx); goto loop; }
        ")" { ClassParser(parser, CLASS_RPAREN, NULL, ctx); goto loop; }
        "," { goto loop; }

        "\"" [^\"]* "\"" {
            ClassParser(parser, CLASS_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        "<<" [^>]+ ">>" {
            ClassParser(parser, CLASS_ANNOTATION, copy_token(token + 2, s->cursor - 2), ctx);
            goto loop;
        }

        [a-zA-Z_][a-zA-Z0-9_]* {
            ClassParser(parser, CLASS_IDENTIFIER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { ClassParser(parser, CLASS_EOF, NULL, ctx); return; }
        * { s->cursor++; goto loop; }
    */
}
