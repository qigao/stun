/*
 * Flex Engine - Shared Expression Compilation Utility (Implementation)
 */

#include "flex/core/expr_compiled_impl.h"

#include <cmath>

namespace flex {

bool compile_mir_expression(const std::string& expr_str,
                            std::shared_ptr<void>& out_ptr) {
    if (out_ptr) {
        const auto* existing =
            static_cast<const MirCompiledExpression*>(out_ptr.get());
        return existing && existing->expression_str == expr_str &&
               existing->mir_program != nullptr;
    }

    auto compiled = std::make_shared<MirCompiledExpression>();
    compiled->expression_str = expr_str;

    compiled->names = MirExpressionProgram::collect_variables(expr_str);
    compiled->symbol_ids.reserve(compiled->names.size());
    for (auto& name : compiled->names)
        compiled->symbol_ids.emplace_back(name);

    compiled->mir_program = MirExpressionProgram::compile(expr_str, compiled->names);
    if (!compiled->mir_program)
        return false;

    out_ptr = compiled;
    return true;
}

bool evaluate_mir_expression_inputs(
    const std::string& expr_str,
    std::shared_ptr<void>& compiled_ptr,
    const std::unordered_map<Symbol, float, SymbolHash>& float_inputs,
    float& result) {
    if (!compile_mir_expression(expr_str, compiled_ptr)) {
        return false;
    }

    auto* data = static_cast<MirCompiledExpression*>(compiled_ptr.get());
    if (!data || !data->mir_program) {
        return false;
    }

    result = data->mir_program->evaluate(float_inputs);
    return std::isfinite(result);
}

bool compile_exprtk(const std::string& expr_str, std::shared_ptr<void>& out_ptr) {
    return compile_mir_expression(expr_str, out_ptr);
}

float evaluate_exprtk_inputs(
    const std::string& expr_str,
    std::shared_ptr<void>& compiled_ptr,
    const std::unordered_map<Symbol, float, SymbolHash>& float_inputs) {

    float result = 0.0f;
    evaluate_mir_expression_inputs(expr_str, compiled_ptr, float_inputs, result);
    return result;
}

} // namespace flex
