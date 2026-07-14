#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/point/point_parser.h"
#include "point_parser_gen.h"

void *PointChartParserAlloc(void *(*mallocProc)(size_t));
void PointChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void PointChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void point_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> point_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("point");

    void* parser = PointChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    point_scan(&s, parser, &ctx);
    PointChartParser(parser, 0, nullptr, &ctx);
    PointChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "point: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
