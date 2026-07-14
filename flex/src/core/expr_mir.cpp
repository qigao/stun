/*
 * Flex Engine - MIR-backed numeric expression program implementation.
 */

#include "flex/core/expr_mir.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
#include <mir-gen.h>
#include <mir.h>
}

namespace flex {
namespace {

extern "C" double flex_mir_sin(double v) { return std::sin(v); }
extern "C" double flex_mir_cos(double v) { return std::cos(v); }
extern "C" double flex_mir_tan(double v) { return std::tan(v); }
extern "C" double flex_mir_sqrt(double v) { return std::sqrt(v); }
extern "C" double flex_mir_abs(double v) { return std::fabs(v); }
extern "C" double flex_mir_floor(double v) { return std::floor(v); }
extern "C" double flex_mir_ceil(double v) { return std::ceil(v); }
extern "C" double flex_mir_round(double v) { return std::round(v); }
extern "C" double flex_mir_exp(double v) { return std::exp(v); }
extern "C" double flex_mir_log(double v) { return std::log(v); }
extern "C" double flex_mir_pow(double a, double b) { return std::pow(a, b); }
extern "C" double flex_mir_fmod(double a, double b) { return std::fmod(a, b); }
extern "C" double flex_mir_min(double a, double b) { return a < b ? a : b; }
extern "C" double flex_mir_max(double a, double b) { return a > b ? a : b; }
extern "C" double flex_mir_lerp(double a, double b, double t) { return a + (b - a) * t; }
extern "C" double flex_mir_clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

enum class NodeKind {
    Number,
    Variable,
    Function,
    Neg,
    Not,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Pow,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    And,
    Or,
};

struct AstNode {
    NodeKind kind = NodeKind::Number;
    double number = 0.0;
    size_t variable = 0;
    std::string function_name;
    std::vector<std::unique_ptr<AstNode>> args;
    std::unique_ptr<AstNode> lhs;
    std::unique_ptr<AstNode> rhs;
};

struct FunctionInfo {
    const char* name;
    size_t arity;
    void* address;
};

const std::vector<FunctionInfo>& all_functions();

const FunctionInfo* find_function(const std::string& name) {
    for (const auto& function : all_functions()) {
        if (name == function.name) {
            return &function;
        }
    }
    return nullptr;
}

const std::vector<FunctionInfo>& all_functions() {
    static const std::vector<FunctionInfo> functions = {
        {"sin", 1, reinterpret_cast<void*>(flex_mir_sin)},
        {"cos", 1, reinterpret_cast<void*>(flex_mir_cos)},
        {"tan", 1, reinterpret_cast<void*>(flex_mir_tan)},
        {"sqrt", 1, reinterpret_cast<void*>(flex_mir_sqrt)},
        {"abs", 1, reinterpret_cast<void*>(flex_mir_abs)},
        {"floor", 1, reinterpret_cast<void*>(flex_mir_floor)},
        {"ceil", 1, reinterpret_cast<void*>(flex_mir_ceil)},
        {"round", 1, reinterpret_cast<void*>(flex_mir_round)},
        {"exp", 1, reinterpret_cast<void*>(flex_mir_exp)},
        {"log", 1, reinterpret_cast<void*>(flex_mir_log)},
        {"pow", 2, reinterpret_cast<void*>(flex_mir_pow)},
        {"fmod", 2, reinterpret_cast<void*>(flex_mir_fmod)},
        {"min", 2, reinterpret_cast<void*>(flex_mir_min)},
        {"max", 2, reinterpret_cast<void*>(flex_mir_max)},
        {"lerp", 3, reinterpret_cast<void*>(flex_mir_lerp)},
        {"clamp", 3, reinterpret_cast<void*>(flex_mir_clamp)},
    };
    return functions;
}

bool is_identifier_start(char ch) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalpha(uch) || ch == '_';
}

bool is_identifier_continue(char ch) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) || ch == '_';
}

bool is_reserved_identifier(const std::string& name) {
    return name == "and" ||
           name == "or" ||
           name == "not" ||
           name == "pi" ||
           name == "epsilon" ||
           find_function(name) != nullptr;
}

struct MirCallTarget {
    MIR_item_t proto = nullptr;
    MIR_item_t import = nullptr;
};

class Parser {
public:
    Parser(const std::string& source, const std::map<std::string, size_t>& variables)
        : source_(source), variables_(variables) {}

    std::unique_ptr<AstNode> parse() {
        auto node = parse_or();
        skip_ws();
        if (!node || pos_ != source_.size()) {
            return nullptr;
        }
        return node;
    }

private:
    std::unique_ptr<AstNode> parse_or() {
        auto lhs = parse_and();
        while (match("||") || match_word("or")) {
            auto rhs = parse_and();
            if (!lhs || !rhs) return nullptr;
            lhs = binary(NodeKind::Or, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<AstNode> parse_and() {
        auto lhs = parse_compare();
        while (match("&&") || match_word("and")) {
            auto rhs = parse_compare();
            if (!lhs || !rhs) return nullptr;
            lhs = binary(NodeKind::And, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<AstNode> parse_compare() {
        auto lhs = parse_add();
        for (;;) {
            NodeKind kind;
            if (match("<=")) kind = NodeKind::LessEqual;
            else if (match(">=")) kind = NodeKind::GreaterEqual;
            else if (match("==")) kind = NodeKind::Equal;
            else if (match("!=")) kind = NodeKind::NotEqual;
            else if (match("<")) kind = NodeKind::Less;
            else if (match(">")) kind = NodeKind::Greater;
            else break;

            auto rhs = parse_add();
            if (!lhs || !rhs) return nullptr;
            lhs = binary(kind, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<AstNode> parse_add() {
        auto lhs = parse_mul();
        for (;;) {
            NodeKind kind;
            if (match("+")) kind = NodeKind::Add;
            else if (match("-")) kind = NodeKind::Sub;
            else break;

            auto rhs = parse_mul();
            if (!lhs || !rhs) return nullptr;
            lhs = binary(kind, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<AstNode> parse_mul() {
        auto lhs = parse_power();
        for (;;) {
            NodeKind kind;
            if (match("*")) kind = NodeKind::Mul;
            else if (match("/")) kind = NodeKind::Div;
            else if (match("%")) kind = NodeKind::Mod;
            else break;

            auto rhs = parse_power();
            if (!lhs || !rhs) return nullptr;
            lhs = binary(kind, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<AstNode> parse_power() {
        auto lhs = parse_unary();
        if (match("^")) {
            auto rhs = parse_power();
            if (!lhs || !rhs) return nullptr;
            lhs = binary(NodeKind::Pow, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<AstNode> parse_unary() {
        if (match("-")) {
            auto node = parse_unary();
            if (!node) return nullptr;
            auto out = std::make_unique<AstNode>();
            out->kind = NodeKind::Neg;
            out->lhs = std::move(node);
            return out;
        }
        if (match("!") || match_word("not")) {
            auto node = parse_unary();
            if (!node) return nullptr;
            auto out = std::make_unique<AstNode>();
            out->kind = NodeKind::Not;
            out->lhs = std::move(node);
            return out;
        }
        return parse_primary();
    }

    std::unique_ptr<AstNode> parse_primary() {
        skip_ws();
        if (match("(")) {
            auto node = parse_or();
            if (!match(")")) return nullptr;
            return node;
        }
        if (pos_ < source_.size() &&
            (std::isdigit(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '.')) {
            return parse_number();
        }
        if (pos_ < source_.size() &&
            (std::isalpha(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '_')) {
            return parse_identifier();
        }
        return nullptr;
    }

    std::unique_ptr<AstNode> parse_number() {
        const char* begin = source_.c_str() + pos_;
        char* end = nullptr;
        double value = std::strtod(begin, &end);
        if (end == begin) return nullptr;
        pos_ += static_cast<size_t>(end - begin);
        auto node = std::make_unique<AstNode>();
        node->kind = NodeKind::Number;
        node->number = value;
        return node;
    }

    std::unique_ptr<AstNode> parse_identifier() {
        size_t start = pos_++;
        while (pos_ < source_.size()) {
            unsigned char ch = static_cast<unsigned char>(source_[pos_]);
            if (!std::isalnum(ch) && source_[pos_] != '_') break;
            ++pos_;
        }
        std::string name = source_.substr(start, pos_ - start);

        skip_ws();
        if (match("(")) {
            std::vector<std::unique_ptr<AstNode>> args;
            skip_ws();
            if (!match(")")) {
                for (;;) {
                    auto arg = parse_or();
                    if (!arg) return nullptr;
                    args.push_back(std::move(arg));
                    if (match(")")) break;
                    if (!match(",")) return nullptr;
                }
            }

            const FunctionInfo* function = find_function(name);
            if (!function || function->arity != args.size()) {
                return nullptr;
            }
            auto node = std::make_unique<AstNode>();
            node->kind = NodeKind::Function;
            node->function_name = std::move(name);
            node->args = std::move(args);
            return node;
        }

        if (name == "pi") {
            auto node = std::make_unique<AstNode>();
            node->kind = NodeKind::Number;
            node->number = 3.14159265358979323846;
            return node;
        }
        if (name == "epsilon") {
            auto node = std::make_unique<AstNode>();
            node->kind = NodeKind::Number;
            node->number = std::numeric_limits<double>::epsilon();
            return node;
        }

        auto it = variables_.find(name);
        if (it == variables_.end()) {
            return nullptr;
        }
        auto node = std::make_unique<AstNode>();
        node->kind = NodeKind::Variable;
        node->variable = it->second;
        return node;
    }

    bool match(const char* token) {
        skip_ws();
        size_t len = std::strlen(token);
        if (source_.compare(pos_, len, token) != 0) {
            return false;
        }
        pos_ += len;
        return true;
    }

    bool match_word(const char* word) {
        skip_ws();
        size_t len = std::strlen(word);
        if (source_.compare(pos_, len, word) != 0) {
            return false;
        }
        size_t end = pos_ + len;
        const bool before_ok = pos_ == 0 ||
            !(std::isalnum(static_cast<unsigned char>(source_[pos_ - 1])) || source_[pos_ - 1] == '_');
        const bool after_ok = end >= source_.size() ||
            !(std::isalnum(static_cast<unsigned char>(source_[end])) || source_[end] == '_');
        if (!before_ok || !after_ok) {
            return false;
        }
        pos_ = end;
        return true;
    }

    void skip_ws() {
        while (pos_ < source_.size() &&
               std::isspace(static_cast<unsigned char>(source_[pos_]))) {
            ++pos_;
        }
    }

    static std::unique_ptr<AstNode> binary(NodeKind kind,
                                           std::unique_ptr<AstNode> lhs,
                                           std::unique_ptr<AstNode> rhs) {
        auto node = std::make_unique<AstNode>();
        node->kind = kind;
        node->lhs = std::move(lhs);
        node->rhs = std::move(rhs);
        return node;
    }

    const std::string& source_;
    const std::map<std::string, size_t>& variables_;
    size_t pos_ = 0;
};

class MirEmitter {
public:
    MirEmitter(MIR_context_t ctx, MIR_item_t func, MIR_reg_t values_reg,
               const std::unordered_map<std::string, MirCallTarget>& calls)
        : ctx_(ctx), func_(func), values_reg_(values_reg), calls_(calls) {}

    MIR_reg_t emit(const AstNode& node) {
        switch (node.kind) {
        case NodeKind::Number:
            return emit_number(node.number);
        case NodeKind::Variable:
            return emit_variable(node.variable);
        case NodeKind::Function:
            return emit_function(node.function_name, node.args);
        case NodeKind::Neg:
            return emit_unary(MIR_DNEG, *node.lhs);
        case NodeKind::Not:
            return emit_truth(*node.lhs, true);
        case NodeKind::Add:
            return emit_binary(MIR_DADD, *node.lhs, *node.rhs);
        case NodeKind::Sub:
            return emit_binary(MIR_DSUB, *node.lhs, *node.rhs);
        case NodeKind::Mul:
            return emit_binary(MIR_DMUL, *node.lhs, *node.rhs);
        case NodeKind::Div:
            return emit_binary(MIR_DDIV, *node.lhs, *node.rhs);
        case NodeKind::Mod:
            return emit_call("fmod", {*node.lhs, *node.rhs});
        case NodeKind::Pow:
            return emit_call("pow", {*node.lhs, *node.rhs});
        case NodeKind::Less:
            return emit_compare(MIR_DBLT, *node.lhs, *node.rhs);
        case NodeKind::LessEqual:
            return emit_compare(MIR_DBLE, *node.lhs, *node.rhs);
        case NodeKind::Greater:
            return emit_compare(MIR_DBGT, *node.lhs, *node.rhs);
        case NodeKind::GreaterEqual:
            return emit_compare(MIR_DBGE, *node.lhs, *node.rhs);
        case NodeKind::Equal:
            return emit_compare(MIR_DBEQ, *node.lhs, *node.rhs);
        case NodeKind::NotEqual:
            return emit_compare(MIR_DBNE, *node.lhs, *node.rhs);
        case NodeKind::And:
            return emit_logical(*node.lhs, *node.rhs, true);
        case NodeKind::Or:
            return emit_logical(*node.lhs, *node.rhs, false);
        }
        return emit_number(0.0);
    }

private:
    MIR_reg_t new_reg() {
        std::ostringstream name;
        name << "_d" << temp_++;
        return MIR_new_func_reg(ctx_, func_->u.func, MIR_T_D, name.str().c_str());
    }

    MIR_reg_t new_int_reg() {
        std::ostringstream name;
        name << "_i" << temp_++;
        return MIR_new_func_reg(ctx_, func_->u.func, MIR_T_I64, name.str().c_str());
    }

    MIR_reg_t emit_number(double value) {
        MIR_reg_t out = new_reg();
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, MIR_DMOV, MIR_new_reg_op(ctx_, out),
                                     MIR_new_double_op(ctx_, value)));
        return out;
    }

    MIR_reg_t emit_variable(size_t index) {
        MIR_reg_t out = new_reg();
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, MIR_DMOV, MIR_new_reg_op(ctx_, out),
                                     MIR_new_mem_op(ctx_, MIR_T_D,
                                                    static_cast<MIR_disp_t>(index * sizeof(double)),
                                                    values_reg_, 0, 1)));
        return out;
    }

    MIR_reg_t emit_unary(MIR_insn_code_t code, const AstNode& node) {
        MIR_reg_t in = emit(node);
        MIR_reg_t out = new_reg();
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, code, MIR_new_reg_op(ctx_, out),
                                     MIR_new_reg_op(ctx_, in)));
        return out;
    }

    MIR_reg_t emit_binary(MIR_insn_code_t code, const AstNode& lhs, const AstNode& rhs) {
        MIR_reg_t l = emit(lhs);
        MIR_reg_t r = emit(rhs);
        MIR_reg_t out = new_reg();
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, code, MIR_new_reg_op(ctx_, out),
                                     MIR_new_reg_op(ctx_, l), MIR_new_reg_op(ctx_, r)));
        return out;
    }

    MIR_reg_t emit_function(const std::string& name,
                            const std::vector<std::unique_ptr<AstNode>>& args) {
        std::vector<std::reference_wrapper<const AstNode>> refs;
        refs.reserve(args.size());
        for (const auto& arg : args) {
            refs.emplace_back(*arg);
        }
        return emit_call(name, refs);
    }

    MIR_reg_t emit_call(const std::string& name,
                        std::initializer_list<std::reference_wrapper<const AstNode>> args) {
        std::vector<std::reference_wrapper<const AstNode>> refs(args.begin(), args.end());
        return emit_call(name, refs);
    }

    MIR_reg_t emit_call(const std::string& name,
                        const std::vector<std::reference_wrapper<const AstNode>>& args) {
        auto target_it = calls_.find(name);
        if (target_it == calls_.end()) {
            return emit_number(0.0);
        }

        std::vector<MIR_reg_t> arg_regs;
        arg_regs.reserve(args.size());
        for (const AstNode& arg : args) {
            arg_regs.push_back(emit(arg));
        }

        MIR_reg_t out = new_reg();
        std::vector<MIR_op_t> operands;
        operands.reserve(3 + arg_regs.size());
        operands.push_back(MIR_new_ref_op(ctx_, target_it->second.proto));
        operands.push_back(MIR_new_ref_op(ctx_, target_it->second.import));
        operands.push_back(MIR_new_reg_op(ctx_, out));
        for (MIR_reg_t reg : arg_regs) {
            operands.push_back(MIR_new_reg_op(ctx_, reg));
        }
        MIR_append_insn(ctx_, func_,
                        MIR_new_call_insn(ctx_, operands.size(),
                                          operands[0], operands[1], operands[2],
                                          operands.size() > 3 ? operands[3] : MIR_new_double_op(ctx_, 0.0),
                                          operands.size() > 4 ? operands[4] : MIR_new_double_op(ctx_, 0.0),
                                          operands.size() > 5 ? operands[5] : MIR_new_double_op(ctx_, 0.0)));
        return out;
    }

    MIR_reg_t emit_compare(MIR_insn_code_t branch, const AstNode& lhs, const AstNode& rhs) {
        MIR_reg_t l = emit(lhs);
        MIR_reg_t r = emit(rhs);
        return emit_bool_branch(branch, l, r);
    }

    MIR_reg_t emit_truth(const AstNode& node, bool invert) {
        MIR_reg_t value = emit(node);
        MIR_reg_t zero = emit_number(0.0);
        return emit_bool_branch(invert ? MIR_DBEQ : MIR_DBNE, value, zero);
    }

    MIR_reg_t emit_logical(const AstNode& lhs, const AstNode& rhs, bool is_and) {
        MIR_reg_t left = emit_truth(lhs, false);
        MIR_reg_t right = emit_truth(rhs, false);
        MIR_reg_t zero = emit_number(0.0);
        MIR_reg_t out = new_reg();
        MIR_reg_t int_out = new_int_reg();
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, MIR_DNE, MIR_new_reg_op(ctx_, int_out),
                                     MIR_new_reg_op(ctx_, left), MIR_new_reg_op(ctx_, zero)));
        if (!is_and) {
            MIR_reg_t right_nonzero =
                emit_bool_branch(MIR_DBNE, right, zero);
            MIR_reg_t right_int = new_int_reg();
            MIR_append_insn(ctx_, func_,
                            MIR_new_insn(ctx_, MIR_DNE, MIR_new_reg_op(ctx_, right_int),
                                         MIR_new_reg_op(ctx_, right_nonzero), MIR_new_reg_op(ctx_, zero)));
            MIR_append_insn(ctx_, func_,
                            MIR_new_insn(ctx_, MIR_OR, MIR_new_reg_op(ctx_, int_out),
                                         MIR_new_reg_op(ctx_, int_out), MIR_new_reg_op(ctx_, right_int)));
        } else {
            MIR_reg_t right_int = new_int_reg();
            MIR_append_insn(ctx_, func_,
                            MIR_new_insn(ctx_, MIR_DNE, MIR_new_reg_op(ctx_, right_int),
                                         MIR_new_reg_op(ctx_, right), MIR_new_reg_op(ctx_, zero)));
            MIR_append_insn(ctx_, func_,
                            MIR_new_insn(ctx_, MIR_AND, MIR_new_reg_op(ctx_, int_out),
                                         MIR_new_reg_op(ctx_, int_out), MIR_new_reg_op(ctx_, right_int)));
        }
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, MIR_I2D, MIR_new_reg_op(ctx_, out),
                                     MIR_new_reg_op(ctx_, int_out)));
        return out;
    }

    MIR_reg_t emit_bool_branch(MIR_insn_code_t branch, MIR_reg_t lhs, MIR_reg_t rhs) {
        MIR_reg_t out = new_reg();
        MIR_label_t true_label = MIR_new_label(ctx_);
        MIR_label_t end_label = MIR_new_label(ctx_);
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, MIR_DMOV, MIR_new_reg_op(ctx_, out),
                                     MIR_new_double_op(ctx_, 0.0)));
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, branch, MIR_new_label_op(ctx_, true_label),
                                     MIR_new_reg_op(ctx_, lhs), MIR_new_reg_op(ctx_, rhs)));
        MIR_append_insn(ctx_, func_, MIR_new_insn(ctx_, MIR_JMP, MIR_new_label_op(ctx_, end_label)));
        MIR_append_insn(ctx_, func_, true_label);
        MIR_append_insn(ctx_, func_,
                        MIR_new_insn(ctx_, MIR_DMOV, MIR_new_reg_op(ctx_, out),
                                     MIR_new_double_op(ctx_, 1.0)));
        MIR_append_insn(ctx_, func_, end_label);
        return out;
    }

    MIR_context_t ctx_ = nullptr;
    MIR_item_t func_ = nullptr;
    MIR_reg_t values_reg_ = 0;
    const std::unordered_map<std::string, MirCallTarget>& calls_;
    int temp_ = 0;
};

using MirFn = double (*)(double*);

} // namespace

struct MirExpressionProgram::Impl {
    MIR_context_t ctx = nullptr;
    MIR_item_t func = nullptr;
    MirFn jit_fn = nullptr;
    std::vector<std::string> names;
    std::vector<Symbol> symbols;
    std::vector<double> values;

    ~Impl() {
        if (ctx) {
            if (jit_fn) {
                MIR_gen_finish(ctx);
            }
            MIR_finish(ctx);
        }
    }
};

MirExpressionProgram::MirExpressionProgram() : impl_(std::make_unique<Impl>()) {}
MirExpressionProgram::~MirExpressionProgram() = default;
MirExpressionProgram::MirExpressionProgram(MirExpressionProgram&&) noexcept = default;
MirExpressionProgram& MirExpressionProgram::operator=(MirExpressionProgram&&) noexcept = default;

std::unique_ptr<MirExpressionProgram> MirExpressionProgram::compile(
    const std::string& expression,
    const std::vector<std::string>& names) {
    std::map<std::string, size_t> variable_map;
    for (size_t i = 0; i < names.size(); ++i) {
        variable_map.emplace(names[i], i);
    }

    Parser parser(expression, variable_map);
    auto ast = parser.parse();
    if (!ast) {
        return nullptr;
    }

    auto program = std::make_unique<MirExpressionProgram>();
    program->impl_->names = names;
    program->impl_->symbols.reserve(names.size());
    program->impl_->values.assign(names.size(), 0.0);
    for (const auto& name : names) {
        program->impl_->symbols.emplace_back(name);
    }

    static std::atomic<unsigned long long> module_index{0};
    std::ostringstream module_name;
    module_name << "flex_expr_mir_"
                << module_index.fetch_add(1, std::memory_order_relaxed);

    MIR_context_t ctx = MIR_init();
    MIR_module_t module = MIR_new_module(ctx, module_name.str().c_str());
    (void)module;

    MIR_type_t result_type = MIR_T_D;
    std::unordered_map<std::string, MirCallTarget> calls;
    for (const FunctionInfo& function : all_functions()) {
        MIR_var_t args[3] = {
            {MIR_T_D, "a", 0},
            {MIR_T_D, "b", 0},
            {MIR_T_D, "c", 0},
        };
        std::string proto_name = std::string("p_") + function.name;
        MirCallTarget target;
        target.proto = MIR_new_proto_arr(ctx, proto_name.c_str(), 1, &result_type,
                                         function.arity, args);
        target.import = MIR_new_import(ctx, function.name);
        calls.emplace(function.name, target);
        MIR_load_external(ctx, function.name, function.address);
    }

    MIR_var_t args[1] = {{MIR_T_P, "values", 0}};
    MIR_item_t func = MIR_new_func_arr(ctx, "eval", 1, &result_type, 1, args);
    MIR_reg_t values_reg = MIR_reg(ctx, "values", func->u.func);

    MirEmitter emitter(ctx, func, values_reg, calls);
    MIR_reg_t result = emitter.emit(*ast);
    MIR_append_insn(ctx, func, MIR_new_ret_insn(ctx, 1, MIR_new_reg_op(ctx, result)));

    MIR_finish_func(ctx);
    MIR_finish_module(ctx);
    MIR_load_module(ctx, module);
    MIR_gen_init(ctx);
    MIR_link(ctx, MIR_set_gen_interface, nullptr);

    program->impl_->ctx = ctx;
    program->impl_->func = func;
    program->impl_->jit_fn = reinterpret_cast<MirFn>(func->addr);
    return program;
}

std::vector<std::string> MirExpressionProgram::collect_variables(const std::string& expression) {
    std::vector<std::string> variables;
    for (size_t i = 0; i < expression.size();) {
        if (!is_identifier_start(expression[i])) {
            ++i;
            continue;
        }

        const size_t start = i++;
        while (i < expression.size() && is_identifier_continue(expression[i])) {
            ++i;
        }

        std::string name = expression.substr(start, i - start);
        size_t next = i;
        while (next < expression.size() &&
               std::isspace(static_cast<unsigned char>(expression[next]))) {
            ++next;
        }
        if (next < expression.size() && expression[next] == '(') {
            continue;
        }
        if (!is_reserved_identifier(name)) {
            variables.push_back(std::move(name));
        }
    }

    std::sort(variables.begin(), variables.end());
    variables.erase(std::unique(variables.begin(), variables.end()), variables.end());
    return variables;
}

float MirExpressionProgram::evaluate(
    const std::unordered_map<Symbol, float, SymbolHash>& float_inputs) {
    for (size_t i = 0; i < impl_->symbols.size(); ++i) {
        auto it = float_inputs.find(impl_->symbols[i]);
        impl_->values[i] = it != float_inputs.end() ? static_cast<double>(it->second) : 0.0;
    }

    return static_cast<float>(evaluate_slots(impl_->values));
}

double MirExpressionProgram::evaluate_double(
    const std::unordered_map<Symbol, double, SymbolHash>& double_inputs) {
    for (size_t i = 0; i < impl_->symbols.size(); ++i) {
        auto it = double_inputs.find(impl_->symbols[i]);
        impl_->values[i] = it != double_inputs.end() ? it->second : 0.0;
    }

    return evaluate_slots(impl_->values);
}

float MirExpressionProgram::evaluate_slots(const float* values, size_t count) {
    if (count != impl_->names.size()) {
        throw std::invalid_argument("MIR expression slot count does not match names()");
    }
    if (values == nullptr && count != 0) {
        throw std::invalid_argument("MIR expression slots must not be null");
    }
    for (size_t i = 0; i < count; ++i) {
        impl_->values[i] = static_cast<double>(values[i]);
    }
    return static_cast<float>(evaluate_slots(impl_->values));
}

double MirExpressionProgram::evaluate_slots(const double* values, size_t count) {
    if (count != impl_->names.size()) {
        throw std::invalid_argument("MIR expression slot count does not match names()");
    }
    if (values == nullptr && count != 0) {
        throw std::invalid_argument("MIR expression slots must not be null");
    }

    // MIR treats the argument as read-only even though its C ABI uses double*.
    double* mutable_values = const_cast<double*>(values);
    if (impl_->jit_fn) {
        return impl_->jit_fn(mutable_values);
    }

    MIR_val_t arg;
    MIR_val_t result;
    arg.a = mutable_values;
    result.d = 0.0;
    MIR_interp_arr(impl_->ctx, impl_->func, &result, 1, &arg);
    return result.d;
}

float MirExpressionProgram::evaluate_slots(const std::vector<float>& values) {
    return evaluate_slots(values.data(), values.size());
}

double MirExpressionProgram::evaluate_slots(const std::vector<double>& values) {
    return evaluate_slots(values.data(), values.size());
}

const std::vector<std::string>& MirExpressionProgram::names() const {
    return impl_->names;
}

bool MirExpressionProgram::uses_jit() const {
    return impl_->jit_fn != nullptr;
}

} // namespace flex
