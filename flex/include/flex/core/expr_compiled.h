/*
 * Flex Engine - Shared Expression Compilation Utility
 *
 * The function names are retained for legacy callers, but the implementation
 * is MIR-backed and does not depend on ExprTk.
 */

#pragma once

#include "flex/core/types.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace flex {

// Forward declaration - full definition in expr_compiled_impl.h.
struct MirCompiledExpression;
using ExprTkCompiled = MirCompiledExpression;

// Compile a MIR expression into a reusable JIT program. The cache pointer is
// populated once and may be evaluated repeatedly by one runtime instance.
bool compile_mir_expression(const std::string& expression,
                            std::shared_ptr<void>& out_ptr);

// Evaluate a MIR expression. Returns false when compilation fails, keeping a
// valid numeric zero distinguishable from an invalid expression.
bool evaluate_mir_expression_inputs(
    const std::string& expression,
    std::shared_ptr<void>& compiled_ptr,
    const std::unordered_map<Symbol, float, SymbolHash>& float_inputs,
    float& result);

// Legacy source-compatible names.
bool compile_exprtk(const std::string& expr_str, std::shared_ptr<void>& out_ptr);

// Evaluate a compiled expression with float inputs.
// Returns the result as a float.
float evaluate_exprtk_inputs(
    const std::string& expr_str,
    std::shared_ptr<void>& compiled_ptr,
    const std::unordered_map<Symbol, float, SymbolHash>& float_inputs);

} // namespace flex
