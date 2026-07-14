#include "flexchart/common/mark_parser_context.h"
#include <cctype>
#include <cstdlib>

namespace flex {
namespace chart {

void MarkParserContext::init(const char* mark_type, int w, int h) {
    chart = std::make_shared<AstChart>();
    chart->width = std::to_string(w);
    chart->height = std::to_string(h);

    mark = std::make_shared<AstMark>();
    mark->type = mark_type;
    mark->data_ref = "default";
    chart->marks.push_back(mark);

    current_data = nullptr;
    current_row.clear();
    error_count = 0;
    error_message.clear();
}

void MarkParserContext::report_unexpected_character(unsigned char ch, int line) {
    ++error_count;
    if (!error_message.empty()) return;

    const std::string mark_type = mark ? mark->type : "chart";
    if (std::isprint(ch)) {
        error_message = mark_type + ": unexpected character '" +
                        std::string(1, static_cast<char>(ch)) + "' at line " +
                        std::to_string(line);
    } else {
        error_message = mark_type + ": unexpected byte " + std::to_string(ch) +
                        " at line " + std::to_string(line);
    }
}

void MarkParserContext::set_title(const char* v) {
    if (v) chart->title = v;
}

void MarkParserContext::set_width(const char* v) {
    if (v) chart->width = v;
}

void MarkParserContext::set_height(const char* v) {
    if (v) chart->height = v;
}

void MarkParserContext::set_expr(const char* expr) {
    if (expr) mark->expr = expr;
}

void MarkParserContext::add_range(const char* var, double start, double end, double step) {
    if (var) mark->ranges[var] = {start, end, step};
}

void MarkParserContext::add_simple_encoding(const char* channel, const char* field) {
    if (!channel || !field) return;
    // Replace existing encoding for same channel
    for (auto& enc : mark->encodings) {
        if (enc->channel == channel) {
            enc->field = field;
            return;
        }
    }
    auto enc = std::make_shared<AstEncoding>();
    enc->channel = channel;
    enc->field = field;
    mark->encodings.push_back(enc);
}

void MarkParserContext::add_style(const char* key, const char* val) {
    if (key && val) mark->styles[key] = std::string(val);
}

void MarkParserContext::add_style_num(const char* key, double val) {
    if (key) mark->styles[key] = val;
}

void MarkParserContext::add_style_bool(const char* key, int val) {
    if (key) mark->styles[key] = (val != 0);
}

void MarkParserContext::set_data_source(const char* url) {
    if (!current_data) {
        current_data = std::make_shared<AstData>();
        current_data->name = "default";
    }
    if (url) current_data->source = url;
    // Immediately commit data with source
    chart->datasets.push_back(current_data);
    current_data = nullptr;
}

void MarkParserContext::add_data_row_kv(const char* key, const char* val) {
    if (key && val) current_row.emplace_back(key, val);
}

void MarkParserContext::finish_data_row() {
    if (current_row.empty()) return;
    if (!current_data) {
        current_data = std::make_shared<AstData>();
        current_data->name = "default";
    }
    for (auto& [k, v] : current_row) {
        current_data->inline_values.push_back(k);
        current_data->inline_values.push_back(v);
    }
    current_row.clear();
}

void MarkParserContext::finish_data_block() {
    if (current_data) {
        chart->datasets.push_back(current_data);
        current_data = nullptr;
    }
}

} // namespace chart
} // namespace flex
