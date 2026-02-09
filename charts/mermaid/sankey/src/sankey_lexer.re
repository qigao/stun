#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sankey/sankey_ast.h"
#include "sankey_parser_gen.h"

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

void sankey_scan(Scanner *s, void *parser, SankeyParserContext *ctx) {
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
        comma = ",";
        quote = "\"";
        
        newline { 
            s->line++; 
            SankeyParser(parser, SANKEY_EOL, NULL, ctx); 
            goto loop; 
        }
        
        ws { goto loop; }
        
        "sankey-beta" { SankeyParser(parser, SANKEY_SANKEY_KW, NULL, ctx); goto loop; }
        "sankey" { SankeyParser(parser, SANKEY_SANKEY_KW, NULL, ctx); goto loop; }
        
        comma { SankeyParser(parser, SANKEY_COMMA, NULL, ctx); goto loop; }
        
        quote [^"\x00]* quote {
             // Handle quoted string, strip quotes
             if (s->cursor - token < 2) {
                 SankeyParser(parser, SANKEY_FIELD, strdup(""), ctx);
             } else {
                 SankeyParser(parser, SANKEY_FIELD, copy_token(token + 1, s->cursor - 1), ctx);
             }
             goto loop;
        }
        
        [^,\r\n\x00]+ {
             // Unquoted field
             // Include spaces but trim? For now just take it all.
             SankeyParser(parser, SANKEY_FIELD, copy_token(token, s->cursor), ctx);
             goto loop;
        }
        
        "\000" { return; }
        * { goto loop; }
    */
}
