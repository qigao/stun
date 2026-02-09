#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kanban/kanban_ast.h"
#include "kanban_parser_gen.h"

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

void kanban_scan(Scanner *s, void *parser, KanbanParserContext *ctx) {
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
            KanbanParser(parser, KANBAN_NL, NULL, ctx); 
            goto loop; 
        }
        
        comment { 
            s->line++; 
            s->at_bol = 1;
            goto loop; 
        }

        ws {
            if(s->at_bol) {
                int len = (int)(s->cursor - token);
                char buf[32];
                snprintf(buf, 32, "%d", len);
                KanbanParser(parser, KANBAN_SPACELIST, strdup(buf), ctx);
                s->at_bol = 0;
            }
            goto loop;
        }

        "kanban" { 
            s->at_bol = 0; 
            KanbanParser(parser, KANBAN_KANBAN_KW, NULL, ctx); 
            goto loop; 
        }

        ":::" { 
            s->at_bol = 0; 
            goto class_state; 
        }

        "::icon(" { 
            s->at_bol = 0; 
            goto icon_state; 
        }

        // Shape Start -> NODE mode
        "((" { s->at_bol = 0; KanbanParser(parser, KANBAN_NODE_DSTART, strdup("(("), ctx); goto node_state; }
        "))" { s->at_bol = 0; KanbanParser(parser, KANBAN_NODE_DSTART, strdup("))"), ctx); goto node_state; } // Explosion?
        "(-" { s->at_bol = 0; KanbanParser(parser, KANBAN_NODE_DSTART, strdup("(-"), ctx); goto node_state; } // Cloud
        "-)" { s->at_bol = 0; KanbanParser(parser, KANBAN_NODE_DSTART, strdup("-)"), ctx); goto node_state; } // Explosion/Cloud Bang?
        "{{" { s->at_bol = 0; KanbanParser(parser, KANBAN_NODE_DSTART, strdup("{{"), ctx); goto node_state; }
        "("  { s->at_bol = 0; KanbanParser(parser, KANBAN_NODE_DSTART, strdup("("), ctx); goto node_state; }
        "["  { s->at_bol = 0; KanbanParser(parser, KANBAN_NODE_DSTART, strdup("["), ctx); goto node_state; }
        
        // Shape Data
        "@{" { 
            s->at_bol = 0; 
            goto shape_data_state; 
        }

        // Node ID - must start with non-whitespace/special to avoid consuming INDENT
        [^ \t\n\(\[\)\{\}@:\000] [^\n\(\[\)\{\}@\000]* {
            s->at_bol = 0;
            char* val = copy_token(token, s->cursor);
            KanbanParser(parser, KANBAN_NODE_ID, val, ctx);
            goto loop;
        }

        "\000" { return; }
        * { goto loop; }
    */

    node_state: // Inside shape
    token = s->cursor;
    /*!re2c
        "))" { KanbanParser(parser, KANBAN_NODE_DEND, strdup("))"), ctx); goto loop; }
        ")"  { KanbanParser(parser, KANBAN_NODE_DEND, strdup(")"), ctx); goto loop; }
        "]"  { KanbanParser(parser, KANBAN_NODE_DEND, strdup("]"), ctx); goto loop; }
        "}}" { KanbanParser(parser, KANBAN_NODE_DEND, strdup("}}"), ctx); goto loop; }
        "-)" { KanbanParser(parser, KANBAN_NODE_DEND, strdup("-)"), ctx); goto loop; }
        
        // Description string
        "\"" [^\"]* "\"" {
            KanbanParser(parser, KANBAN_NODE_DESCR, copy_quoted(token, s->cursor), ctx);
            goto node_state;
        }

        // Raw description
        [^\)\]\}\-\"\000]+ {
            KanbanParser(parser, KANBAN_NODE_DESCR, copy_token(token, s->cursor), ctx);
            goto node_state;
        }
        
        "\000" { return; }
        * { goto node_state; } 
    */

    class_state:
    token = s->cursor;
    /*!re2c
        [^\n]+ {
             KanbanParser(parser, KANBAN_CLASS, copy_token(token, s->cursor), ctx);
             goto loop;
        }
        "\n" { 
            s->line++; 
            s->at_bol = 1;
            goto loop; 
        }
    */

    icon_state:
    token = s->cursor;
    /*!re2c
        [^\)]+ {
             KanbanParser(parser, KANBAN_ICON, copy_token(token, s->cursor), ctx);
             goto icon_end;
        }
    */
    icon_end:
    token = s->cursor;
    /*!re2c
        ")" { goto loop; }
        * { goto loop; }
    */

    shape_data_state:
    token = s->cursor;
    /*!re2c
        "}" { goto loop; } // End of shape data
        
        "\"" [^\"]* "\"" {
             KanbanParser(parser, KANBAN_SHAPE_DATA, copy_quoted(token, s->cursor), ctx);
             goto shape_data_state;
        }
        
        [^}\"\000]+ {
             KanbanParser(parser, KANBAN_SHAPE_DATA, copy_token(token, s->cursor), ctx);
             goto shape_data_state;
        }
        
        "\000" { return; }
    */
}
