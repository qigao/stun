#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mindmap/mindmap_ast.h"
#include "mindmap_parser_gen.h"

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
    int at_bol;
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

void mindmap_scan(Scanner *s, void *parser, MindmapParserContext *ctx) {
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
            s->at_bol = 1; 
            MindmapParser(parser, MINDMAP_NL, NULL, ctx); 
            goto loop; 
        }
        comment { 
            s->line++; 
            s->at_bol = 1;
            MindmapParser(parser, MINDMAP_NL, NULL, ctx); 
            goto loop; 
        }
        
        ws { 
            if (s->at_bol) {
                int length = (int)(s->cursor - token);
                char buf[32];
                snprintf(buf, 32, "%d", length);
                MindmapParser(parser, MINDMAP_INDENT, strdup(buf), ctx); 
                s->at_bol = 0;
            }
            goto loop; 
        }

        "mindmap"           { s->at_bol = 0; MindmapParser(parser, MINDMAP_START, strdup("mindmap"), ctx); goto loop; }
        ":::"               { s->at_bol = 0; goto class_state; }
        "::icon("           { s->at_bol = 0; goto icon_state; }

        // Node Shapes Start (Transition to shape_state)
        "(("                { s->at_bol = 0; MindmapParser(parser, MINDMAP_SHAPE_CIRCLE_START, strdup("(("), ctx); goto shape_state; }
        "))"                { s->at_bol = 0; MindmapParser(parser, MINDMAP_SHAPE_BANG_START, strdup("))"), ctx); goto shape_state; }
        "("                 { s->at_bol = 0; MindmapParser(parser, MINDMAP_SHAPE_ROUNDED_START, strdup("("), ctx); goto shape_state; }
        "["                 { s->at_bol = 0; MindmapParser(parser, MINDMAP_SHAPE_SQUARE_START, strdup("["), ctx); goto shape_state; }
        "{{"                { s->at_bol = 0; MindmapParser(parser, MINDMAP_SHAPE_HEXAGON_START, strdup("{{"), ctx); goto shape_state; }
        "(-"                { s->at_bol = 0; MindmapParser(parser, MINDMAP_SHAPE_CLOUD_START, strdup("(-"), ctx); goto shape_state; }
        "-)"                { s->at_bol = 0; MindmapParser(parser, MINDMAP_SHAPE_BANG_START, strdup("-)"), ctx); goto shape_state; } 
        
        // Quoted string
        "\"" [^\"]* "\"" {
            s->at_bol = 0;
            MindmapParser(parser, MINDMAP_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        // Identifiers
        [a-zA-Z0-9_]+ {
            s->at_bol = 0;
            MindmapParser(parser, MINDMAP_ID, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        // Catch-all
        "\000" { return; }
        * { 
            s->at_bol = 0;
            goto loop; 
        } 
    */

    shape_state:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        // Node Shapes End (Return to loop)
        "))"                { MindmapParser(parser, MINDMAP_SHAPE_CIRCLE_END, strdup("))"), ctx); goto loop; }
        ")"                 { MindmapParser(parser, MINDMAP_SHAPE_ROUNDED_END, strdup(")"), ctx); goto loop; }
        "]"                 { MindmapParser(parser, MINDMAP_SHAPE_SQUARE_END, strdup("]"), ctx); goto loop; }
        "}}"                { MindmapParser(parser, MINDMAP_SHAPE_HEXAGON_END, strdup("}}"), ctx); goto loop; }
        "-)"                { MindmapParser(parser, MINDMAP_SHAPE_CLOUD_END, strdup("-)"), ctx); goto loop; }
        "(("                { MindmapParser(parser, MINDMAP_SHAPE_BANG_END, strdup("(("), ctx); goto loop; }

        // Content inside shape
        "\"" [^\"]* "\"" {
            MindmapParser(parser, MINDMAP_STRING, copy_quoted(token, s->cursor), ctx);
            goto shape_state;
        }


        [a-zA-Z0-9_ \t]+ {
            MindmapParser(parser, MINDMAP_ID, copy_token(token, s->cursor), ctx);
            goto shape_state;
        }

        // [ \t]+ { goto shape_state; } // Merged into ID rule
        
        * { goto shape_state; } // Ignore other chars (or error?)
    */

    class_state:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        [a-zA-Z0-9_\-]+ {
            MindmapParser(parser, MINDMAP_CLASS, copy_token(token, s->cursor), ctx);
            goto loop;
        }
        * { goto loop; } 
    */

    icon_state:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        [^\)]+ {
             MindmapParser(parser, MINDMAP_ICON, copy_token(token, s->cursor), ctx);
             goto icon_end;
        }
    */
    icon_end:
    token = s->cursor;
    /*!re2c
        ")" { goto loop; }
        * { goto loop; }
    */
}
