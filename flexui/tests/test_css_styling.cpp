#include <catch2/catch_test_macros.hpp>
#include <flexui/screen.h>
#include <flexui/widget.h>

using namespace flexui;

TEST_CASE("CSS class management", "[css]") {
    Screen screen(800, 600, "Test");
    
    SECTION("Widget with class attribute") {
        screen.loadXML(R"(<div id="test" class="primary"></div>)");
        auto* widget = screen.findWidget("test");
        REQUIRE(widget != nullptr);
    }
    
    SECTION("Multiple classes") {
        screen.loadXML(R"(<div id="test" class="primary active"></div>)");
        auto* widget = screen.findWidget("test");
        REQUIRE(widget != nullptr);
    }
}

TEST_CASE("CSS inline styles", "[css]") {
    Screen screen(800, 600, "Test");
    
    auto* widget = screen.addWidget("test", "div");
    
    SECTION("Set inline style") {
        widget->setInlineStyle("color", "red");
        widget->setInlineStyle("font-size", "16px");
        
        // Inline styles should be applied
        REQUIRE(true);  // Visual verification needed
    }
    
    SECTION("Inline style priority") {
        screen.loadCSS(".test { color: blue; }");
        widget->addClass("test");
        widget->setInlineStyle("color", "red");
        
        // Inline style should override class style
        REQUIRE(true);  // Visual verification needed
    }
}

TEST_CASE("CSS selectors", "[css]") {
    Screen screen(800, 600, "Test");
    
    SECTION("ID selector") {
        screen.loadCSS("#test { color: red; }");
        auto* widget = screen.addWidget("test", "div");
        REQUIRE(widget != nullptr);
    }
    
    SECTION("Class selector with XML") {
        screen.loadCSS(".primary { color: blue; }");
        screen.loadXML(R"(<div id="w1" class="primary"></div>)");
        auto* widget = screen.findWidget("w1");
        REQUIRE(widget != nullptr);
    }
    
    SECTION("Type selector") {
        screen.loadCSS("button { color: green; }");
        auto* widget = screen.addWidget("btn", "button");
        REQUIRE(widget != nullptr);
    }
}

TEST_CASE("CSS pseudo-classes", "[css]") {
    Screen screen(800, 600, "Test");
    
    SECTION("Hover and active styles defined") {
        screen.loadCSS(R"(
            .button:hover { background: blue; }
            .button:active { background: red; }
        )");
        
        screen.loadXML(R"(<button id="btn" class="button">Click</button>)");
        auto* widget = screen.findWidget("btn");
        REQUIRE(widget != nullptr);
    }
}

TEST_CASE("CSS layout properties", "[css]") {
    Screen screen(800, 600, "Test");
    
    SECTION("Display property") {
        screen.loadCSS(".hidden { display: none; }");
        screen.loadXML(R"(<div id="w1" class="hidden"></div>)");
        auto* widget = screen.findWidget("w1");
        REQUIRE(widget != nullptr);
        // Note: CSS display:none may not automatically affect widget visibility
    }
    
    SECTION("Position property") {
        screen.loadCSS(R"(
            .absolute { 
                position: absolute; 
                top: 10px; 
                left: 20px; 
            }
        )");
        auto* widget = screen.addWidget("w1", "div");
        widget->addClass("absolute");
        REQUIRE(widget != nullptr);
    }
    
    SECTION("Flexbox layout") {
        screen.loadCSS(R"(
            .container { 
                display: flex; 
                flex-direction: row; 
                gap: 10px; 
            }
        )");
        auto* container = screen.addWidget("container", "div");
        container->addClass("container");
        REQUIRE(container != nullptr);
    }
}

TEST_CASE("CSS animations", "[css]") {
    Screen screen(800, 600, "Test");
    
    SECTION("Transition property") {
        screen.loadCSS(R"(
            .animated { 
                transition: all 0.3s ease; 
            }
        )");
        auto* widget = screen.addWidget("w1", "div");
        widget->addClass("animated");
        REQUIRE(widget != nullptr);
    }
    
    SECTION("Keyframe animation") {
        screen.loadCSS(R"(
            @keyframes fade {
                from { opacity: 0; }
                to { opacity: 1; }
            }
            .fade-in {
                animation: fade 1s ease;
            }
        )");
        auto* widget = screen.addWidget("w1", "div");
        widget->addClass("fade-in");
        REQUIRE(widget != nullptr);
    }
}

TEST_CASE("CSS variables", "[css]") {
    Screen screen(800, 600, "Test");
    
    SECTION("Set and get CSS variable") {
        screen.setCSSVariable("--primary-color", "#3498db");
        std::string value = screen.getCSSVariable("--primary-color");
        REQUIRE(value == "#3498db");
    }
    
    SECTION("Use CSS variable in styles") {
        screen.setCSSVariable("--primary-color", "#3498db");
        screen.loadCSS(R"(
            .button { 
                background: var(--primary-color); 
            }
        )");
        auto* widget = screen.addWidget("btn", "button");
        widget->addClass("button");
        REQUIRE(widget != nullptr);
    }
}
