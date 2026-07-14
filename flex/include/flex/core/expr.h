/*
 * Flex Engine - Expression Evaluator
 *
 * Shared MIR-backed math expression evaluator for the DSL.
 * All math evaluation across the engine should go through this class.
 *
 * Supports:
 *   - Named float variables with bind/set
 *   - Expression caching (compile once, evaluate many)
 *   - Built-in constants (pi, epsilon, etc.)
 *   - Formatted output for SVG/template rendering
 */

#pragma once

#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <utility>

namespace flex {

class Expr {
public:
    Expr();
    ~Expr();

    Expr(const Expr&) = delete;
    Expr& operator=(const Expr&) = delete;
    Expr(Expr&&) noexcept;
    Expr& operator=(Expr&&) noexcept;

    // ── Variable Management ──

    // Register a named variable. Must be called before any eval() that uses it.
    // Returns false if the name is already registered.
    bool add_variable(const std::string& name, float initial_value = 0.0f);

    // Set a previously registered variable's value.
    void set(const std::string& name, float value);

    // Convenience: set multiple variables at once.
    void set(std::initializer_list<std::pair<const char*, float>> vars);

    // ── Evaluation ──

    // Evaluate an expression string. Returns 0.0 on parse failure.
    // Compiled expressions are cached for repeated evaluation.
    double eval(const std::string& expr_str);

    // Evaluate while preserving the distinction between a valid zero result
    // and a compile failure.
    std::optional<double> try_eval(const std::string& expr_str);

    // Evaluate and return formatted string (trimmed trailing zeros).
    std::string eval_fmt(const std::string& expr_str);

    // Build a polygon points string from paired x,y expressions:
    // "x_expr,y_expr x_expr,y_expr ..."
    std::string eval_points(const std::vector<std::pair<std::string, std::string>>& pairs);

    // ── Discovery-based Compilation ──

    // Compile an expression, auto-discovering any unknown variables.
    // Discovered variables are registered automatically (initial value 0.0).
    // Returns the list of discovered variable names, or empty on parse failure.
    std::vector<std::string> compile(const std::string& expr_str);

    // Evaluate a previously compiled expression. Returns 0.0 if not compiled.
    double eval_compiled(const std::string& expr_str);

    // ── Formatting ──

    // Format a double as a string with trimmed trailing zeros.
    static std::string format_number(double v);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace flex
