#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/ddf_document.h>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/svg_importer.h>

using namespace whiteboard::ddf;

TEST_CASE("Import basic rect", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg" width="200" height="200">
            <rect x="10" y="20" width="100" height="50" fill="red" stroke="blue" stroke-width="2"/>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->type == "rect");
    REQUIRE(shape->geometry.at("x") == 10.0f);
    REQUIRE(shape->geometry.at("y") == 20.0f);
    REQUIRE(shape->geometry.at("width") == 100.0f);
    REQUIRE(shape->geometry.at("height") == 50.0f);
    REQUIRE(shape->inline_style.at("fill") == "red");
    REQUIRE(shape->inline_style.at("stroke") == "blue");
    REQUIRE(shape->inline_style.at("stroke_width") == "2");
}

TEST_CASE("Import circle", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <circle cx="50" cy="60" r="30" fill="green"/>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->type == "circle");
    REQUIRE(shape->geometry.at("cx") == 50.0f);
    REQUIRE(shape->geometry.at("cy") == 60.0f);
    REQUIRE(shape->geometry.at("r") == 30.0f);
    REQUIRE(shape->inline_style.at("fill") == "green");
}

TEST_CASE("Import text", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <text x="10" y="20" font-size="16" fill="black">Hello World</text>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->type == "text");
    REQUIRE(shape->text == "Hello World");
    REQUIRE(shape->geometry.at("x") == 10.0f);
    REQUIRE(shape->geometry.at("y") == 20.0f);
    REQUIRE(shape->inline_style.at("font_size") == "16");
}

TEST_CASE("Import group", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <g id="group1">
                <rect x="0" y="0" width="50" height="50"/>
                <circle cx="25" cy="25" r="10"/>
            </g>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 3); // 1 group + 2 children
    
    // Find the group
    const Shape* group = nullptr;
    for (const auto* shape : shapes) {
        if (shape->type == "group") {
            group = shape;
            break;
        }
    }
    
    REQUIRE(group != nullptr);
    REQUIRE(group->id == "group1");
    REQUIRE(group->child_shape_ids.size() == 2);
    
    // Verify children have correct parent
    int children_with_correct_parent = 0;
    for (const auto* shape : shapes) {
        if (shape->parent_shape_id == "group1") {
            children_with_correct_parent++;
        }
    }
    REQUIRE(children_with_correct_parent == 2);
}

TEST_CASE("Import with inline style", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <rect x="10" y="10" width="50" height="50" style="fill: blue; stroke: red; stroke-width: 3"/>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->inline_style.at("fill") == "blue");
    REQUIRE(shape->inline_style.at("stroke") == "red");
    REQUIRE(shape->inline_style.at("stroke_width") == "3");
}

TEST_CASE("Import path", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <path d="M 10 10 L 50 50 L 10 50 Z" fill="yellow"/>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->type == "path");
    REQUIRE(shape->inline_style.at("d") == "M 10 10 L 50 50 L 10 50 Z");
    REQUIRE(shape->inline_style.at("fill") == "yellow");
}

TEST_CASE("Import invalid SVG", "[svg_import]") {
    DDFDocument doc;
    std::string svg = "This is not valid SVG";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE_FALSE(success);
}

TEST_CASE("Import empty SVG", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 0);
}

TEST_CASE("Round-trip: rect export and import", "[svg_import][round_trip]") {
    DDFDocument doc1;
    
    // Create a rect shape
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 10.0f;
    rect.geometry["y"] = 20.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 50.0f;
    rect.inline_style["fill"] = "red";
    rect.inline_style["stroke"] = "blue";
    rect.inline_style["stroke_width"] = "2";
    doc1.shape_layer().add_shape(rect);
    
    // Export to SVG
    std::string svg = doc1.export_to_svg();
    REQUIRE(!svg.empty());
    
    // Import back
    DDFDocument doc2;
    bool success = doc2.import_from_svg(svg);
    REQUIRE(success);
    
    // Verify the shape was preserved
    auto shapes = doc2.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* imported = shapes[0];
    REQUIRE(imported->type == "rect");
    REQUIRE(imported->geometry.at("x") == 10.0f);
    REQUIRE(imported->geometry.at("y") == 20.0f);
    REQUIRE(imported->geometry.at("width") == 100.0f);
    REQUIRE(imported->geometry.at("height") == 50.0f);
    REQUIRE(imported->inline_style.at("fill") == "red");
    REQUIRE(imported->inline_style.at("stroke") == "blue");
}

TEST_CASE("Round-trip: circle export and import", "[svg_import][round_trip]") {
    DDFDocument doc1;
    
    // Create a circle shape
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 50.0f;
    circle.geometry["cy"] = 60.0f;
    circle.geometry["r"] = 30.0f;
    circle.inline_style["fill"] = "green";
    doc1.shape_layer().add_shape(circle);
    
    // Export to SVG
    std::string svg = doc1.export_to_svg();
    
    // Import back
    DDFDocument doc2;
    bool success = doc2.import_from_svg(svg);
    REQUIRE(success);
    
    // Verify the shape was preserved
    auto shapes = doc2.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* imported = shapes[0];
    REQUIRE(imported->type == "circle");
    REQUIRE(imported->geometry.at("cx") == 50.0f);
    REQUIRE(imported->geometry.at("cy") == 60.0f);
    REQUIRE(imported->geometry.at("r") == 30.0f);
    REQUIRE(imported->inline_style.at("fill") == "green");
}

TEST_CASE("Round-trip: text export and import", "[svg_import][round_trip]") {
    DDFDocument doc1;
    
    // Create a text shape
    Shape text;
    text.id = "text1";
    text.type = "text";
    text.text = "Hello World";
    text.geometry["x"] = 10.0f;
    text.geometry["y"] = 20.0f;
    text.inline_style["font_size"] = "16";
    text.inline_style["fill"] = "black";
    doc1.shape_layer().add_shape(text);
    
    // Export to SVG
    std::string svg = doc1.export_to_svg();
    
    // Import back
    DDFDocument doc2;
    bool success = doc2.import_from_svg(svg);
    REQUIRE(success);
    
    // Verify the shape was preserved
    auto shapes = doc2.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* imported = shapes[0];
    REQUIRE(imported->type == "text");
    REQUIRE(imported->text == "Hello World");
    REQUIRE(imported->geometry.at("x") == 10.0f);
    REQUIRE(imported->geometry.at("y") == 20.0f);
}

TEST_CASE("Round-trip: group with children", "[svg_import][round_trip]") {
    DDFDocument doc1;
    
    // Create shapes
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 50.0f;
    rect.geometry["height"] = 50.0f;
    doc1.shape_layer().add_shape(rect);
    
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 25.0f;
    circle.geometry["cy"] = 25.0f;
    circle.geometry["r"] = 10.0f;
    doc1.shape_layer().add_shape(circle);
    
    // Group them
    std::string group_id = doc1.shape_layer().group_shapes({"rect1", "circle1"}, "group1");
    
    // Export to SVG
    std::string svg = doc1.export_to_svg();
    
    // Import back
    DDFDocument doc2;
    bool success = doc2.import_from_svg(svg);
    REQUIRE(success);
    
    // Verify the structure was preserved
    auto shapes = doc2.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 3); // 1 group + 2 children
    
    // Find the group
    const Shape* group = nullptr;
    for (const auto* shape : shapes) {
        if (shape->type == "group") {
            group = shape;
            break;
        }
    }
    
    REQUIRE(group != nullptr);
    REQUIRE(group->child_shape_ids.size() == 2);
}

TEST_CASE("Round-trip: multiple shapes", "[svg_import][round_trip]") {
    DDFDocument doc1;
    
    // Create multiple shapes
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 10.0f;
    rect.geometry["y"] = 10.0f;
    rect.geometry["width"] = 50.0f;
    rect.geometry["height"] = 50.0f;
    rect.inline_style["fill"] = "red";
    doc1.shape_layer().add_shape(rect);
    
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 100.0f;
    circle.geometry["cy"] = 100.0f;
    circle.geometry["r"] = 25.0f;
    circle.inline_style["fill"] = "blue";
    doc1.shape_layer().add_shape(circle);
    
    Shape text;
    text.id = "text1";
    text.type = "text";
    text.text = "Test";
    text.geometry["x"] = 150.0f;
    text.geometry["y"] = 150.0f;
    doc1.shape_layer().add_shape(text);
    
    // Export to SVG
    std::string svg = doc1.export_to_svg();
    
    // Import back
    DDFDocument doc2;
    bool success = doc2.import_from_svg(svg);
    REQUIRE(success);
    
    // Verify all shapes were preserved
    auto shapes = doc2.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 3);
    
    // Count shape types
    int rect_count = 0;
    int circle_count = 0;
    int text_count = 0;
    
    for (const auto* shape : shapes) {
        if (shape->type == "rect") rect_count++;
        else if (shape->type == "circle") circle_count++;
        else if (shape->type == "text") text_count++;
    }
    
    REQUIRE(rect_count == 1);
    REQUIRE(circle_count == 1);
    REQUIRE(text_count == 1);
}

TEST_CASE("Import preserves transform", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <rect x="10" y="10" width="50" height="50" transform="translate(100, 50)"/>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->transform.size() == 6);
    // Transform should be parsed (basic check)
    REQUIRE(shape->transform[4] == 100.0f); // translate x
    REQUIRE(shape->transform[5] == 50.0f);  // translate y
}

TEST_CASE("Import handles opacity", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <rect x="10" y="10" width="50" height="50" opacity="0.5"/>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->inline_style.at("opacity") == "0.5");
}

TEST_CASE("Import nested groups", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <g id="outer">
                <g id="inner">
                    <rect x="0" y="0" width="50" height="50"/>
                </g>
            </g>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 3); // 2 groups + 1 rect
    
    // Find outer group
    const Shape* outer = nullptr;
    for (const auto* shape : shapes) {
        if (shape->id == "outer") {
            outer = shape;
            break;
        }
    }
    
    REQUIRE(outer != nullptr);
    REQUIRE(outer->type == "group");
    REQUIRE(outer->child_shape_ids.size() == 1);
}

TEST_CASE("Import handles missing attributes gracefully", "[svg_import]") {
    DDFDocument doc;
    std::string svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg">
            <rect width="50" height="50"/>
        </svg>
    )";
    
    bool success = doc.import_from_svg(svg);
    REQUIRE(success);
    
    auto shapes = doc.shape_layer().get_all_shapes();
    REQUIRE(shapes.size() == 1);
    
    const auto* shape = shapes[0];
    REQUIRE(shape->type == "rect");
    // x and y should default to 0
    REQUIRE(shape->geometry.at("x") == 0.0f);
    REQUIRE(shape->geometry.at("y") == 0.0f);
}
