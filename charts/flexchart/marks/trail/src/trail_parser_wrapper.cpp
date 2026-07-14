#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/trail/trail_parser.h"
#include "trail_parser_gen.h"

void *TrailChartParserAlloc(void *(*mallocProc)(size_t));
void TrailChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void TrailChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void trail_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> trail_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("trail");

    void* parser = TrailChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    trail_scan(&s, parser, &ctx);
    TrailChartParser(parser, 0, nullptr, &ctx);
    TrailChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "trail: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
