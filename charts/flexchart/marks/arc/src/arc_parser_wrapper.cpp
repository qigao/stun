#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/arc/arc_parser.h"
#include "arc_parser_gen.h"

void *ArcChartParserAlloc(void *(*mallocProc)(size_t));
void ArcChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void ArcChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void arc_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> arc_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("arc");

    void* parser = ArcChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    arc_scan(&s, parser, &ctx);
    ArcChartParser(parser, 0, nullptr, &ctx);
    ArcChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "arc: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
