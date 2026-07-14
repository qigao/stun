/*
 * Flex Engine - MIR-backed numeric expression program.
 *
 * This is the runtime backend for hot binding/state-machine expressions.
 * Unsupported syntax is reported at compile time.
 */

#pragma once

#include "flex/core/types.h"
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace flex {

class MirExpressionProgram {
public:
    MirExpressionProgram();
    ~MirExpressionProgram();

    MirExpressionProgram(const MirExpressionProgram&) = delete;
    MirExpressionProgram& operator=(const MirExpressionProgram&) = delete;
    MirExpressionProgram(MirExpressionProgram&&) noexcept;
    MirExpressionProgram& operator=(MirExpressionProgram&&) noexcept;

    static std::unique_ptr<MirExpressionProgram> compile(const std::string& expression,
                                                         const std::vector<std::string>& names);

    static std::vector<std::string> collect_variables(const std::string& expression);

    float evaluate(const std::unordered_map<Symbol, float, SymbolHash>& float_inputs);
    double evaluate_double(const std::unordered_map<Symbol, double, SymbolHash>& double_inputs);

    // Values are positional and must follow names() exactly.
    float evaluate_slots(const float* values, size_t count);
    double evaluate_slots(const double* values, size_t count);
    float evaluate_slots(const std::vector<float>& values);
    double evaluate_slots(const std::vector<double>& values);

    const std::vector<std::string>& names() const;
    bool uses_jit() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace flex
