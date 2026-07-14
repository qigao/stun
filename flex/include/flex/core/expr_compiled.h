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

// Forward declaration - full definition in expr_compiled_impl.h
struct ExprTkCompiled;

// Compile an expression string into a shared cache pointer.
// Returns true on success. No-ops if already compiled.
bool compile_exprtk(const std::string& expr_str, std::shared_ptr<void>& out_ptr);

// Evaluate a compiled expression with float inputs.
// Returns the result as a float.
float evaluate_exprtk_inputs(
    const std::string& expr_str,
    std::shared_ptr<void>& compiled_ptr,
    const std::unordered_map<Symbol, float, SymbolHash>& float_inputs);

} // namespace flex
