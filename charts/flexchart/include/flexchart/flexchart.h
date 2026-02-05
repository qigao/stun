#pragma once

#include "flexchart/chart_ast.h"
#include <string>
#include <memory>

namespace flex {
namespace chart {

/**
 * @brief Parses a Chart DSL source string into an AST.
 * 
 * @param source The DSL source text.
 * @param program Pointer to an AstProgram to populate.
 * @param error_msg String to populate with error message if parsing fails.
 * @return true if parsing was successful, false otherwise.
 */
bool parse_chart(const char *source, AstProgram *program, std::string &error_msg);

} // namespace chart
} // namespace flex
