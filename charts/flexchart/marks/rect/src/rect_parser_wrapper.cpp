#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/rect/rect_parser.h"
#include "rect_parser_gen.h"

void *RectChartParserAlloc(void *(*mallocProc)(size_t));
void RectChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void RectChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void rect_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> rect_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("rect");

    void* parser = RectChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    rect_scan(&s, parser, &ctx);
    RectChartParser(parser, 0, nullptr, &ctx);
    RectChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "rect: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
