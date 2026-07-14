#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/pie/pie_parser.h"
#include "pie_parser_gen.h"

void *PieChartParserAlloc(void *(*mallocProc)(size_t));
void PieChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void PieChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void pie_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> pie_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("pie");

    void* parser = PieChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    pie_scan(&s, parser, &ctx);
    PieChartParser(parser, 0, nullptr, &ctx);
    PieChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "pie: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
