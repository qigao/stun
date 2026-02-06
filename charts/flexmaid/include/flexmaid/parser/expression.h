#pragma once

#include <ir/unified_diagram.h>
#include <string>
#include <vector>
#include <memory>

namespace flex::modules::flexmaid {

// Token 定义
struct Token {
    enum Type {
        DIAGRAM_TYPE, DIRECTION, IDENTIFIER, STRING, ARROW,
        SHAPE_OPEN, SHAPE_CLOSE, COLON, SEMICOLON, NEWLINE,
        PIPE, SUBGRAPH, END, STYLING, DIRECTION_KEYWORD,
        PIE, TITLE, GITGRAPH, COMMIT, BRANCH, CHECKOUT, MERGE,
        HASH, ACC_TITLE, ACC_DESCR, PARTICIPANT, NOTE, AND, COMMA,
        END_OF_FILE
    };
    Type type;
    std::string value;
    int line = 1, column = 1;
};

// 解析上下文
struct ParseContext {
    const std::vector<Token>* tokens = nullptr;
    size_t pos = 0;
    UnifiedDiagram* diagram = nullptr;
    std::vector<size_t> subgraph_stack;
    
    const Token& current() const;
    const Token& peek(size_t offset = 1) const;
    bool match(Token::Type type) const;
    bool match_value(const std::string& val) const;
    void advance();
    bool at_end() const;
    void skip_newlines();
};

// Interpreter Pattern: 抽象表达式
class IExpression {
public:
    virtual ~IExpression() = default;
    virtual bool interpret(ParseContext& ctx) = 0;
};

// 终结符表达式: 节点
class NodeExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override;
};

// 终结符表达式: 边
class EdgeExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override;
    
protected:
    virtual EdgeStyle identify_style(const std::string& arrow);
    virtual EdgeDecoration identify_decoration(const std::string& arrow);
};

// 终结符表达式: 子图
class SubgraphExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override;
};

// 非终结符表达式: 语句序列
class StatementsExpr : public IExpression {
public:
    void add(std::unique_ptr<IExpression> expr);
    bool interpret(ParseContext& ctx) override;
    
private:
    std::vector<std::unique_ptr<IExpression>> exprs_;
};

} // namespace flex::modules::flexmaid
