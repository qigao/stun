#include "flex/core/expr_c.h"

#include "flex/core/expr_mir.h"

#include <unordered_map>

extern "C" int flex_expr_eval_f64(const char* expression, double* out_value) {
    if (!expression || !out_value) {
        return 0;
    }

    auto program = flex::MirExpressionProgram::compile(expression, {});
    if (!program) {
        return 0;
    }

    const std::unordered_map<flex::Symbol, double, flex::SymbolHash> inputs;
    *out_value = program->evaluate_double(inputs);
    return 1;
}

extern "C" int flex_expr_eval_f32(const char* expression, float* out_value) {
    if (!out_value) {
        return 0;
    }

    double value = 0.0;
    if (!flex_expr_eval_f64(expression, &value)) {
        return 0;
    }

    *out_value = static_cast<float>(value);
    return 1;
}

extern "C" int flex_expr_eval_i32(const char* expression, int* out_value) {
    if (!out_value) {
        return 0;
    }

    double value = 0.0;
    if (!flex_expr_eval_f64(expression, &value)) {
        return 0;
    }

    *out_value = static_cast<int>(value);
    return 1;
}
