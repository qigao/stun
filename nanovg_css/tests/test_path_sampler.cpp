#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/nanovg_css_path_sampler.h"
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace nvgcss;

TEST_CASE("Path Sampler - Line Sampling", "[path][sampler]") {
    PathSampler sampler;
    
    SECTION("Horizontal line") {
        auto samples = sampler.sample_path("M 0 0 L 100 0", 10.0f);
        
        REQUIRE(samples.size() > 0);
        REQUIRE_THAT(sampler.get_total_length(), Catch::Matchers::WithinRel(100.0f, 0.01f));
        
        // First sample should be at start
        REQUIRE_THAT(samples.front().x, Catch::Matchers::WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(samples.front().y, Catch::Matchers::WithinAbs(0.0f, 0.1f));
        
        // Last sample should be at end
        REQUIRE_THAT(samples.back().x, Catch::Matchers::WithinAbs(100.0f, 0.1f));
        REQUIRE_THAT(samples.back().y, Catch::Matchers::WithinAbs(0.0f, 0.1f));
        
        // Angle should be 0 (horizontal right)
        REQUIRE_THAT(samples.back().angle, Catch::Matchers::WithinAbs(0.0f, 0.01f));
    }
    
    SECTION("Vertical line") {
        auto samples = sampler.sample_path("M 0 0 L 0 100", 10.0f);
        
        REQUIRE(samples.size() > 0);
        REQUIRE_THAT(sampler.get_total_length(), Catch::Matchers::WithinRel(100.0f, 0.01f));
        
        // Angle should be π/2 (vertical down)
        float expected_angle = M_PI / 2.0f;
        REQUIRE_THAT(samples.back().angle, Catch::Matchers::WithinAbs(expected_angle, 0.01f));
    }
    
    SECTION("Diagonal line") {
        auto samples = sampler.sample_path("M 0 0 L 100 100", 10.0f);
        
        float expected_length = sqrtf(100*100 + 100*100);
        REQUIRE_THAT(sampler.get_total_length(), Catch::Matchers::WithinRel(expected_length, 0.01f));
        
        // Angle should be π/4 (45 degrees)
        float expected_angle = M_PI / 4.0f;
        REQUIRE_THAT(samples.back().angle, Catch::Matchers::WithinAbs(expected_angle, 0.01f));
    }
}

TEST_CASE("Path Sampler - Bezier Sampling", "[path][sampler]") {
    PathSampler sampler;
    
    SECTION("Quadratic bezier") {
        // Simple quadratic curve
        auto samples = sampler.sample_path("M 0 0 Q 50 100 100 0", 5.0f);
        
        REQUIRE(samples.size() > 5);  // Should have multiple samples
        REQUIRE(sampler.get_total_length() > 100.0f);  // Curve is longer than straight line
        
        // First and last points
        REQUIRE_THAT(samples.front().x, Catch::Matchers::WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(samples.back().x, Catch::Matchers::WithinAbs(100.0f, 0.1f));
    }
    
    SECTION("Cubic bezier") {
        // Simple cubic curve
        auto samples = sampler.sample_path("M 0 0 C 33 100 66 100 100 0", 5.0f);
        
        REQUIRE(samples.size() > 5);
        REQUIRE(sampler.get_total_length() > 100.0f);
    }
}

TEST_CASE("Path Sampler - Complex Paths", "[path][sampler]") {
    PathSampler sampler;
    
    SECTION("Multi-segment path") {
        auto samples = sampler.sample_path("M 0 0 L 50 0 L 50 50 L 0 50 Z", 10.0f);
        
        // Square: 4 sides of 50 each = 200 total
        REQUIRE_THAT(sampler.get_total_length(), Catch::Matchers::WithinRel(200.0f, 0.01f));
    }
    
    SECTION("Mixed path commands") {
        auto samples = sampler.sample_path("M 0 0 L 50 0 Q 75 25 50 50 L 0 50 Z", 5.0f);
        
        REQUIRE(samples.size() > 10);
        REQUIRE(sampler.get_total_length() > 150.0f);
    }
}

TEST_CASE("Path Interpolation", "[path][sampler]") {
    PathSampler sampler;
    auto samples = sampler.sample_path("M 0 0 L 100 0", 10.0f);
    
    SECTION("Interpolate at midpoint") {
        auto pos = interpolate_path_position(samples, 50.0f);
        
        REQUIRE_THAT(pos.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(pos.y, Catch::Matchers::WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(pos.distance, Catch::Matchers::WithinAbs(50.0f, 0.1f));
    }
    
    SECTION("Interpolate at start") {
        auto pos = interpolate_path_position(samples, 0.0f);
        
        REQUIRE_THAT(pos.x, Catch::Matchers::WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(pos.y, Catch::Matchers::WithinAbs(0.0f, 0.1f));
    }
    
    SECTION("Interpolate at end") {
        auto pos = interpolate_path_position(samples, 100.0f);
        
        REQUIRE_THAT(pos.x, Catch::Matchers::WithinAbs(100.0f, 1.0f));
        REQUIRE_THAT(pos.y, Catch::Matchers::WithinAbs(0.0f, 0.1f));
    }
    
    SECTION("Interpolate beyond end") {
        auto pos = interpolate_path_position(samples, 150.0f);
        
        // Should clamp to end
        REQUIRE_THAT(pos.x, Catch::Matchers::WithinAbs(100.0f, 1.0f));
    }
}
