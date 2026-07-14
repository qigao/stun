#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/text/text_parser.h"
#include "text_parser_gen.h"

void *TextChartParserAlloc(void *(*mallocProc)(size_t));
void TextChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void TextChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void text_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> text_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("text");

    void* parser = TextChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    text_scan(&s, parser, &ctx);
    TextChartParser(parser, 0, nullptr, &ctx);
    TextChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "text: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
