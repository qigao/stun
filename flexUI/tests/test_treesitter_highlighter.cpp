/*
 * Unit tests for tree-sitter highlighter integration
 */

#include <tinytest.h>
#undef group
#include "test_support.h"
#include <flexUI/syntax_highlighter.h>

using namespace flexUI;

spec("Tree-sitter highlighter loads C and JSON language plugins") {
    describe("the HighlighterFactory") {
        auto& factory = HighlighterFactory::instance();

        describe("requesting highlighter for 'c'") {
            auto* highlighter = factory.get("c");

            it("it should return a valid highlighter") {
                check(highlighter != nullptr);
                check(std::string(highlighter->language()) == "c");
            }
        }

        describe("requesting highlighter for 'json'") {
            auto* highlighter = factory.get("json");

            it("it should return a valid highlighter") {
                check(highlighter != nullptr);
                check(std::string(highlighter->language()) == "json");
            }
        }
    }
}

spec("Tree-sitter C highlighter tokenizes code correctly") {
    describe("a C highlighter") {
        auto* highlighter = HighlighterFactory::instance().get("c");
        check(highlighter != nullptr);

        describe("highlighting a simple C program") {
            const char* code = R"(#include <stdio.h>

int main(void) {
    printf("hello\n");
    return 0;
})";
            auto tokens = highlighter->highlight(code);

            it("it should produce tokens") {
                check(!tokens.empty());
            }

            it("it should identify preprocessor directive") {
                bool found_preproc = false;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Preprocessor) {
                        found_preproc = true;
                        break;
                    }
                }
                check(found_preproc);
            }

            it("it should identify keywords") {
                bool found_return = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Keyword) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text == "return") found_return = true;
                    }
                }
                check(found_return);
            }

            it("it should identify functions") {
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
                check(found_printf);
                check(found_main);
            }

            it("it should identify types") {
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
                check(found_int);
                check(found_void);
            }

            it("it should identify numbers") {
                bool found_zero = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Number) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text == "0") found_zero = true;
                    }
                }
                check(found_zero);
            }
        }

        describe("highlighting string literals") {
            const char* code = R"(const char* s = "hello";)";
            auto tokens = highlighter->highlight(code);

            it("it should identify string literal") {
                bool found_string = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::String) {
                        std::string text(src.substr(tok.start, tok.length));
                        if (text.find("hello") != std::string::npos) found_string = true;
                    }
                }
                check(found_string);
            }
        }

        describe("highlighting comments") {
            const char* code = "int x; // comment\nint y; /* block */";
            auto tokens = highlighter->highlight(code);

            it("it should identify comments") {
                int comment_count = 0;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::Comment) comment_count++;
                }
                check(comment_count >= 2);
            }
        }
    }
}

spec("Tree-sitter JSON highlighter tokenizes code correctly") {
    describe("a JSON highlighter") {
        auto* highlighter = HighlighterFactory::instance().get("json");
        check(highlighter != nullptr);

        describe("highlighting a JSON object") {
            const char* code = "{\"name\": \"flexUI\", \"version\": 1, \"active\": true}";
            auto tokens = highlighter->highlight(code);

            it("it should produce tokens") {
                check(!tokens.empty());
            }

            it("it should identify keys as variables") {
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
                check(found_name);
                check(found_version);
            }

            it("it should identify values") {
                bool found_string = false;
                bool found_number = false;
                bool found_constant = false;
                std::string_view src = code;
                for (const auto& tok : tokens) {
                    if (tok.type == HighlightType::String) found_string = true;
                    if (tok.type == HighlightType::Number) found_number = true;
                    if (tok.type == HighlightType::Constant) found_constant = true;
                }
                check(found_string);
                check(found_number);
                check(found_constant);
            }
        }
    }
}

spec("Tree-sitter highlighter handles edge cases") {
    describe("a C highlighter") {
        auto* highlighter = HighlighterFactory::instance().get("c");
        check(highlighter != nullptr);

        describe("highlighting empty string") {
            auto tokens = highlighter->highlight("");

            it("it should return empty tokens") {
                check(tokens.empty());
            }
        }

        describe("highlighting whitespace only") {
            auto tokens = highlighter->highlight("   \n\t  ");

            it("it should return plain tokens") {
                check(!tokens.empty());
                for (const auto& tok : tokens) {
                    check(tok.type == HighlightType::Plain);
                }
            }
        }
    }
}

spec("Unsupported language returns nullptr") {
    describe("the HighlighterFactory") {
        auto& factory = HighlighterFactory::instance();

        describe("requesting highlighter for unknown language") {
            auto* highlighter = factory.get("unknown_language_xyz");

            it("it should return nullptr") {
                check(highlighter == nullptr);
            }
        }
    }
}
