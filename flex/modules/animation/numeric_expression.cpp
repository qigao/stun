#include "flex/animation/numeric_expression.h"

#include "flex/core/expr_mir.h"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace flex::animation {
namespace {

const std::vector<std::string>& animation_input_names() {
    static const std::vector<std::string> names{
        "time", "progress", "from", "to"};
    return names;
}

} // namespace

NumericExpression::NumericExpression(const std::string& expression)
    : program_(MirExpressionProgram::compile(expression, animation_input_names())) {
    if (!program_) {
        throw std::invalid_argument("invalid MIR animation expression: " + expression);
    }
}

NumericExpression::~NumericExpression() = default;
NumericExpression::NumericExpression(NumericExpression&&) noexcept = default;
NumericExpression& NumericExpression::operator=(NumericExpression&&) noexcept = default;

float NumericExpression::sample(const NumericExpressionInputs& inputs) {
    const float slots[]{inputs.time, inputs.progress, inputs.from, inputs.to};
    const float result = program_->evaluate_slots(slots, 4);
    if (!std::isfinite(result)) {
        throw std::runtime_error("MIR animation expression produced a non-finite number");
    }
    return result;
}

bool NumericExpression::uses_jit() const {
    return program_->uses_jit();
}

} // namespace flex::animation
