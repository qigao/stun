/*
 * Unit tests for tree-sitter highlighter integration
 */

#include <catch2/catch_all.hpp>
#include <flexUI/syntax_highlighter.h>

using namespace flexUI;

SCENARIO("Tree-sitter highlighter loads C and JSON language plugins", "[highlighter][plugin]") {
    GIVEN("the HighlighterFactory") {
        auto& factory = HighlighterFactory::instance();

        WHEN("requesting highlighter for 'c'") {
            auto* highlighter = factory.get("c");

            THEN("it should return a valid highlighter") {
                REQUIRE(highlighter != nullptr);
                REQUIRE(std::string(highlighter->language()) == "c");
            }
        }

        WHEN("requesting highlighter for 'json'") {
            auto* highlighter = factory.get("json");

            THEN("it should return a valid highlighter") {
                REQUIRE(highlighter != nullptr);
                REQUIRE(std::string(highlighter->language()) == "json");
            }
        }
    }
}

SCENARIO("Tree-sitter C highlighter tokenizes code correctly", "[highlighter][c]") {
    GIVEN("a C highlighter") {
        auto* highlighter = HighlighterFactory::instance().get("c");
        REQUIRE(highlighter != nullptr);

        WHEN("highlighting a simple C program") {
            const char* code = R"(#include <stdio.h>

int main(void) {
    printf("hello\n");
    return 0;
})";
            auto tokens = highlighter->highlight(code);

            THEN("it should produce tokens") {
                REQUIRE(!tokens.empty());
            }

            THEN("it should identify preprocessor directive") {
                bool found_preproc = false;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Preprocessor) {
                        found_preproc = true;
                        break;
                    }
                }
                REQUIRE(found_preproc);
            }

            THEN("it should identify keywords") {
                bool found_return = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Keyword) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text == "return") found_return = true;
                    }
                }
                REQUIRE(found_return);
            }

            THEN("it should identify functions") {
                bool found_printf = false;
                bool found_main = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Function) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text == "printf") found_printf = true;
                        if (text == "main") found_main = true;
                    }
                }
                REQUIRE(found_printf);
                REQUIRE(found_main);
            }

            THEN("it should identify types") {
                bool found_int = false;
                bool found_void = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Type) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text == "int") found_int = true;
                        if (text == "void") found_void = true;
                    }
                }
                REQUIRE(found_int);
                REQUIRE(found_void);
            }

            THEN("it should identify numbers") {
                bool found_zero = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Number) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text == "0") found_zero = true;
                    }
                }
                REQUIRE(found_zero);
            }
        }

        WHEN("highlighting string literals") {
            const char* code = R"(const char* s = "hello";)";
            auto tokens = highlighter->highlight(code);

            THEN("it should identify string literal") {
                bool found_string = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::String) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text.find("hello") != std::string::npos) found_string = true;
                    }
                }
                REQUIRE(found_string);
            }
        }

        WHEN("highlighting comments") {
            const char* code = "int x; // comment\nint y; /* block */";
            auto tokens = highlighter->highlight(code);

            THEN("it should identify comments") {
                int comment_count = 0;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Comment) comment_count++;
                }
                REQUIRE(comment_count >= 2);
            }
        }
    }
}

SCENARIO("Tree-sitter JSON highlighter tokenizes code correctly", "[highlighter][json]") {
    GIVEN("a JSON highlighter") {
        auto* highlighter = HighlighterFactory::instance().get("json");
        REQUIRE(highlighter != nullptr);

        WHEN("highlighting a JSON object") {
            const char* code = "{\"name\": \"flexUI\", \"version\": 1, \"active\": true}";
            auto tokens = highlighter->highlight(code);

            THEN("it should produce tokens") {
                REQUIRE(!tokens.empty());
            }

            THEN("it should identify keys as variables") {
                bool found_name = false;
                bool found_version = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Variable) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text == "\"name\"") found_name = true;
                        if (text == "\"version\"") found_version = true;
                    }
                }
                REQUIRE(found_name);
                REQUIRE(found_version);
            }

            THEN("it should identify values") {
                bool found_string = false;
                bool found_number = false;
                bool found_constant = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::String) found_string = true;
                    if (tok.type == HighlightType::Number) found_number = true;
                    if (tok.type == HighlightType::Constant) found_constant = true;
                }
                REQUIRE(found_string);
                REQUIRE(found_number);
                REQUIRE(found_constant);
            }
        }
    }
}

SCENARIO("Tree-sitter highlighter handles edge cases", "[highlighter][edge]") {
    GIVEN("a C highlighter") {
        auto* highlighter = HighlighterFactory::instance().get("c");
        REQUIRE(highlighter != nullptr);

        WHEN("highlighting empty string") {
            auto tokens = highlighter->highlight("");

            THEN("it should return empty tokens") {
                REQUIRE(tokens.empty());
            }
        }

        WHEN("highlighting whitespace only") {
            auto tokens = highlighter->highlight("   \n\t  ");

            THEN("it should return plain tokens") {
                REQUIRE(!tokens.empty());
                for (const auto& tok : tokens) {
                    REQUIRE(tok.type == HighlightType::Plain);
                }
            }
        }
    }
}

SCENARIO("Unsupported language returns nullptr", "[highlighter][unsupported]") {
    GIVEN("the HighlighterFactory") {
        auto& factory = HighlighterFactory::instance();

        WHEN("requesting highlighter for unknown language") {
            auto* highlighter = factory.get("unknown_language_xyz");

            THEN("it should return nullptr") {
                REQUIRE(highlighter == nullptr);
            }
        }
    }
}
