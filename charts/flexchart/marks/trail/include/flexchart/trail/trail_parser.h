#pragma once

#include "flexchart/chart_ast.h"
#include <string>
#include <memory>

namespace flex {
namespace chart {
    std::shared_ptr<AstChart> trail_chart_parse_ast(const char* input, std::string& error);
}
}
