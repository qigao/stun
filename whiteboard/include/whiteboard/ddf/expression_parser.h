#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>


namespace whiteboard {
namespace ddf {

// Forward declarations
struct DataNode;
struct Shape;

/**
 * @brief Token types for expression parsing
 */
enum class TokenType {
  // Literals
  Number,
  String,
  Identifier,

  // Operators
  Plus,     // +
  Minus,    // -
  Multiply, // *
  Divide,   // /
  Modulo,   // %

  // Comparison
  Equal,        // ==
  NotEqual,     // !=
  Greater,      // >
  Less,         // <
  GreaterEqual, // >=
  LessEqual,    // <=

  // Logical
  And, // &&
  Or,  // ||
  Not, // !

  // Delimiters
  LeftParen,  // (
  RightParen, // )
  Dot,        // .
  Pipe,       // |
  Colon,      // :
  Comma,      // ,
  Question,   // ?

  // Special
  EndOfExpression,
  Invalid
};

/**
 * @brief Token structure for expression parsing
 */
struct Token {
  TokenType type;
  std::string value;
  int position; // Position in original string

  Token() : type(TokenType::Invalid), position(0) {}
  Token(TokenType t, const std::string &v, int pos) : type(t), value(v), position(pos) {}
};

/**
 * @brief Expression tokenizer
 *
 * Tokenizes expression strings into a sequence of tokens for parsing.
 * Handles operators, identifiers, numbers, strings, and special characters.
 */
class ExpressionTokenizer {
public:
  ExpressionTokenizer() = default;

  /**
   * @brief Tokenize an expression string
   * @param expression The expression to tokenize (without {{ }})
   * @return Vector of tokens
   */
  std::vector<Token> tokenize(const std::string &expression);

  /**
   * @brief Get the last error message
   */
  const std::string &get_error() const { return error_; }

  /**
   * @brief Check if tokenization had errors
   */
  bool has_error() const { return !error_.empty(); }

private:
  std::string expression_;
  size_t position_;
  std::string error_;

  // Helper methods
  char current_char() const;
  char peek_char(int offset = 1) const;
  void advance();
  void skip_whitespace();

  Token read_number();
  Token read_identifier();
  Token read_string();
  Token read_operator();

  bool is_digit(char c) const;
  bool is_alpha(char c) const;
  bool is_alnum(char c) const;
  bool is_whitespace(char c) const;

  void set_error(const std::string &message);
};

/**
 * @brief AST node types
 */
enum class ASTNodeType {
    Number,
    String,
    Identifier,
    BinaryOp,
    UnaryOp,
    TernaryOp,
    MemberAccess,
    FilterCall
};

/**
 * @brief Base class for AST nodes
 */
struct ASTNode {
    ASTNodeType type;
    
    virtual ~ASTNode() = default;
    
protected:
    ASTNode(ASTNodeType t) : type(t) {}
};

/**
 * @brief Number literal node
 */
struct NumberNode : public ASTNode {
    double value;
    
    NumberNode(double v) : ASTNode(ASTNodeType::Number), value(v) {}
};

/**
 * @brief String literal node
 */
struct StringNode : public ASTNode {
    std::string value;
    
    StringNode(const std::string& v) : ASTNode(ASTNodeType::String), value(v) {}
};

/**
 * @brief Identifier node (variable name)
 */
struct IdentifierNode : public ASTNode {
    std::string name;
    
    IdentifierNode(const std::string& n) : ASTNode(ASTNodeType::Identifier), name(n) {}
};

/**
 * @brief Binary operation node
 */
struct BinaryOpNode : public ASTNode {
    TokenType op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    
    BinaryOpNode(TokenType o, std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r)
        : ASTNode(ASTNodeType::BinaryOp), op(o), left(std::move(l)), right(std::move(r)) {}
};

/**
 * @brief Unary operation node
 */
struct UnaryOpNode : public ASTNode {
    TokenType op;
    std::unique_ptr<ASTNode> operand;
    
    UnaryOpNode(TokenType o, std::unique_ptr<ASTNode> operand)
        : ASTNode(ASTNodeType::UnaryOp), op(o), operand(std::move(operand)) {}
};

/**
 * @brief Ternary operation node (condition ? true : false)
 */
struct TernaryOpNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> true_expr;
    std::unique_ptr<ASTNode> false_expr;
    
    TernaryOpNode(std::unique_ptr<ASTNode> cond, 
                  std::unique_ptr<ASTNode> true_e, 
                  std::unique_ptr<ASTNode> false_e)
        : ASTNode(ASTNodeType::TernaryOp), 
          condition(std::move(cond)), 
          true_expr(std::move(true_e)), 
          false_expr(std::move(false_e)) {}
};

/**
 * @brief Member access node (object.property)
 */
struct MemberAccessNode : public ASTNode {
    std::unique_ptr<ASTNode> object;
    std::string member;
    
    MemberAccessNode(std::unique_ptr<ASTNode> obj, const std::string& mem)
        : ASTNode(ASTNodeType::MemberAccess), object(std::move(obj)), member(mem) {}
};

/**
 * @brief Filter call node (value | filter: arg)
 */
struct FilterCallNode : public ASTNode {
    std::unique_ptr<ASTNode> value;
    std::string filter_name;
    std::vector<std::unique_ptr<ASTNode>> arguments;
    
    FilterCallNode(std::unique_ptr<ASTNode> val, const std::string& name)
        : ASTNode(ASTNodeType::FilterCall), value(std::move(val)), filter_name(name) {}
};

/**
 * @brief Expression parser
 * 
 * Parses tokenized expressions into an Abstract Syntax Tree (AST).
 * Supports arithmetic, comparison, logical operations, and ternary operator.
 */
class ExpressionParser {
public:
    ExpressionParser() = default;
    
    /**
     * @brief Parse tokens into an AST
     * @param tokens Vector of tokens from tokenizer
     * @return Root node of the AST, or nullptr on error
     */
    std::unique_ptr<ASTNode> parse(const std::vector<Token>& tokens);
    
    /**
     * @brief Get the last error message
     */
    const std::string& get_error() const { return error_; }
    
    /**
     * @brief Check if parsing had errors
     */
    bool has_error() const { return !error_.empty(); }

private:
    std::vector<Token> tokens_;
    size_t position_;
    std::string error_;
    
    // Recursive descent parser methods (in order of precedence)
    std::unique_ptr<ASTNode> parse_expression();
    std::unique_ptr<ASTNode> parse_ternary();
    std::unique_ptr<ASTNode> parse_logical_or();
    std::unique_ptr<ASTNode> parse_logical_and();
    std::unique_ptr<ASTNode> parse_equality();
    std::unique_ptr<ASTNode> parse_comparison();
    std::unique_ptr<ASTNode> parse_additive();
    std::unique_ptr<ASTNode> parse_multiplicative();
    std::unique_ptr<ASTNode> parse_unary();
    std::unique_ptr<ASTNode> parse_postfix();
    std::unique_ptr<ASTNode> parse_primary();
    
    // Helper methods
    const Token& current_token() const;
    const Token& peek_token(int offset = 1) const;
    void advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    Token consume(TokenType type, const std::string& error_message);
    
    void set_error(const std::string& message);
};

/**
 * @brief Value type for expression evaluation
 */
struct Value {
    enum class Type {
        Number,
        String,
        Boolean,
        Null
    };
    
    Type type;
    double number_value;
    std::string string_value;
    bool boolean_value;
    
    Value() : type(Type::Null), number_value(0), boolean_value(false) {}
    Value(double v) : type(Type::Number), number_value(v), boolean_value(false) {}
    Value(const std::string& v) : type(Type::String), string_value(v), number_value(0), boolean_value(false) {}
    Value(bool v) : type(Type::Boolean), boolean_value(v), number_value(0) {}
    
    bool is_truthy() const {
        if (type == Type::Boolean) return boolean_value;
        if (type == Type::Number) return number_value != 0;
        if (type == Type::String) return !string_value.empty();
        return false;
    }
    
    std::string to_string() const {
        if (type == Type::String) return string_value;
        if (type == Type::Number) return std::to_string(number_value);
        if (type == Type::Boolean) return boolean_value ? "true" : "false";
        return "null";
    }
};

/**
 * @brief Evaluation context for expressions
 */
struct EvaluationContext {
    std::map<std::string, Value> variables;  // Built-in variables (index, parent, canvas, etc.)
    DataNode* data_node = nullptr;           // Current data node
    Shape* shape = nullptr;                  // Current shape
    Shape* parent_shape = nullptr;           // Parent shape
    
    Value get_variable(const std::string& name) const {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        return Value();  // Null
    }
};

/**
 * @brief Expression evaluator
 * 
 * Evaluates AST nodes with an evaluation context.
 * Supports built-in variables and filters.
 */
class ExpressionEvaluator {
public:
    ExpressionEvaluator() = default;
    
    /**
     * @brief Evaluate an AST
     * @param root Root node of the AST
     * @param context Evaluation context
     * @return Evaluated value
     */
    Value evaluate(const ASTNode* root, const EvaluationContext& context);
    
    /**
     * @brief Get the last error message
     */
    const std::string& get_error() const { return error_; }
    
    /**
     * @brief Check if evaluation had errors
     */
    bool has_error() const { return !error_.empty(); }

private:
    std::string error_;
    
    Value evaluate_node(const ASTNode* node, const EvaluationContext& context);
    Value evaluate_binary_op(const BinaryOpNode* node, const EvaluationContext& context);
    Value evaluate_unary_op(const UnaryOpNode* node, const EvaluationContext& context);
    Value evaluate_ternary_op(const TernaryOpNode* node, const EvaluationContext& context);
    Value evaluate_member_access(const MemberAccessNode* node, const EvaluationContext& context);
    Value evaluate_filter(const FilterCallNode* node, const EvaluationContext& context);
    
    // Built-in filters
    Value apply_filter(const std::string& filter_name, const Value& value, 
                      const std::vector<Value>& args);
    
    void set_error(const std::string& message);
};

} // namespace ddf
} // namespace whiteboard
