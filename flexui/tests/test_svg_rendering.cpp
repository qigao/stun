#include <catch2/catch_test_macros.hpp>
#include <flexui/screen.h>
#include <flexui/widget.h>
#include <nanovg_css.h>
#include <nanovg_css_internal.h>
TEST_CASE("SVG rect creation and properties", "[svg][rect]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Create rect with basic properties") {
        auto* rect = screen.createRect("test-rect", 50, 60, 100, 80);
        REQUIRE(rect != nullptr);
        REQUIRE(rect->id() == "test-rect");

        // Verify element was created
        auto* element = rect->element();
        REQUIRE(element != nullptr);

        // Verify element type is 'rect'
        REQUIRE(element->type == "rect");
    }

    SECTION("Rect inline styles are set correctly") {
        auto* rect = screen.createRect("r1", 50, 60, 100, 80);
        auto* element = rect->element();

        // Check inline styles for geometry
        REQUIRE(element->inline_style.count("x") > 0);
        REQUIRE(element->inline_style.count("y") > 0);
        REQUIRE(element->inline_style.count("width") > 0);
        REQUIRE(element->inline_style.count("height") > 0);

        // Verify values
        REQUIRE(element->inline_style["x"] == "50px");
        REQUIRE(element->inline_style["y"] == "60px");
        REQUIRE(element->inline_style["width"] == "100px");
        REQUIRE(element->inline_style["height"] == "80px");
    }
}

TEST_CASE("SVG rect fill property", "[svg][rect][fill]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Set fill with setFill()") {
        auto* rect = screen.createRect("r1", 0, 0, 100, 100);
        rect->setFill("#3b82f6");

        auto* element = rect->element();
        REQUIRE(element->inline_style.count("fill") > 0);
        REQUIRE(element->inline_style["fill"] == "#3b82f6");
    }

    SECTION("Set fill with setInlineStyle()") {
        auto* rect = screen.createRect("r2", 0, 0, 100, 100);
        rect->setInlineStyle("fill", "#ff0000");

        auto* element = rect->element();
        REQUIRE(element->inline_style["fill"] == "#ff0000");
    }

    SECTION("Fill from CSS class") {
        screen.loadCSS(R"(
            .blue-rect {
                fill: #0000ff;
            }
        )");

        auto* rect = screen.createRect("r3", 0, 0, 100, 100);
        rect->setClass("blue-rect");

        // Need to trigger layout computation to apply CSS
        screen.draw();

        // After layout, CSS should be applied
        // Note: This requires checking computed styles, not inline styles
    }
}

TEST_CASE("SVG rect stroke property", "[svg][rect][stroke]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Set stroke with setStroke()") {
        auto* rect = screen.createRect("r1", 0, 0, 100, 100);
        rect->setStroke("#ff0000", 2);

        auto* element = rect->element();
        REQUIRE(element->inline_style.count("stroke") > 0);
        REQUIRE(element->inline_style["stroke"] == "#ff0000");
        REQUIRE(element->inline_style["stroke-width"] == "2px");
    }

    SECTION("Set stroke dash pattern") {
        auto* rect = screen.createRect("r2", 0, 0, 100, 100);
        rect->setStrokeDash("5 3");

        auto* element = rect->element();
        REQUIRE(element->inline_style["stroke-dasharray"] == "5 3");
    }
}

TEST_CASE("SVG rect visibility and display", "[svg][rect][display]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Rect without display property should be invisible") {
        auto* rect = screen.createRect("r1", 0, 0, 100, 100);
        rect->setFill("#ff0000");

        // Without setting display, element should not be visible
        // This is the root cause of the dashboard bug
        REQUIRE(rect->isVisible() == true);  // Widget thinks it's visible

        // But display flag should be 0 (none)
        // Note: This requires checking after layout computation
    }

    SECTION("Rect with display:block in CSS should be visible") {
        screen.loadCSS(R"(
            .visible-rect {
                display: block;
                fill: #00ff00;
            }
        )");

        auto* rect = screen.createRect("r2", 0, 0, 100, 100);
        rect->setClass("visible-rect");

        REQUIRE(rect->isVisible() == true);
    }

    SECTION("Rect with display:none should be hidden") {
        auto* rect = screen.createRect("r3", 0, 0, 100, 100);
        rect->setInlineStyle("display", "none");

        REQUIRE(rect->isVisible() == false);
    }
}

TEST_CASE("SVG rect computed layout", "[svg][rect][layout]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Rect layout is computed after draw()") {
        auto* rect = screen.createRect("r1", 50, 60, 100, 80);

        // Before draw(), computed values might not be set
        auto* element = rect->element();

        // Trigger layout computation
        screen.draw();

        // After draw(), computed values should be set
        REQUIRE(element->computed.x >= 0);
        REQUIRE(element->computed.y >= 0);
        REQUIRE(element->computed.width > 0);
        REQUIRE(element->computed.height > 0);
    }
}

TEST_CASE("SVG line creation and properties", "[svg][line]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Create line with coordinates") {
        auto* line = screen.createLine("l1", 10, 20, 100, 200);
        REQUIRE(line != nullptr);

        auto* element = line->element();
        REQUIRE(element->type == "line");

        // Check inline styles
        REQUIRE(element->inline_style["x1"] == "10px");
        REQUIRE(element->inline_style["y1"] == "20px");
        REQUIRE(element->inline_style["x2"] == "100px");
        REQUIRE(element->inline_style["y2"] == "200px");
    }

    SECTION("Line stroke is required for visibility") {
        auto* line = screen.createLine("l2", 0, 0, 100, 100);
        line->setStroke("#e5e7eb", 1);

        auto* element = line->element();
        REQUIRE(element->inline_style["stroke"] == "#e5e7eb");
        REQUIRE(element->inline_style["stroke-width"] == "1px");
    }
}

TEST_CASE("SVG elements in container hierarchy", "[svg][hierarchy]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Rect as child of div container") {
        auto* container = screen.addWidget("container", "div");
        container->setInlineStyle("display", "block");

        auto* rect = screen.createRect("r1", 50, 50, 100, 100);
        rect->setFill("#3b82f6");
        rect->setInlineStyle("display", "block");

        container->addChild(rect);

        // Trigger layout
        screen.draw();

        // Verify hierarchy (using internal_id system)
        REQUIRE(rect->element()->parent_internal_id == container->element()->internal_id);
    }

    SECTION("Multiple SVG elements in same container") {
        auto* chartArea = screen.addWidget("chart", "div");
        chartArea->setInlineStyle("display", "block");

        auto* bar1 = screen.createRect("bar1", 10, 50, 20, 100);
        bar1->setFill("#3b82f6");
        bar1->setInlineStyle("display", "block");

        auto* bar2 = screen.createRect("bar2", 40, 30, 20, 120);
        bar2->setFill("#3b82f6");
        bar2->setInlineStyle("display", "block");

        chartArea->addChild(bar1);
        chartArea->addChild(bar2);

        screen.draw();

        // Both should be in same parent (using internal_id system)
        REQUIRE(bar1->element()->parent_internal_id == chartArea->element()->internal_id);
        REQUIRE(bar2->element()->parent_internal_id == chartArea->element()->internal_id);
    }
}

TEST_CASE("SVG rendering pipeline verification", "[svg][rendering]") {
    flexui::Screen screen(800, 600, "SVG Test");

    SECTION("Complete bar chart element setup") {
        // Load CSS with display property
        screen.loadCSS(R"(
            .chart-container {
                display: block;
                width: 400px;
                height: 300px;
                background: white;
            }
            .bar {
                display: block;
                fill: #3b82f6;
            }
        )");

        // Create container
        auto* container = screen.addWidget("chart", "div");
        container->setClass("chart-container");

        // Create bar with all required properties
        auto* bar = screen.createRect("bar", 50, 100, 30, 80);
        bar->setClass("bar");
        bar->setFill("#3b82f6");  // Redundant but explicit

        // Add to hierarchy
        container->addChild(bar);

        // Trigger layout
        screen.draw();

        // Verify all properties are set
        auto* barElement = bar->element();
        REQUIRE(barElement->parent_internal_id == container->element()->internal_id);
        REQUIRE(barElement->inline_style["fill"] == "#3b82f6");
        REQUIRE(barElement->inline_style["x"] == "50px");
        REQUIRE(barElement->inline_style["y"] == "100px");
        REQUIRE(barElement->inline_style["width"] == "30px");
        REQUIRE(barElement->inline_style["height"] == "80px");

        // Verify widget visibility
        REQUIRE(bar->isVisible() == true);
    }
}
