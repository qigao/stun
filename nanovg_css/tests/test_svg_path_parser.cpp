#include "nanovg_css_svg_path.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "test_config.h"

INIT_TEST_LOGGING();

TEST_CASE("SVG Path Parser - MoveTo and LineTo", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M 10 20 L 30 40");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0].type == 'M');
    REQUIRE(cmds[0].params[0] == 10.0f);
    REQUIRE(cmds[0].params[1] == 20.0f);
    REQUIRE(cmds[1].type == 'L');
    REQUIRE(cmds[1].params[0] == 30.0f);
    REQUIRE(cmds[1].params[1] == 40.0f);
}

TEST_CASE("SVG Path Parser - Cubic Bezier", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M 0 0 C 10 10 20 20 30 30");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[1].type == 'C');
    REQUIRE(cmds[1].params.size() == 6);
    REQUIRE(cmds[1].params[0] == 10.0f);
    REQUIRE(cmds[1].params[5] == 30.0f);
}

TEST_CASE("SVG Path Parser - Relative Commands", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("m 10 20 l 5 5");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0].type == 'M');
    REQUIRE(cmds[0].relative == true);
    REQUIRE(cmds[1].type == 'L');
    REQUIRE(cmds[1].relative == true);
}

TEST_CASE("SVG Path Parser - Implicit Commands", "[svg][path]") {
    // "M 10 20 30 40" should become M 10 20 L 30 40
    auto cmds = nvgcss::SVGPathParser::parse("M 10 20 30 40");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0].type == 'M');
    REQUIRE(cmds[1].type == 'L');
    REQUIRE(cmds[1].params[0] == 30.0f);
}

TEST_CASE("SVG Path Parser - Complex Path", "[svg][path]") {
    // M 990 275 Q 1010 250 1030 275 T 1070 275
    auto cmds = nvgcss::SVGPathParser::parse("M 990 275 Q 1010 250 1030 275 T 1070 275");
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[0].type == 'M');
    REQUIRE(cmds[1].type == 'Q');
    REQUIRE(cmds[2].type == 'T'); // T command (smooth quadratic)
}

TEST_CASE("SVG Path Parser - Arc Command", "[svg][path]") {
    // A rx ry x-axis-rotation large-arc-flag sweep-flag x y
    auto cmds = nvgcss::SVGPathParser::parse("M 10 10 A 30 50 45 0 1 100 200");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[1].type == 'A');
    REQUIRE(cmds[1].params.size() == 7);
    REQUIRE(cmds[1].params[0] == 30.0f);  // rx
    REQUIRE(cmds[1].params[1] == 50.0f);  // ry
    REQUIRE(cmds[1].params[2] == 45.0f);  // rotation
    REQUIRE(cmds[1].params[3] == 0.0f);   // large-arc
    REQUIRE(cmds[1].params[4] == 1.0f);   // sweep
    REQUIRE(cmds[1].params[5] == 100.0f); // x
    REQUIRE(cmds[1].params[6] == 200.0f); // y
}

TEST_CASE("SVG Path Parser - Negative Numbers", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M -10 -20 L -30.5 -40.75");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0].params[0] == -10.0f);
    REQUIRE(cmds[0].params[1] == -20.0f);
    REQUIRE(cmds[1].params[0] == -30.5f);
    REQUIRE(cmds[1].params[1] == -40.75f);
}

TEST_CASE("SVG Path Parser - Scientific Notation", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M 1e2 2.5e-1 L 3E+2 4.0E0");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0].params[0] == 100.0f);      // 1e2
    REQUIRE(cmds[0].params[1] == 0.25f);       // 2.5e-1
    REQUIRE(cmds[1].params[0] == 300.0f);      // 3E+2
    REQUIRE(cmds[1].params[1] == 4.0f);        // 4.0E0
}

TEST_CASE("SVG Path Parser - Comma Separators", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M 10,20 L 30,40");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0].params[0] == 10.0f);
    REQUIRE(cmds[0].params[1] == 20.0f);
    REQUIRE(cmds[1].params[0] == 30.0f);
    REQUIRE(cmds[1].params[1] == 40.0f);
}

TEST_CASE("SVG Path Parser - Mixed Separators", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M10,20L30 40");
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0].params[0] == 10.0f);
    REQUIRE(cmds[1].params[0] == 30.0f);
}

TEST_CASE("SVG Path Parser - Compressed Path", "[svg][path]") {
    // No whitespace, common in optimized SVG
    auto cmds = nvgcss::SVGPathParser::parse("M10 20L30 40z");
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[0].type == 'M');
    REQUIRE(cmds[1].type == 'L');
    REQUIRE(cmds[2].type == 'Z');
}

TEST_CASE("SVG Path Parser - Multiple Implicit Commands", "[svg][path]") {
    // Multiple implicit LineTo after MoveTo
    auto cmds = nvgcss::SVGPathParser::parse("M 0 0 10 10 20 20");
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[0].type == 'M');
    REQUIRE(cmds[1].type == 'L');
    REQUIRE(cmds[1].params[0] == 10.0f);
    REQUIRE(cmds[2].type == 'L');
    REQUIRE(cmds[2].params[0] == 20.0f);
}

TEST_CASE("SVG Path Parser - Horizontal and Vertical Lines", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M 10 10 H 50 V 100");
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[1].type == 'H');
    REQUIRE(cmds[1].params.size() == 1);
    REQUIRE(cmds[1].params[0] == 50.0f);
    REQUIRE(cmds[2].type == 'V');
    REQUIRE(cmds[2].params.size() == 1);
    REQUIRE(cmds[2].params[0] == 100.0f);
}

TEST_CASE("SVG Path Parser - Smooth Curves", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("M 0 0 C 10 10 20 20 30 30 S 50 50 60 60");
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[1].type == 'C');
    REQUIRE(cmds[2].type == 'S');
    REQUIRE(cmds[2].params.size() == 4);
}

TEST_CASE("SVG Path Parser - Empty Path", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("");
    REQUIRE(cmds.empty());
}

TEST_CASE("SVG Path Parser - Whitespace Only", "[svg][path]") {
    auto cmds = nvgcss::SVGPathParser::parse("   \t\n  ");
    REQUIRE(cmds.empty());
}

TEST_CASE("SVG Path Parser - Real World Example - Circle", "[svg][path]") {
    // Typical circle path using cubic beziers
    auto cmds = nvgcss::SVGPathParser::parse(
        "M 100,50 "
        "C 100,77.6 77.6,100 50,100 22.4,100 0,77.6 0,50 0,22.4 22.4,0 50,0 77.6,0 100,22.4 100,50 Z"
    );
    REQUIRE(cmds.size() == 6);  // M + 4 C commands + Z
    REQUIRE(cmds[0].type == 'M');
    REQUIRE(cmds[1].type == 'C');
    REQUIRE(cmds[2].type == 'C');  // Implicit C
    REQUIRE(cmds[3].type == 'C');  // Implicit C
    REQUIRE(cmds[4].type == 'C');  // Implicit C
    REQUIRE(cmds[5].type == 'Z');
}
