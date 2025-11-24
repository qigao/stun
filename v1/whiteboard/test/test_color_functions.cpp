#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <nanovg.h>
#include <whiteboard/ddf/render_node.h>

using namespace whiteboard::ddf;

// Helper to compare colors with tolerance
bool colors_equal(NVGcolor a, NVGcolor b, float tolerance = 0.02f) {
  return std::abs(a.r - b.r) < tolerance && std::abs(a.g - b.g) < tolerance &&
         std::abs(a.b - b.b) < tolerance && std::abs(a.a - b.a) < tolerance;
}

TEST_CASE("RenderNode parses RGB color function", "[render][colors]") {
  RenderNode node;

  // rgb(255, 0, 0) - red
  auto red = node.parse_color("rgb(255, 0, 0)");
  REQUIRE(colors_equal(red, nvgRGB(255, 0, 0)));

  // rgb(0, 255, 0) - green
  auto green = node.parse_color("rgb(0, 255, 0)");
  REQUIRE(colors_equal(green, nvgRGB(0, 255, 0)));

  // rgb(0, 0, 255) - blue
  auto blue = node.parse_color("rgb(0, 0, 255)");
  REQUIRE(colors_equal(blue, nvgRGB(0, 0, 255)));
}

TEST_CASE("RenderNode parses RGBA color function", "[render][colors]") {
  RenderNode node;

  // rgba(255, 0, 0, 1.0) - opaque red
  auto red = node.parse_color("rgba(255, 0, 0, 1.0)");
  REQUIRE(colors_equal(red, nvgRGBA(255, 0, 0, 255)));

  // rgba(0, 255, 0, 0.5) - semi-transparent green
  auto green = node.parse_color("rgba(0, 255, 0, 0.5)");
  REQUIRE(colors_equal(green, nvgRGBA(0, 255, 0, 127)));

  // rgba(0, 0, 255, 0.0) - fully transparent blue
  auto blue = node.parse_color("rgba(0, 0, 255, 0.0)");
  REQUIRE(colors_equal(blue, nvgRGBA(0, 0, 255, 0)));
}

TEST_CASE("RenderNode parses HSL color function", "[render][colors]") {
  RenderNode node;

  // hsl(0, 100%, 50%) - red
  auto red = node.parse_color("hsl(0, 100%, 50%)");
  REQUIRE(colors_equal(red, nvgRGB(255, 0, 0)));

  // hsl(120, 100%, 50%) - green
  auto green = node.parse_color("hsl(120, 100%, 50%)");
  REQUIRE(colors_equal(green, nvgRGB(0, 255, 0)));

  // hsl(240, 100%, 50%) - blue
  auto blue = node.parse_color("hsl(240, 100%, 50%)");
  REQUIRE(colors_equal(blue, nvgRGB(0, 0, 255)));

  // hsl(0, 0%, 50%) - gray
  auto gray = node.parse_color("hsl(0, 0%, 50%)");
  REQUIRE(colors_equal(gray, nvgRGB(127, 127, 127), 0.05f));
}

TEST_CASE("RenderNode parses HSLA color function", "[render][colors]") {
  RenderNode node;

  // hsla(0, 100%, 50%, 1.0) - opaque red
  auto red = node.parse_color("hsla(0, 100%, 50%, 1.0)");
  REQUIRE(colors_equal(red, nvgRGBA(255, 0, 0, 255)));

  // hsla(120, 100%, 50%, 0.5) - semi-transparent green
  auto green = node.parse_color("hsla(120, 100%, 50%, 0.5)");
  REQUIRE(colors_equal(green, nvgRGBA(0, 255, 0, 127)));

  // hsla(240, 100%, 50%, 0.0) - fully transparent blue
  auto blue = node.parse_color("hsla(240, 100%, 50%, 0.0)");
  REQUIRE(colors_equal(blue, nvgRGBA(0, 0, 255, 0)));
}

TEST_CASE("RenderNode parses hex colors", "[render][colors]") {
  RenderNode node;

  // #RGB format
  auto red = node.parse_color("#f00");
  REQUIRE(colors_equal(red, nvgRGB(255, 0, 0)));

  // #RRGGBB format
  auto green = node.parse_color("#00ff00");
  REQUIRE(colors_equal(green, nvgRGB(0, 255, 0)));

  // #RRGGBBAA format
  auto blue = node.parse_color("#0000ff80");
  REQUIRE(colors_equal(blue, nvgRGBA(0, 0, 255, 128)));
}

TEST_CASE("RenderNode parses named colors", "[render][colors]") {
  RenderNode node;

  REQUIRE(colors_equal(node.parse_color("red"), nvgRGB(255, 0, 0)));
  REQUIRE(colors_equal(node.parse_color("green"), nvgRGB(0, 128, 0)));
  REQUIRE(colors_equal(node.parse_color("blue"), nvgRGB(0, 0, 255)));
  REQUIRE(colors_equal(node.parse_color("white"), nvgRGB(255, 255, 255)));
  REQUIRE(colors_equal(node.parse_color("black"), nvgRGB(0, 0, 0)));
  REQUIRE(colors_equal(node.parse_color("transparent"), nvgRGBA(0, 0, 0, 0)));
}
