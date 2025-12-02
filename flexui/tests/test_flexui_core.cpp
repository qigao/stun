#include <catch2/catch_test_macros.hpp>
#include <flexui/screen.h>
#include <flexui/widget.h>

TEST_CASE("Screen creation and basic properties", "[core]") {
    flexui::Screen screen(800, 600, "Test Window");
    
    REQUIRE(screen.getWidth() == 800);
    REQUIRE(screen.getHeight() == 600);
    REQUIRE(screen.vg() != nullptr);
    REQUIRE(screen.renderer() != nullptr);
}

TEST_CASE("Widget creation and management", "[widgets]") {
    flexui::Screen screen(800, 600, "Test");
    
    SECTION("Create widget with ID") {
        auto* widget = screen.addWidget("test-widget", "div");
        REQUIRE(widget != nullptr);
        REQUIRE(widget->id() == "test-widget");
    }
    
    SECTION("Find widget by ID") {
        screen.addWidget("findme", "div");
        auto* found = screen.findWidget("findme");
        REQUIRE(found != nullptr);
        REQUIRE(found->id() == "findme");
    }
    
    SECTION("Widget not found returns nullptr") {
        auto* notFound = screen.findWidget("nonexistent");
        REQUIRE(notFound == nullptr);
    }
}

TEST_CASE("CSS loading and parsing", "[css]") {
    flexui::Screen screen(800, 600, "Test");
    
    SECTION("Load valid CSS") {
        bool result = screen.loadCSS(R"(
            .test { 
                color: red; 
                font-size: 16px;
            }
        )");
        REQUIRE(result == true);
    }
    
    SECTION("CSS variables") {
        screen.setCSSVariable("--primary-color", "#3498db");
        std::string value = screen.getCSSVariable("--primary-color");
        REQUIRE(value == "#3498db");
    }
}

TEST_CASE("XML/HTML loading and parsing", "[xml]") {
    flexui::Screen screen(800, 600, "Test");
    
    SECTION("Load simple XML") {
        bool result = screen.loadXML("<div id='test'></div>");
        REQUIRE(result == true);
        
        auto* widget = screen.findWidget("test");
        REQUIRE(widget != nullptr);
    }
    
    SECTION("Load nested XML") {
        bool result = screen.loadXML(R"(
            <div id='parent'>
                <div id='child1'></div>
                <div id='child2'></div>
            </div>
        )");
        REQUIRE(result == true);
        
        auto* parent = screen.findWidget("parent");
        auto* child1 = screen.findWidget("child1");
        auto* child2 = screen.findWidget("child2");
        
        REQUIRE(parent != nullptr);
        REQUIRE(child1 != nullptr);
        REQUIRE(child2 != nullptr);
    }
    
    SECTION("XML with attributes") {
        bool result = screen.loadXML(R"(
            <button id='btn' class='primary'>Click Me</button>
        )");
        REQUIRE(result == true);
        
        auto* btn = screen.findWidget("btn");
        REQUIRE(btn != nullptr);
    }
}

TEST_CASE("SVG element creation", "[svg]") {
    flexui::Screen screen(800, 600, "Test");
    
    SECTION("Create circle") {
        auto* circle = screen.createCircle("c1", 100, 100, 50);
        REQUIRE(circle != nullptr);
        REQUIRE(circle->id() == "c1");
    }
    
    SECTION("Create line") {
        auto* line = screen.createLine("l1", 0, 0, 100, 100);
        REQUIRE(line != nullptr);
        REQUIRE(line->id() == "l1");
    }
    
    SECTION("Create rect") {
        auto* rect = screen.createRect("r1", 50, 50, 100, 80);
        REQUIRE(rect != nullptr);
        REQUIRE(rect->id() == "r1");
    }
    
    SECTION("Create ellipse") {
        auto* ellipse = screen.createEllipse("e1", 200, 200, 60, 40);
        REQUIRE(ellipse != nullptr);
        REQUIRE(ellipse->id() == "e1");
    }
    
    SECTION("Create path") {
        auto* path = screen.createPath("p1");
        REQUIRE(path != nullptr);
        path->addPathPoint(0, 0);
        path->addPathPoint(100, 100);
        path->addPathPoint(200, 50);
    }
}

TEST_CASE("JavaScript engine", "[javascript]") {
    flexui::Screen screen(800, 600, "Test");
    
    SECTION("Execute simple JavaScript") {
        bool result = screen.loadJS("var x = 42;");
        REQUIRE(result == true);
    }
    
    SECTION("Execute function definition") {
        bool result = screen.loadJS(R"(
            function add(a, b) {
                return a + b;
            }
        )");
        REQUIRE(result == true);
    }
    
    SECTION("JavaScript with console.log") {
        bool result = screen.loadJS(R"(
            console.log("Test message");
        )");
        REQUIRE(result == true);
    }
}

TEST_CASE("Event handling", "[events]") {
    flexui::Screen screen(800, 600, "Test");
    
    SECTION("Register event handler") {
        bool handlerCalled = false;
        
        screen.registerHandler("testHandler", [&](flexui::Widget* w) {
            handlerCalled = true;
            return true;
        });
        
        // Create widget with onclick
        screen.loadXML(R"(
            <button id='btn' onclick='testHandler'>Click</button>
        )");
        
        auto* btn = screen.findWidget("btn");
        REQUIRE(btn != nullptr);
        
        // Simulate click
        btn->handleClick(0, 0);
        REQUIRE(handlerCalled == true);
    }
}

TEST_CASE("Widget visibility", "[widgets]") {
    flexui::Screen screen(800, 600, "Test");
    
    auto* widget = screen.addWidget("test", "div");
    REQUIRE(widget != nullptr);
    
    SECTION("Widget visible by default") {
        REQUIRE(widget->isVisible() == true);
    }
    
    SECTION("Hide widget with inline style") {
        widget->setInlineStyle("display", "none");
        REQUIRE(widget->isVisible() == false);
    }
}
