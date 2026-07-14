#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/image/image_parser.h"
#include "image_parser_gen.h"

void *ImageChartParserAlloc(void *(*mallocProc)(size_t));
void ImageChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void ImageChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void image_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> image_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("image");

    void* parser = ImageChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    image_scan(&s, parser, &ctx);
    ImageChartParser(parser, 0, nullptr, &ctx);
    ImageChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "image: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
