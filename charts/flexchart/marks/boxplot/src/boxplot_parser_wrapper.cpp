#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/boxplot/boxplot_parser.h"
#include "boxplot_parser_gen.h"

void *BoxplotChartParserAlloc(void *(*mallocProc)(size_t));
void BoxplotChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void BoxplotChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void boxplot_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> boxplot_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("boxplot");

    void* parser = BoxplotChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    boxplot_scan(&s, parser, &ctx);
    BoxplotChartParser(parser, 0, nullptr, &ctx);
    BoxplotChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "boxplot: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
