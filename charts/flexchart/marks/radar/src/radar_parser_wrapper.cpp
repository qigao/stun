#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/radar/radar_parser.h"
#include "radar_parser_gen.h"

void *RadarChartParserAlloc(void *(*mallocProc)(size_t));
void RadarChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void RadarChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void radar_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> radar_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("radar");

    void* parser = RadarChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    radar_scan(&s, parser, &ctx);
    RadarChartParser(parser, 0, nullptr, &ctx);
    RadarChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "radar: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
