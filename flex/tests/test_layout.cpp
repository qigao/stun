/*
 * Flex Layout System Tests
 * Tests Flexbox layout parsing from DSL and runtime behavior
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "flex/flex_ast.h"
#include "flex/flex_parser.h"
#include "flex/group.h"
#include "flex/shape.h"
#include "flex/types.h"

using namespace flex;
using namespace flex::parser;
using Catch::Matchers::WithinAbs;

// ============================================================================
// DSL PARSING TESTS: Layout Properties
// ============================================================================

TEST_CASE("Parser: Layout mode property", "[layout][parser]") {
    auto program = parse(R"(
        scene LayoutTest {
            group container {
                layout: flex
                width: 400, height: 300
            }
        }
    )");

    REQUIRE(program != nullptr);
    REQUIRE(program->scene != nullptr);
    REQUIRE(program->scene->children.size() == 1);

    auto& container = program->scene->children[0];
    REQUIRE(container->type == "group");
    REQUIRE(std::get<std::string>(container->properties["layout"]) == "flex");
}

TEST_CASE("Parser: Flex direction property", "[layout][parser]") {
    SECTION("Row direction") {
        auto program = parse(R"(
            scene Test {
                group row {
                    layout: flex
                    flexDirection: row
                }
            }
        )");

        REQUIRE(program != nullptr);
        auto& group = program->scene->children[0];
        REQUIRE(std::get<std::string>(group->properties["flexDirection"]) == "row");
    }

    SECTION("Column direction") {
        auto program = parse(R"(
            scene Test {
                group column {
                    layout: flex
                    flexDirection: column
                }
            }
        )");

        REQUIRE(program != nullptr);
        auto& group = program->scene->children[0];
        REQUIRE(std::get<std::string>(group->properties["flexDirection"]) == "column");
    }
}

TEST_CASE("Parser: Justify content property", "[layout][parser]") {
    auto program = parse(R"(
        scene Test {
            group centered {
                layout: flex
                justifyContent: center
            }
            group spaced {
                layout: flex
                justifyContent: spaceBetween
            }
        }
    )");

    REQUIRE(program != nullptr);
    REQUIRE(program->scene->children.size() == 2);

    auto& centered = program->scene->children[0];
    REQUIRE(std::get<std::string>(centered->properties["justifyContent"]) == "center");

    auto& spaced = program->scene->children[1];
    REQUIRE(std::get<std::string>(spaced->properties["justifyContent"]) == "spaceBetween");
}

TEST_CASE("Parser: Align items property", "[layout][parser]") {
    auto program = parse(R"(
        scene Test {
            group container {
                layout: flex
                alignItems: center
            }
        }
    )");

    REQUIRE(program != nullptr);
    auto& container = program->scene->children[0];
    REQUIRE(std::get<std::string>(container->properties["alignItems"]) == "center");
}

TEST_CASE("Parser: Gap property", "[layout][parser]") {
    auto program = parse(R"(
        scene Test {
            group container {
                layout: flex
                gap: 20
            }
        }
    )");

    REQUIRE(program != nullptr);
    auto& container = program->scene->children[0];
    REQUIRE(std::get<float>(container->properties["gap"]) == 20.0f);
}

TEST_CASE("Parser: Flex wrap property", "[layout][parser]") {
    auto program = parse(R"(
        scene Test {
            group wrapped {
                layout: flex
                flexWrap: wrap
            }
            group nowrap {
                layout: flex
                flexWrap: nowrap
            }
        }
    )");

    REQUIRE(program != nullptr);
    REQUIRE(program->scene->children.size() == 2);

    auto& wrapped = program->scene->children[0];
    REQUIRE(std::get<std::string>(wrapped->properties["flexWrap"]) == "wrap");

    auto& nowrap = program->scene->children[1];
    REQUIRE(std::get<std::string>(nowrap->properties["flexWrap"]) == "nowrap");
}

TEST_CASE("Parser: Align self property", "[layout][parser]") {
    auto program = parse(R"(
        scene Test {
            group container {
                layout: flex

                rect item1 { alignSelf: start, width: 50 }
                rect item2 { alignSelf: center, width: 50 }
                rect item3 { alignSelf: end, width: 50 }
            }
        }
    )");

    REQUIRE(program != nullptr);
    auto& container = program->scene->children[0];
    REQUIRE(container->children.size() == 3);

    REQUIRE(std::get<std::string>(container->children[0]->properties["alignSelf"]) == "start");
    REQUIRE(std::get<std::string>(container->children[1]->properties["alignSelf"]) == "center");
    REQUIRE(std::get<std::string>(container->children[2]->properties["alignSelf"]) == "end");
}

TEST_CASE("Parser: Padding property", "[layout][parser]") {
    auto program = parse(R"(
        scene Test {
            group uniform {
                layout: flex
                padding: 20
            }
            group individual {
                layout: flex
                paddingTop: 10
                paddingRight: 20
                paddingBottom: 30
                paddingLeft: 40
            }
        }
    )");

    REQUIRE(program != nullptr);
    REQUIRE(program->scene->children.size() == 2);

    auto& uniform = program->scene->children[0];
    REQUIRE(std::get<float>(uniform->properties["padding"]) == 20.0f);

    auto& individual = program->scene->children[1];
    REQUIRE(std::get<float>(individual->properties["paddingTop"]) == 10.0f);
    REQUIRE(std::get<float>(individual->properties["paddingRight"]) == 20.0f);
    REQUIRE(std::get<float>(individual->properties["paddingBottom"]) == 30.0f);
    REQUIRE(std::get<float>(individual->properties["paddingLeft"]) == 40.0f);
}

TEST_CASE("Parser: Space evenly justify content", "[layout][parser]") {
    auto program = parse(R"(
        scene Test {
            group container {
                layout: flex
                justifyContent: spaceEvenly
            }
        }
    )");

    REQUIRE(program != nullptr);
    auto& container = program->scene->children[0];
    REQUIRE(std::get<std::string>(container->properties["justifyContent"]) == "spaceEvenly");
}

TEST_CASE("Parser: Complete layout configuration", "[layout][parser]") {
    auto program = parse(R"(
        scene CompleteLayout {
            group flexContainer {
                layout: flex
                flexDirection: row
                justifyContent: spaceBetween
                alignItems: center
                gap: 15
                width: 500, height: 100

                rect item1 { width: 80, height: 40, fill: #ff0000 }
                rect item2 { width: 80, height: 60, fill: #00ff00 }
                rect item3 { width: 80, height: 50, fill: #0000ff }
            }
        }
    )");

    REQUIRE(program != nullptr);
    REQUIRE(program->scene != nullptr);

    auto& container = program->scene->children[0];
    REQUIRE(container->type == "group");
    REQUIRE(container->id == "flexContainer");

    // Verify all layout properties parsed correctly
    REQUIRE(std::get<std::string>(container->properties["layout"]) == "flex");
    REQUIRE(std::get<std::string>(container->properties["flexDirection"]) == "row");
    REQUIRE(std::get<std::string>(container->properties["justifyContent"]) == "spaceBetween");
    REQUIRE(std::get<std::string>(container->properties["alignItems"]) == "center");
    REQUIRE(std::get<float>(container->properties["gap"]) == 15.0f);
    REQUIRE(std::get<float>(container->properties["width"]) == 500.0f);
    REQUIRE(std::get<float>(container->properties["height"]) == 100.0f);

    // Verify children
    REQUIRE(container->children.size() == 3);
    REQUIRE(container->children[0]->id == "item1");
    REQUIRE(container->children[1]->id == "item2");
    REQUIRE(container->children[2]->id == "item3");
}

// ============================================================================
// RUNTIME TESTS: Group Layout Methods
// ============================================================================

TEST_CASE("Group: Layout mode setting", "[layout][runtime]") {
    auto group = Group::create();

    REQUIRE(group->layout() == LayoutMode::None);

    group->set_layout(LayoutMode::Flex);
    REQUIRE(group->layout() == LayoutMode::Flex);
}

TEST_CASE("Group: Flex direction setting", "[layout][runtime]") {
    auto group = Group::create();

    REQUIRE(group->flex_direction() == FlexDirection::Row);

    group->set_flex_direction(FlexDirection::Column);
    REQUIRE(group->flex_direction() == FlexDirection::Column);

    group->set_flex_direction(FlexDirection::RowReverse);
    REQUIRE(group->flex_direction() == FlexDirection::RowReverse);

    group->set_flex_direction(FlexDirection::ColumnReverse);
    REQUIRE(group->flex_direction() == FlexDirection::ColumnReverse);
}

TEST_CASE("Group: Justify content setting", "[layout][runtime]") {
    auto group = Group::create();

    REQUIRE(group->justify_content() == JustifyContent::Start);

    group->set_justify_content(JustifyContent::Center);
    REQUIRE(group->justify_content() == JustifyContent::Center);

    group->set_justify_content(JustifyContent::End);
    REQUIRE(group->justify_content() == JustifyContent::End);

    group->set_justify_content(JustifyContent::SpaceBetween);
    REQUIRE(group->justify_content() == JustifyContent::SpaceBetween);

    group->set_justify_content(JustifyContent::SpaceAround);
    REQUIRE(group->justify_content() == JustifyContent::SpaceAround);
}

TEST_CASE("Group: Align items setting", "[layout][runtime]") {
    auto group = Group::create();

    REQUIRE(group->align_items() == AlignItems::Start);

    group->set_align_items(AlignItems::Center);
    REQUIRE(group->align_items() == AlignItems::Center);

    group->set_align_items(AlignItems::End);
    REQUIRE(group->align_items() == AlignItems::End);

    group->set_align_items(AlignItems::Stretch);
    REQUIRE(group->align_items() == AlignItems::Stretch);
}

TEST_CASE("Group: Gap setting", "[layout][runtime]") {
    auto group = Group::create();

    REQUIRE(group->gap() == 0.0f);

    group->set_gap(10.0f);
    REQUIRE(group->gap() == 10.0f);

    group->set_gap(25.5f);
    REQUIRE(group->gap() == 25.5f);
}

TEST_CASE("Group: Padding setting", "[layout][runtime]") {
    auto group = Group::create();

    SECTION("Uniform padding") {
        group->set_padding(20.0f);
        REQUIRE(group->padding_top() == 20.0f);
        REQUIRE(group->padding_right() == 20.0f);
        REQUIRE(group->padding_bottom() == 20.0f);
        REQUIRE(group->padding_left() == 20.0f);
    }

    SECTION("Individual padding") {
        group->set_padding(10.0f, 20.0f, 30.0f, 40.0f);
        REQUIRE(group->padding_top() == 10.0f);
        REQUIRE(group->padding_right() == 20.0f);
        REQUIRE(group->padding_bottom() == 30.0f);
        REQUIRE(group->padding_left() == 40.0f);
    }
}

TEST_CASE("Group: Flex wrap setting", "[layout][runtime]") {
    auto group = Group::create();

    REQUIRE(group->flex_wrap() == FlexWrap::NoWrap);

    group->set_flex_wrap(FlexWrap::Wrap);
    REQUIRE(group->flex_wrap() == FlexWrap::Wrap);

    group->set_flex_wrap(FlexWrap::NoWrap);
    REQUIRE(group->flex_wrap() == FlexWrap::NoWrap);
}

TEST_CASE("Node: Align self setting", "[layout][runtime]") {
    auto shape = Shape::create();

    REQUIRE(shape->align_self() == AlignSelf::Auto);

    shape->set_align_self(AlignSelf::Start);
    REQUIRE(shape->align_self() == AlignSelf::Start);

    shape->set_align_self(AlignSelf::Center);
    REQUIRE(shape->align_self() == AlignSelf::Center);

    shape->set_align_self(AlignSelf::End);
    REQUIRE(shape->align_self() == AlignSelf::End);

    shape->set_align_self(AlignSelf::Stretch);
    REQUIRE(shape->align_self() == AlignSelf::Stretch);
}

// ============================================================================
// LAYOUT ALGORITHM TESTS
// ============================================================================

// Helper to create a rect shape with size
static Shape::Ptr make_rect(float w, float h) {
    auto shape = Shape::create();
    shape->set_rect(w, h);
    return shape;
}

TEST_CASE("Layout: Row direction positions children horizontally", "[layout][algorithm]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Row);
    container->set_layout_size(300, 100);

    // Add three children with known sizes
    auto child1 = make_rect(50, 30);
    auto child2 = make_rect(50, 30);
    auto child3 = make_rect(50, 30);

    container->add_child(child1);
    container->add_child(child2);
    container->add_child(child3);

    container->perform_layout();

    // Children should be positioned horizontally
    // child1 at x=0, child2 at x=50, child3 at x=100
    REQUIRE(child1->x() == 0.0f);
    REQUIRE(child2->x() == 50.0f);
    REQUIRE(child3->x() == 100.0f);

    // All children should be at y=0 (start alignment)
    REQUIRE(child1->y() == 0.0f);
    REQUIRE(child2->y() == 0.0f);
    REQUIRE(child3->y() == 0.0f);
}

TEST_CASE("Layout: Column direction positions children vertically", "[layout][algorithm]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Column);
    container->set_layout_size(100, 300);

    auto child1 = make_rect(50, 30);
    auto child2 = make_rect(50, 40);
    auto child3 = make_rect(50, 30);

    container->add_child(child1);
    container->add_child(child2);
    container->add_child(child3);

    container->perform_layout();

    // Children should be positioned vertically
    REQUIRE(child1->y() == 0.0f);
    REQUIRE(child2->y() == 30.0f);
    REQUIRE(child3->y() == 70.0f);

    // All children should be at x=0 (start alignment)
    REQUIRE(child1->x() == 0.0f);
    REQUIRE(child2->x() == 0.0f);
    REQUIRE(child3->x() == 0.0f);
}

TEST_CASE("Layout: Gap adds spacing between children", "[layout][algorithm]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Row);
    container->set_gap(10.0f);
    container->set_layout_size(400, 100);

    auto child1 = make_rect(50, 30);
    auto child2 = make_rect(50, 30);
    auto child3 = make_rect(50, 30);

    container->add_child(child1);
    container->add_child(child2);
    container->add_child(child3);

    container->perform_layout();

    // Children should be positioned with gap
    // child1 at x=0, child2 at x=60 (50+10), child3 at x=120 (50+10+50+10)
    REQUIRE(child1->x() == 0.0f);
    REQUIRE(child2->x() == 60.0f);
    REQUIRE(child3->x() == 120.0f);
}

TEST_CASE("Layout: Justify content center", "[layout][algorithm]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Row);
    container->set_justify_content(JustifyContent::Center);
    container->set_layout_size(300, 100);

    auto child1 = make_rect(50, 30);
    auto child2 = make_rect(50, 30);

    container->add_child(child1);
    container->add_child(child2);

    container->perform_layout();

    // Total content width = 100, container width = 300
    // Free space = 200, offset = 100
    // child1 at x=100, child2 at x=150
    REQUIRE(child1->x() == 100.0f);
    REQUIRE(child2->x() == 150.0f);
}

TEST_CASE("Layout: Justify content space-between", "[layout][algorithm]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Row);
    container->set_justify_content(JustifyContent::SpaceBetween);
    container->set_layout_size(300, 100);

    auto child1 = make_rect(50, 30);
    auto child2 = make_rect(50, 30);
    auto child3 = make_rect(50, 30);

    container->add_child(child1);
    container->add_child(child2);
    container->add_child(child3);

    container->perform_layout();

    // Total content width = 150, container width = 300
    // Free space = 150, gap = 75 (150 / 2)
    // child1 at x=0, child2 at x=125 (50+75), child3 at x=250 (50+75+50+75)
    REQUIRE(child1->x() == 0.0f);
    REQUIRE_THAT(child2->x(), WithinAbs(125.0f, 0.1f));
    REQUIRE_THAT(child3->x(), WithinAbs(250.0f, 0.1f));
}

TEST_CASE("Layout: Align items center", "[layout][algorithm]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Row);
    container->set_align_items(AlignItems::Center);
    container->set_layout_size(300, 100);

    auto child1 = make_rect(50, 20);
    auto child2 = make_rect(50, 40);
    auto child3 = make_rect(50, 60);

    container->add_child(child1);
    container->add_child(child2);
    container->add_child(child3);

    container->perform_layout();

    // Container height = 100
    // child1 (h=20): y = (100-20)/2 = 40
    // child2 (h=40): y = (100-40)/2 = 30
    // child3 (h=60): y = (100-60)/2 = 20
    REQUIRE_THAT(child1->y(), WithinAbs(40.0f, 0.1f));
    REQUIRE_THAT(child2->y(), WithinAbs(30.0f, 0.1f));
    REQUIRE_THAT(child3->y(), WithinAbs(20.0f, 0.1f));
}

TEST_CASE("Layout: Align items end", "[layout][algorithm]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Row);
    container->set_align_items(AlignItems::End);
    container->set_layout_size(300, 100);

    auto child1 = make_rect(50, 20);
    auto child2 = make_rect(50, 40);
    auto child3 = make_rect(50, 60);

    container->add_child(child1);
    container->add_child(child2);
    container->add_child(child3);

    container->perform_layout();

    // Container height = 100
    // child1 (h=20): y = 100 - 20 = 80
    // child2 (h=40): y = 100 - 40 = 60
    // child3 (h=60): y = 100 - 60 = 40
    REQUIRE_THAT(child1->y(), WithinAbs(80.0f, 0.1f));
    REQUIRE_THAT(child2->y(), WithinAbs(60.0f, 0.1f));
    REQUIRE_THAT(child3->y(), WithinAbs(40.0f, 0.1f));
}

// ============================================================================
// NESTED LAYOUT TESTS
// ============================================================================

TEST_CASE("Layout: Nested flex containers", "[layout][nested]") {
    // Outer container (column)
    auto outer = Group::create();
    outer->set_layout(LayoutMode::Flex);
    outer->set_flex_direction(FlexDirection::Column);
    outer->set_gap(10.0f);
    outer->set_layout_size(200, 200);

    // Inner row 1
    auto row1 = Group::create();
    row1->set_layout(LayoutMode::Flex);
    row1->set_flex_direction(FlexDirection::Row);
    row1->set_gap(5.0f);
    row1->set_layout_size(200, 40);

    auto item1a = make_rect(30, 30);
    auto item1b = make_rect(30, 30);
    row1->add_child(item1a);
    row1->add_child(item1b);

    // Inner row 2
    auto row2 = Group::create();
    row2->set_layout(LayoutMode::Flex);
    row2->set_flex_direction(FlexDirection::Row);
    row2->set_gap(5.0f);
    row2->set_layout_size(200, 40);

    auto item2a = make_rect(30, 30);
    auto item2b = make_rect(30, 30);
    row2->add_child(item2a);
    row2->add_child(item2b);

    outer->add_child(row1);
    outer->add_child(row2);

    // Perform layout on inner containers first
    row1->perform_layout();
    row2->perform_layout();

    // Then outer container
    outer->perform_layout();

    // Check inner layout (row direction with gap)
    REQUIRE(item1a->x() == 0.0f);
    REQUIRE(item1b->x() == 35.0f);  // 30 + 5 gap

    REQUIRE(item2a->x() == 0.0f);
    REQUIRE(item2b->x() == 35.0f);

    // Check outer layout (column direction with gap)
    REQUIRE(row1->y() == 0.0f);
    REQUIRE(row2->y() == 50.0f);  // 40 + 10 gap
}

// ============================================================================
// INTEGRATION TESTS: DSL to Runtime Layout
// ============================================================================

TEST_CASE("Integration: Parse and apply layout from DSL", "[layout][integration]") {
    const char* source = R"(
        scene ToolbarDemo {
            width: 500
            height: 100

            group toolbar {
                layout: flex
                flexDirection: row
                justifyContent: center
                alignItems: center
                gap: 10
                width: 500, height: 60

                rect btn1 { width: 80, height: 40, fill: #3498db }
                rect btn2 { width: 80, height: 40, fill: #2ecc71 }
                rect btn3 { width: 80, height: 40, fill: #e74c3c }
            }
        }
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->scene != nullptr);

    auto& toolbar = program->scene->children[0];

    // Verify layout properties parsed
    REQUIRE(std::get<std::string>(toolbar->properties["layout"]) == "flex");
    REQUIRE(std::get<std::string>(toolbar->properties["flexDirection"]) == "row");
    REQUIRE(std::get<std::string>(toolbar->properties["justifyContent"]) == "center");
    REQUIRE(std::get<std::string>(toolbar->properties["alignItems"]) == "center");
    REQUIRE(std::get<float>(toolbar->properties["gap"]) == 10.0f);

    // Verify children
    REQUIRE(toolbar->children.size() == 3);
}

TEST_CASE("Integration: data_binding.flex layout properties", "[layout][integration]") {
    const char* source = R"(
        scene counter {
            width: 400
            height: 300

            group mainLayout {
                layout: flex
                flexDirection: column
                justifyContent: center
                alignItems: center
                x: 0, y: 0
                width: 400, height: 300
                gap: 30

                text title {
                    content: "Data Binding Demo"
                    fontSize: 28
                }

                group buttonRow {
                    layout: flex
                    flexDirection: row
                    justifyContent: center
                    alignItems: center
                    gap: 40
                    width: 400, height: 60

                    group incrementButton {
                        width: 80, height: 45
                    }

                    group decrementButton {
                        width: 80, height: 45
                    }
                }
            }
        }
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);

    auto& mainLayout = program->scene->children[0];

    // Main layout properties
    REQUIRE(std::get<std::string>(mainLayout->properties["layout"]) == "flex");
    REQUIRE(std::get<std::string>(mainLayout->properties["flexDirection"]) == "column");
    REQUIRE(std::get<std::string>(mainLayout->properties["justifyContent"]) == "center");
    REQUIRE(std::get<std::string>(mainLayout->properties["alignItems"]) == "center");
    REQUIRE(std::get<float>(mainLayout->properties["gap"]) == 30.0f);

    // Button row properties
    auto& buttonRow = mainLayout->children[1];
    REQUIRE(buttonRow->id == "buttonRow");
    REQUIRE(std::get<std::string>(buttonRow->properties["layout"]) == "flex");
    REQUIRE(std::get<std::string>(buttonRow->properties["flexDirection"]) == "row");
    REQUIRE(std::get<float>(buttonRow->properties["gap"]) == 40.0f);

    // Button row children
    REQUIRE(buttonRow->children.size() == 2);
    REQUIRE(buttonRow->children[0]->id == "incrementButton");
    REQUIRE(buttonRow->children[1]->id == "decrementButton");
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_CASE("Layout: Empty container", "[layout][edge]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_layout_size(100, 100);

    // Should not crash with no children
    REQUIRE_NOTHROW(container->perform_layout());
}

TEST_CASE("Layout: Single child", "[layout][edge]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::Flex);
    container->set_flex_direction(FlexDirection::Row);
    container->set_justify_content(JustifyContent::Center);
    container->set_align_items(AlignItems::Center);
    container->set_layout_size(200, 100);

    auto child = make_rect(50, 30);
    container->add_child(child);

    container->perform_layout();

    // Single child should be centered
    REQUIRE_THAT(child->x(), WithinAbs(75.0f, 0.1f));  // (200-50)/2
    REQUIRE_THAT(child->y(), WithinAbs(35.0f, 0.1f));  // (100-30)/2
}

TEST_CASE("Layout: No layout mode (manual positioning)", "[layout][edge]") {
    auto container = Group::create();
    container->set_layout(LayoutMode::None);  // Default
    container->set_layout_size(200, 100);

    auto child1 = make_rect(50, 30);
    auto child2 = make_rect(50, 30);

    child1->set_x(10);
    child1->set_y(20);
    child2->set_x(100);
    child2->set_y(50);

    container->add_child(child1);
    container->add_child(child2);

    container->perform_layout();

    // Children should retain manual positions
    REQUIRE(child1->x() == 10.0f);
    REQUIRE(child1->y() == 20.0f);
    REQUIRE(child2->x() == 100.0f);
    REQUIRE(child2->y() == 50.0f);
}
