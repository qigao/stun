#pragma once

#include "flexchart/chart_ast.h"
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace flex {
namespace chart {

// Shared context used by all 17 per-mark lemon parsers.
// Grammar actions call methods on this struct to build AstChart directly.
struct MarkParserContext {
    std::shared_ptr<AstChart> chart;
    std::shared_ptr<AstMark>  mark;
    std::shared_ptr<AstData>  current_data;
    std::vector<std::pair<std::string,std::string>> current_row;
    int error_count = 0;
    std::string error_message;

    void init(const char* mark_type, int w = 600, int h = 400);
    void report_unexpected_character(unsigned char ch, int line);

    // Common properties
    void set_title(const char* v);
    void set_width(const char* v);
    void set_height(const char* v);

    // Expression + range support
    void set_expr(const char* expr);
    void add_range(const char* var, double start, double end, double step);

    // Encoding (simple key-value or block)
    void add_simple_encoding(const char* channel, const char* field);

    // Style properties
    void add_style(const char* key, const char* val);
    void add_style_num(const char* key, double val);
    void add_style_bool(const char* key, int val);

    // Data
    void set_data_source(const char* url);
    void add_data_row_kv(const char* key, const char* val);
    void finish_data_row();
    void finish_data_block();
};

} // namespace chart
} // namespace flex
