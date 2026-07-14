#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include "flexchart/common/mark_parser_context.h"
#include "flexchart/rule/rule_parser.h"
#include "rule_parser_gen.h"

void *RuleChartParserAlloc(void *(*mallocProc)(size_t));
void RuleChartParser(void *yyp, int yymajor, char* yyminor, flex::chart::MarkParserContext *ctx);
void RuleChartParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void rule_scan(Scanner *s, void *parser, flex::chart::MarkParserContext *ctx);

namespace flex {
namespace chart {

std::shared_ptr<AstChart> rule_chart_parse_ast(const char* input, std::string& error) {
    MarkParserContext ctx;
    ctx.init("rule");

    void* parser = RuleChartParserAlloc(malloc);

    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = nullptr;
    s.line = 1;

    rule_scan(&s, parser, &ctx);
    RuleChartParser(parser, 0, nullptr, &ctx);
    RuleChartParserFree(parser, free);

    if (ctx.error_count > 0) {
        error = ctx.error_message.empty() ? "rule: parse error" : ctx.error_message;
        return nullptr;
    }

    return ctx.chart;
}

} // namespace chart
} // namespace flex
