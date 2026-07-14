#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xychart/xychart_ast.h"
#include "xychart_parser_gen.h"

void XYParser(void *parser, int token, void *value, XYParserContext *ctx);

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

void xychart_scan(Scanner *s, void *parser, XYParserContext *ctx) {
    const char *token;

    loop:
    token = s->cursor;
    
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        newline = [\r\n]+;
        ws = [ \t]+;
        
        newline { 
            s->line++; 
            XYParser(parser, XY_NL, NULL, ctx); 
            goto loop; 
        }
        
        ws { goto loop; }
        
        "%%" [^\r\n]* { goto loop; } // skip comments

        "xychart-beta" | "xychart" { XYParser(parser, XY_XY_KW, NULL, ctx); goto loop; }
        "vertical" { XYParser(parser, XY_ORIENTATION, strdup("vertical"), ctx); goto loop; }
        "horizontal" { XYParser(parser, XY_ORIENTATION, strdup("horizontal"), ctx); goto loop; }
        "title" { XYParser(parser, XY_TITLE_KW, NULL, ctx); goto loop; }
        "accTitle" { XYParser(parser, XY_ACC_TITLE_KW, NULL, ctx); goto loop; }
        "accDescr" { XYParser(parser, XY_ACC_DESCR_KW, NULL, ctx); goto loop; }
        "x-axis" { XYParser(parser, XY_X_AXIS_KW, NULL, ctx); goto loop; }
        "y-axis" { XYParser(parser, XY_Y_AXIS_KW, NULL, ctx); goto loop; }
        "line" { XYParser(parser, XY_LINE_KW, NULL, ctx); goto loop; }
        "bar" { XYParser(parser, XY_BAR_KW, NULL, ctx); goto loop; }
        
        "-->" { XYParser(parser, XY_ARROW, NULL, ctx); goto loop; }
        "[" { XYParser(parser, XY_SQR_START, NULL, ctx); goto loop; }
        "]" { XYParser(parser, XY_SQR_END, NULL, ctx); goto loop; }
        "," { XYParser(parser, XY_COMMA, NULL, ctx); goto loop; }
        ":" { XYParser(parser, XY_COLON, NULL, ctx); goto loop; }
        ";" { XYParser(parser, XY_SEMI, NULL, ctx); goto loop; }

        // Numbers
        [+-]? ( [0-9]+ ("." [0-9]*)? | "." [0-9]+ ) {
            XYParser(parser, XY_NUMBER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        // Quoted strings
        "\"" [^"\x00]* "\"" {
             if (s->cursor - token < 2) {
                 XYParser(parser, XY_TEXT, strdup(""), ctx);
             } else {
                 XYParser(parser, XY_TEXT, copy_token(token + 1, s->cursor - 1), ctx);
             }
             goto loop;
        }

        // Markdown strings (roughly)
        "\"`" [^`\x00]* "`\"" {
             if (s->cursor - token < 4) {
                 XYParser(parser, XY_TEXT, strdup(""), ctx);
             } else {
                 XYParser(parser, XY_TEXT, copy_token(token + 2, s->cursor - 2), ctx);
             }
             goto loop;
        }

        // AlphaNum and more (General Text)
        [a-zA-Z0-9_+=\.*#\-\&/]+ {
             XYParser(parser, XY_TEXT, copy_token(token, s->cursor), ctx);
             goto loop;
        }
        
        "\000" { return; }
        * { goto loop; }
    */
}
