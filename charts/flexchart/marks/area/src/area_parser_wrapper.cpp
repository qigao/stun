#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/area/area_parser.h"
#include "area_parser_gen.h"

void *AreaChartParserAlloc(void *(*mallocProc)(size_t));
void AreaChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void AreaChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void area_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> area_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("area");

    void* parser = AreaChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    area_scan(&s, parser, &ctx);
    AreaChartParser(parser, 0, nullptr, &ctx);
    AreaChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "area: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
