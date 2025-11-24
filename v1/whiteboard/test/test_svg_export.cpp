#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/ddf_document.h>
#include <whiteboard/ddf/svg_renderer.h>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/connector_layer.h>
#include <string>
#include <fstream>

using namespace whiteboard::ddf;

// ============================================================================
// Helper Functions for SVG Validation
// ============================================================================

bool contains_substring(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

bool is_valid_svg_header(const std::string& svg) {
    return contains_substring(svg, "<?xml version=\"1.0\"") &&
           contains_substring(svg, "<svg") &&
           contains_substring(svg, "xmlns=\"http://www.w3.org/2000/svg\"");
}

bool is_valid_svg_closing(const std::string& svg) {
    return contains_substring(svg, "</svg>");
}

int count_occurrences(const std::string& str, const std::string& substr) {
    int count = 0;
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
        ++count;
        pos += substr.length();
    }
    return count;
}

// ============================================================================
// Basic SVG Export Tests
// ============================================================================

TEST_CASE("SVGRenderer exports empty document", "[svg_export]") {
    DDFDocument doc;
    SVGRenderer renderer;
    
    std::string svg = renderer.render(doc);
    
    REQUIRE(!svg.empty());
    REQUIRE(is_valid_svg_header(svg));
    REQUIRE(is_valid_svg_closing(svg));
}

TEST_CASE("SVGRenderer exports document with metadata", "[svg_export]") {
    DDFDocument doc;
    doc.set_metadata("title", "Test Diagram");
    doc.set_metadata("author", "Test User");
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<metadata>"));
    REQUIRE(contains_substring(svg, "Test Diagram"));
    REQUIRE(contains_substring(svg, "Test User"));
    REQUIRE(contains_substring(svg, "</metadata>"));
}

// ============================================================================
// Shape Export Tests
// ============================================================================

TEST_CASE("SVGRenderer exports rectangle shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 100.0f;
    rect.geometry["y"] = 200.0f;
    rect.geometry["width"] = 120.0f;
    rect.geometry["height"] = 60.0f;
    rect.inline_style["fill"] = "#3498db";
    rect.inline_style["stroke"] = "#2c3e50";
    rect.inline_style["stroke-width"] = "2";
    doc.shape_layer().add_shape(rect);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<rect"));
    REQUIRE(contains_substring(svg, "id=\"rect1\""));
    REQUIRE(contains_substring(svg, "x=\"100\""));
    REQUIRE(contains_substring(svg, "y=\"200\""));
    REQUIRE(contains_substring(svg, "width=\"120\""));
    REQUIRE(contains_substring(svg, "height=\"60\""));
    REQUIRE(contains_substring(svg, "fill: #3498db"));
    REQUIRE(contains_substring(svg, "stroke: #2c3e50"));
}

TEST_CASE("SVGRenderer exports circle shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 150.0f;
    circle.geometry["cy"] = 150.0f;
    circle.geometry["r"] = 50.0f;
    circle.inline_style["fill"] = "#e74c3c";
    doc.shape_layer().add_shape(circle);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<circle"));
    REQUIRE(contains_substring(svg, "id=\"circle1\""));
    REQUIRE(contains_substring(svg, "cx=\"150\""));
    REQUIRE(contains_substring(svg, "cy=\"150\""));
    REQUIRE(contains_substring(svg, "r=\"50\""));
    REQUIRE(contains_substring(svg, "fill: #e74c3c"));
}

TEST_CASE("SVGRenderer exports ellipse shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape ellipse;
    ellipse.id = "ellipse1";
    ellipse.type = "ellipse";
    ellipse.geometry["cx"] = 200.0f;
    ellipse.geometry["cy"] = 100.0f;
    ellipse.geometry["rx"] = 80.0f;
    ellipse.geometry["ry"] = 40.0f;
    ellipse.inline_style["fill"] = "#2ecc71";
    doc.shape_layer().add_shape(ellipse);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<ellipse"));
    REQUIRE(contains_substring(svg, "id=\"ellipse1\""));
    REQUIRE(contains_substring(svg, "cx=\"200\""));
    REQUIRE(contains_substring(svg, "cy=\"100\""));
    REQUIRE(contains_substring(svg, "rx=\"80\""));
    REQUIRE(contains_substring(svg, "ry=\"40\""));
}

TEST_CASE("SVGRenderer exports text shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape text;
    text.id = "text1";
    text.type = "text";
    text.geometry["x"] = 50.0f;
    text.geometry["y"] = 50.0f;
    text.text = "Hello World";
    text.inline_style["font-size"] = "16";
    text.inline_style["fill"] = "#000000";
    doc.shape_layer().add_shape(text);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<text"));
    REQUIRE(contains_substring(svg, "id=\"text1\""));
    REQUIRE(contains_substring(svg, "x=\"50\""));
    REQUIRE(contains_substring(svg, "y=\"50\""));
    REQUIRE(contains_substring(svg, "Hello World"));
    REQUIRE(contains_substring(svg, "</text>"));
}

TEST_CASE("SVGRenderer exports line shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape line;
    line.id = "line1";
    line.type = "line";
    line.geometry["x1"] = 0.0f;
    line.geometry["y1"] = 0.0f;
    line.geometry["x2"] = 100.0f;
    line.geometry["y2"] = 100.0f;
    line.inline_style["stroke"] = "#000000";
    line.inline_style["stroke-width"] = "2";
    doc.shape_layer().add_shape(line);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<line"));
    REQUIRE(contains_substring(svg, "id=\"line1\""));
    REQUIRE(contains_substring(svg, "x1=\"0\""));
    REQUIRE(contains_substring(svg, "y1=\"0\""));
    REQUIRE(contains_substring(svg, "x2=\"100\""));
    REQUIRE(contains_substring(svg, "y2=\"100\""));
}

TEST_CASE("SVGRenderer exports polygon shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape polygon;
    polygon.id = "polygon1";
    polygon.type = "polygon";
    polygon.text = "0,0 100,0 50,100";  // Triangle
    polygon.inline_style["fill"] = "#9b59b6";
    doc.shape_layer().add_shape(polygon);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<polygon"));
    REQUIRE(contains_substring(svg, "id=\"polygon1\""));
    REQUIRE(contains_substring(svg, "points=\"0,0 100,0 50,100\""));
}

TEST_CASE("SVGRenderer exports polyline shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape polyline;
    polyline.id = "polyline1";
    polyline.type = "polyline";
    polyline.text = "0,0 50,25 100,0 150,25";
    polyline.inline_style["stroke"] = "#34495e";
    polyline.inline_style["fill"] = "none";
    doc.shape_layer().add_shape(polyline);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<polyline"));
    REQUIRE(contains_substring(svg, "id=\"polyline1\""));
    REQUIRE(contains_substring(svg, "points=\"0,0 50,25 100,0 150,25\""));
}

TEST_CASE("SVGRenderer exports path shape", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape path;
    path.id = "path1";
    path.type = "path";
    path.text = "M 10 10 L 90 90 L 10 90 Z";  // Path data stored in text field
    path.inline_style["fill"] = "#f39c12";
    doc.shape_layer().add_shape(path);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<path"));
    REQUIRE(contains_substring(svg, "id=\"path1\""));
    REQUIRE(contains_substring(svg, "d=\"M 10 10 L 90 90 L 10 90 Z\""));
}

// ============================================================================
// Shape with Classes and Styles Tests
// ============================================================================

TEST_CASE("SVGRenderer exports shape with CSS classes", "[svg_export][styles]") {
    DDFDocument doc;
    
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 50.0f;
    rect.classes.push_back("card");
    rect.classes.push_back("highlighted");
    doc.shape_layer().add_shape(rect);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "class=\"card highlighted\""));
}

TEST_CASE("SVGRenderer exports shape with transform", "[svg_export][transforms]") {
    DDFDocument doc;
    
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 50.0f;
    rect.transform = {1.0f, 0.0f, 0.0f, 1.0f, 100.0f, 200.0f};  // Translation
    doc.shape_layer().add_shape(rect);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "transform=\"matrix(1,0,0,1,100,200)\""));
}

TEST_CASE("SVGRenderer exports rectangle with rounded corners", "[svg_export][shapes]") {
    DDFDocument doc;
    
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 50.0f;
    rect.geometry["rx"] = 10.0f;
    rect.geometry["ry"] = 10.0f;
    doc.shape_layer().add_shape(rect);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "rx=\"10\""));
    REQUIRE(contains_substring(svg, "ry=\"10\""));
}

// ============================================================================
// Group Export Tests
// ============================================================================

TEST_CASE("SVGRenderer exports group with children", "[svg_export][groups]") {
    DDFDocument doc;
    
    // Create parent group
    Shape group;
    group.id = "group1";
    group.type = "group";
    group.child_shape_ids.push_back("rect1");
    group.child_shape_ids.push_back("circle1");
    doc.shape_layer().add_shape(group);
    
    // Create child shapes
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 50.0f;
    rect.geometry["height"] = 50.0f;
    rect.parent_shape_id = "group1";
    doc.shape_layer().add_shape(rect);
    
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 75.0f;
    circle.geometry["cy"] = 25.0f;
    circle.geometry["r"] = 20.0f;
    circle.parent_shape_id = "group1";
    doc.shape_layer().add_shape(circle);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<g"));
    REQUIRE(contains_substring(svg, "id=\"group1\""));
    REQUIRE(contains_substring(svg, "</g>"));
    REQUIRE(contains_substring(svg, "id=\"rect1\""));
    REQUIRE(contains_substring(svg, "id=\"circle1\""));
}

TEST_CASE("SVGRenderer exports nested groups", "[svg_export][groups]") {
    DDFDocument doc;
    
    // Create parent group
    Shape parent_group;
    parent_group.id = "parent";
    parent_group.type = "group";
    parent_group.child_shape_ids.push_back("child_group");
    doc.shape_layer().add_shape(parent_group);
    
    // Create child group
    Shape child_group;
    child_group.id = "child_group";
    child_group.type = "group";
    child_group.parent_shape_id = "parent";
    child_group.child_shape_ids.push_back("rect1");
    doc.shape_layer().add_shape(child_group);
    
    // Create shape in child group
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 50.0f;
    rect.geometry["height"] = 50.0f;
    rect.parent_shape_id = "child_group";
    doc.shape_layer().add_shape(rect);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    // Should have nested <g> tags
    REQUIRE(count_occurrences(svg, "<g") >= 2);
    REQUIRE(count_occurrences(svg, "</g>") >= 2);
}

// ============================================================================
// Boolean Group Export Tests
// ============================================================================

TEST_CASE("SVGRenderer exports boolean subtract group", "[svg_export][boolean]") {
    DDFDocument doc;
    
    // Create boolean group
    Shape bool_group;
    bool_group.id = "bool1";
    bool_group.type = "boolean_group";
    bool_group.inline_style["boolean-operation"] = "subtract";
    bool_group.child_shape_ids.push_back("rect1");
    bool_group.child_shape_ids.push_back("circle1");
    doc.shape_layer().add_shape(bool_group);
    
    // Create shapes
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 100.0f;
    rect.parent_shape_id = "bool1";
    doc.shape_layer().add_shape(rect);
    
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 50.0f;
    circle.geometry["cy"] = 50.0f;
    circle.geometry["r"] = 30.0f;
    circle.parent_shape_id = "bool1";
    doc.shape_layer().add_shape(circle);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<mask"));
    REQUIRE(contains_substring(svg, "bool1_mask"));
    REQUIRE(contains_substring(svg, "fill: white"));
    REQUIRE(contains_substring(svg, "fill: black"));
}

TEST_CASE("SVGRenderer exports boolean intersect group", "[svg_export][boolean]") {
    DDFDocument doc;
    
    // Create boolean group
    Shape bool_group;
    bool_group.id = "bool1";
    bool_group.type = "boolean_group";
    bool_group.inline_style["boolean-operation"] = "intersect";
    bool_group.child_shape_ids.push_back("rect1");
    bool_group.child_shape_ids.push_back("circle1");
    doc.shape_layer().add_shape(bool_group);
    
    // Create shapes
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 100.0f;
    rect.parent_shape_id = "bool1";
    doc.shape_layer().add_shape(rect);
    
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 50.0f;
    circle.geometry["cy"] = 50.0f;
    circle.geometry["r"] = 30.0f;
    circle.parent_shape_id = "bool1";
    doc.shape_layer().add_shape(circle);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<clipPath"));
    REQUIRE(contains_substring(svg, "bool1_clip"));
}

// ============================================================================
// Connector Export Tests
// ============================================================================

TEST_CASE("SVGRenderer exports connector with straight path", "[svg_export][connectors]") {
    DDFDocument doc;
    
    // Create shapes
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    shape1.geometry["x"] = 0.0f;
    shape1.geometry["y"] = 0.0f;
    shape1.geometry["width"] = 50.0f;
    shape1.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(shape1);
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "rect";
    shape2.geometry["x"] = 200.0f;
    shape2.geometry["y"] = 0.0f;
    shape2.geometry["width"] = 50.0f;
    shape2.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(shape2);
    
    // Create connector
    Connector conn;
    conn.id = "conn1";
    conn.from.shape_id = "shape1";
    conn.from.connection_point_id = "right";
    conn.to.shape_id = "shape2";
    conn.to.connection_point_id = "left";
    conn.path_points = {50.0f, 25.0f, 200.0f, 25.0f};  // Straight line
    conn.style["stroke"] = "#000000";
    conn.style["stroke-width"] = "2";
    doc.connector_layer().add_connector(conn);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<path"));
    REQUIRE(contains_substring(svg, "id=\"conn1\""));
    REQUIRE(contains_substring(svg, "d=\"M 50"));
    REQUIRE(contains_substring(svg, "L 200"));
}

TEST_CASE("SVGRenderer exports connector with arrow markers", "[svg_export][connectors]") {
    DDFDocument doc;
    
    // Create shapes
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    shape1.geometry["x"] = 0.0f;
    shape1.geometry["y"] = 0.0f;
    shape1.geometry["width"] = 50.0f;
    shape1.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(shape1);
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "rect";
    shape2.geometry["x"] = 200.0f;
    shape2.geometry["y"] = 0.0f;
    shape2.geometry["width"] = 50.0f;
    shape2.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(shape2);
    
    // Create connector with arrows
    Connector conn;
    conn.id = "conn1";
    conn.from.shape_id = "shape1";
    conn.from.connection_point_id = "right";
    conn.to.shape_id = "shape2";
    conn.to.connection_point_id = "left";
    conn.path_points = {50.0f, 25.0f, 200.0f, 25.0f};
    conn.arrow_start = ArrowType::Circle;
    conn.arrow_end = ArrowType::Arrow;
    conn.style["stroke"] = "#e74c3c";
    doc.connector_layer().add_connector(conn);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<defs>"));
    REQUIRE(contains_substring(svg, "<marker"));
    REQUIRE(contains_substring(svg, "arrow_start_conn1"));
    REQUIRE(contains_substring(svg, "arrow_end_conn1"));
    REQUIRE(contains_substring(svg, "marker-start=\"url(#arrow_start_conn1)\""));
    REQUIRE(contains_substring(svg, "marker-end=\"url(#arrow_end_conn1)\""));
}

TEST_CASE("SVGRenderer exports connector with label", "[svg_export][connectors]") {
    DDFDocument doc;
    
    // Create shapes
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    shape1.geometry["x"] = 0.0f;
    shape1.geometry["y"] = 0.0f;
    shape1.geometry["width"] = 50.0f;
    shape1.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(shape1);
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "rect";
    shape2.geometry["x"] = 200.0f;
    shape2.geometry["y"] = 0.0f;
    shape2.geometry["width"] = 50.0f;
    shape2.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(shape2);
    
    // Create connector with label
    Connector conn;
    conn.id = "conn1";
    conn.from.shape_id = "shape1";
    conn.from.connection_point_id = "right";
    conn.to.shape_id = "shape2";
    conn.to.connection_point_id = "left";
    conn.path_points = {50.0f, 25.0f, 200.0f, 25.0f};
    
    ConnectorLabel label;
    label.text = "connects to";
    label.position = 0.5f;
    label.offset_x = 0.0f;
    label.offset_y = -10.0f;
    conn.label = label;
    
    doc.connector_layer().add_connector(conn);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "<text"));
    REQUIRE(contains_substring(svg, "connects to"));
    REQUIRE(contains_substring(svg, "text-anchor=\"middle\""));
}

// ============================================================================
// Complex Document Export Tests
// ============================================================================

TEST_CASE("SVGRenderer exports complex document with multiple shapes", "[svg_export][complex]") {
    DDFDocument doc;
    doc.set_metadata("title", "Complex Diagram");
    
    // Add multiple shapes
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 50.0f;
    rect.inline_style["fill"] = "#3498db";
    doc.shape_layer().add_shape(rect);
    
    Shape circle;
    circle.id = "circle1";
    circle.type = "circle";
    circle.geometry["cx"] = 200.0f;
    circle.geometry["cy"] = 25.0f;
    circle.geometry["r"] = 25.0f;
    circle.inline_style["fill"] = "#e74c3c";
    doc.shape_layer().add_shape(circle);
    
    Shape text;
    text.id = "text1";
    text.type = "text";
    text.geometry["x"] = 50.0f;
    text.geometry["y"] = 100.0f;
    text.text = "Test";
    doc.shape_layer().add_shape(text);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(is_valid_svg_header(svg));
    REQUIRE(is_valid_svg_closing(svg));
    REQUIRE(contains_substring(svg, "Complex Diagram"));
    REQUIRE(contains_substring(svg, "id=\"rect1\""));
    REQUIRE(contains_substring(svg, "id=\"circle1\""));
    REQUIRE(contains_substring(svg, "id=\"text1\""));
}

// ============================================================================
// XML Escaping Tests
// ============================================================================

TEST_CASE("SVGRenderer escapes special XML characters", "[svg_export][escaping]") {
    DDFDocument doc;
    
    Shape text;
    text.id = "text1";
    text.type = "text";
    text.geometry["x"] = 0.0f;
    text.geometry["y"] = 0.0f;
    text.text = "Test <>&\"' characters";
    doc.shape_layer().add_shape(text);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    REQUIRE(contains_substring(svg, "&lt;"));
    REQUIRE(contains_substring(svg, "&gt;"));
    REQUIRE(contains_substring(svg, "&amp;"));
    REQUIRE(contains_substring(svg, "&quot;"));
    REQUIRE(contains_substring(svg, "&apos;"));
}

// ============================================================================
// SVG Validation Tests
// ============================================================================

TEST_CASE("SVGRenderer generates well-formed SVG", "[svg_export][validation]") {
    DDFDocument doc;
    
    // Add various shapes
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(rect);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    // Check for balanced tags
    int svg_open = count_occurrences(svg, "<svg");
    int svg_close = count_occurrences(svg, "</svg>");
    REQUIRE(svg_open == svg_close);
    
    // Check for proper XML declaration
    REQUIRE(svg.find("<?xml") == 0);
    
    // Check for required namespaces
    REQUIRE(contains_substring(svg, "xmlns=\"http://www.w3.org/2000/svg\""));
}

TEST_CASE("SVGRenderer handles empty connector path", "[svg_export][connectors]") {
    DDFDocument doc;
    
    // Create connector with empty path
    Connector conn;
    conn.id = "conn1";
    conn.from.shape_id = "shape1";
    conn.to.shape_id = "shape2";
    // path_points is empty
    doc.connector_layer().add_connector(conn);
    
    SVGRenderer renderer;
    std::string svg = renderer.render(doc);
    
    // Should not crash and should not include the connector
    REQUIRE(is_valid_svg_header(svg));
    REQUIRE(is_valid_svg_closing(svg));
}

TEST_CASE("SVGRenderer exports to file", "[svg_export][file]") {
    DDFDocument doc;
    doc.set_metadata("title", "File Export Test");
    
    Shape rect;
    rect.id = "rect1";
    rect.type = "rect";
    rect.geometry["x"] = 0.0f;
    rect.geometry["y"] = 0.0f;
    rect.geometry["width"] = 100.0f;
    rect.geometry["height"] = 50.0f;
    doc.shape_layer().add_shape(rect);
    
    // Export to file
    std::string filepath = "test_export.svg";
    bool success = doc.export_to_svg_file(filepath);
    REQUIRE(success);
    
    // Verify file exists and contains valid SVG
    std::ifstream file(filepath);
    REQUIRE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    REQUIRE(is_valid_svg_header(content));
    REQUIRE(contains_substring(content, "File Export Test"));
    REQUIRE(contains_substring(content, "id=\"rect1\""));
    
    // Clean up
    std::remove(filepath.c_str());
}
