#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "timeline/timeline_ast.h"
#include "timeline_parser_gen.h"

void *TimelineParserAlloc(void *(*mallocProc)(size_t));
void TimelineParser(void *yyp, int yymajor, void* yyminor, TimelineParserContext *ctx);
void TimelineParserFree(void *p, void (*freeProc)(void*));
void TimelineParserTrace(FILE *TraceFILE, char *zTracePrompt);

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void timeline_scan(Scanner *s, void *parser, TimelineParserContext *ctx);

TimelineDiagram* timeline_parse(const char* input) {
    if(!input) return NULL;

    TimelineParserContext ctx;
    ctx.diagram = timeline_create_diagram();
    if (!ctx.diagram) return NULL;
    ctx.current_section = NULL;
    ctx.current_period = NULL;
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = TimelineParserAlloc(malloc);
    if (!parser) {
        timeline_free_diagram(ctx.diagram);
        return NULL;
    }
    // TimelineParserTrace(stdout, "parser >> ");
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    timeline_scan(&s, parser, &ctx);
    
    // Lexer sends TL_EOF, we just need to send 0 to signal end of input
    TimelineParser(parser, 0, NULL, &ctx);
    
    TimelineParserFree(parser, free);

    if (ctx.error_count > 0) {
        if(ctx.error_message) {
             printf("Timeline Parser Error: %s\n", ctx.error_message);
             free(ctx.error_message);
        }
        timeline_free_diagram(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}
