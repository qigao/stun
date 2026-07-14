/*
 * Flex Engine - Shared Expression Compilation Utility (Implementation)
 */

#include "flex/core/expr_compiled_impl.h"

namespace flex {

bool compile_exprtk(const std::string& expr_str, std::shared_ptr<void>& out_ptr) {
    if (out_ptr) return true;

    auto compiled = std::make_shared<ExprTkCompiled>();
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

float evaluate_exprtk_inputs(
    const std::string& expr_str,
    std::shared_ptr<void>& compiled_ptr,
    const std::unordered_map<Symbol, float, SymbolHash>& float_inputs) {

    if (!compiled_ptr) {
        if (!compile_exprtk(expr_str, compiled_ptr))
            return 0.0f;
    }

    auto* data = static_cast<ExprTkCompiled*>(compiled_ptr.get());
    if (!data) return 0.0f;

    return data->mir_program ? data->mir_program->evaluate(float_inputs) : 0.0f;
}

} // namespace flex
