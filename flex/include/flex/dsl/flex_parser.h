/*
 * Flex DSL Parser Header
 */

#pragma once

#include "flex_ast.h"
#include <memory>

namespace flex {
namespace parser {

// Parse DSL source into AST
std::shared_ptr<AstProgram> parse(const char* source);

// Error handling
const char* get_error();
int get_error_line();
int get_error_column();

} // namespace parser
} // namespace flex
