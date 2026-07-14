/*
 * Flex Engine - Expression Evaluator Implementation
 */

#include "flex/core/expr.h"
#include "flex/core/expr_mir.h"

#include <iomanip>
#include <map>
#include <sstream>

namespace flex {

struct Expr::Impl {
    struct CachedProgram {
        std::unique_ptr<MirExpressionProgram> program;
        std::vector<const float*> inputs;
        std::vector<float> slots;
    };

    std::map<std::string, float> vars;
    std::map<std::string, CachedProgram> cache;

    CachedProgram bind(std::unique_ptr<MirExpressionProgram> program) {
        CachedProgram cached;
        cached.inputs.reserve(program->names().size());
        cached.slots.resize(program->names().size());
        for (const auto& name : program->names()) {
            cached.inputs.push_back(&vars.at(name));
        }
        cached.program = std::move(program);
        return cached;
    }
};

Expr::Expr() : pimpl_(std::make_unique<Impl>()) {}
Expr::~Expr() = default;
Expr::Expr(Expr&&) noexcept = default;
Expr& Expr::operator=(Expr&&) noexcept = default;

bool Expr::add_variable(const std::string& name, float initial_value) {
    auto [it, inserted] = pimpl_->vars.emplace(name, initial_value);
    (void)it;
    if (inserted) {
        pimpl_->cache.clear();
    }
    return inserted;
}

void Expr::set(const std::string& name, float value) {
    auto it = pimpl_->vars.find(name);
    if (it != pimpl_->vars.end()) it->second = value;
}

void Expr::set(std::initializer_list<std::pair<const char*, float>> vars) {
    for (auto& [name, value] : vars) set(name, value);
}

double Expr::eval(const std::string& expr_str) {
    const auto result = try_eval(expr_str);
    return result.value_or(0.0);
}

std::optional<double> Expr::try_eval(const std::string& expr_str) {
    auto it = pimpl_->cache.find(expr_str);
    if (it == pimpl_->cache.end()) {
        std::vector<std::string> names;
        names.reserve(pimpl_->vars.size());
        for (const auto& [name, value] : pimpl_->vars) {
            (void)value;
            names.push_back(name);
        }

        auto program = MirExpressionProgram::compile(expr_str, names);
        if (!program) {
            return std::nullopt;
        }
        it = pimpl_->cache.emplace(expr_str, pimpl_->bind(std::move(program))).first;
    }

    auto& cached = it->second;
    for (size_t i = 0; i < cached.inputs.size(); ++i) {
        cached.slots[i] = *cached.inputs[i];
    }
    return static_cast<double>(cached.program->evaluate_slots(cached.slots));
}

std::string Expr::eval_fmt(const std::string& expr_str) {
    return format_number(eval(expr_str));
}

std::string Expr::eval_points(const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::string result;
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (i > 0) result += ' ';
        result += format_number(eval(pairs[i].first));
        result += ',';
        result += format_number(eval(pairs[i].second));
    }
    return result;
}

std::vector<std::string> Expr::compile(const std::string& expr_str) {
    if (pimpl_->cache.count(expr_str))
        return {};

    std::vector<std::string> variables = MirExpressionProgram::collect_variables(expr_str);
    for (auto& name : variables)
        add_variable(name);

    auto program = MirExpressionProgram::compile(expr_str, variables);
    if (!program)
        return {};

    pimpl_->cache.emplace(expr_str, pimpl_->bind(std::move(program)));
    return variables;
}

double Expr::eval_compiled(const std::string& expr_str) {
    if (!pimpl_->cache.count(expr_str))
        return 0.0;
    return eval(expr_str);
}

std::string Expr::format_number(double v) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    std::string s = ss.str();
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') s.pop_back();
    return s;
}

} // namespace flex
