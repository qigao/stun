/**
 * @file test_lexbor_integration.cpp
 * @brief Test Lexbor integration for CSS and HTML parsing
 */

#include <catch2/catch_test_macros.hpp>
#include <lexbor/css/css.h>
#include <lexbor/html/html.h>
#include <string>
#include <iostream>

TEST_CASE("Lexbor CSS Parser - Basic", "[lexbor][css]") {
    SECTION("Parse simple CSS rule") {
        const char* css = ".card { fill: red; stroke: blue; }";
        
        // Create parser
        lxb_css_parser_t* parser = lxb_css_parser_create();
        REQUIRE(parser != nullptr);
        
        lxb_status_t status = lxb_css_parser_init(parser, nullptr);
        REQUIRE(status == LXB_STATUS_OK);
        
        // Parse CSS
        lxb_css_stylesheet_t* sheet = lxb_css_stylesheet_parse(
            parser, 
            reinterpret_cast<const lxb_char_t*>(css), 
            strlen(css)
        );
        
        REQUIRE(sheet != nullptr);
        
        // Verify stylesheet was created (just check it's not null)
        // Note: Lexbor 2.5.0 API doesn't expose rules directly
        
        // Cleanup
        lxb_css_stylesheet_destroy(sheet, true);
        lxb_css_parser_destroy(parser, true);
    }
    
    SECTION("Parse multiple CSS rules") {
        const char* css = R"(
            .card { fill: white; }
            #shape1 { stroke: blue; }
            rect { opacity: 0.8; }
        )";
        
        lxb_css_parser_t* parser = lxb_css_parser_create();
        REQUIRE(parser != nullptr);
        
        lxb_status_t status = lxb_css_parser_init(parser, nullptr);
        REQUIRE(status == LXB_STATUS_OK);
        
        lxb_css_stylesheet_t* sheet = lxb_css_stylesheet_parse(
            parser,
            reinterpret_cast<const lxb_char_t*>(css),
            strlen(css)
        );
        
        REQUIRE(sheet != nullptr);
        
        // Verify stylesheet was created (just check it's not null)
        // Note: Lexbor 2.5.0 API doesn't expose rules directly
        
        // Cleanup
        lxb_css_stylesheet_destroy(sheet, true);
        lxb_css_parser_destroy(parser, true);
    }
    
    SECTION("Parse CSS with advanced selectors") {
        const char* css = R"(
            .card > .title { fill: blue; }
            rect[data-status="active"] { fill: green; }
            .card:nth-child(2n) { opacity: 0.5; }
            .card:hover:not(.disabled) { fill: lightblue; }
        )";
        
        lxb_css_parser_t* parser = lxb_css_parser_create();
        REQUIRE(parser != nullptr);
        
        lxb_status_t status = lxb_css_parser_init(parser, nullptr);
        REQUIRE(status == LXB_STATUS_OK);
        
        lxb_css_stylesheet_t* sheet = lxb_css_stylesheet_parse(
            parser,
            reinterpret_cast<const lxb_char_t*>(css),
            strlen(css)
        );
        
        REQUIRE(sheet != nullptr);
        
        // Cleanup
        lxb_css_stylesheet_destroy(sheet, true);
        lxb_css_parser_destroy(parser, true);
    }
}

TEST_CASE("Lexbor HTML Parser - Basic", "[lexbor][html]") {
    SECTION("Parse simple HTML") {
        const char* html = "<b>Bold</b> <i>italic</i>";
        
        // Create document
        lxb_html_document_t* document = lxb_html_document_create();
        REQUIRE(document != nullptr);
        
        lxb_status_t status = lxb_html_document_parse(
            document,
            reinterpret_cast<const lxb_char_t*>(html),
            strlen(html)
        );
        
        REQUIRE(status == LXB_STATUS_OK);
        
        // Get body element (cast to dom element)
        lxb_html_body_element_t* body_html = lxb_html_document_body_element(document);
        REQUIRE(body_html != nullptr);
        
        // Cast: html_body_element -> html_element -> dom_element
        lxb_html_element_t* html_elem = lxb_html_interface_element(body_html);
        lxb_dom_element_t* body = lxb_dom_interface_element(html_elem);
        REQUIRE(body != nullptr);
        
        // Cleanup
        lxb_html_document_destroy(document);
    }
    
    SECTION("Parse HTML with links") {
        const char* html = R"(<a href="https://example.com">Link</a>)";
        
        lxb_html_document_t* document = lxb_html_document_create();
        REQUIRE(document != nullptr);
        
        lxb_status_t status = lxb_html_document_parse(
            document,
            reinterpret_cast<const lxb_char_t*>(html),
            strlen(html)
        );
        
        REQUIRE(status == LXB_STATUS_OK);
        
        // Cleanup
        lxb_html_document_destroy(document);
    }
    
    SECTION("Parse HTML with lists") {
        const char* html = R"(
            <ul>
                <li>Item 1</li>
                <li>Item 2</li>
                <li>Item 3</li>
            </ul>
        )";
        
        lxb_html_document_t* document = lxb_html_document_create();
        REQUIRE(document != nullptr);
        
        lxb_status_t status = lxb_html_document_parse(
            document,
            reinterpret_cast<const lxb_char_t*>(html),
            strlen(html)
        );
        
        REQUIRE(status == LXB_STATUS_OK);
        
        // Cleanup
        lxb_html_document_destroy(document);
    }
}

TEST_CASE("Lexbor Performance", "[lexbor][performance]") {
    SECTION("CSS parsing performance") {
        // Generate large CSS
        std::string css;
        for (int i = 0; i < 1000; i++) {
            css += ".class" + std::to_string(i) + " { fill: red; }\n";
        }
        
        lxb_css_parser_t* parser = lxb_css_parser_create();
        REQUIRE(parser != nullptr);
        
        lxb_status_t status = lxb_css_parser_init(parser, nullptr);
        REQUIRE(status == LXB_STATUS_OK);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        lxb_css_stylesheet_t* sheet = lxb_css_stylesheet_parse(
            parser,
            reinterpret_cast<const lxb_char_t*>(css.c_str()),
            css.size()
        );
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        REQUIRE(sheet != nullptr);
        
        // Should parse 1000 rules in < 10ms
        INFO("Parsed 1000 CSS rules in " << duration.count() << "ms");
        REQUIRE(duration.count() < 10);
        
        // Cleanup
        lxb_css_stylesheet_destroy(sheet, true);
        lxb_css_parser_destroy(parser, true);
    }
}

TEST_CASE("Lexbor Error Handling", "[lexbor][errors]") {
    SECTION("Handle invalid CSS") {
        const char* invalid_css = ".card { fill: ; }";  // Missing value
        
        lxb_css_parser_t* parser = lxb_css_parser_create();
        REQUIRE(parser != nullptr);
        
        lxb_status_t status = lxb_css_parser_init(parser, nullptr);
        REQUIRE(status == LXB_STATUS_OK);
        
        lxb_css_stylesheet_t* sheet = lxb_css_stylesheet_parse(
            parser,
            reinterpret_cast<const lxb_char_t*>(invalid_css),
            strlen(invalid_css)
        );
        
        // Lexbor should still create a stylesheet (it's lenient)
        REQUIRE(sheet != nullptr);
        
        // Cleanup
        lxb_css_stylesheet_destroy(sheet, true);
        lxb_css_parser_destroy(parser, true);
    }
    
    SECTION("Handle malformed HTML") {
        const char* malformed_html = "<b>Unclosed bold";
        
        lxb_html_document_t* document = lxb_html_document_create();
        REQUIRE(document != nullptr);
        
        // Lexbor should auto-close tags
        lxb_status_t status = lxb_html_document_parse(
            document,
            reinterpret_cast<const lxb_char_t*>(malformed_html),
            strlen(malformed_html)
        );
        
        REQUIRE(status == LXB_STATUS_OK);
        
        // Cleanup
        lxb_html_document_destroy(document);
    }
}
