#pragma once

#include "flexchart/chart_ast.h"
#include <string>
#include <string_view>
#include <memory>

namespace flex {
namespace chart {

// Unified helper function to detect chart type from source by parsing the first alphabetical word.
std::string detect_type(std::string_view src);

// Unified parse entry point — detects chart type from source,
// dispatches to the appropriate per-mark parser, and builds an AstProgram.
bool parse_chart(const char* source, AstProgram* program, std::string& error);

} // namespace chart
} // namespace flex
