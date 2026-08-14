#include "tinytest.h"

#include "flex/core/expr_compiled.h"

#include <memory>
#include <unordered_map>

namespace {

constexpr std::size_t kEvaluationSamples = 100000;
constexpr std::size_t kCompilationSamples = 100;

} // namespace

suite("Flex MIR expression performance") {
  bench("compares cached JIT evaluation with compile-and-evaluate") {
    std::unordered_map<flex::Symbol, float, flex::SymbolHash> inputs;
    inputs[flex::Symbol("time")] = 0.75f;
    inputs[flex::Symbol("progress")] = 0.5f;
    inputs[flex::Symbol("from")] = 10.0f;
    inputs[flex::Symbol("to")] = 30.0f;

    const std::string expression =
        "lerp(from, to, smoothstep(0, 1, progress)) + sin(time)";
    std::shared_ptr<void> cached;
    check(flex::compile_mir_expression(expression, cached));
    const std::string derivative_expression =
        "derivative(lerp(from, to, smoothstep(0, 1, progress)) + sin(time), time)";
    std::shared_ptr<void> cached_derivative;
    check(flex::compile_mir_expression(derivative_expression, cached_derivative));

    float sink = 0.0f;
    bool all_ok = true;
    benchmark_batch("MIR cached JIT evaluation", kEvaluationSamples) {
      all_ok = flex::evaluate_mir_expression_inputs(expression, cached, inputs, sink) &&
               all_ok;
    }


    benchmark_batch("MIR cached JIT derivative", kEvaluationSamples) {
      all_ok = flex::evaluate_mir_expression_inputs(
                   derivative_expression, cached_derivative, inputs, sink) &&
               all_ok;
    }

    benchmark_batch("MIR compile and evaluate", kCompilationSamples) {
      std::shared_ptr<void> one_shot;
      all_ok = flex::evaluate_mir_expression_inputs(expression, one_shot, inputs, sink) &&
               all_ok;
    }
    check(all_ok);
    check(sink > 0.0f);
  }
}
