#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "quadrant/quadrant_ast.h"
#include "quadrant_parser_gen.h"

void QuadrantParser(void *parser, int token, void *value,
                    QuadrantParserContext *ctx);

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

void quadrant_scan(Scanner *s, void *parser, QuadrantParserContext *ctx) {
    const char *token;

    loop:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        newline = [\n];
        ws = [ \t]+;
        comment = ("%%"| "#") [^\n]* newline;

        newline { 
            s->line++; 
            QuadrantParser(parser, QUADRANT_NL, NULL, ctx); 
            goto loop; 
        }
        
        comment { 
            s->line++; 
            goto loop; 
        }
        
        ws { goto loop; }

        "quadrantChart" { 
            QuadrantParser(parser, QUADRANT_QUADRANT_KW, NULL, ctx); 
            goto loop; 
        }

        "x-axis" { QuadrantParser(parser, QUADRANT_X_AXIS, NULL, ctx); goto loop; }
        "y-axis" { QuadrantParser(parser, QUADRANT_Y_AXIS, NULL, ctx); goto loop; }
        "-->" { QuadrantParser(parser, QUADRANT_AXIS_DELIM, NULL, ctx); goto loop; }

        "quadrant-1" { QuadrantParser(parser, QUADRANT_QUADRANT_1, NULL, ctx); goto loop; }
        "quadrant-2" { QuadrantParser(parser, QUADRANT_QUADRANT_2, NULL, ctx); goto loop; }
        "quadrant-3" { QuadrantParser(parser, QUADRANT_QUADRANT_3, NULL, ctx); goto loop; }
        "quadrant-4" { QuadrantParser(parser, QUADRANT_QUADRANT_4, NULL, ctx); goto loop; }

        "title" [ \t] [^\n#;]* {
             // Extract title
             const char* val_start = token + 5;
             while(*val_start == ' ' || *val_start == '\t') val_start++;
             QuadrantParser(parser, QUADRANT_TITLE, copy_token(val_start, s->cursor), ctx);
             goto loop;
        }

        "accTitle" [ \t]* ":" [ \t]* [^\n#;]* {
             const char* val_start = token;
             while(*val_start != ':') val_start++;
             val_start++; // Skip :
             while(*val_start == ' ' || *val_start == '\t') val_start++;
             QuadrantParser(parser, QUADRANT_ACC_TITLE, copy_token(val_start, s->cursor), ctx);
             goto loop;
        }

        "accDescr" [ \t]* ":" [ \t]* [^\n#;]* {
             const char* val_start = token;
             while(*val_start != ':') val_start++;
             val_start++; // Skip :
             while(*val_start == ' ' || *val_start == '\t') val_start++;
             QuadrantParser(parser, QUADRANT_ACC_DESCR, copy_token(val_start, s->cursor), ctx);
             goto loop;
        }

        // Point coordinates [x, y]
        "[" [ \t]* {
             QuadrantParser(parser, QUADRANT_POINT_START, NULL, ctx);
             goto point_x_state;
        }

        ":::" [a-zA-Z0-9_]+ {
             QuadrantParser(parser, QUADRANT_CLASS_NAME, copy_token(token + 3, s->cursor), ctx);
             goto loop;
        }

        // Text (Axis labels, Quadrant text, Point text)
        [a-zA-Z0-9_\-\.]+ {
             QuadrantParser(parser, QUADRANT_TEXT, copy_token(token, s->cursor), ctx);
             goto loop;
        }

        // Quoted string as TEXT
        "\"" [^\"]* "\"" {
             // unquote
             if (s->cursor - token < 2) { // Should not happen
                  QuadrantParser(parser, QUADRANT_TEXT, strdup(""), ctx);
             } else {
                  QuadrantParser(parser, QUADRANT_TEXT, copy_token(token + 1, s->cursor - 1), ctx);
             }
             goto loop;
        }

        "\000" { return; }
        * { goto loop; }
    */

    point_x_state:
    token = s->cursor;
    /*!re2c
        [^,\]\x00]+ {
             QuadrantParser(parser, QUADRANT_POINT_X, copy_token(token, s->cursor), ctx);
             goto point_comma_state;
        }
    */

    point_comma_state:
    token = s->cursor;
    /*!re2c
        [ \t]* "," [ \t]* {
             goto point_y_state;
        }
    */

    point_y_state:
    token = s->cursor;
    /*!re2c
        [^\]\x00]+ {
             QuadrantParser(parser, QUADRANT_POINT_Y, copy_token(token, s->cursor), ctx);
             goto point_end_state;
        }
    */

    point_end_state:
    token = s->cursor;
    /*!re2c
        [ \t]* "]" {
             QuadrantParser(parser, QUADRANT_POINT_END, NULL, ctx);
             goto loop;
        }
    */
}
