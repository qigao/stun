#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/bar/bar_parser.h"
#include "bar_parser_gen.h"

void *BarChartParserAlloc(void *(*mallocProc)(size_t));
void BarChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void BarChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void bar_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> bar_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("bar");

    void* parser = BarChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    bar_scan(&s, parser, &ctx);
    BarChartParser(parser, 0, nullptr, &ctx);
    BarChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "bar: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
