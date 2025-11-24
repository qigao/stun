#include "whiteboard/lexbor/lexbor_css_parser.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

using namespace whiteboard::lexbor;

// Test basic calc() expressions
TEST_CASE("Basic arithmetic operations", "[calc]") {
  LexborCSSParser parser;

  // Addition
  auto result = parser.evaluate_calc("calc(10px + 20px)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(30.0f, 0.001f));
  REQUIRE(result->unit == "px");

  // Subtraction
  result = parser.evaluate_calc("calc(100px - 25px)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(75.0f, 0.001f));
  REQUIRE(result->unit == "px");

  // Multiplication
  result = parser.evaluate_calc("calc(10px * 2)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(20.0f, 0.001f));
  REQUIRE(result->unit == "px");

  // Division
  result = parser.evaluate_calc("calc(100px / 4)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(25.0f, 0.001f));
  REQUIRE(result->unit == "px");
}

// Test calc() with percentages
TEST_CASE("Percentage calculations", "[calc]") {
  LexborCSSParser parser;

  // Percentage with context (e.g., parent width = 200px)
  auto result = parser.evaluate_calc("calc(50% - 10px)", 200.0f);
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(90.0f, 0.001f)); // 100px - 10px
  REQUIRE(result->unit == "px");

  // Percentage addition
  result = parser.evaluate_calc("calc(100% + 20px)", 150.0f);
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(170.0f, 0.001f)); // 150px + 20px
  REQUIRE(result->unit == "px");
}

// Test calc() with nested expressions
TEST_CASE("Nested expressions", "[calc]") {
  LexborCSSParser parser;

  // Nested parentheses
  auto result = parser.evaluate_calc("calc((10px + 20px) * 2)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(60.0f, 0.001f));
  REQUIRE(result->unit == "px");

  // Complex expression
  result = parser.evaluate_calc("calc(100px - (20px + 10px))");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(70.0f, 0.001f));
  REQUIRE(result->unit == "px");
}

// Test calc() operator precedence
TEST_CASE("Operator precedence", "[calc]") {
  LexborCSSParser parser;

  // Multiplication before addition
  auto result = parser.evaluate_calc("calc(10px + 5px * 2)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(20.0f, 0.001f)); // 10 + (5 * 2)
  REQUIRE(result->unit == "px");

  // Division before subtraction
  result = parser.evaluate_calc("calc(100px - 20px / 2)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(90.0f, 0.001f)); // 100 - (20 / 2)
  REQUIRE(result->unit == "px");
}

// Test calc() with whitespace
TEST_CASE("Whitespace handling", "[calc]") {
  LexborCSSParser parser;

  // Various whitespace patterns
  auto result = parser.evaluate_calc("calc(  10px  +  20px  )");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(30.0f, 0.001f));

  result = parser.evaluate_calc("calc(10px+20px)");
  REQUIRE(result.has_value());
  REQUIRE_THAT(result->value, Catch::Matchers::WithinRel(30.0f, 0.001f));
}

// Test invalid calc() expressions
TEST_CASE("Invalid expressions", "[calc]") {
  LexborCSSParser parser;

  // Mismatched units in addition
  auto result = parser.evaluate_calc("calc(10px + 20%)");
  REQUIRE_FALSE(result.has_value()); // Can't add px and % without context

  // Division by zero
  result = parser.evaluate_calc("calc(10px / 0)");
  REQUIRE_FALSE(result.has_value());

  // Invalid syntax
  result = parser.evaluate_calc("calc(10px +)");
  REQUIRE_FALSE(result.has_value());

  // Unmatched parentheses
  result = parser.evaluate_calc("calc((10px + 20px)");
  REQUIRE_FALSE(result.has_value());
}
