#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/line/line_parser.h"
#include "line_parser_gen.h"

void *LineChartParserAlloc(void *(*mallocProc)(size_t));
void LineChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void LineChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void line_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> line_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("line");

    void* parser = LineChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    line_scan(&s, parser, &ctx);
    LineChartParser(parser, 0, nullptr, &ctx);
    LineChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "line: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

}
}
