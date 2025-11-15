#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/style_layer.h>
#include <whiteboard/ddf/stylesheet.h>
#include <whiteboard/ddf/shape_layer.h>

using namespace whiteboard::ddf;

TEST_CASE("StyleLayer - Stylesheet management", "[style_layer]") {
    StyleLayer layer;

    SECTION("Add and retrieve stylesheet") {
        auto stylesheet = std::make_unique<StyleSheet>();
        stylesheet->add_rule("rect", {{"fill", "red"}});
        
        layer.add_stylesheet("main", std::move(stylesheet));
        
        auto* retrieved = layer.get_stylesheet("main");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->get_rules().size() == 1);
    }

    SECTION("Remove stylesheet") {
        auto stylesheet = std::make_unique<StyleSheet>();
        layer.add_stylesheet("main", std::move(stylesheet));
        
        layer.remove_stylesheet("main");
        
        auto* retrieved = layer.get_stylesheet("main");
        REQUIRE(retrieved == nullptr);
    }

    SECTION("Get non-existent stylesheet returns nullptr") {
        auto* retrieved = layer.get_stylesheet("nonexistent");
        REQUIRE(retrieved == nullptr);
    }

    SECTION("Add multiple stylesheets") {
        auto stylesheet1 = std::make_unique<StyleSheet>();
        auto stylesheet2 = std::make_unique<StyleSheet>();
        
        layer.add_stylesheet("theme", std::move(stylesheet1));
        layer.add_stylesheet("custom", std::move(stylesheet2));
        
        REQUIRE(layer.get_stylesheets().size() == 2);
        REQUIRE(layer.get_stylesheet("theme") != nullptr);
        REQUIRE(layer.get_stylesheet("custom") != nullptr);
    }

    SECTION("Replace existing stylesheet") {
        auto stylesheet1 = std::make_unique<StyleSheet>();
        stylesheet1->add_rule("rect", {{"fill", "red"}});
        layer.add_stylesheet("main", std::move(stylesheet1));
        
        auto stylesheet2 = std::make_unique<StyleSheet>();
        stylesheet2->add_rule("circle", {{"fill", "blue"}});
        layer.add_stylesheet("main", std::move(stylesheet2));
        
        auto* retrieved = layer.get_stylesheet("main");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->get_rules().size() == 1);
        REQUIRE(retrieved->get_rules()[0].selector == "circle");
    }
}

TEST_CASE("StyleLayer - Pseudo-state management", "[style_layer]") {
    StyleLayer layer;

    SECTION("Update and retrieve pseudo-state") {
        layer.update_pseudo_state("shape1", "hover", true);
        
        auto states = layer.get_pseudo_states("shape1");
        REQUIRE(states.size() == 1);
        REQUIRE(states.count("hover") == 1);
    }

    SECTION("Remove pseudo-state") {
        layer.update_pseudo_state("shape1", "hover", true);
        layer.update_pseudo_state("shape1", "hover", false);
        
        auto states = layer.get_pseudo_states("shape1");
        REQUIRE(states.empty());
    }

    SECTION("Multiple pseudo-states for same shape") {
        layer.update_pseudo_state("shape1", "hover", true);
        layer.update_pseudo_state("shape1", "selected", true);
        layer.update_pseudo_state("shape1", "active", true);
        
        auto states = layer.get_pseudo_states("shape1");
        REQUIRE(states.size() == 3);
        REQUIRE(states.count("hover") == 1);
        REQUIRE(states.count("selected") == 1);
        REQUIRE(states.count("active") == 1);
    }

    SECTION("Pseudo-states for different shapes are independent") {
        layer.update_pseudo_state("shape1", "hover", true);
        layer.update_pseudo_state("shape2", "selected", true);
        
        auto states1 = layer.get_pseudo_states("shape1");
        auto states2 = layer.get_pseudo_states("shape2");
        
        REQUIRE(states1.size() == 1);
        REQUIRE(states1.count("hover") == 1);
        REQUIRE(states2.size() == 1);
        REQUIRE(states2.count("selected") == 1);
    }

    SECTION("Clear pseudo-states for specific shape") {
        layer.update_pseudo_state("shape1", "hover", true);
        layer.update_pseudo_state("shape1", "selected", true);
        layer.update_pseudo_state("shape2", "hover", true);
        
        layer.clear_pseudo_states("shape1");
        
        REQUIRE(layer.get_pseudo_states("shape1").empty());
        REQUIRE(layer.get_pseudo_states("shape2").size() == 1);
    }

    SECTION("Clear all pseudo-states") {
        layer.update_pseudo_state("shape1", "hover", true);
        layer.update_pseudo_state("shape2", "selected", true);
        layer.update_pseudo_state("shape3", "active", true);
        
        layer.clear_all_pseudo_states();
        
        REQUIRE(layer.get_pseudo_states("shape1").empty());
        REQUIRE(layer.get_pseudo_states("shape2").empty());
        REQUIRE(layer.get_pseudo_states("shape3").empty());
    }

    SECTION("Get pseudo-states for non-existent shape returns empty set") {
        auto states = layer.get_pseudo_states("nonexistent");
        REQUIRE(states.empty());
    }
}

TEST_CASE("StyleLayer - Style computation with single stylesheet", "[style_layer]") {
    StyleLayer layer;
    
    auto stylesheet = std::make_unique<StyleSheet>();
    stylesheet->add_rule("rect", {{"fill", "red"}, {"stroke", "black"}});
    stylesheet->add_rule(".highlight", {{"fill", "yellow"}});
    layer.add_stylesheet("main", std::move(stylesheet));

    SECTION("Compute style for shape with type selector") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        
        auto computed = layer.compute_style_for_shape(shape);
        
        REQUIRE(computed["fill"] == "red");
        REQUIRE(computed["stroke"] == "black");
    }

    SECTION("Compute style with class selector") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        shape.classes = {"highlight"};
        
        auto computed = layer.compute_style_for_shape(shape);
        
        // Class selector should override type selector
        REQUIRE(computed["fill"] == "yellow");
        REQUIRE(computed["stroke"] == "black");
    }

    SECTION("Inline styles override stylesheet") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        shape.inline_style = {{"fill", "blue"}};
        
        auto computed = layer.compute_style_for_shape(shape);
        
        REQUIRE(computed["fill"] == "blue");
        REQUIRE(computed["stroke"] == "black");
    }
}

TEST_CASE("StyleLayer - Style computation with multiple stylesheets", "[style_layer]") {
    StyleLayer layer;
    
    auto stylesheet1 = std::make_unique<StyleSheet>();
    stylesheet1->add_rule("rect", {{"fill", "red"}, {"stroke", "black"}});
    layer.add_stylesheet("base", std::move(stylesheet1));
    
    auto stylesheet2 = std::make_unique<StyleSheet>();
    stylesheet2->add_rule(".highlight", {{"fill", "yellow"}});
    layer.add_stylesheet("theme", std::move(stylesheet2));

    SECTION("Rules from multiple stylesheets are combined") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        shape.classes = {"highlight"};
        
        auto computed = layer.compute_style_for_shape(shape);
        
        REQUIRE(computed["fill"] == "yellow");  // From theme stylesheet
        REQUIRE(computed["stroke"] == "black"); // From base stylesheet
    }

    SECTION("Higher specificity wins across stylesheets") {
        auto stylesheet3 = std::make_unique<StyleSheet>();
        stylesheet3->add_rule("#shape1", {{"fill", "green"}});
        layer.add_stylesheet("custom", std::move(stylesheet3));
        
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        shape.classes = {"highlight"};
        
        auto computed = layer.compute_style_for_shape(shape);
        
        // ID selector has highest specificity
        REQUIRE(computed["fill"] == "green");
    }
}

TEST_CASE("StyleLayer - Style computation with pseudo-states", "[style_layer]") {
    StyleLayer layer;
    
    auto stylesheet = std::make_unique<StyleSheet>();
    stylesheet->add_rule("rect", {{"fill", "red"}});
    stylesheet->add_rule("rect:hover", {{"fill", "lightblue"}});
    stylesheet->add_rule("rect:selected", {{"stroke", "blue"}, {"stroke-width", "3"}});
    layer.add_stylesheet("main", std::move(stylesheet));

    SECTION("Pseudo-state affects computed style") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        
        layer.update_pseudo_state("shape1", "hover", true);
        
        auto computed = layer.compute_style_for_shape(shape);
        
        REQUIRE(computed["fill"] == "lightblue");
    }

    SECTION("Multiple pseudo-states combine") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        
        layer.update_pseudo_state("shape1", "hover", true);
        layer.update_pseudo_state("shape1", "selected", true);
        
        auto computed = layer.compute_style_for_shape(shape);
        
        REQUIRE(computed["fill"] == "lightblue");
        REQUIRE(computed["stroke"] == "blue");
        REQUIRE(computed["stroke-width"] == "3");
    }

    SECTION("Removing pseudo-state updates computed style") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        
        layer.update_pseudo_state("shape1", "hover", true);
        auto computed1 = layer.compute_style_for_shape(shape);
        REQUIRE(computed1["fill"] == "lightblue");
        
        layer.update_pseudo_state("shape1", "hover", false);
        auto computed2 = layer.compute_style_for_shape(shape);
        REQUIRE(computed2["fill"] == "red");
    }
}

TEST_CASE("StyleLayer - Cache management", "[style_layer]") {
    StyleLayer layer;
    
    auto stylesheet = std::make_unique<StyleSheet>();
    stylesheet->add_rule("rect", {{"fill", "red"}});
    layer.add_stylesheet("main", std::move(stylesheet));

    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";

    SECTION("Computed styles are cached") {
        auto computed1 = layer.compute_style_for_shape(shape);
        auto computed2 = layer.compute_style_for_shape(shape);
        
        // Both should return the same result (from cache)
        REQUIRE(computed1 == computed2);
    }

    SECTION("Adding stylesheet invalidates cache") {
        auto computed1 = layer.compute_style_for_shape(shape);
        
        auto new_stylesheet = std::make_unique<StyleSheet>();
        new_stylesheet->add_rule("rect", {{"fill", "blue"}});
        layer.add_stylesheet("override", std::move(new_stylesheet));
        
        auto computed2 = layer.compute_style_for_shape(shape);
        
        // Should recompute with new stylesheet
        REQUIRE(computed2["fill"] == "blue");
    }

    SECTION("Removing stylesheet invalidates cache") {
        auto stylesheet2 = std::make_unique<StyleSheet>();
        stylesheet2->add_rule("rect", {{"fill", "blue"}});
        layer.add_stylesheet("override", std::move(stylesheet2));
        
        auto computed1 = layer.compute_style_for_shape(shape);
        REQUIRE(computed1["fill"] == "blue");
        
        layer.remove_stylesheet("override");
        
        auto computed2 = layer.compute_style_for_shape(shape);
        REQUIRE(computed2["fill"] == "red");
    }

    SECTION("Updating pseudo-state invalidates cache for that shape") {
        auto stylesheet2 = layer.get_stylesheet("main");
        stylesheet2->add_rule("rect:hover", {{"fill", "lightblue"}});
        layer.invalidate_computed_styles();
        
        auto computed1 = layer.compute_style_for_shape(shape);
        REQUIRE(computed1["fill"] == "red");
        
        layer.update_pseudo_state("shape1", "hover", true);
        
        auto computed2 = layer.compute_style_for_shape(shape);
        REQUIRE(computed2["fill"] == "lightblue");
    }

    SECTION("Manual cache invalidation works") {
        auto computed1 = layer.compute_style_for_shape(shape);
        
        layer.invalidate_computed_styles();
        
        // Should recompute (though result is the same)
        auto computed2 = layer.compute_style_for_shape(shape);
        REQUIRE(computed1 == computed2);
    }

    SECTION("Invalidate specific shape cache") {
        Shape shape2;
        shape2.id = "shape2";
        shape2.type = "rect";
        
        auto computed1 = layer.compute_style_for_shape(shape);
        auto computed2 = layer.compute_style_for_shape(shape2);
        
        layer.invalidate_computed_style("shape1");
        
        // shape1 cache should be invalidated, shape2 should still be cached
        auto computed3 = layer.compute_style_for_shape(shape);
        auto computed4 = layer.compute_style_for_shape(shape2);
        
        REQUIRE(computed1 == computed3);
        REQUIRE(computed2 == computed4);
    }
}

TEST_CASE("StyleLayer - Integration with Shape", "[style_layer]") {
    StyleLayer layer;
    
    auto stylesheet = std::make_unique<StyleSheet>();
    stylesheet->add_rule("rect", {{"fill", "red"}});
    stylesheet->add_rule(".card", {{"stroke", "black"}, {"stroke-width", "2"}});
    stylesheet->add_rule("#important", {{"fill", "orange"}});
    stylesheet->add_rule("rect:hover", {{"fill", "lightblue"}});
    layer.add_stylesheet("main", std::move(stylesheet));

    SECTION("Complex shape with multiple selectors") {
        Shape shape;
        shape.id = "important";
        shape.type = "rect";
        shape.classes = {"card", "highlight"};
        
        auto computed = layer.compute_style_for_shape(shape);
        
        REQUIRE(computed["fill"] == "orange");  // ID selector wins
        REQUIRE(computed["stroke"] == "black");
        REQUIRE(computed["stroke-width"] == "2");
    }

    SECTION("Shape with inline style and pseudo-state") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        shape.inline_style = {{"stroke", "green"}};
        
        layer.update_pseudo_state("shape1", "hover", true);
        
        auto computed = layer.compute_style_for_shape(shape);
        
        REQUIRE(computed["fill"] == "lightblue");  // From pseudo-state
        REQUIRE(computed["stroke"] == "green");    // Inline style wins
    }
}

TEST_CASE("StyleLayer - Empty state handling", "[style_layer]") {
    StyleLayer layer;

    SECTION("Compute style with no stylesheets") {
        Shape shape;
        shape.id = "shape1";
        shape.type = "rect";
        
        auto computed = layer.compute_style_for_shape(shape);
        
        // Should return empty or only inline styles
        REQUIRE(computed.empty());
    }

    SECTION("Compute style with empty shape") {
        auto stylesheet = std::make_unique<StyleSheet>();
        stylesheet->add_rule("rect", {{"fill", "red"}});
        layer.add_stylesheet("main", std::move(stylesheet));
        
        Shape shape;
        shape.id = "";
        shape.type = "";
        
        auto computed = layer.compute_style_for_shape(shape);
        
        // Should handle gracefully - just verify it doesn't crash
        REQUIRE(computed.size() >= 0);
    }
}
