#include <nanovg.h>
#include <nanovg_css.h>
#include <nanovg_css_internal.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("Manual background color preservation", "[style][background]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    
    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    
    // Set manual color (mimicking the example)
    // Note: Not setting type, so it defaults to NONE
    elem->style.background.color = nvgRGBA(255, 0, 0, 255);
    
    // Verify initial state
    REQUIRE(elem->style.background.type == nvgcss::BackgroundType::NONE);
    REQUIRE(elem->style.background.color.r == 1.0f);
    
    // Run update (simulating frame update)
    nvgcssUpdate(renderer, 0.016f);
    
    // Verify color is preserved and type is fixed
    REQUIRE(elem->style.background.type == nvgcss::BackgroundType::COLOR);
    REQUIRE(elem->style.background.color.r == 1.0f);
    REQUIRE(elem->style.background.color.a == 1.0f);
    
    nvgcssDeleteRenderer(renderer);
}
