#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/errorbar/errorbar_parser.h"
#include "errorbar_parser_gen.h"

void *ErrorbarChartParserAlloc(void *(*mallocProc)(size_t));
void ErrorbarChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void ErrorbarChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void errorbar_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> errorbar_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("errorbar");

    void* parser = ErrorbarChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    errorbar_scan(&s, parser, &ctx);
    ErrorbarChartParser(parser, 0, nullptr, &ctx);
    ErrorbarChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "errorbar: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
