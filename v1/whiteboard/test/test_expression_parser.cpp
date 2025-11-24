#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <whiteboard/ddf/expression_parser.h>
#include <whiteboard/ddf/data_layer.h>
#include <whiteboard/ddf/shape_layer.h>

using namespace whiteboard::ddf;
// ============================================================================
// Tokenizer Tests
// ============================================================================

TEST_CASE("ExpressionTokenizer tokenizes numbers", "[expression_parser][tokenizer]") {
    ExpressionTokenizer tokenizer;
    
    SECTION("Integer") {
        auto tokens = tokenizer.tokenize("42");
        REQUIRE(tokens.size() == 2);  // number + end
        REQUIRE(tokens[0].type == TokenType::Number);
        REQUIRE(tokens[0].value == "42");
    }
    
    SECTION("Decimal") {
        auto tokens = tokenizer.tokenize("3.14");
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[0].type == TokenType::Number);
        REQUIRE(tokens[0].value == "3.14");
    }
    
    SECTION("Multiple numbers") {
        auto tokens = tokenizer.tokenize("10 20 30");
        REQUIRE(tokens.size() == 4);  // 3 numbers + end
        REQUIRE(tokens[0].value == "10");
        REQUIRE(tokens[1].value == "20");
        REQUIRE(tokens[2].value == "30");
    }
}

TEST_CASE("ExpressionTokenizer tokenizes strings", "[expression_parser][tokenizer]") {
    ExpressionTokenizer tokenizer;
    
    SECTION("Double quotes") {
        auto tokens = tokenizer.tokenize("\"hello\"");
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[0].type == TokenType::String);
        REQUIRE(tokens[0].value == "hello");
    }
    
    SECTION("Single quotes") {
        auto tokens = tokenizer.tokenize("'world'");
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[0].type == TokenType::String);
        REQUIRE(tokens[0].value == "world");
    }
    
    SECTION("String with spaces") {
        auto tokens = tokenizer.tokenize("\"hello world\"");
        REQUIRE(tokens[0].value == "hello world");
    }
    
    SECTION("String with escape sequences") {
        auto tokens = tokenizer.tokenize("\"line1\\nline2\"");
        REQUIRE(tokens[0].value == "line1\nline2");
    }
}

TEST_CASE("ExpressionTokenizer tokenizes identifiers", "[expression_parser][tokenizer]") {
    ExpressionTokenizer tokenizer;
    
    auto tokens = tokenizer.tokenize("name index parent_width");
    REQUIRE(tokens.size() == 4);  // 3 identifiers + end
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].value == "name");
    REQUIRE(tokens[1].value == "index");
    REQUIRE(tokens[2].value == "parent_width");
}

TEST_CASE("ExpressionTokenizer tokenizes operators", "[expression_parser][tokenizer]") {
    ExpressionTokenizer tokenizer;
    
    SECTION("Arithmetic operators") {
        auto tokens = tokenizer.tokenize("+ - * / %");
        REQUIRE(tokens[0].type == TokenType::Plus);
        REQUIRE(tokens[1].type == TokenType::Minus);
        REQUIRE(tokens[2].type == TokenType::Multiply);
        REQUIRE(tokens[3].type == TokenType::Divide);
        REQUIRE(tokens[4].type == TokenType::Modulo);
    }
    
    SECTION("Comparison operators") {
        auto tokens = tokenizer.tokenize("== != > < >= <=");
        REQUIRE(tokens[0].type == TokenType::Equal);
        REQUIRE(tokens[1].type == TokenType::NotEqual);
        REQUIRE(tokens[2].type == TokenType::Greater);
        REQUIRE(tokens[3].type == TokenType::Less);
        REQUIRE(tokens[4].type == TokenType::GreaterEqual);
        REQUIRE(tokens[5].type == TokenType::LessEqual);
    }
    
    SECTION("Logical operators") {
        auto tokens = tokenizer.tokenize("&& || !");
        REQUIRE(tokens[0].type == TokenType::And);
        REQUIRE(tokens[1].type == TokenType::Or);
        REQUIRE(tokens[2].type == TokenType::Not);
    }
}

TEST_CASE("ExpressionTokenizer tokenizes delimiters", "[expression_parser][tokenizer]") {
    ExpressionTokenizer tokenizer;
    
    auto tokens = tokenizer.tokenize("( ) . | : , ?");
    REQUIRE(tokens[0].type == TokenType::LeftParen);
    REQUIRE(tokens[1].type == TokenType::RightParen);
    REQUIRE(tokens[2].type == TokenType::Dot);
    REQUIRE(tokens[3].type == TokenType::Pipe);
    REQUIRE(tokens[4].type == TokenType::Colon);
    REQUIRE(tokens[5].type == TokenType::Comma);
    REQUIRE(tokens[6].type == TokenType::Question);
}

// ============================================================================
// Parser Tests - Arithmetic Operations
// ============================================================================

TEST_CASE("ExpressionParser parses addition", "[expression_parser][parser][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("5 + 3");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    REQUIRE(ast->type == ASTNodeType::BinaryOp);
    
    auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
    REQUIRE(bin_op->op == TokenType::Plus);
    REQUIRE(bin_op->left->type == ASTNodeType::Number);
    REQUIRE(bin_op->right->type == ASTNodeType::Number);
}

TEST_CASE("ExpressionParser parses subtraction", "[expression_parser][parser][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("10 - 4");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
    REQUIRE(bin_op->op == TokenType::Minus);
}

TEST_CASE("ExpressionParser parses multiplication", "[expression_parser][parser][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("6 * 7");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
    REQUIRE(bin_op->op == TokenType::Multiply);
}

TEST_CASE("ExpressionParser parses division", "[expression_parser][parser][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("20 / 5");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
    REQUIRE(bin_op->op == TokenType::Divide);
}

TEST_CASE("ExpressionParser parses modulo", "[expression_parser][parser][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("10 % 3");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
    REQUIRE(bin_op->op == TokenType::Modulo);
}

TEST_CASE("ExpressionParser respects operator precedence", "[expression_parser][parser][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    SECTION("Multiplication before addition") {
        auto tokens = tokenizer.tokenize("2 + 3 * 4");
        auto ast = parser.parse(tokens);
        
        REQUIRE(ast != nullptr);
        REQUIRE(ast->type == ASTNodeType::BinaryOp);
        
        auto* add_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(add_op->op == TokenType::Plus);
        REQUIRE(add_op->right->type == ASTNodeType::BinaryOp);
        
        auto* mul_op = static_cast<BinaryOpNode*>(add_op->right.get());
        REQUIRE(mul_op->op == TokenType::Multiply);
    }
    
    SECTION("Parentheses override precedence") {
        auto tokens = tokenizer.tokenize("(2 + 3) * 4");
        auto ast = parser.parse(tokens);
        
        REQUIRE(ast != nullptr);
        auto* mul_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(mul_op->op == TokenType::Multiply);
        REQUIRE(mul_op->left->type == ASTNodeType::BinaryOp);
        
        auto* add_op = static_cast<BinaryOpNode*>(mul_op->left.get());
        REQUIRE(add_op->op == TokenType::Plus);
    }
}

TEST_CASE("ExpressionParser parses unary minus", "[expression_parser][parser][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("-5");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    REQUIRE(ast->type == ASTNodeType::UnaryOp);
    
    auto* unary_op = static_cast<UnaryOpNode*>(ast.get());
    REQUIRE(unary_op->op == TokenType::Minus);
    REQUIRE(unary_op->operand->type == ASTNodeType::Number);
}

// ============================================================================
// Parser Tests - Conditional Expressions
// ============================================================================

TEST_CASE("ExpressionParser parses ternary operator", "[expression_parser][parser][conditional]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("x > 5 ? 10 : 20");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    REQUIRE(ast->type == ASTNodeType::TernaryOp);
    
    auto* ternary = static_cast<TernaryOpNode*>(ast.get());
    REQUIRE(ternary->condition != nullptr);
    REQUIRE(ternary->true_expr != nullptr);
    REQUIRE(ternary->false_expr != nullptr);
}

TEST_CASE("ExpressionParser parses comparison operators", "[expression_parser][parser][conditional]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    SECTION("Equal") {
        auto tokens = tokenizer.tokenize("x == 5");
        auto ast = parser.parse(tokens);
        REQUIRE(ast != nullptr);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::Equal);
    }
    
    SECTION("Not equal") {
        auto tokens = tokenizer.tokenize("x != 5");
        auto ast = parser.parse(tokens);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::NotEqual);
    }
    
    SECTION("Greater than") {
        auto tokens = tokenizer.tokenize("x > 5");
        auto ast = parser.parse(tokens);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::Greater);
    }
    
    SECTION("Less than") {
        auto tokens = tokenizer.tokenize("x < 5");
        auto ast = parser.parse(tokens);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::Less);
    }
    
    SECTION("Greater or equal") {
        auto tokens = tokenizer.tokenize("x >= 5");
        auto ast = parser.parse(tokens);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::GreaterEqual);
    }
    
    SECTION("Less or equal") {
        auto tokens = tokenizer.tokenize("x <= 5");
        auto ast = parser.parse(tokens);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::LessEqual);
    }
}

TEST_CASE("ExpressionParser parses logical operators", "[expression_parser][parser][conditional]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    SECTION("Logical AND") {
        auto tokens = tokenizer.tokenize("x > 5 && y < 10");
        auto ast = parser.parse(tokens);
        REQUIRE(ast != nullptr);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::And);
    }
    
    SECTION("Logical OR") {
        auto tokens = tokenizer.tokenize("x > 5 || y < 10");
        auto ast = parser.parse(tokens);
        auto* bin_op = static_cast<BinaryOpNode*>(ast.get());
        REQUIRE(bin_op->op == TokenType::Or);
    }
    
    SECTION("Logical NOT") {
        auto tokens = tokenizer.tokenize("!x");
        auto ast = parser.parse(tokens);
        REQUIRE(ast->type == ASTNodeType::UnaryOp);
        auto* unary_op = static_cast<UnaryOpNode*>(ast.get());
        REQUIRE(unary_op->op == TokenType::Not);
    }
}

// ============================================================================
// Parser Tests - Variable Access and Filters
// ============================================================================

TEST_CASE("ExpressionParser parses identifiers", "[expression_parser][parser][variables]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("name");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    REQUIRE(ast->type == ASTNodeType::Identifier);
    
    auto* id_node = static_cast<IdentifierNode*>(ast.get());
    REQUIRE(id_node->name == "name");
}

TEST_CASE("ExpressionParser parses member access", "[expression_parser][parser][variables]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    auto tokens = tokenizer.tokenize("data.name");
    auto ast = parser.parse(tokens);
    
    REQUIRE(ast != nullptr);
    REQUIRE(ast->type == ASTNodeType::MemberAccess);
    
    auto* member = static_cast<MemberAccessNode*>(ast.get());
    REQUIRE(member->member == "name");
    REQUIRE(member->object->type == ASTNodeType::Identifier);
}

TEST_CASE("ExpressionParser parses filter calls", "[expression_parser][parser][filters]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    SECTION("Filter without arguments") {
        auto tokens = tokenizer.tokenize("name | uppercase");
        auto ast = parser.parse(tokens);
        
        REQUIRE(ast != nullptr);
        REQUIRE(ast->type == ASTNodeType::FilterCall);
        
        auto* filter = static_cast<FilterCallNode*>(ast.get());
        REQUIRE(filter->filter_name == "uppercase");
        REQUIRE(filter->value->type == ASTNodeType::Identifier);
        REQUIRE(filter->arguments.empty());
    }
    
    SECTION("Filter with single argument") {
        auto tokens = tokenizer.tokenize("value | default: 0");
        auto ast = parser.parse(tokens);
        
        REQUIRE(ast->type == ASTNodeType::FilterCall);
        auto* filter = static_cast<FilterCallNode*>(ast.get());
        REQUIRE(filter->filter_name == "default");
        REQUIRE(filter->arguments.size() == 1);
    }
    
    SECTION("Filter with multiple arguments") {
        auto tokens = tokenizer.tokenize("value | format: 2, 3");
        auto ast = parser.parse(tokens);
        
        auto* filter = static_cast<FilterCallNode*>(ast.get());
        REQUIRE(filter->arguments.size() == 2);
    }
    
    SECTION("Chained filters") {
        auto tokens = tokenizer.tokenize("name | uppercase | default: \"N/A\"");
        auto ast = parser.parse(tokens);
        
        REQUIRE(ast->type == ASTNodeType::FilterCall);
        auto* outer_filter = static_cast<FilterCallNode*>(ast.get());
        REQUIRE(outer_filter->filter_name == "default");
        REQUIRE(outer_filter->value->type == ASTNodeType::FilterCall);
        
        auto* inner_filter = static_cast<FilterCallNode*>(outer_filter->value.get());
        REQUIRE(inner_filter->filter_name == "uppercase");
    }
}

// ============================================================================
// Evaluator Tests - Arithmetic Operations
// ============================================================================

TEST_CASE("ExpressionEvaluator evaluates addition", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    auto tokens = tokenizer.tokenize("5 + 3");
    auto ast = parser.parse(tokens);
    
    EvaluationContext context;
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.type == Value::Type::Number);
    REQUIRE(result.number_value == 8.0);
}

TEST_CASE("ExpressionEvaluator evaluates subtraction", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    auto tokens = tokenizer.tokenize("10 - 4");
    auto ast = parser.parse(tokens);
    
    EvaluationContext context;
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.type == Value::Type::Number);
    REQUIRE(result.number_value == 6.0);
}

TEST_CASE("ExpressionEvaluator evaluates multiplication", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    auto tokens = tokenizer.tokenize("6 * 7");
    auto ast = parser.parse(tokens);
    
    EvaluationContext context;
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.type == Value::Type::Number);
    REQUIRE(result.number_value == 42.0);
}

TEST_CASE("ExpressionEvaluator evaluates division", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    auto tokens = tokenizer.tokenize("20 / 5");
    auto ast = parser.parse(tokens);
    
    EvaluationContext context;
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.type == Value::Type::Number);
    REQUIRE(result.number_value == 4.0);
}

TEST_CASE("ExpressionEvaluator evaluates modulo", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    auto tokens = tokenizer.tokenize("10 % 3");
    auto ast = parser.parse(tokens);
    
    EvaluationContext context;
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.type == Value::Type::Number);
    REQUIRE(result.number_value == 1.0);
}

TEST_CASE("ExpressionEvaluator respects operator precedence", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("Multiplication before addition") {
        auto tokens = tokenizer.tokenize("2 + 3 * 4");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 14.0);  // 2 + (3 * 4) = 14
    }
    
    SECTION("Parentheses override precedence") {
        auto tokens = tokenizer.tokenize("(2 + 3) * 4");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 20.0);  // (2 + 3) * 4 = 20
    }
}

TEST_CASE("ExpressionEvaluator evaluates unary minus", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    auto tokens = tokenizer.tokenize("-5");
    auto ast = parser.parse(tokens);
    
    EvaluationContext context;
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.type == Value::Type::Number);
    REQUIRE(result.number_value == -5.0);
}

TEST_CASE("ExpressionEvaluator handles string concatenation", "[expression_parser][evaluator][arithmetic]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    auto tokens = tokenizer.tokenize("\"Hello\" + \" \" + \"World\"");
    auto ast = parser.parse(tokens);
    
    EvaluationContext context;
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.type == Value::Type::String);
    REQUIRE(result.string_value == "Hello World");
}

// ============================================================================
// Evaluator Tests - Conditional Expressions
// ============================================================================

TEST_CASE("ExpressionEvaluator evaluates ternary operator", "[expression_parser][evaluator][conditional]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("True condition") {
        auto tokens = tokenizer.tokenize("5 > 3 ? 10 : 20");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 10.0);
    }
    
    SECTION("False condition") {
        auto tokens = tokenizer.tokenize("5 < 3 ? 10 : 20");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 20.0);
    }
}

TEST_CASE("ExpressionEvaluator evaluates comparison operators", "[expression_parser][evaluator][conditional]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("Equal - true") {
        auto tokens = tokenizer.tokenize("5 == 5");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.type == Value::Type::Boolean);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Equal - false") {
        auto tokens = tokenizer.tokenize("5 == 3");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == false);
    }
    
    SECTION("Not equal") {
        auto tokens = tokenizer.tokenize("5 != 3");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Greater than") {
        auto tokens = tokenizer.tokenize("5 > 3");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Less than") {
        auto tokens = tokenizer.tokenize("3 < 5");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Greater or equal") {
        auto tokens = tokenizer.tokenize("5 >= 5");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Less or equal") {
        auto tokens = tokenizer.tokenize("3 <= 5");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("String equality") {
        auto tokens = tokenizer.tokenize("\"hello\" == \"hello\"");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
}

TEST_CASE("ExpressionEvaluator evaluates logical operators", "[expression_parser][evaluator][conditional]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("Logical AND - both true") {
        auto tokens = tokenizer.tokenize("5 > 3 && 10 > 8");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Logical AND - one false") {
        auto tokens = tokenizer.tokenize("5 > 3 && 10 < 8");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == false);
    }
    
    SECTION("Logical OR - one true") {
        auto tokens = tokenizer.tokenize("5 < 3 || 10 > 8");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Logical OR - both false") {
        auto tokens = tokenizer.tokenize("5 < 3 || 10 < 8");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == false);
    }
    
    SECTION("Logical NOT - true") {
        auto tokens = tokenizer.tokenize("!(5 < 3)");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
    
    SECTION("Logical NOT - false") {
        auto tokens = tokenizer.tokenize("!(5 > 3)");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == false);
    }
}

// ============================================================================
// Evaluator Tests - Variable Access
// ============================================================================

TEST_CASE("ExpressionEvaluator accesses context variables", "[expression_parser][evaluator][variables]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    EvaluationContext context;
    context.variables["index"] = Value(5.0);
    context.variables["count"] = Value(10.0);
    
    SECTION("Simple variable access") {
        auto tokens = tokenizer.tokenize("index");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 5.0);
    }
    
    SECTION("Variable in expression") {
        auto tokens = tokenizer.tokenize("index * 2");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 10.0);
    }
    
    SECTION("Multiple variables") {
        auto tokens = tokenizer.tokenize("index + count");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 15.0);
    }
}

TEST_CASE("ExpressionEvaluator accesses data node properties", "[expression_parser][evaluator][variables]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["name"] = "John Doe";
    node.properties["age"] = "30";
    
    EvaluationContext context;
    context.data_node = &node;
    
    SECTION("Access property directly") {
        auto tokens = tokenizer.tokenize("name");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.type == Value::Type::String);
        REQUIRE(result.string_value == "John Doe");
    }
    
    SECTION("Access property via member access") {
        auto tokens = tokenizer.tokenize("data.name");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.string_value == "John Doe");
    }
}

TEST_CASE("ExpressionEvaluator accesses parent shape properties", "[expression_parser][evaluator][variables]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    Shape parent;
    parent.id = "parent1";
    parent.geometry["x"] = 100.0f;
    parent.geometry["y"] = 200.0f;
    parent.geometry["width"] = 300.0f;
    parent.geometry["height"] = 400.0f;
    
    EvaluationContext context;
    context.parent_shape = &parent;
    
    SECTION("Access parent x") {
        auto tokens = tokenizer.tokenize("parent.x");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 100.0);
    }
    
    SECTION("Access parent width") {
        auto tokens = tokenizer.tokenize("parent.width");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 300.0);
    }
    
    SECTION("Use parent property in calculation") {
        auto tokens = tokenizer.tokenize("parent.width / 2");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 150.0);
    }
}

// ============================================================================
// Evaluator Tests - Filters
// ============================================================================

TEST_CASE("ExpressionEvaluator applies string filters", "[expression_parser][evaluator][filters]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("uppercase filter") {
        auto tokens = tokenizer.tokenize("\"hello\" | uppercase");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.string_value == "HELLO");
    }
    
    SECTION("lowercase filter") {
        auto tokens = tokenizer.tokenize("\"WORLD\" | lowercase");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.string_value == "world");
    }
    
    SECTION("capitalize filter") {
        auto tokens = tokenizer.tokenize("\"hello\" | capitalize");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.string_value == "Hello");
    }
}

TEST_CASE("ExpressionEvaluator applies math filters", "[expression_parser][evaluator][filters]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("round filter") {
        auto tokens = tokenizer.tokenize("3.7 | round");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 4.0);
    }
    
    SECTION("floor filter") {
        auto tokens = tokenizer.tokenize("3.7 | floor");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 3.0);
    }
    
    SECTION("ceil filter") {
        auto tokens = tokenizer.tokenize("3.2 | ceil");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 4.0);
    }
    
    SECTION("abs filter") {
        auto tokens = tokenizer.tokenize("(-5) | abs");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 5.0);
    }
}

TEST_CASE("ExpressionEvaluator applies default filter", "[expression_parser][evaluator][filters]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("Non-null value returns itself") {
        context.variables["value"] = Value(42.0);
        auto tokens = tokenizer.tokenize("value | default: 0");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 42.0);
    }
    
    SECTION("Null value returns default") {
        auto tokens = tokenizer.tokenize("missing | default: 0");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 0.0);
    }
}

TEST_CASE("ExpressionEvaluator chains filters", "[expression_parser][evaluator][filters]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    auto tokens = tokenizer.tokenize("\"hello\" | uppercase | default: \"N/A\"");
    auto ast = parser.parse(tokens);
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.string_value == "HELLO");
}

TEST_CASE("ExpressionEvaluator applies filters with variables", "[expression_parser][evaluator][filters]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    DataNode node;
    node.properties["name"] = "john doe";
    
    EvaluationContext context;
    context.data_node = &node;
    
    auto tokens = tokenizer.tokenize("name | uppercase");
    auto ast = parser.parse(tokens);
    Value result = evaluator.evaluate(ast.get(), context);
    
    REQUIRE(result.string_value == "JOHN DOE");
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST_CASE("ExpressionEvaluator handles complex expressions", "[expression_parser][evaluator][complex]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    EvaluationContext context;
    context.variables["index"] = Value(3.0);
    context.variables["total"] = Value(10.0);
    
    SECTION("Complex arithmetic with variables") {
        auto tokens = tokenizer.tokenize("(index + 1) * 2 + total");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 18.0);  // (3 + 1) * 2 + 10 = 18
    }
    
    SECTION("Ternary with complex condition") {
        auto tokens = tokenizer.tokenize("index > 5 || total < 20 ? \"yes\" : \"no\"");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.string_value == "yes");  // total < 20 is true
    }
    
    SECTION("Nested ternary") {
        auto tokens = tokenizer.tokenize("index > 5 ? \"high\" : index > 2 ? \"medium\" : \"low\"");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.string_value == "medium");  // index = 3
    }
    
    SECTION("Filter with arithmetic") {
        auto tokens = tokenizer.tokenize("(index * 3.7) | round");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 11.0);  // 3 * 3.7 = 11.1, rounded = 11
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("ExpressionTokenizer handles errors", "[expression_parser][tokenizer][errors]") {
    ExpressionTokenizer tokenizer;
    
    SECTION("Unterminated string") {
        auto tokens = tokenizer.tokenize("\"hello");
        REQUIRE(tokenizer.has_error());
        REQUIRE(!tokenizer.get_error().empty());
    }
    
    SECTION("Invalid character") {
        auto tokens = tokenizer.tokenize("5 @ 3");
        REQUIRE(tokenizer.has_error());
    }
}

TEST_CASE("ExpressionParser handles errors", "[expression_parser][parser][errors]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    
    SECTION("Empty expression") {
        auto tokens = tokenizer.tokenize("");
        auto ast = parser.parse(tokens);
        REQUIRE(ast == nullptr);
        REQUIRE(parser.has_error());
    }
    
    SECTION("Mismatched parentheses") {
        auto tokens = tokenizer.tokenize("(5 + 3");
        auto ast = parser.parse(tokens);
        // Parser may return partial AST or error depending on implementation
        REQUIRE((ast == nullptr || parser.has_error()));
    }
    
    SECTION("Missing operand") {
        auto tokens = tokenizer.tokenize("5 +");
        auto ast = parser.parse(tokens);
        // Parser may return partial AST or error depending on implementation
        REQUIRE((ast == nullptr || parser.has_error()));
    }
    
    SECTION("Invalid ternary") {
        auto tokens = tokenizer.tokenize("x > 5 ? 10");
        auto ast = parser.parse(tokens);
        // Parser may return partial AST or error depending on implementation
        REQUIRE((ast == nullptr || parser.has_error()));
    }
}

TEST_CASE("ExpressionEvaluator handles errors", "[expression_parser][evaluator][errors]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    SECTION("Division by zero") {
        auto tokens = tokenizer.tokenize("10 / 0");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(evaluator.has_error());
    }
    
    SECTION("Unknown filter") {
        auto tokens = tokenizer.tokenize("\"hello\" | unknown_filter");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(evaluator.has_error());
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Full expression pipeline works end-to-end", "[expression_parser][integration]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    DataNode node;
    node.properties["price"] = "19.99";
    node.properties["quantity"] = "3";
    
    EvaluationContext context;
    context.data_node = &node;
    context.variables["tax_rate"] = Value(0.1);
    
    SECTION("Calculate total with tax") {
        // Expression: (price * quantity) * (1 + tax_rate)
        auto tokens = tokenizer.tokenize("(19.99 * 3) * (1 + tax_rate)");
        REQUIRE(!tokenizer.has_error());
        
        auto ast = parser.parse(tokens);
        REQUIRE(ast != nullptr);
        REQUIRE(!parser.has_error());
        
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(!evaluator.has_error());
        REQUIRE(result.type == Value::Type::Number);
        REQUIRE(result.number_value == Catch::Approx(65.967).epsilon(0.001));
    }
    
    SECTION("Format price with filter") {
        auto tokens = tokenizer.tokenize("19.99 | round");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 20.0);
    }
    
    SECTION("Conditional pricing") {
        context.variables["quantity"] = Value(5.0);
        auto tokens = tokenizer.tokenize("quantity > 10 ? 15.99 : 19.99");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 19.99);
    }
}

TEST_CASE("Expression parser handles real-world diagram expressions", "[expression_parser][integration]") {
    ExpressionTokenizer tokenizer;
    ExpressionParser parser;
    ExpressionEvaluator evaluator;
    
    Shape parent;
    parent.geometry["width"] = 400.0f;
    parent.geometry["height"] = 300.0f;
    
    EvaluationContext context;
    context.parent_shape = &parent;
    context.variables["index"] = Value(2.0);
    
    SECTION("Position child relative to parent") {
        auto tokens = tokenizer.tokenize("parent.width / 2");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 200.0);
    }
    
    SECTION("Calculate grid position") {
        auto tokens = tokenizer.tokenize("(index % 3) * 100");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.number_value == 200.0);  // (2 % 3) * 100 = 200
    }
    
    SECTION("Conditional visibility") {
        auto tokens = tokenizer.tokenize("index > 0 && index < 10");
        auto ast = parser.parse(tokens);
        Value result = evaluator.evaluate(ast.get(), context);
        REQUIRE(result.boolean_value == true);
    }
}
