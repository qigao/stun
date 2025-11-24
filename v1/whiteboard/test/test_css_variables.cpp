#include <catch2/catch_test_macros.hpp>
#include "whiteboard/lexbor/lexbor_css_parser.h"

using namespace whiteboard::lexbor;

// Test basic CSS variable definition and resolution
TEST_CASE("Basic CSS variable definition and resolution", "[css-variables]") {
    CSSVariableResolver resolver;
    
    // Define variables
    resolver.set_variable("--primary-color", "blue");
    resolver.set_variable("--spacing", "10px");
    resolver.set_variable("--font-size", "14px");
    
    // Resolve variables
    auto result = resolver.resolve("var(--primary-color)");
    REQUIRE(result == "blue");
    
    result = resolver.resolve("var(--spacing)");
    REQUIRE(result == "10px");
    
    result = resolver.resolve("var(--font-size)");
    REQUIRE(result == "14px");
}

// Test CSS variable with fallback values
TEST_CASE("CSS variable with fallback values", "[css-variables]") {
    CSSVariableResolver resolver;
    
    resolver.set_variable("--primary-color", "blue");
    
    // Variable exists - use it
    auto result = resolver.resolve("var(--primary-color, red)");
    REQUIRE(result == "blue");
    
    // Variable doesn't exist - use fallback
    result = resolver.resolve("var(--undefined-color, red)");
    REQUIRE(result == "red");
    
    // Multiple fallbacks (nested)
    result = resolver.resolve("var(--undefined, var(--primary-color, green))");
    REQUIRE(result == "blue");
    
    // All undefined - use final fallback
    result = resolver.resolve("var(--undefined1, var(--undefined2, yellow))");
    REQUIRE(result == "yellow");
}

// Test CSS variable in property values
TEST_CASE("CSS variable in property values", "[css-variables]") {
    CSSVariableResolver resolver;
    
    resolver.set_variable("--border-width", "2px");
    resolver.set_variable("--border-style", "solid");
    resolver.set_variable("--border-color", "red");
    
    // Single variable
    auto result = resolver.resolve("var(--border-width)");
    REQUIRE(result == "2px");
    
    // Multiple variables in one value
    result = resolver.resolve("var(--border-width) var(--border-style) var(--border-color)");
    REQUIRE(result == "2px solid red");
    
    // Variable mixed with literal values
    result = resolver.resolve("var(--border-width) solid blue");
    REQUIRE(result == "2px solid blue");
}

// Test CSS variable with calc()
TEST_CASE("CSS variable with calc()", "[css-variables]") {
    CSSVariableResolver resolver;
    
    resolver.set_variable("--base-size", "10px");
    resolver.set_variable("--multiplier", "2");
    
    // Variable in calc()
    auto result = resolver.resolve("calc(var(--base-size) * var(--multiplier))");
    REQUIRE(result == "calc(10px * 2)");
    
    // After resolution, calc should be evaluable
    LexborCSSParser parser;
    auto calc_result = parser.evaluate_calc(result);
    REQUIRE(calc_result.has_value());
    REQUIRE(calc_result->value == 20.0f);
    REQUIRE(calc_result->unit == "px");
}

// Test CSS variable scoping (root vs local)
TEST_CASE("CSS variable scoping", "[css-variables]") {
    CSSVariableResolver resolver;
    
    // Root level variables
    resolver.set_variable("--primary-color", "blue");
    resolver.set_variable("--secondary-color", "green");
    
    // Create a scoped resolver
    auto scoped = resolver.create_scope();
    scoped.set_variable("--primary-color", "red");  // Override
    
    // Root resolver still has original value
    REQUIRE(resolver.resolve("var(--primary-color)") == "blue");
    
    // Scoped resolver has overridden value
    REQUIRE(scoped.resolve("var(--primary-color)") == "red");
    
    // Scoped resolver inherits non-overridden values
    REQUIRE(scoped.resolve("var(--secondary-color)") == "green");
}

// Test CSS variable with color functions
TEST_CASE("CSS variable with color functions", "[css-variables]") {
    CSSVariableResolver resolver;
    
    resolver.set_variable("--red", "255");
    resolver.set_variable("--green", "128");
    resolver.set_variable("--blue", "0");
    resolver.set_variable("--alpha", "0.8");
    
    // Variable in rgb()
    auto result = resolver.resolve("rgb(var(--red), var(--green), var(--blue))");
    REQUIRE(result == "rgb(255, 128, 0)");
    
    // Variable in rgba()
    result = resolver.resolve("rgba(var(--red), var(--green), var(--blue), var(--alpha))");
    REQUIRE(result == "rgba(255, 128, 0, 0.8)");
}

// Test invalid CSS variable syntax
TEST_CASE("Invalid CSS variable syntax", "[css-variables]") {
    CSSVariableResolver resolver;
    
    resolver.set_variable("--color", "blue");
    
    // Missing closing parenthesis
    auto result = resolver.resolve("var(--color");
    REQUIRE(result == "var(--color");  // Return as-is if invalid
    
    // Empty variable name
    result = resolver.resolve("var()");
    REQUIRE(result == "var()");
    
    // Invalid variable name (doesn't start with --)
    result = resolver.resolve("var(color)");
    REQUIRE(result == "var(color)");
}

// Test CSS variable updates
TEST_CASE("CSS variable updates", "[css-variables]") {
    CSSVariableResolver resolver;
    
    resolver.set_variable("--color", "blue");
    REQUIRE(resolver.resolve("var(--color)") == "blue");
    
    // Update variable
    resolver.set_variable("--color", "red");
    REQUIRE(resolver.resolve("var(--color)") == "red");
    
    // Remove variable
    resolver.remove_variable("--color");
    auto result = resolver.resolve("var(--color, green)");
    REQUIRE(result == "green");  // Falls back to default
}

// Test CSS variable in stylesheet
TEST_CASE("CSS variable in stylesheet", "[css-variables]") {
    EnhancedStyleSheet stylesheet;
    
    // Parse CSS with variables
    std::string css = R"(
        :root {
            --primary-color: blue;
            --spacing: 10px;
        }
        
        .button {
            background: var(--primary-color);
            padding: var(--spacing);
        }
    )";
    
    REQUIRE(stylesheet.parse_css(css));
    
    // Get variable resolver
    auto& resolver = stylesheet.get_variable_resolver();
    
    // Check variables were extracted
    REQUIRE(resolver.resolve("var(--primary-color)") == "blue");
    REQUIRE(resolver.resolve("var(--spacing)") == "10px");
}
