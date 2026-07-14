/*
 * Flex Animation - MIR-backed numeric animation expressions.
 */

#pragma once

#include <memory>
#include <string>

namespace flex {
class MirExpressionProgram;
}

namespace flex::animation {

struct NumericExpressionInputs {
    float time = 0.0f;
    float progress = 0.0f;
    float from = 0.0f;
    float to = 0.0f;
};

class NumericExpression {
public:
    explicit NumericExpression(const std::string& expression);
    ~NumericExpression();

    NumericExpression(const NumericExpression&) = delete;
    NumericExpression& operator=(const NumericExpression&) = delete;
    NumericExpression(NumericExpression&&) noexcept;
    NumericExpression& operator=(NumericExpression&&) noexcept;

    float sample(const NumericExpressionInputs& inputs);
    bool uses_jit() const;

private:
    std::unique_ptr<MirExpressionProgram> program_;
};

} // namespace flex::animation
