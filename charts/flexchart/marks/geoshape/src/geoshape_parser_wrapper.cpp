#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/geoshape/geoshape_parser.h"
#include "geoshape_parser_gen.h"

void *GeoshapeChartParserAlloc(void *(*mallocProc)(size_t));
void GeoshapeChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void GeoshapeChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void geoshape_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> geoshape_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("geoshape");

    void* parser = GeoshapeChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    geoshape_scan(&s, parser, &ctx);
    GeoshapeChartParser(parser, 0, nullptr, &ctx);
    GeoshapeChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "geoshape: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
