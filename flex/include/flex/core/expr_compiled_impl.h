/*
 * Flex Engine - compiled expression struct definition
 *
 * Include this in .cpp files that need the full ExprTkCompiled definition.
 * Headers should use the forward declaration from expr_compiled.h instead.
 */

#pragma once

#include "flex/core/expr_compiled.h"
#include "flex/core/expr_mir.h"

namespace flex {

struct ExprTkCompiled {
    std::string expression_str;
    std::vector<std::string> names;
    std::vector<Symbol> symbol_ids;
    std::unique_ptr<MirExpressionProgram> mir_program;
};

} // namespace flex
