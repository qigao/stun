#include <whiteboard/ddf/expression_parser.h>
#include <whiteboard/ddf/data_layer.h>
#include <whiteboard/ddf/shape_layer.h>
#include <cmath>

namespace whiteboard {
namespace ddf {

std::vector<Token> ExpressionTokenizer::tokenize(const std::string& expression) {
    expression_ = expression;
    position_ = 0;
    error_.clear();
    
    std::vector<Token> tokens;
    
    while (position_ < expression_.length()) {
        skip_whitespace();
        
        if (position_ >= expression_.length()) {
            break;
        }
        
        char c = current_char();
        
        // Numbers
        if (is_digit(c) || (c == '.' && is_digit(peek_char()))) {
            tokens.push_back(read_number());
        }
        // Identifiers
        else if (is_alpha(c) || c == '_') {
            tokens.push_back(read_identifier());
        }
        // Strings
        else if (c == '"' || c == '\'') {
            tokens.push_back(read_string());
        }
        // Operators and delimiters
        else {
            Token token = read_operator();
            if (token.type != TokenType::Invalid) {
                tokens.push_back(token);
            } else if (has_error()) {
                return tokens;  // Return partial tokens on error
            }
        }
        
        if (has_error()) {
            return tokens;
        }
    }
    
    // Add end of expression token
    tokens.push_back(Token(TokenType::EndOfExpression, "", static_cast<int>(position_)));
    
    return tokens;
}

char ExpressionTokenizer::current_char() const {
    if (position_ < expression_.length()) {
        return expression_[position_];
    }
    return '\0';
}

char ExpressionTokenizer::peek_char(int offset) const {
    size_t pos = position_ + offset;
    if (pos < expression_.length()) {
        return expression_[pos];
    }
    return '\0';
}

void ExpressionTokenizer::advance() {
    if (position_ < expression_.length()) {
        position_++;
    }
}

void ExpressionTokenizer::skip_whitespace() {
    while (position_ < expression_.length() && is_whitespace(current_char())) {
        advance();
    }
}

Token ExpressionTokenizer::read_number() {
    int start_pos = static_cast<int>(position_);
    std::string value;
    
    // Read digits before decimal point
    while (is_digit(current_char())) {
        value += current_char();
        advance();
    }
    
    // Read decimal point and digits after
    if (current_char() == '.' && is_digit(peek_char())) {
        value += current_char();
        advance();
        
        while (is_digit(current_char())) {
            value += current_char();
            advance();
        }
    }
    
    return Token(TokenType::Number, value, start_pos);
}

Token ExpressionTokenizer::read_identifier() {
    int start_pos = static_cast<int>(position_);
    std::string value;
    
    while (is_alnum(current_char()) || current_char() == '_') {
        value += current_char();
        advance();
    }
    
    return Token(TokenType::Identifier, value, start_pos);
}

Token ExpressionTokenizer::read_string() {
    int start_pos = static_cast<int>(position_);
    char quote = current_char();
    advance();  // Skip opening quote
    
    std::string value;
    
    while (current_char() != '\0' && current_char() != quote) {
        if (current_char() == '\\' && peek_char() != '\0') {
            // Handle escape sequences
            advance();
            char escaped = current_char();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                default: value += escaped; break;
            }
            advance();
        } else {
            value += current_char();
            advance();
        }
    }
    
    if (current_char() == quote) {
        advance();  // Skip closing quote
    } else {
        set_error("Unterminated string literal");
    }
    
    return Token(TokenType::String, value, start_pos);
}

Token ExpressionTokenizer::read_operator() {
    int start_pos = static_cast<int>(position_);
    char c = current_char();
    char next = peek_char();
    
    // Two-character operators
    if (c == '=' && next == '=') {
        advance();
        advance();
        return Token(TokenType::Equal, "==", start_pos);
    }
    if (c == '!' && next == '=') {
        advance();
        advance();
        return Token(TokenType::NotEqual, "!=", start_pos);
    }
    if (c == '>' && next == '=') {
        advance();
        advance();
        return Token(TokenType::GreaterEqual, ">=", start_pos);
    }
    if (c == '<' && next == '=') {
        advance();
        advance();
        return Token(TokenType::LessEqual, "<=", start_pos);
    }
    if (c == '&' && next == '&') {
        advance();
        advance();
        return Token(TokenType::And, "&&", start_pos);
    }
    if (c == '|' && next == '|') {
        advance();
        advance();
        return Token(TokenType::Or, "||", start_pos);
    }
    
    // Single-character operators
    advance();
    
    switch (c) {
        case '+': return Token(TokenType::Plus, "+", start_pos);
        case '-': return Token(TokenType::Minus, "-", start_pos);
        case '*': return Token(TokenType::Multiply, "*", start_pos);
        case '/': return Token(TokenType::Divide, "/", start_pos);
        case '%': return Token(TokenType::Modulo, "%", start_pos);
        case '>': return Token(TokenType::Greater, ">", start_pos);
        case '<': return Token(TokenType::Less, "<", start_pos);
        case '!': return Token(TokenType::Not, "!", start_pos);
        case '(': return Token(TokenType::LeftParen, "(", start_pos);
        case ')': return Token(TokenType::RightParen, ")", start_pos);
        case '.': return Token(TokenType::Dot, ".", start_pos);
        case '|': return Token(TokenType::Pipe, "|", start_pos);
        case ':': return Token(TokenType::Colon, ":", start_pos);
        case ',': return Token(TokenType::Comma, ",", start_pos);
        case '?': return Token(TokenType::Question, "?", start_pos);
        default:
            set_error(std::string("Unexpected character: ") + c);
            return Token(TokenType::Invalid, std::string(1, c), start_pos);
    }
}

bool ExpressionTokenizer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool ExpressionTokenizer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool ExpressionTokenizer::is_alnum(char c) const {
    return is_alpha(c) || is_digit(c);
}

bool ExpressionTokenizer::is_whitespace(char c) const {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void ExpressionTokenizer::set_error(const std::string& message) {
    error_ = message + " at position " + std::to_string(position_);
}

// Expression Parser Implementation

std::unique_ptr<ASTNode> ExpressionParser::parse(const std::vector<Token>& tokens) {
    tokens_ = tokens;
    position_ = 0;
    error_.clear();
    
    if (tokens_.empty() || tokens_[0].type == TokenType::EndOfExpression) {
        set_error("Empty expression");
        return nullptr;
    }
    
    auto result = parse_expression();
    
    // Check if we consumed all tokens
    if (!has_error() && current_token().type != TokenType::EndOfExpression) {
        set_error("Unexpected token after expression");
        return nullptr;
    }
    
    return result;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_expression() {
    return parse_ternary();
}

std::unique_ptr<ASTNode> ExpressionParser::parse_ternary() {
    auto expr = parse_logical_or();
    
    if (match(TokenType::Question)) {
        auto true_expr = parse_expression();
        consume(TokenType::Colon, "Expected ':' in ternary expression");
        auto false_expr = parse_expression();
        
        return std::make_unique<TernaryOpNode>(
            std::move(expr), 
            std::move(true_expr), 
            std::move(false_expr)
        );
    }
    
    return expr;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_logical_or() {
    auto left = parse_logical_and();
    
    while (match(TokenType::Or)) {
        TokenType op = tokens_[position_ - 1].type;
        auto right = parse_logical_and();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_logical_and() {
    auto left = parse_equality();
    
    while (match(TokenType::And)) {
        TokenType op = tokens_[position_ - 1].type;
        auto right = parse_equality();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_equality() {
    auto left = parse_comparison();
    
    while (match(TokenType::Equal) || match(TokenType::NotEqual)) {
        TokenType op = tokens_[position_ - 1].type;
        auto right = parse_comparison();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_comparison() {
    auto left = parse_additive();
    
    while (match(TokenType::Greater) || match(TokenType::GreaterEqual) ||
           match(TokenType::Less) || match(TokenType::LessEqual)) {
        TokenType op = tokens_[position_ - 1].type;
        auto right = parse_additive();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_additive() {
    auto left = parse_multiplicative();
    
    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        TokenType op = tokens_[position_ - 1].type;
        auto right = parse_multiplicative();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_multiplicative() {
    auto left = parse_unary();
    
    while (match(TokenType::Multiply) || match(TokenType::Divide) || match(TokenType::Modulo)) {
        TokenType op = tokens_[position_ - 1].type;
        auto right = parse_unary();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_unary() {
    if (match(TokenType::Not) || match(TokenType::Minus)) {
        TokenType op = tokens_[position_ - 1].type;
        auto operand = parse_unary();
        return std::make_unique<UnaryOpNode>(op, std::move(operand));
    }
    
    return parse_postfix();
}

std::unique_ptr<ASTNode> ExpressionParser::parse_postfix() {
    auto expr = parse_primary();
    
    while (true) {
        if (match(TokenType::Dot)) {
            // Member access: object.property
            Token member = consume(TokenType::Identifier, "Expected property name after '.'");
            expr = std::make_unique<MemberAccessNode>(std::move(expr), member.value);
        }
        else if (match(TokenType::Pipe)) {
            // Filter call: value | filter or value | filter: arg
            Token filter = consume(TokenType::Identifier, "Expected filter name after '|'");
            auto filter_node = std::make_unique<FilterCallNode>(std::move(expr), filter.value);
            
            // Check for filter arguments
            if (match(TokenType::Colon)) {
                // Parse filter arguments (comma-separated)
                do {
                    filter_node->arguments.push_back(parse_primary());
                } while (match(TokenType::Comma));
            }
            
            expr = std::move(filter_node);
        }
        else {
            break;
        }
    }
    
    return expr;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_primary() {
    // Number literal
    if (check(TokenType::Number)) {
        Token token = current_token();
        advance();
        double value = std::stod(token.value);
        return std::make_unique<NumberNode>(value);
    }
    
    // String literal
    if (check(TokenType::String)) {
        Token token = current_token();
        advance();
        return std::make_unique<StringNode>(token.value);
    }
    
    // Identifier
    if (check(TokenType::Identifier)) {
        Token token = current_token();
        advance();
        return std::make_unique<IdentifierNode>(token.value);
    }
    
    // Parenthesized expression
    if (match(TokenType::LeftParen)) {
        auto expr = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after expression");
        return expr;
    }
    
    set_error("Expected expression");
    return nullptr;
}

const Token& ExpressionParser::current_token() const {
    if (position_ < tokens_.size()) {
        return tokens_[position_];
    }
    static Token end_token(TokenType::EndOfExpression, "", -1);
    return end_token;
}

const Token& ExpressionParser::peek_token(int offset) const {
    size_t pos = position_ + offset;
    if (pos < tokens_.size()) {
        return tokens_[pos];
    }
    static Token end_token(TokenType::EndOfExpression, "", -1);
    return end_token;
}

void ExpressionParser::advance() {
    if (position_ < tokens_.size()) {
        position_++;
    }
}

bool ExpressionParser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool ExpressionParser::check(TokenType type) const {
    return current_token().type == type;
}

Token ExpressionParser::consume(TokenType type, const std::string& error_message) {
    if (check(type)) {
        Token token = current_token();
        advance();
        return token;
    }
    
    set_error(error_message);
    return Token(TokenType::Invalid, "", -1);
}

void ExpressionParser::set_error(const std::string& message) {
    if (error_.empty()) {  // Only set first error
        error_ = message;
        if (position_ < tokens_.size()) {
            error_ += " at position " + std::to_string(tokens_[position_].position);
        }
    }
}

// Expression Evaluator Implementation

Value ExpressionEvaluator::evaluate(const ASTNode* root, const EvaluationContext& context) {
    error_.clear();
    
    if (!root) {
        set_error("Null AST node");
        return Value();
    }
    
    return evaluate_node(root, context);
}

Value ExpressionEvaluator::evaluate_node(const ASTNode* node, const EvaluationContext& context) {
    if (!node) return Value();
    
    switch (node->type) {
        case ASTNodeType::Number:
            return Value(static_cast<const NumberNode*>(node)->value);
            
        case ASTNodeType::String:
            return Value(static_cast<const StringNode*>(node)->value);
            
        case ASTNodeType::Identifier: {
            const auto* id_node = static_cast<const IdentifierNode*>(node);
            
            // Check context variables first
            Value var = context.get_variable(id_node->name);
            if (var.type != Value::Type::Null) {
                return var;
            }
            
            // Check data node properties
            if (context.data_node) {
                auto it = context.data_node->properties.find(id_node->name);
                if (it != context.data_node->properties.end()) {
                    return Value(it->second);
                }
            }
            
            return Value();  // Null if not found
        }
            
        case ASTNodeType::BinaryOp:
            return evaluate_binary_op(static_cast<const BinaryOpNode*>(node), context);
            
        case ASTNodeType::UnaryOp:
            return evaluate_unary_op(static_cast<const UnaryOpNode*>(node), context);
            
        case ASTNodeType::TernaryOp:
            return evaluate_ternary_op(static_cast<const TernaryOpNode*>(node), context);
            
        case ASTNodeType::MemberAccess:
            return evaluate_member_access(static_cast<const MemberAccessNode*>(node), context);
            
        case ASTNodeType::FilterCall:
            return evaluate_filter(static_cast<const FilterCallNode*>(node), context);
            
        default:
            set_error("Unknown AST node type");
            return Value();
    }
}

Value ExpressionEvaluator::evaluate_binary_op(const BinaryOpNode* node, const EvaluationContext& context) {
    Value left = evaluate_node(node->left.get(), context);
    Value right = evaluate_node(node->right.get(), context);
    
    switch (node->op) {
        // Arithmetic
        case TokenType::Plus:
            if (left.type == Value::Type::Number && right.type == Value::Type::Number) {
                return Value(left.number_value + right.number_value);
            }
            // String concatenation
            return Value(left.to_string() + right.to_string());
            
        case TokenType::Minus:
            return Value(left.number_value - right.number_value);
            
        case TokenType::Multiply:
            return Value(left.number_value * right.number_value);
            
        case TokenType::Divide:
            if (right.number_value == 0) {
                set_error("Division by zero");
                return Value();
            }
            return Value(left.number_value / right.number_value);
            
        case TokenType::Modulo:
            return Value(std::fmod(left.number_value, right.number_value));
            
        // Comparison
        case TokenType::Equal:
            if (left.type == Value::Type::Number && right.type == Value::Type::Number) {
                return Value(left.number_value == right.number_value);
            }
            return Value(left.to_string() == right.to_string());
            
        case TokenType::NotEqual:
            if (left.type == Value::Type::Number && right.type == Value::Type::Number) {
                return Value(left.number_value != right.number_value);
            }
            return Value(left.to_string() != right.to_string());
            
        case TokenType::Greater:
            return Value(left.number_value > right.number_value);
            
        case TokenType::Less:
            return Value(left.number_value < right.number_value);
            
        case TokenType::GreaterEqual:
            return Value(left.number_value >= right.number_value);
            
        case TokenType::LessEqual:
            return Value(left.number_value <= right.number_value);
            
        // Logical
        case TokenType::And:
            return Value(left.is_truthy() && right.is_truthy());
            
        case TokenType::Or:
            return Value(left.is_truthy() || right.is_truthy());
            
        default:
            set_error("Unknown binary operator");
            return Value();
    }
}

Value ExpressionEvaluator::evaluate_unary_op(const UnaryOpNode* node, const EvaluationContext& context) {
    Value operand = evaluate_node(node->operand.get(), context);
    
    switch (node->op) {
        case TokenType::Not:
            return Value(!operand.is_truthy());
            
        case TokenType::Minus:
            return Value(-operand.number_value);
            
        default:
            set_error("Unknown unary operator");
            return Value();
    }
}

Value ExpressionEvaluator::evaluate_ternary_op(const TernaryOpNode* node, const EvaluationContext& context) {
    Value condition = evaluate_node(node->condition.get(), context);
    
    if (condition.is_truthy()) {
        return evaluate_node(node->true_expr.get(), context);
    } else {
        return evaluate_node(node->false_expr.get(), context);
    }
}

Value ExpressionEvaluator::evaluate_member_access(const MemberAccessNode* node, const EvaluationContext& context) {
    Value object = evaluate_node(node->object.get(), context);
    
    // For now, member access on identifiers only
    // Could be extended to support nested objects
    
    const auto* id_node = dynamic_cast<const IdentifierNode*>(node->object.get());
    if (!id_node) {
        set_error("Member access only supported on identifiers");
        return Value();
    }
    
    // Check for special objects
    if (id_node->name == "parent" && context.parent_shape) {
        // Access parent shape properties
        if (node->member == "x" && context.parent_shape->geometry.count("x")) {
            return Value(context.parent_shape->geometry.at("x"));
        }
        if (node->member == "y" && context.parent_shape->geometry.count("y")) {
            return Value(context.parent_shape->geometry.at("y"));
        }
        if (node->member == "width" && context.parent_shape->geometry.count("width")) {
            return Value(context.parent_shape->geometry.at("width"));
        }
        if (node->member == "height" && context.parent_shape->geometry.count("height")) {
            return Value(context.parent_shape->geometry.at("height"));
        }
    }
    
    if (id_node->name == "canvas") {
        // Access canvas properties from context
        Value canvas = context.get_variable("canvas");
        // Would need nested object support for canvas.width, etc.
    }
    
    if (id_node->name == "data" && context.data_node) {
        // Access data node properties
        auto it = context.data_node->properties.find(node->member);
        if (it != context.data_node->properties.end()) {
            return Value(it->second);
        }
    }
    
    return Value();
}

Value ExpressionEvaluator::evaluate_filter(const FilterCallNode* node, const EvaluationContext& context) {
    Value value = evaluate_node(node->value.get(), context);
    
    // Evaluate filter arguments
    std::vector<Value> args;
    for (const auto& arg_node : node->arguments) {
        args.push_back(evaluate_node(arg_node.get(), context));
    }
    
    return apply_filter(node->filter_name, value, args);
}

Value ExpressionEvaluator::apply_filter(const std::string& filter_name, const Value& value, 
                                       const std::vector<Value>& args) {
    // String transformations
    if (filter_name == "uppercase") {
        std::string str = value.to_string();
        for (char& c : str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return Value(str);
    }
    
    if (filter_name == "lowercase") {
        std::string str = value.to_string();
        for (char& c : str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return Value(str);
    }
    
    if (filter_name == "capitalize") {
        std::string str = value.to_string();
        if (!str.empty()) {
            str[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(str[0])));
        }
        return Value(str);
    }
    
    // Math operations
    if (filter_name == "round") {
        return Value(std::round(value.number_value));
    }
    
    if (filter_name == "floor") {
        return Value(std::floor(value.number_value));
    }
    
    if (filter_name == "ceil") {
        return Value(std::ceil(value.number_value));
    }
    
    if (filter_name == "abs") {
        return Value(std::abs(value.number_value));
    }
    
    // Default value filter
    if (filter_name == "default") {
        if (value.type == Value::Type::Null && !args.empty()) {
            return args[0];
        }
        return value;
    }
    
    // Format filter (simple implementation)
    if (filter_name == "format") {
        // Would need more sophisticated formatting
        return value;
    }
    
    // Color map filter (placeholder)
    if (filter_name == "color_map") {
        // Would map values to colors based on ranges
        return value;
    }
    
    set_error("Unknown filter: " + filter_name);
    return value;
}

void ExpressionEvaluator::set_error(const std::string& message) {
    if (error_.empty()) {  // Only set first error
        error_ = message;
    }
}

} // namespace ddf
} // namespace whiteboard
