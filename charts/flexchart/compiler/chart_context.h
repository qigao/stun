#pragma once

#include "flexchart/chart_ast.h"
#include "chart_token.h"
#include <memory>
#include <string>

namespace flex {
namespace chart {

struct ParseContext {
    AstProgram* program;
    std::shared_ptr<AstChart> current_chart;
    std::shared_ptr<AstData> current_data;
    std::shared_ptr<AstMark> current_mark;
    std::shared_ptr<AstSignal> current_signal;
    std::shared_ptr<AstState> current_state;
    std::shared_ptr<AstTransform> current_transform;
    std::shared_ptr<AstEncoding> current_encoding;
    std::shared_ptr<AstScale> current_scale;
    std::shared_ptr<AstAxis> current_axis;
    std::shared_ptr<AstComposition> current_composition;
    std::shared_ptr<AstSignalEvent> current_signal_event;
    std::string error_message;
    
    // Pool to manage semantic value strings
    std::vector<std::unique_ptr<std::string>> string_pool;

    ParseContext(AstProgram* p) : program(p) {}

    bool ensure_chart() {
        if (!current_chart) {
            current_chart = std::make_shared<AstChart>();
            program->views.push_back(current_chart);
            return true;
        }
        return false;
    }

    std::string* add_string(const std::string& s) {
        string_pool.push_back(std::make_unique<std::string>(s));
        return string_pool.back().get();
    }

    AstValue to_value(std::string* s) {
        if (!s) return "";
        if (*s == "true") return true;
        if (*s == "false") return false;
        try {
            size_t pos;
            double d = std::stod(*s, &pos);
            if (pos == s->length()) return d;
        } catch (...) {}
        return *s;
    }
};

} // namespace chart
} // namespace flex
