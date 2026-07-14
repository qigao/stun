#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/errorband/errorband_parser.h"
#include "errorband_parser_gen.h"

void *ErrorbandChartParserAlloc(void *(*mallocProc)(size_t));
void ErrorbandChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void ErrorbandChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void errorband_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> errorband_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("errorband");

    void* parser = ErrorbandChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    errorband_scan(&s, parser, &ctx);
    ErrorbandChartParser(parser, 0, nullptr, &ctx);
    ErrorbandChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "errorband: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
