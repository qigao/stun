#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "journey/journey_ast.h"
#include "journey_parser_gen.h"

void *JourneyParserAlloc(void *(*mallocProc)(size_t));
void JourneyParser(void *yyp, int yymajor, void* yyminor, JourneyParserContext *ctx);
void JourneyParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
    int at_bol;
} Scanner;

void journey_scan(Scanner *s, void *parser, JourneyParserContext *ctx);

JourneyDiagram* journey_parse(const char* input) {
    if(!input) return NULL;

    JourneyParserContext ctx;
    ctx.diagram = journey_create_diagram();
    ctx.error_count = 0;
    ctx.error_message = NULL;
    ctx.current_section = NULL;

    void* parser = JourneyParserAlloc(malloc);
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;
    s.at_bol = 1;

    journey_scan(&s, parser, &ctx);
    
    JourneyParser(parser, 0, NULL, &ctx);
    JourneyParserFree(parser, free);

    if (ctx.error_count > 0) {
        if(ctx.error_message) {
             printf("Journey Parser Error: %s\n", ctx.error_message);
             free(ctx.error_message);
        }
        journey_free_diagram(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}
