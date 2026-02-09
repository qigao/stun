#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sankey/sankey_ast.h"
#include "sankey_parser_gen.h"

void *SankeyParserAlloc(void *(*mallocProc)(size_t));
void SankeyParser(void *yyp, int yymajor, void* yyminor, SankeyParserContext *ctx);
void SankeyParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void sankey_scan(Scanner *s, void *parser, SankeyParserContext *ctx);

SankeyDiagram* sankey_parse(const char* input) {
    if(!input) return NULL;

    SankeyParserContext ctx;
    ctx.diagram = sankey_create_diagram();
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = SankeyParserAlloc(malloc);
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    sankey_scan(&s, parser, &ctx);
    
    SankeyParser(parser, SANKEY_EOL, NULL, &ctx);
    SankeyParser(parser, 0, NULL, &ctx);
    SankeyParserFree(parser, free);

    if (ctx.error_count > 0) {
        if(ctx.error_message) {
             printf("Sankey Parser Error: %s\n", ctx.error_message);
             free(ctx.error_message);
        }
        sankey_free_diagram(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}
