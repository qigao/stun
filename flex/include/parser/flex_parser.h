/*
 * Flex DSL Parser Header
 */

#pragma once

#include "flex_ast.h"
#include "flex.h"
#include <memory>

namespace flex {
namespace parser {

// Parse DSL source into AST
std::shared_ptr<AstProgram> parse(const char* source);

// Convert AST to Runtime objects
Artboard::Ptr convert_ast_scene(const std::shared_ptr<AstScene>& scene);

// Error handling
const char* get_error();
int get_error_line();
int get_error_column();

} // namespace parser
} // namespace flex
