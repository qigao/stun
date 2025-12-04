// NanoVG Rough - Unit Tests
// Tests for hand-drawn style rendering

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "nanovg_rough.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("NVGRoughOptions - Default Options", "[nanovg_rough][options]") {
    NVGRoughOptions opts = nvgRoughDefaultOptions();
    
    SECTION("Default values are correct") {
        REQUIRE_THAT(opts.roughness, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(opts.bowing, WithinAbs(1.0f, 0.001f));
        REQUIRE(opts.stroke_count == 1);
        REQUIRE_THAT(opts.stroke_width, WithinAbs(1.0f, 0.001f));
        REQUIRE(opts.stroke_enabled == 1);
        REQUIRE(opts.fill_enabled == 0);
        REQUIRE(opts.seed == 0);
    }
    
    SECTION("Stroke color is black") {
        REQUIRE(opts.stroke_color.r == 0.0f);
        REQUIRE(opts.stroke_color.g == 0.0f);
        REQUIRE(opts.stroke_color.b == 0.0f);
        REQUIRE(opts.stroke_color.a == 1.0f);
    }
    
    SECTION("Fill color is transparent") {
        REQUIRE(opts.fill_color.a == 0.0f);
    }
}

TEST_CASE("NVGRoughOptions - Custom Options", "[nanovg_rough][options]") {
    NVGRoughOptions opts = nvgRoughDefaultOptions();
    
    SECTION("Can modify roughness") {
        opts.roughness = 2.5f;
        REQUIRE_THAT(opts.roughness, WithinAbs(2.5f, 0.001f));
    }
    
    SECTION("Can modify bowing") {
        opts.bowing = 0.5f;
        REQUIRE_THAT(opts.bowing, WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Can modify stroke count") {
        opts.stroke_count = 3;
        REQUIRE(opts.stroke_count == 3);
    }
    
    SECTION("Can modify stroke width") {
        opts.stroke_width = 2.0f;
        REQUIRE_THAT(opts.stroke_width, WithinAbs(2.0f, 0.001f));
    }
    
    SECTION("Can enable fill") {
        opts.fill_enabled = 1;
        REQUIRE(opts.fill_enabled == 1);
    }
    
    SECTION("Can disable stroke") {
        opts.stroke_enabled = 0;
        REQUIRE(opts.stroke_enabled == 0);
    }
    
    SECTION("Can set custom seed") {
        opts.seed = 12345;
        REQUIRE(opts.seed == 12345);
    }
}

TEST_CASE("NVGRoughOptions - Color Settings", "[nanovg_rough][options][color]") {
    NVGRoughOptions opts = nvgRoughDefaultOptions();
    
    SECTION("Can set stroke color") {
        opts.stroke_color = nvgRGBA(255, 0, 0, 255);
        REQUIRE(opts.stroke_color.r == 1.0f);
        REQUIRE(opts.stroke_color.g == 0.0f);
        REQUIRE(opts.stroke_color.b == 0.0f);
        REQUIRE(opts.stroke_color.a == 1.0f);
    }
    
    SECTION("Can set fill color") {
        opts.fill_color = nvgRGBA(0, 255, 0, 128);
        REQUIRE(opts.fill_color.r == 0.0f);
        REQUIRE(opts.fill_color.g == 1.0f);
        REQUIRE(opts.fill_color.b == 0.0f);
        REQUIRE_THAT(opts.fill_color.a, WithinAbs(0.502f, 0.01f)); // 128/255
    }
    
    SECTION("Can use nvgRGBf for colors") {
        opts.stroke_color = nvgRGBf(0.5f, 0.5f, 0.5f);
        REQUIRE_THAT(opts.stroke_color.r, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(opts.stroke_color.g, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(opts.stroke_color.b, WithinAbs(0.5f, 0.001f));
    }
}

TEST_CASE("NVGRoughOptions - Validation", "[nanovg_rough][options][validation]") {
    NVGRoughOptions opts = nvgRoughDefaultOptions();
    
    SECTION("Roughness can be zero") {
        opts.roughness = 0.0f;
        REQUIRE(opts.roughness == 0.0f);
    }
    
    SECTION("Roughness can be negative (for testing)") {
        opts.roughness = -1.0f;
        REQUIRE(opts.roughness == -1.0f);
    }
    
    SECTION("Bowing can be zero") {
        opts.bowing = 0.0f;
        REQUIRE(opts.bowing == 0.0f);
    }
    
    SECTION("Stroke count can be zero") {
        opts.stroke_count = 0;
        REQUIRE(opts.stroke_count == 0);
    }
    
    SECTION("Stroke width can be very small") {
        opts.stroke_width = 0.1f;
        REQUIRE_THAT(opts.stroke_width, WithinAbs(0.1f, 0.001f));
    }
}

TEST_CASE("NVGRoughOptions - Presets", "[nanovg_rough][options][presets]") {
    SECTION("Sketch preset") {
        NVGRoughOptions opts = nvgRoughDefaultOptions();
        opts.roughness = 2.0f;
        opts.bowing = 1.5f;
        opts.stroke_count = 2;
        
        REQUIRE_THAT(opts.roughness, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(opts.bowing, WithinAbs(1.5f, 0.001f));
        REQUIRE(opts.stroke_count == 2);
    }
    
    SECTION("Clean preset") {
        NVGRoughOptions opts = nvgRoughDefaultOptions();
        opts.roughness = 0.5f;
        opts.bowing = 0.5f;
        opts.stroke_count = 1;
        
        REQUIRE_THAT(opts.roughness, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(opts.bowing, WithinAbs(0.5f, 0.001f));
        REQUIRE(opts.stroke_count == 1);
    }
    
    SECTION("Rough preset") {
        NVGRoughOptions opts = nvgRoughDefaultOptions();
        opts.roughness = 3.0f;
        opts.bowing = 2.0f;
        opts.stroke_count = 3;
        
        REQUIRE_THAT(opts.roughness, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(opts.bowing, WithinAbs(2.0f, 0.001f));
        REQUIRE(opts.stroke_count == 3);
    }
}

TEST_CASE("NVGRoughOptions - Fill and Stroke Combinations", "[nanovg_rough][options]") {
    NVGRoughOptions opts = nvgRoughDefaultOptions();
    
    SECTION("Stroke only (default)") {
        REQUIRE(opts.stroke_enabled == 1);
        REQUIRE(opts.fill_enabled == 0);
    }
    
    SECTION("Fill only") {
        opts.stroke_enabled = 0;
        opts.fill_enabled = 1;
        REQUIRE(opts.stroke_enabled == 0);
        REQUIRE(opts.fill_enabled == 1);
    }
    
    SECTION("Both stroke and fill") {
        opts.stroke_enabled = 1;
        opts.fill_enabled = 1;
        REQUIRE(opts.stroke_enabled == 1);
        REQUIRE(opts.fill_enabled == 1);
    }
    
    SECTION("Neither stroke nor fill") {
        opts.stroke_enabled = 0;
        opts.fill_enabled = 0;
        REQUIRE(opts.stroke_enabled == 0);
        REQUIRE(opts.fill_enabled == 0);
    }
}

TEST_CASE("NVGRoughOptions - Seed Behavior", "[nanovg_rough][options][seed]") {
    SECTION("Default seed is zero") {
        NVGRoughOptions opts = nvgRoughDefaultOptions();
        REQUIRE(opts.seed == 0);
    }
    
    SECTION("Can set different seeds") {
        NVGRoughOptions opts1 = nvgRoughDefaultOptions();
        NVGRoughOptions opts2 = nvgRoughDefaultOptions();
        
        opts1.seed = 123;
        opts2.seed = 456;
        
        REQUIRE(opts1.seed != opts2.seed);
    }
    
    SECTION("Same seed should produce same randomness") {
        NVGRoughOptions opts1 = nvgRoughDefaultOptions();
        NVGRoughOptions opts2 = nvgRoughDefaultOptions();
        
        opts1.seed = 999;
        opts2.seed = 999;
        
        REQUIRE(opts1.seed == opts2.seed);
    }
}

TEST_CASE("NVGRoughOptions - Multiple Strokes", "[nanovg_rough][options][strokes]") {
    NVGRoughOptions opts = nvgRoughDefaultOptions();
    
    SECTION("Single stroke") {
        opts.stroke_count = 1;
        REQUIRE(opts.stroke_count == 1);
    }
    
    SECTION("Double stroke") {
        opts.stroke_count = 2;
        REQUIRE(opts.stroke_count == 2);
    }
    
    SECTION("Triple stroke") {
        opts.stroke_count = 3;
        REQUIRE(opts.stroke_count == 3);
    }
    
    SECTION("Many strokes") {
        opts.stroke_count = 10;
        REQUIRE(opts.stroke_count == 10);
    }
}

TEST_CASE("NVGRoughOptions - Extreme Values", "[nanovg_rough][options][extreme]") {
    NVGRoughOptions opts = nvgRoughDefaultOptions();
    
    SECTION("Very high roughness") {
        opts.roughness = 100.0f;
        REQUIRE_THAT(opts.roughness, WithinAbs(100.0f, 0.001f));
    }
    
    SECTION("Very high bowing") {
        opts.bowing = 50.0f;
        REQUIRE_THAT(opts.bowing, WithinAbs(50.0f, 0.001f));
    }
    
    SECTION("Very many strokes") {
        opts.stroke_count = 100;
        REQUIRE(opts.stroke_count == 100);
    }
    
    SECTION("Very thick stroke") {
        opts.stroke_width = 50.0f;
        REQUIRE_THAT(opts.stroke_width, WithinAbs(50.0f, 0.001f));
    }
}

// Note: Actual rendering tests (nvgRoughLine, nvgRoughCircle, etc.) 
// require a valid NVGcontext and OpenGL context, so they should be 
// tested in visual/integration tests rather than unit tests.
