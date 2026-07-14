#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/tick/tick_parser.h"
#include "tick_parser_gen.h"

void *TickChartParserAlloc(void *(*mallocProc)(size_t));
void TickChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void TickChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void tick_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> tick_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("tick");

    void* parser = TickChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    tick_scan(&s, parser, &ctx);
    TickChartParser(parser, 0, nullptr, &ctx);
    TickChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "tick: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
