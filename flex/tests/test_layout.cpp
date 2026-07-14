/*
 * Flex Layout System Tests
 * Tests Flexbox layout parsing from DSL and runtime behavior
 */

#include "tinytest.h"

#include "flex/dsl.h"
#include "flex/core.h"
#include "flex/lowering.h"
#include "backends/renderer.h"
#include "flex/dsl/parser.h"
#include "flex/core/renderer.h"
#include "flex/core/scene.h"
#include "flex/lowering/ast_to_runtime.h"
#include "flex.h"
#include "backends/thorvg/init.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include "backends/d2d/init.h"
#endif

using namespace flex;
using namespace flex::parser;

namespace {

std::unique_ptr<Renderer> null_renderer_factory(CanvasHandle) {
  return nullptr;
}

struct WithinAbsMatcher {
  float expected;
  float epsilon;
};

inline WithinAbsMatcher WithinAbs(float expected, float epsilon) {
  return {expected, epsilon};
}

inline void check_close(float actual, float expected, float epsilon = 0.001f) {
  check_float_eq(actual, expected, epsilon);
}

struct TempWorkspace {
  std::filesystem::path root;

  TempWorkspace() {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root = std::filesystem::temp_directory_path() /
           ("flex_dsl_tests_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
  }

  ~TempWorkspace() {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }

  std::filesystem::path write_file(const std::string& relative_path,
                                   const std::string& contents) {
    auto path = root / relative_path;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << contents;
    return path;
  }
};

inline Shape* make_rect(ArenaAllocator& arena, float w, float h) {
  auto shape = Shape::create(arena);
  shape->set_rect(w, h);
  return shape;
}

#define REQUIRE(expr) check(expr)
#define REQUIRE_FALSE(expr) check_false(expr)
#define REQUIRE_THAT(actual, matcher) \
  check_close((actual), (matcher).expected, (matcher).epsilon)
#define REQUIRE_NOTHROW(expr) check_nothrow(expr)
#define TEST_CASE(description, tags) it(description)

} // namespace

suite("flex::layout") {

// ============================================================================
// RENDERER BACKEND TESTS
// ============================================================================

TEST_CASE("Renderer factory: backend registry", "[renderer][backend]") {
  REQUIRE(std::string(renderer_backend_name(RendererBackend::ThorVG)) == "ThorVG");
  REQUIRE_FALSE(is_renderer_backend_available(RendererBackend::ThorVG));
  REQUIRE_FALSE(is_renderer_backend_available(RendererBackend::Custom));
  REQUIRE(default_renderer_factory() == nullptr);

  REQUIRE(thorvg_backend::register_backend());
  REQUIRE(is_renderer_backend_available(RendererBackend::ThorVG));
  REQUIRE(default_renderer_factory() != nullptr);

  auto previous = register_renderer_backend(RendererBackend::Custom, &null_renderer_factory);
  REQUIRE(previous == nullptr);
  REQUIRE(is_renderer_backend_available(RendererBackend::Custom));
  REQUIRE(create_renderer(RendererBackend::Custom, nullptr) == nullptr);

  auto registered = register_renderer_backend(RendererBackend::Custom, previous);
  REQUIRE(registered == &null_renderer_factory);
  REQUIRE_FALSE(is_renderer_backend_available(RendererBackend::Custom));
}

#ifdef _WIN32
TEST_CASE("Renderer factory: direct2d becomes default on Windows", "[renderer][backend][d2d]") {
  auto previous_default = set_default_renderer_factory(nullptr);
  auto previous_thorvg = register_renderer_backend(RendererBackend::ThorVG, nullptr);
  auto previous_d2d = register_renderer_backend(RendererBackend::Direct2D, nullptr);

  REQUIRE(thorvg_backend::register_backend());
  REQUIRE(default_renderer_factory() == renderer_backend_factory(RendererBackend::ThorVG));

  REQUIRE(d2d_backend::register_backend());
  REQUIRE(default_renderer_factory() == renderer_backend_factory(RendererBackend::Direct2D));

  register_renderer_backend(RendererBackend::Direct2D, previous_d2d);
  register_renderer_backend(RendererBackend::ThorVG, previous_thorvg);
  set_default_renderer_factory(previous_default);
}
#endif

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

  auto &container = program->scene->children[0];
  REQUIRE(container->type == "group");
  REQUIRE(std::get<std::string>(container->properties["layout"]) == "flex");
}

group("Parser: Flex direction property") {
  it("Row direction") {
    auto program = parse(R"(
            scene Test {
                group row {
                    layout: flex
                    flexDirection: row
                }
            }
        )");

    REQUIRE(program != nullptr);
    auto &group = program->scene->children[0];
    REQUIRE(std::get<std::string>(group->properties["flexDirection"]) == "row");
  }

  it("Column direction") {
    auto program = parse(R"(
            scene Test {
                group column {
                    layout: flex
                    flexDirection: column
                }
            }
        )");

    REQUIRE(program != nullptr);
    auto &group = program->scene->children[0];
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

  auto &centered = program->scene->children[0];
  REQUIRE(std::get<std::string>(centered->properties["justifyContent"]) == "center");

  auto &spaced = program->scene->children[1];
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
  auto &container = program->scene->children[0];
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
  auto &container = program->scene->children[0];
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

  auto &wrapped = program->scene->children[0];
  REQUIRE(std::get<std::string>(wrapped->properties["flexWrap"]) == "wrap");

  auto &nowrap = program->scene->children[1];
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
  auto &container = program->scene->children[0];
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

  auto &uniform = program->scene->children[0];
  REQUIRE(std::get<float>(uniform->properties["padding"]) == 20.0f);

  auto &individual = program->scene->children[1];
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
  auto &container = program->scene->children[0];
  REQUIRE(std::get<std::string>(container->properties["justifyContent"]) == "spaceEvenly");
}

TEST_CASE("Definition: Component prop bindings load and apply", "[layout][binding][component]") {
  auto comp = Component::create("TestComponentBindingLayout");
  comp->add_prop("width", 10.0f);
  comp->add_prop("height", 10.0f);
  comp->set_builder([](const Props& props) {
    auto shape = std::make_shared<Shape>();
    float w = get_prop_float(props, "width", 10.0f);
    float h = get_prop_float(props, "height", 10.0f);
    shape->set_rect(w, h);
    shape->set_layout_width(w);
    shape->set_layout_height(h);
    return shape;
  });
  ComponentRegistry::instance().register_component(comp);

  const char* source = R"(
        scene BindingDemo {
        TestComponentBindingLayout badge {
            x: ${$posX}
            y: ${$posY}
            width: ${$badgeW}
            height: ${$badgeH}
        }

            rect box {
                width: ${$boxW}
                height: ${$boxH}
            }
        }
    )";

  auto def = Definition::load(source);
  REQUIRE(def != nullptr);
  REQUIRE_FALSE(def->has_error());

  auto instance = Instance::create(def);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->scene() != nullptr);

  instance->set_input("badgeW", 40.0f);
  instance->set_input("badgeH", 12.0f);
  instance->set_input("boxW", 30.0f);
  instance->set_input("boxH", 14.0f);
  instance->set_input("posX", 10.0f);
  instance->set_input("posY", 20.0f);
  instance->advance(0.0f);

  auto* badge = instance->scene()->find("badge");
  REQUIRE(badge != nullptr);
  auto* badge_shape = dynamic_cast<Shape*>(badge);
  REQUIRE(badge_shape != nullptr);

  auto* box = dynamic_cast<Shape*>(instance->scene()->find("box"));
  REQUIRE(box != nullptr);
  REQUIRE_THAT(box->layout_width(), WithinAbs(30.0f, 0.001f));
  REQUIRE_THAT(box->layout_height(), WithinAbs(14.0f, 0.001f));
  REQUIRE_THAT(box->rect().width, WithinAbs(30.0f, 0.001f));
  REQUIRE_THAT(box->rect().height, WithinAbs(14.0f, 0.001f));

  instance->set_input("badgeW", 60.0f);
  instance->advance(0.0f);

  auto* badge_after = instance->scene()->find("badge");
  REQUIRE(badge_after != nullptr);
  REQUIRE(badge_after != badge);
  instance->set_input("posX", 12.0f);
  instance->set_input("posY", 24.0f);
  instance->advance(0.0f);

  badge_after = instance->scene()->find("badge");
  REQUIRE(badge_after != nullptr);
  REQUIRE_THAT(badge_after->x(), WithinAbs(12.0f, 0.001f));
  REQUIRE_THAT(badge_after->y(), WithinAbs(24.0f, 0.001f));
  auto* badge_shape_after = dynamic_cast<Shape*>(badge_after);
  REQUIRE(badge_shape_after != nullptr);
  REQUIRE_THAT(badge_shape_after->rect().width, WithinAbs(60.0f, 0.001f));
  REQUIRE_THAT(badge_shape_after->layout_width(), WithinAbs(60.0f, 0.001f));
  REQUIRE_THAT(badge_shape_after->layout_height(), WithinAbs(12.0f, 0.001f));
}

TEST_CASE("Definition: Component prop priority over node props", "[layout][binding][component]") {
  auto comp = Component::create("TestComponentPropPriority");
  comp->add_prop("x", 8.0f);
  comp->set_builder([](const Props& props) {
    auto shape = std::make_shared<Shape>();
    float w = get_prop_float(props, "x", 8.0f);
    shape->set_rect(w, 5.0f);
    shape->set_layout_width(w);
    shape->set_layout_height(5.0f);
    return shape;
  });
  ComponentRegistry::instance().register_component(comp);

  const char* source = R"(
        scene PriorityDemo {
            TestComponentPropPriority badge {
                x: ${$xInput}
            }
        }
    )";

  auto def = Definition::load(source);
  REQUIRE(def != nullptr);
  REQUIRE_FALSE(def->has_error());

  auto instance = Instance::create(def);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->scene() != nullptr);

  instance->set_input("xInput", 42.0f);
  instance->advance(0.0f);

  auto* badge = instance->scene()->find("badge");
  REQUIRE(badge != nullptr);
  REQUIRE_THAT(badge->x(), WithinAbs(0.0f, 0.001f));
  auto* badge_shape = dynamic_cast<Shape*>(badge);
  REQUIRE(badge_shape != nullptr);
  REQUIRE_THAT(badge_shape->rect().width, WithinAbs(42.0f, 0.001f));

  instance->set_input("xInput", 60.0f);
  instance->advance(0.0f);

  auto* badge_after = instance->scene()->find("badge");
  REQUIRE(badge_after != nullptr);
  auto* badge_shape_after = dynamic_cast<Shape*>(badge_after);
  REQUIRE(badge_shape_after != nullptr);
  REQUIRE_THAT(badge_shape_after->rect().width, WithinAbs(60.0f, 0.001f));
}

TEST_CASE("Definition: Component prop type mismatch fails load", "[layout][binding][component]") {
  auto comp = Component::create("TestComponentPropTypeMismatch");
  comp->add_prop("width", 10.0f);
  comp->set_builder([](const Props& props) {
    auto shape = std::make_shared<Shape>();
    float w = get_prop_float(props, "width", 10.0f);
    shape->set_rect(w, 5.0f);
    shape->set_layout_width(w);
    shape->set_layout_height(5.0f);
    return shape;
  });
  ComponentRegistry::instance().register_component(comp);

  const char* source = R"(
        scene TypeMismatchDemo {
            TestComponentPropTypeMismatch badge {
                width: "oops"
            }
        }
    )";

  auto def = Definition::load(source);
  REQUIRE(def != nullptr);
  REQUIRE(def->has_error());
}

TEST_CASE("Definition: DSL component bindings support local components and $() syntax",
          "[layout][binding][component][dsl]") {
  const char *source = R"(
        component Badge {
            width: 10
            height: 12
            fill: #ff6600

            rect icon {
                width: $width
                height: $height
                fill: $fill
            }
        }

        scene LocalComponentDemo {
            Badge badge {
                x: $(posX)
                width: $(badgeW)
                fill: $(badgeColor)
            }

            rect box {
                width: $(boxW)
                height: $(boxH)
            }
        }
    )";

  auto def = Definition::load(source);
  REQUIRE(def != nullptr);
  REQUIRE_FALSE(def->has_error());

  auto instance = Instance::create(def);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->scene() != nullptr);

  instance->set_input("badgeW", 40.0f);
  instance->set_input("boxW", 30.0f);
  instance->set_input("boxH", 14.0f);
  instance->set_input("posX", 15.0f);
  instance->set_input("badgeColor", "#00ff00");
  instance->advance(0.0f);

  auto *badge = dynamic_cast<Shape *>(instance->scene()->find("badge"));
  REQUIRE(badge != nullptr);
  REQUIRE_THAT(badge->x(), WithinAbs(15.0f, 0.001f));
  REQUIRE_THAT(badge->rect().width, WithinAbs(40.0f, 0.001f));
  REQUIRE_THAT(badge->rect().height, WithinAbs(12.0f, 0.001f));
  REQUIRE(badge->fill().color.to_rgba32() == Color::from_hex("#00ff00").to_rgba32());

  auto *box = dynamic_cast<Shape *>(instance->scene()->find("box"));
  REQUIRE(box != nullptr);
  REQUIRE_THAT(box->layout_width(), WithinAbs(30.0f, 0.001f));
  REQUIRE_THAT(box->layout_height(), WithinAbs(14.0f, 0.001f));
  REQUIRE_THAT(box->rect().width, WithinAbs(30.0f, 0.001f));
  REQUIRE_THAT(box->rect().height, WithinAbs(14.0f, 0.001f));

  instance->set_input("badgeW", 60.0f);
  instance->set_input("posX", 18.0f);
  instance->advance(0.0f);

  auto *badge_after = dynamic_cast<Shape *>(instance->scene()->find("badge"));
  REQUIRE(badge_after != nullptr);
  REQUIRE(badge_after != badge);
  REQUIRE_THAT(badge_after->x(), WithinAbs(18.0f, 0.001f));
  REQUIRE_THAT(badge_after->rect().width, WithinAbs(60.0f, 0.001f));
  REQUIRE_THAT(badge_after->rect().height, WithinAbs(12.0f, 0.001f));
}

TEST_CASE("Definition: Pseudo-class styles lower without overwriting base style",
          "[layout][pseudo][dsl]") {
  const char *source = R"(
        scene PseudoDemo {
            rect badge {
                fill: #111111
                :hover {
                    fill: #222222
                }
            }
        }
    )";

  auto def = Definition::load(source);
  REQUIRE(def != nullptr);
  REQUIRE_FALSE(def->has_error());
  REQUIRE(def->scene() != nullptr);

  auto *badge = dynamic_cast<Shape *>(def->scene()->find("badge"));
  REQUIRE(badge != nullptr);
  REQUIRE(badge->fill().color.to_rgba32() == Color::from_hex("#111111").to_rgba32());

  auto *styles = badge->pseudo_class_styles();
  REQUIRE(styles != nullptr);
  REQUIRE(styles->count(":hover") == 1);

  auto hover = styles->find(":hover");
  hover->second.apply_to(badge);
  REQUIRE(badge->fill().color.to_rgba32() == Color::from_hex("#222222").to_rgba32());
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

  auto &container = program->scene->children[0];
  REQUIRE(container->type == "group");
  REQUIRE(container->id == "flexContainer");

  REQUIRE(std::get<std::string>(container->properties["layout"]) == "flex");
  REQUIRE(std::get<std::string>(container->properties["flexDirection"]) == "row");
  REQUIRE(std::get<std::string>(container->properties["justifyContent"]) == "spaceBetween");
  REQUIRE(std::get<std::string>(container->properties["alignItems"]) == "center");
  REQUIRE(std::get<float>(container->properties["gap"]) == 15.0f);
  REQUIRE(std::get<float>(container->properties["width"]) == 500.0f);
  REQUIRE(std::get<float>(container->properties["height"]) == 100.0f);

  REQUIRE(container->children.size() == 3);
  REQUIRE(container->children[0]->id == "item1");
  REQUIRE(container->children[1]->id == "item2");
  REQUIRE(container->children[2]->id == "item3");
}

// ============================================================================
// RUNTIME TESTS: Group Layout Methods
// ============================================================================

TEST_CASE("Group: Layout mode setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto group = Group::create(arena);

  REQUIRE(group->layout() == LayoutMode::None);

  group->set_layout(LayoutMode::Flex);
  REQUIRE(group->layout() == LayoutMode::Flex);
}

TEST_CASE("Group: Flex direction setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto group = Group::create(arena);

  REQUIRE(group->flex_direction() == FlexDirection::Row);

  group->set_flex_direction(FlexDirection::Column);
  REQUIRE(group->flex_direction() == FlexDirection::Column);

  group->set_flex_direction(FlexDirection::RowReverse);
  REQUIRE(group->flex_direction() == FlexDirection::RowReverse);

  group->set_flex_direction(FlexDirection::ColumnReverse);
  REQUIRE(group->flex_direction() == FlexDirection::ColumnReverse);
}

TEST_CASE("Group: Justify content setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto group = Group::create(arena);

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
  ArenaAllocator arena(4096);
  auto group = Group::create(arena);

  REQUIRE(group->align_items() == AlignItems::Start);

  group->set_align_items(AlignItems::Center);
  REQUIRE(group->align_items() == AlignItems::Center);

  group->set_align_items(AlignItems::End);
  REQUIRE(group->align_items() == AlignItems::End);

  group->set_align_items(AlignItems::Stretch);
  REQUIRE(group->align_items() == AlignItems::Stretch);
}

TEST_CASE("Group: Gap setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto group = Group::create(arena);

  REQUIRE(group->gap() == 0.0f);

  group->set_gap(10.0f);
  REQUIRE(group->gap() == 10.0f);

  group->set_gap(25.5f);
  REQUIRE(group->gap() == 25.5f);
}

group("Group: Padding setting") {
  it("Uniform padding") {
    ArenaAllocator arena(4096);
    auto group = Group::create(arena);

    group->set_padding(20.0f);
    REQUIRE(group->padding_top() == 20.0f);
    REQUIRE(group->padding_right() == 20.0f);
    REQUIRE(group->padding_bottom() == 20.0f);
    REQUIRE(group->padding_left() == 20.0f);
  }

  it("Individual padding") {
    ArenaAllocator arena(4096);
    auto group = Group::create(arena);

    group->set_padding(10.0f, 20.0f, 30.0f, 40.0f);
    REQUIRE(group->padding_top() == 10.0f);
    REQUIRE(group->padding_right() == 20.0f);
    REQUIRE(group->padding_bottom() == 30.0f);
    REQUIRE(group->padding_left() == 40.0f);
  }
}

TEST_CASE("Group: Flex wrap setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto group = Group::create(arena);

  REQUIRE(group->flex_wrap() == FlexWrap::NoWrap);

  group->set_flex_wrap(FlexWrap::Wrap);
  REQUIRE(group->flex_wrap() == FlexWrap::Wrap);

  group->set_flex_wrap(FlexWrap::NoWrap);
  REQUIRE(group->flex_wrap() == FlexWrap::NoWrap);
}

TEST_CASE("Node: Align self setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto shape = Shape::create(arena);

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

TEST_CASE("Layout: Row direction positions children horizontally", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_layout_size(300, 100);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 30);
  auto child3 = make_rect(arena, 50, 30);

  container->add_child(child1);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE(child1->x() == 0.0f);
  REQUIRE(child2->x() == 50.0f);
  REQUIRE(child3->x() == 100.0f);

  REQUIRE(child1->y() == 0.0f);
  REQUIRE(child2->y() == 0.0f);
  REQUIRE(child3->y() == 0.0f);
}

TEST_CASE("Layout: Column direction positions children vertically", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Column);
  container->set_layout_size(100, 300);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 40);
  auto child3 = make_rect(arena, 50, 30);

  container->add_child(child1);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE(child1->y() == 0.0f);
  REQUIRE(child2->y() == 30.0f);
  REQUIRE(child3->y() == 70.0f);

  REQUIRE(child1->x() == 0.0f);
  REQUIRE(child2->x() == 0.0f);
  REQUIRE(child3->x() == 0.0f);
}

TEST_CASE("Layout: Gap adds spacing between children", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_gap(10.0f);
  container->set_layout_size(400, 100);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 30);
  auto child3 = make_rect(arena, 50, 30);

  container->add_child(child1);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE(child1->x() == 0.0f);
  REQUIRE(child2->x() == 60.0f);
  REQUIRE(child3->x() == 120.0f);
}

TEST_CASE("Layout: Justify content center", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_justify_content(JustifyContent::Center);
  container->set_layout_size(300, 100);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 30);

  container->add_child(child1);
  container->add_child(child2);

  container->perform_layout();

  REQUIRE(child1->x() == 100.0f);
  REQUIRE(child2->x() == 150.0f);
}

TEST_CASE("Layout: Justify content space-between", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_justify_content(JustifyContent::SpaceBetween);
  container->set_layout_size(300, 100);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 30);
  auto child3 = make_rect(arena, 50, 30);

  container->add_child(child1);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE(child1->x() == 0.0f);
  REQUIRE_THAT(child2->x(), WithinAbs(125.0f, 0.1f));
  REQUIRE_THAT(child3->x(), WithinAbs(250.0f, 0.1f));
}

TEST_CASE("Layout: Align items center", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_align_items(AlignItems::Center);
  container->set_layout_size(300, 100);

  auto child1 = make_rect(arena, 50, 20);
  auto child2 = make_rect(arena, 50, 40);
  auto child3 = make_rect(arena, 50, 60);

  container->add_child(child1);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE_THAT(child1->y(), WithinAbs(40.0f, 0.1f));
  REQUIRE_THAT(child2->y(), WithinAbs(30.0f, 0.1f));
  REQUIRE_THAT(child3->y(), WithinAbs(20.0f, 0.1f));
}

TEST_CASE("Layout: Align items end", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_align_items(AlignItems::End);
  container->set_layout_size(300, 100);

  auto child1 = make_rect(arena, 50, 20);
  auto child2 = make_rect(arena, 50, 40);
  auto child3 = make_rect(arena, 50, 60);

  container->add_child(child1);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE_THAT(child1->y(), WithinAbs(80.0f, 0.1f));
  REQUIRE_THAT(child2->y(), WithinAbs(60.0f, 0.1f));
  REQUIRE_THAT(child3->y(), WithinAbs(40.0f, 0.1f));
}

// ============================================================================
// NESTED LAYOUT TESTS
// ============================================================================

TEST_CASE("Layout: Nested flex containers", "[layout][nested]") {
  ArenaAllocator arena(8192);
  
  auto outer = Group::create(arena);
  outer->set_layout(LayoutMode::Flex);
  outer->set_flex_direction(FlexDirection::Column);
  outer->set_gap(10.0f);
  outer->set_layout_size(200, 200);

  auto row1 = Group::create(arena);
  row1->set_layout(LayoutMode::Flex);
  row1->set_flex_direction(FlexDirection::Row);
  row1->set_gap(5.0f);
  row1->set_layout_size(200, 40);

  auto item1a = make_rect(arena, 30, 30);
  auto item1b = make_rect(arena, 30, 30);
  row1->add_child(item1a);
  row1->add_child(item1b);

  auto row2 = Group::create(arena);
  row2->set_layout(LayoutMode::Flex);
  row2->set_flex_direction(FlexDirection::Row);
  row2->set_gap(5.0f);
  row2->set_layout_size(200, 40);

  auto item2a = make_rect(arena, 30, 30);
  auto item2b = make_rect(arena, 30, 30);
  row2->add_child(item2a);
  row2->add_child(item2b);

  outer->add_child(row1);
  outer->add_child(row2);

  row1->perform_layout();
  row2->perform_layout();
  outer->perform_layout();

  REQUIRE(item1a->x() == 0.0f);
  REQUIRE(item1b->x() == 35.0f);

  REQUIRE(item2a->x() == 0.0f);
  REQUIRE(item2b->x() == 35.0f);

  REQUIRE(row1->y() == 0.0f);
  REQUIRE(row2->y() == 50.0f);
}

TEST_CASE("Layout: Auto-sized nested flex groups contribute intrinsic main size", "[layout][nested]") {
  ArenaAllocator arena(8192);

  auto outer = Group::create(arena);
  outer->set_layout(LayoutMode::Flex);
  outer->set_flex_direction(FlexDirection::Column);
  outer->set_gap(5.0f);
  outer->set_layout_size(200.0f, 200.0f);

  auto inner = Group::create(arena);
  inner->set_layout(LayoutMode::Flex);
  inner->set_flex_direction(FlexDirection::Column);
  inner->set_gap(10.0f);
  inner->set_layout_width(100.0f);

  auto top = make_rect(arena, 50.0f, 30.0f);
  auto bottom = make_rect(arena, 50.0f, 30.0f);
  inner->add_child(top);
  inner->add_child(bottom);

  auto sibling = make_rect(arena, 40.0f, 20.0f);

  outer->add_child(inner);
  outer->add_child(sibling);
  outer->perform_layout();

  REQUIRE_THAT(sibling->y(), WithinAbs(75.0f, 0.001f));

  inner->perform_layout();
  REQUIRE_THAT(bottom->y(), WithinAbs(40.0f, 0.001f));
  REQUIRE_THAT(inner->bounds().height, WithinAbs(70.0f, 0.001f));
}

TEST_CASE("FlexLayoutEngine measures nested auto-sized groups without stale bounds", "[layout][nested][engine]") {
  ArenaAllocator arena(8192);

  auto outer = Group::create(arena);
  outer->set_layout_size(200.0f, 200.0f);

  auto inner = Group::create(arena);
  inner->set_layout(LayoutMode::Flex);
  inner->set_flex_direction(FlexDirection::Column);
  inner->set_gap(10.0f);
  inner->set_layout_width(100.0f);

  auto top = make_rect(arena, 50.0f, 30.0f);
  auto bottom = make_rect(arena, 50.0f, 30.0f);
  inner->add_child(top);
  inner->add_child(bottom);

  auto sibling = make_rect(arena, 40.0f, 20.0f);
  outer->add_child(inner);
  outer->add_child(sibling);

  FlexLayoutEngine layout(outer);
  layout.set_direction(FlexDirection::Column);
  layout.set_gap(5.0f);
  layout.layout(200.0f, 200.0f);

  REQUIRE_THAT(sibling->y(), WithinAbs(75.0f, 0.001f));
}

// ============================================================================
// INTEGRATION TESTS: DSL to Runtime Layout
// ============================================================================

TEST_CASE("Integration: Parse and apply layout from DSL", "[layout][integration]") {
  const char *source = R"(
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

  auto &toolbar = program->scene->children[0];

  REQUIRE(std::get<std::string>(toolbar->properties["layout"]) == "flex");
  REQUIRE(std::get<std::string>(toolbar->properties["flexDirection"]) == "row");
  REQUIRE(std::get<std::string>(toolbar->properties["justifyContent"]) == "center");
  REQUIRE(std::get<std::string>(toolbar->properties["alignItems"]) == "center");
  REQUIRE(std::get<float>(toolbar->properties["gap"]) == 10.0f);

  REQUIRE(toolbar->children.size() == 3);
}

TEST_CASE("Integration: data_binding.flex layout properties", "[layout][integration]") {
  const char *source = R"(
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

  auto &mainLayout = program->scene->children[0];

  REQUIRE(std::get<std::string>(mainLayout->properties["layout"]) == "flex");
  REQUIRE(std::get<std::string>(mainLayout->properties["flexDirection"]) == "column");
  REQUIRE(std::get<std::string>(mainLayout->properties["justifyContent"]) == "center");
  REQUIRE(std::get<std::string>(mainLayout->properties["alignItems"]) == "center");
  REQUIRE(std::get<float>(mainLayout->properties["gap"]) == 30.0f);

  auto &buttonRow = mainLayout->children[1];
  REQUIRE(buttonRow->id == "buttonRow");
  REQUIRE(std::get<std::string>(buttonRow->properties["layout"]) == "flex");
  REQUIRE(std::get<std::string>(buttonRow->properties["flexDirection"]) == "row");
  REQUIRE(std::get<float>(buttonRow->properties["gap"]) == 40.0f);

  REQUIRE(buttonRow->children.size() == 2);
  REQUIRE(buttonRow->children[0]->id == "incrementButton");
  REQUIRE(buttonRow->children[1]->id == "decrementButton");
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_CASE("Layout: Empty container", "[layout][edge]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_layout_size(100, 100);

  REQUIRE_NOTHROW(container->perform_layout());
}

TEST_CASE("Layout: Single child", "[layout][edge]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_justify_content(JustifyContent::Center);
  container->set_align_items(AlignItems::Center);
  container->set_layout_size(200, 100);

  auto child = make_rect(arena, 50, 30);
  container->add_child(child);

  container->perform_layout();

  REQUIRE_THAT(child->x(), WithinAbs(75.0f, 0.1f));
  REQUIRE_THAT(child->y(), WithinAbs(35.0f, 0.1f));
}

TEST_CASE("Layout: No layout mode (manual positioning)", "[layout][edge]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::None);
  container->set_layout_size(200, 100);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 30);

  child1->set_x(10);
  child1->set_y(20);
  child2->set_x(100);
  child2->set_y(50);

  container->add_child(child1);
  container->add_child(child2);

  container->perform_layout();

  REQUIRE(child1->x() == 10.0f);
  REQUIRE(child1->y() == 20.0f);
  REQUIRE(child2->x() == 100.0f);
  REQUIRE(child2->y() == 50.0f);
}

// ============================================================================
// POSITION ABSOLUTE TESTS
// ============================================================================

TEST_CASE("Parser: Position absolute property", "[layout][parser]") {
  auto program = parse(R"(
        scene Test {
            group container {
                layout: flex
                flexDirection: row
                width: 400, height: 100

                rect background {
                    width: 400, height: 100
                    fill: #ffffff
                    position: absolute
                }
                rect item1 { width: 50, height: 30, fill: #ff0000 }
                rect item2 { width: 50, height: 30, fill: #00ff00 }
            }
        }
    )");

  REQUIRE(program != nullptr);
  auto &container = program->scene->children[0];
  REQUIRE(container->children.size() == 3);

  auto &background = container->children[0];
  REQUIRE(std::get<std::string>(background->properties["position"]) == "absolute");
}

TEST_CASE("Node: Position absolute setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto shape = Shape::create(arena);

  REQUIRE(shape->position_absolute() == false);

  shape->set_position_absolute(true);
  REQUIRE(shape->position_absolute() == true);

  shape->set_position_absolute(false);
  REQUIRE(shape->position_absolute() == false);
}

TEST_CASE("Layout: Position absolute excludes from flex layout", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_layout_size(300, 100);

  auto background = make_rect(arena, 300, 100);
  background->set_position_absolute(true);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 30);

  container->add_child(background);
  container->add_child(child1);
  container->add_child(child2);

  container->perform_layout();

  REQUIRE(background->x() == 0.0f);
  REQUIRE(background->y() == 0.0f);

  REQUIRE(child1->x() == 0.0f);
  REQUIRE(child2->x() == 50.0f);
}

TEST_CASE("Layout: Multiple absolute positioned children", "[layout][algorithm]") {
  ArenaAllocator arena(8192);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_gap(10.0f);
  container->set_layout_size(300, 100);

  auto bg1 = make_rect(arena, 300, 100);
  bg1->set_position_absolute(true);
  bg1->set_x(0);
  bg1->set_y(0);

  auto bg2 = make_rect(arena, 280, 80);
  bg2->set_position_absolute(true);
  bg2->set_x(10);
  bg2->set_y(10);

  auto child1 = make_rect(arena, 50, 30);
  auto child2 = make_rect(arena, 50, 30);
  auto child3 = make_rect(arena, 50, 30);

  container->add_child(bg1);
  container->add_child(child1);
  container->add_child(bg2);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE(bg1->x() == 0.0f);
  REQUIRE(bg1->y() == 0.0f);
  REQUIRE(bg2->x() == 10.0f);
  REQUIRE(bg2->y() == 10.0f);

  REQUIRE(child1->x() == 0.0f);
  REQUIRE(child2->x() == 60.0f);
  REQUIRE(child3->x() == 120.0f);
}

TEST_CASE("Layout: Fixed positioned child uses root viewport coordinates",
          "[layout][algorithm][fixed]") {
  ArenaAllocator arena(8192);
  auto scene = Scene::create(800.0f, 600.0f, arena);

  auto container = Group::create(arena);
  container->set_position(100.0f, 50.0f);
  container->set_layout(LayoutMode::Flex);
  container->set_layout_size(200.0f, 100.0f);
  scene->add_child(container);

  auto fixed = make_rect(arena, 40.0f, 30.0f);
  fixed->set_layout_size(40.0f, 30.0f);
  fixed->set_position_mode(PositionMode::Fixed);
  fixed->set_position_offsets(10.0f, NAN, NAN, 15.0f);
  container->add_child(fixed);

  container->perform_layout();

  auto world = fixed->to_world(Vec2(0.0f, 0.0f));
  REQUIRE_THAT(world.x, WithinAbs(15.0f, 0.001f));
  REQUIRE_THAT(world.y, WithinAbs(10.0f, 0.001f));
  REQUIRE_THAT(fixed->layout_width(), WithinAbs(40.0f, 0.001f));
  REQUIRE_THAT(fixed->layout_height(), WithinAbs(30.0f, 0.001f));
}

TEST_CASE("Layout: Fixed positioned percentages resolve against updated scene viewport",
          "[layout][algorithm][fixed]") {
  ArenaAllocator arena(8192);
  auto scene = Scene::create(800.0f, 600.0f, arena);
  scene->set_size(1000.0f, 700.0f);

  auto container = Group::create(arena);
  container->set_position(120.0f, 80.0f);
  container->set_layout(LayoutMode::Flex);
  container->set_layout_size(200.0f, 120.0f);
  scene->add_child(container);

  auto fixed = make_rect(arena, 10.0f, 10.0f);
  fixed->set_position_mode(PositionMode::Fixed);
  fixed->set_width_percent(50.0f);
  fixed->set_height_percent(25.0f);
  fixed->set_position_offsets(0.0f, 10.0f, NAN, NAN);
  container->add_child(fixed);

  container->perform_layout();

  auto world = fixed->to_world(Vec2(0.0f, 0.0f));
  REQUIRE_THAT(fixed->layout_width(), WithinAbs(500.0f, 0.001f));
  REQUIRE_THAT(fixed->layout_height(), WithinAbs(175.0f, 0.001f));
  REQUIRE_THAT(world.x, WithinAbs(490.0f, 0.001f));
  REQUIRE_THAT(world.y, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Layout: Fixed viewport fallback ignores root bounds inflation",
          "[layout][algorithm][fixed]") {
  ArenaAllocator arena(8192);
  auto scene = Scene::create(0.0f, 0.0f, arena);

  auto inflated = make_rect(arena, 900.0f, 500.0f);
  scene->add_child(inflated);

  auto container = Group::create(arena);
  container->set_position(120.0f, 80.0f);
  container->set_layout(LayoutMode::Flex);
  container->set_layout_size(200.0f, 120.0f);
  scene->add_child(container);

  auto fixed = make_rect(arena, 10.0f, 10.0f);
  fixed->set_position_mode(PositionMode::Fixed);
  fixed->set_width_percent(50.0f);
  fixed->set_height_percent(25.0f);
  fixed->set_position_offsets(0.0f, 10.0f, NAN, NAN);
  container->add_child(fixed);

  container->perform_layout();

  auto world = fixed->to_world(Vec2(0.0f, 0.0f));
  REQUIRE_THAT(fixed->layout_width(), WithinAbs(100.0f, 0.001f));
  REQUIRE_THAT(fixed->layout_height(), WithinAbs(30.0f, 0.001f));
  REQUIRE_THAT(world.x, WithinAbs(90.0f, 0.001f));
  REQUIRE_THAT(world.y, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Layout: Fixed percentages ignore root bounds when viewport size is unset",
          "[layout][algorithm][fixed]") {
  ArenaAllocator arena(8192);
  auto scene = Scene::create(800.0f, 600.0f, arena);
  scene->set_size(0.0f, 0.0f);

  auto viewport_sizer = make_rect(arena, 800.0f, 600.0f);
  viewport_sizer->set_layout_size(800.0f, 600.0f);
  scene->add_child(viewport_sizer);

  auto fixed = make_rect(arena, 10.0f, 10.0f);
  fixed->set_position_mode(PositionMode::Fixed);
  fixed->set_width_percent(50.0f);
  fixed->set_height_percent(25.0f);
  fixed->set_position_offsets(NAN, 10.0f, 10.0f, NAN);
  scene->add_child(fixed);

  scene->root()->perform_layout();

  auto world = fixed->to_world(Vec2(0.0f, 0.0f));
  REQUIRE_THAT(world.x, WithinAbs(-10.0f, 0.001f));
  REQUIRE_THAT(world.y, WithinAbs(-10.0f, 0.001f));
}

TEST_CASE("Layout: Fixed children ignore transformed ancestors for world coordinates and hit testing",
          "[layout][algorithm][fixed][events]") {
  ArenaAllocator arena(8192);
  auto scene = Scene::create(400.0f, 300.0f, arena);

  auto container = Group::create(arena);
  container->set_position(120.0f, 70.0f);
  container->set_rotation(90.0f);
  container->set_scale(2.0f, 1.5f);
  container->set_clip(true);
  container->set_layout_size(80.0f, 60.0f);
  scene->add_child(container);

  auto box = make_rect(arena, 30.0f, 20.0f);
  box->set_layout_size(30.0f, 20.0f);
  box->set_position_mode(PositionMode::Fixed);
  box->set_position_offsets(25.0f, NAN, NAN, 15.0f);
  container->add_child(box);
  container->perform_layout();

  auto world = box->to_world(Vec2{0.0f, 0.0f});
  REQUIRE_THAT(world.x, WithinAbs(15.0f, 0.001f));
  REQUIRE_THAT(world.y, WithinAbs(25.0f, 0.001f));

  auto local = box->to_local(Vec2{20.0f, 30.0f});
  REQUIRE_THAT(local.x, WithinAbs(5.0f, 0.001f));
  REQUIRE_THAT(local.y, WithinAbs(5.0f, 0.001f));
}

// ============================================================================
// FLEX GROW/SHRINK/BASIS TESTS
// ============================================================================

TEST_CASE("Parser: Flex grow/shrink/basis properties", "[layout][parser]") {
  auto program = parse(R"(
        scene Test {
            group container {
                layout: flex
                flexDirection: row
                width: 400, height: 100

                rect item1 { width: 50, flexGrow: 1 }
                rect item2 { width: 50, flexGrow: 2, flexShrink: 0 }
                rect item3 { flexBasis: 100, flexGrow: 1 }
            }
        }
    )");

  REQUIRE(program != nullptr);
  auto &container = program->scene->children[0];
  REQUIRE(container->children.size() == 3);

  auto &item1 = container->children[0];
  REQUIRE(std::get<float>(item1->properties["flexGrow"]) == 1.0f);

  auto &item2 = container->children[1];
  REQUIRE(std::get<float>(item2->properties["flexGrow"]) == 2.0f);
  REQUIRE(std::get<float>(item2->properties["flexShrink"]) == 0.0f);

  auto &item3 = container->children[2];
  REQUIRE(std::get<float>(item3->properties["flexBasis"]) == 100.0f);
  REQUIRE(std::get<float>(item3->properties["flexGrow"]) == 1.0f);
}

TEST_CASE("Node: Flex grow/shrink/basis setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto shape = Shape::create(arena);

  REQUIRE(shape->flex_grow() == 0.0f);
  REQUIRE(shape->flex_shrink() == 1.0f);
  REQUIRE(shape->flex_basis() == 0.0f);
  REQUIRE(shape->flex_basis_auto());

  shape->set_flex_grow(2.0f);
  REQUIRE(shape->flex_grow() == 2.0f);

  shape->set_flex_shrink(0.5f);
  REQUIRE(shape->flex_shrink() == 0.5f);

  shape->set_flex_basis(100.0f);
  REQUIRE(shape->flex_basis() == 100.0f);
  REQUIRE_FALSE(shape->flex_basis_auto());

  shape->set_flex(3.0f, 2.0f, 50.0f);
  REQUIRE(shape->flex_grow() == 3.0f);
  REQUIRE(shape->flex_shrink() == 2.0f);
  REQUIRE(shape->flex_basis() == 50.0f);
  REQUIRE_FALSE(shape->flex_basis_auto());
}

TEST_CASE("Layout: Explicit zero flex basis differs from auto", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_layout_size(300, 100);

  auto fixed = make_rect(arena, 100, 30);
  auto flexible = make_rect(arena, 240, 30);
  flexible->set_flex(1.0f, 1.0f, 0.0f);
  flexible->set_flex_basis_auto(false);

  container->add_child(fixed);
  container->add_child(flexible);
  container->perform_layout();

  REQUIRE_THAT(fixed->layout_width(), WithinAbs(100.0f, 0.01f));
  REQUIRE_THAT(flexible->layout_width(), WithinAbs(200.0f, 0.01f));
}

TEST_CASE("Layout: Flex grow distributes extra space", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_layout_size(300, 100);

  auto child1 = make_rect(arena, 50, 30);
  child1->set_flex_grow(1.0f);

  auto child2 = make_rect(arena, 50, 30);
  child2->set_flex_grow(2.0f);

  container->add_child(child1);
  container->add_child(child2);

  container->perform_layout();

  REQUIRE(child1->x() == 0.0f);
  REQUIRE_THAT(child2->x(), WithinAbs(116.67f, 1.0f));
}

TEST_CASE("Layout: Flex shrink when content exceeds container", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_layout_size(100, 50);

  auto child1 = make_rect(arena, 50, 30);
  child1->set_flex_shrink(1.0f);

  auto child2 = make_rect(arena, 50, 30);
  child2->set_flex_shrink(1.0f);

  auto child3 = make_rect(arena, 50, 30);
  child3->set_flex_shrink(1.0f);

  container->add_child(child1);
  container->add_child(child2);
  container->add_child(child3);

  container->perform_layout();

  REQUIRE(child1->x() == 0.0f);
  REQUIRE_THAT(child2->x(), WithinAbs(33.33f, 1.0f));
  REQUIRE_THAT(child3->x(), WithinAbs(66.67f, 1.0f));
}

TEST_CASE("Layout: Flex basis overrides content size", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto container = Group::create(arena);
  container->set_layout(LayoutMode::Flex);
  container->set_flex_direction(FlexDirection::Row);
  container->set_layout_size(300, 100);

  auto child1 = make_rect(arena, 50, 30);
  child1->set_flex_basis(100.0f);

  auto child2 = make_rect(arena, 50, 30);

  container->add_child(child1);
  container->add_child(child2);

  container->perform_layout();

  REQUIRE(child1->x() == 0.0f);
  REQUIRE(child2->x() == 100.0f);
}

// ============================================================================
// ANCHOR PROPERTY TESTS
// ============================================================================

TEST_CASE("Parser: Anchor property", "[layout][parser]") {
  auto program = parse(R"(
        scene Test {
            rect centered { x: 50, y: 50, width: 100, height: 100, anchor: center }
            rect topRight { x: 100, y: 0, width: 50, height: 50, anchor: topRight }
            rect bottom { x: 50, y: 100, width: 50, height: 50, anchor: bottom }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene->children.size() == 3);

  auto &centered = program->scene->children[0];
  REQUIRE(std::get<std::string>(centered->properties["anchor"]) == "center");

  auto &topRight = program->scene->children[1];
  REQUIRE(std::get<std::string>(topRight->properties["anchor"]) == "topRight");

  auto &bottom = program->scene->children[2];
  REQUIRE(std::get<std::string>(bottom->properties["anchor"]) == "bottom");
}

TEST_CASE("Node: Anchor property setting", "[layout][runtime]") {
  ArenaAllocator arena(4096);
  auto shape = Shape::create(arena);

  REQUIRE(shape->anchor() == Anchor::TopLeft);

  shape->set_anchor(Anchor::Center);
  REQUIRE(shape->anchor() == Anchor::Center);

  shape->set_anchor(Anchor::BottomRight);
  REQUIRE(shape->anchor() == Anchor::BottomRight);
}

TEST_CASE("Layout: Anchor center positions element by center point", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto shape = Shape::create(arena);
  shape->set_rect(100, 60);
  shape->set_anchor(Anchor::Center);
  shape->set_position(50, 30);

  auto transform = shape->world_transform();

  REQUIRE_THAT(transform.data[2], WithinAbs(0.0f, 0.1f));
  REQUIRE_THAT(transform.data[5], WithinAbs(0.0f, 0.1f));
}

TEST_CASE("Layout: Anchor topRight positions element by top-right corner", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto shape = Shape::create(arena);
  shape->set_rect(100, 60);
  shape->set_anchor(Anchor::TopRight);
  shape->set_position(100, 0);

  auto transform = shape->world_transform();

  REQUIRE_THAT(transform.data[2], WithinAbs(0.0f, 0.1f));
  REQUIRE_THAT(transform.data[5], WithinAbs(0.0f, 0.1f));
}

TEST_CASE("Layout: Anchor bottom positions element by bottom-center", "[layout][algorithm]") {
  ArenaAllocator arena(4096);
  auto shape = Shape::create(arena);
  shape->set_rect(100, 60);
  shape->set_anchor(Anchor::Bottom);
  shape->set_position(50, 60);

  auto transform = shape->world_transform();

  REQUIRE_THAT(transform.data[2], WithinAbs(0.0f, 0.1f));
  REQUIRE_THAT(transform.data[5], WithinAbs(0.0f, 0.1f));
}

// ============================================================================
// DEFINITION / IMPORT CONTRACT TESTS
// ============================================================================

TEST_CASE("Definition: load ignores imports in raw source", "[definition][import]") {
  TempWorkspace workspace;
  auto main_file = workspace.write_file(
      "main.flex",
      R"(
        import "shared.flex"

        scene Main {
          rect mainRect { width: 10, height: 10 }
        }

        anim "localFade" {
          duration: 0.5
          track "#mainRect/opacity" {
            keyframe 0 -> 0
            keyframe 0.5 -> 1
          }
        }
      )");
  workspace.write_file(
      "shared.flex",
      R"(
        anim "importedFade" {
          duration: 1
          track "#mainRect/x" {
            keyframe 0 -> 0
            keyframe 1 -> 100
          }
        }
      )");

  std::ifstream input(main_file, std::ios::binary);
  std::stringstream buffer;
  buffer << input.rdbuf();

  auto from_source = Definition::load(buffer.str().c_str());
  REQUIRE(from_source != nullptr);
  REQUIRE_FALSE(from_source->has_error());
  REQUIRE(from_source->timelines().size() == 1);

  auto from_file = Definition::load_file(main_file.string().c_str());
  REQUIRE(from_file != nullptr);
  REQUIRE_FALSE(from_file->has_error());
  REQUIRE(from_file->timelines().size() == 2);
}

TEST_CASE("Definition: load_file merges imported runtime objects but not imported scene",
          "[definition][import]") {
  TempWorkspace workspace;
  auto main_file = workspace.write_file(
      "main.flex",
      R"(
        import "shared.flex"

        scene Main {
          Badge badge {
            x: 10
            y: 20
          }

          rect mainRect {
            width: 20
            height: 10
          }
        }
      )");
  workspace.write_file(
      "shared.flex",
      R"(
        component Badge {
          group badgeRoot {
            text label {
              content: "Imported"
            }
          }
        }

        scene Imported {
          rect importedOnly {
            width: 99
            height: 99
          }
        }

        assets {
          image logo: "images/logo.png"
        }

        anim "importedFade" {
          duration: 1
          track "#mainRect/opacity" {
            keyframe 0 -> 0
            keyframe 1 -> 1
          }
        }

        machine importedMachine {
          layer main {
            state idle { initial: true }
          }
        }
      )");

  auto definition = Definition::load_file(main_file.string().c_str());
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());
  REQUIRE(definition->scene() != nullptr);
  REQUIRE(definition->scene()->find("badge") != nullptr);
  REQUIRE(definition->scene()->find("mainRect") != nullptr);
  REQUIRE(definition->scene()->find("importedOnly") == nullptr);
  REQUIRE(definition->timelines().size() == 1);
  REQUIRE(definition->machines().size() == 1);

  auto instance = Instance::create(definition);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->asset_manager() != nullptr);
  auto expected_logo_path = (workspace.root / "images" / "logo.png").generic_string();
  check_eq(std::string(instance->asset_manager()->resolve_path("logo")),
           expected_logo_path);
}

TEST_CASE("Definition: imported constants do not affect main file parse-time properties",
          "[definition][import][const]") {
  TempWorkspace workspace;
  auto main_file = workspace.write_file(
      "main.flex",
      R"(
        import "shared.flex"

        scene Main {
          width: importedWidth
          height: 200
        }
      )");
  workspace.write_file(
      "shared.flex",
      R"(
        const importedWidth = 1024
      )");

  auto definition = Definition::load_file(main_file.string().c_str());
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());
  REQUIRE(definition->scene() != nullptr);

  // Import merge happens after the main file is parsed, so imported constants
  // are not available when scene properties are evaluated.
  REQUIRE_THAT(definition->scene()->width(), WithinAbs(800.0f, 0.001f));
  REQUIRE_THAT(definition->scene()->height(), WithinAbs(200.0f, 0.001f));
}

TEST_CASE("Definition: imported asset paths are rebased to the imported file directory",
          "[definition][import][asset]") {
  TempWorkspace workspace;
  auto main_file = workspace.write_file(
      "main.flex",
      R"(
        import "nested/shared.flex"

        scene Main {
          rect root {
            width: 10
            height: 10
          }
        }
      )");
  workspace.write_file(
      "nested/shared.flex",
      R"(
        assets {
          image logo: "images/logo.png"
        }
      )");

  auto definition = Definition::load_file(main_file.string().c_str());
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());

  auto instance = Instance::create(definition);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->asset_manager() != nullptr);

  auto expected_logo_path =
      (workspace.root / "nested" / "images" / "logo.png").generic_string();
  check_eq(std::string(instance->asset_manager()->resolve_path("logo")),
           expected_logo_path);
}

TEST_CASE("Definition: instances created from one definition isolate scene and bindings",
          "[definition][instance][scene]") {
  auto definition = Definition::load(R"(
        scene Shared {
          rect item {
            x: $offset
            y: 20
            width: 30
            height: 40
          }
        }
      )");
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());

  auto first = Instance::create(definition);
  auto second = Instance::create(definition);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  REQUIRE(first->scene() != definition->scene());
  REQUIRE(second->scene() != definition->scene());
  REQUIRE(first->scene() != second->scene());

  auto* first_item = first->scene()->find("item");
  auto* second_item = second->scene()->find("item");
  auto* definition_item = definition->scene()->find("item");
  REQUIRE(first_item != nullptr);
  REQUIRE(second_item != nullptr);
  REQUIRE(definition_item != nullptr);

  first->set_input("offset", 123.0f);
  second->set_input("offset", 456.0f);
  first->advance(0.0f);
  second->advance(0.0f);

  REQUIRE_THAT(first_item->x(), WithinAbs(123.0f, 0.001f));
  REQUIRE_THAT(second_item->x(), WithinAbs(456.0f, 0.001f));
  REQUIRE_THAT(definition_item->x(), WithinAbs(0.0f, 0.001f));

  first_item->set_x(123.0f);
  REQUIRE_THAT(second_item->x(), WithinAbs(456.0f, 0.001f));
  REQUIRE_THAT(definition_item->x(), WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Definition: DSL machine transitions drive animation params in instances",
          "[definition][machine][instance][animation]") {
  auto definition = Definition::load(R"(
        scene Motion {
          rect box {
            x: 0
            y: 0
            width: 20
            height: 20
          }
        }

        anim "pulse" {
          duration: 4
          track "#box/y" {
            keyframe 0 -> 0
            keyframe 4 -> 100
          }
        }

        machine motionMachine {
          layer main {
            state idle {
              initial: true
            }

            state active {
              play "pulse" with {
                duration: ${dur}
                speed: ${rate}
              }
            }

            transition idle -> active when ${trigger}
          }
        }
      )");
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());
  REQUIRE(definition->scene() != nullptr);
  REQUIRE(definition->timelines().size() == 1);
  REQUIRE(definition->machines().size() == 1);

  auto machine_probe = definition->machines()[0]->clone();
  REQUIRE(machine_probe != nullptr);
  if (!machine_probe) {
    return;
  }

  int callback_count = 0;
  StateChangeInfo captured;
  machine_probe->set_state_change_callback([&](const StateChangeInfo& info) {
    ++callback_count;
    captured = info;
  });
  machine_probe->set_input("trigger", 1.0f);
  machine_probe->update(0.0f);

  check_int_eq(callback_count, 1);
  check_eq(captured.to_state, std::string("active"));
  check_eq(captured.animation, std::string("pulse"));
  check_int_eq(static_cast<int>(captured.animation_params.size()), 2);
  check_eq(captured.animation_params.at("duration"), std::string("dur"));
  check_eq(captured.animation_params.at("speed"), std::string("rate"));

  auto instance = Instance::create(definition);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->scene() != nullptr);

  auto* box = dynamic_cast<Shape*>(instance->scene()->find("box"));
  REQUIRE(box != nullptr);
  if (!box) {
    return;
  }

  auto* machine = instance->get_machine("motionMachine");
  REQUIRE(machine != nullptr);
  if (!machine) {
    return;
  }

  auto* layer = machine->get_layer("main");
  REQUIRE(layer != nullptr);
  if (!layer) {
    return;
  }

  check_eq(layer->current_state(), std::string("idle"));
  REQUIRE_THAT(box->y(), WithinAbs(0.0f, 0.001f));

  instance->set_input("trigger", true);
  instance->set_input("dur", 1.0f);
  instance->set_input("rate", 3.0f);
  instance->advance(0.5f);

  check_eq(layer->current_state(), std::string("active"));
  REQUIRE_THAT(box->y(), WithinAbs(100.0f, 0.001f));
}

TEST_CASE("Definition: DSL machine transitions drive set actions in instances",
          "[definition][machine][instance][actions]") {
  auto definition = Definition::load(R"(
        scene Motion {
          rect box {
            x: 0
            y: 0
            width: 20
            height: 20
          }
        }

        machine motionMachine {
          layer main {
            state idle {
              initial: true
            }

            state active {
              set #box.x: 42
            }

            transition idle -> active when ${trigger}
          }
        }
      )");
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());
  REQUIRE(definition->scene() != nullptr);
  REQUIRE(definition->machines().size() == 1);

  auto machine_probe = definition->machines()[0]->clone();
  REQUIRE(machine_probe != nullptr);
  if (!machine_probe) {
    return;
  }

  int callback_count = 0;
  StateChangeInfo captured;
  machine_probe->set_state_change_callback([&](const StateChangeInfo& info) {
    ++callback_count;
    captured = info;
  });
  machine_probe->set_input("trigger", 1.0f);
  machine_probe->update(0.0f);

  check_int_eq(callback_count, 1);
  check_eq(captured.to_state, std::string("active"));
  check_int_eq(static_cast<int>(captured.actions.size()), 1);
  check_eq(captured.actions[0].node_id, std::string("box"));
  check_eq(captured.actions[0].property, std::string("x"));
  check_eq(captured.actions[0].expression, std::string("42.000000"));

  auto instance = Instance::create(definition);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->scene() != nullptr);

  auto* box = dynamic_cast<Shape*>(instance->scene()->find("box"));
  REQUIRE(box != nullptr);
  if (!box) {
    return;
  }

  auto* machine = instance->get_machine("motionMachine");
  REQUIRE(machine != nullptr);
  if (!machine) {
    return;
  }

  auto* layer = machine->get_layer("main");
  REQUIRE(layer != nullptr);
  if (!layer) {
    return;
  }

  check_eq(layer->current_state(), std::string("idle"));
  REQUIRE_THAT(box->x(), WithinAbs(0.0f, 0.001f));

  instance->set_input("trigger", true);
  instance->advance(0.0f);

  check_eq(layer->current_state(), std::string("active"));
  REQUIRE_THAT(box->x(), WithinAbs(42.0f, 0.001f));
}

TEST_CASE("Runtime machine: audio-only initial state fires state change callback",
          "[runtime][machine][audio]") {
  RuntimeStateMachine machine("machine");
  machine.add_layer("main");

  auto* layer = machine.get_layer("main");
  REQUIRE(layer != nullptr);
  layer->add_state("idle", true, "", "bgm");

  int callback_count = 0;
  StateChangeInfo captured;
  machine.set_state_change_callback([&](const StateChangeInfo& info) {
    ++callback_count;
    captured = info;
  });

  machine.trigger_initial_animations();

  REQUIRE(layer->current_state() == "idle");
  check_int_eq(callback_count, 1);
  check_eq(captured.layer, std::string("main"));
  check_eq(captured.to_state, std::string("idle"));
  check_eq(captured.play_audio, std::string("bgm"));
}

TEST_CASE("Instance: pointer events use node-local coordinates through transforms",
          "[instance][events][coordinates]") {
  auto definition = Definition::load(R"(
        scene HitTest {
          group panel {
            x: 100
            y: 50

            rect box {
              x: 20
              y: 10
              width: 40
              height: 30
            }
          }
        }
      )");
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());

  auto instance = Instance::create(definition);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->scene() != nullptr);

  auto* panel = dynamic_cast<Group*>(instance->scene()->find("panel"));
  auto* box = dynamic_cast<Shape*>(instance->scene()->find("box"));
  REQUIRE(panel != nullptr);
  REQUIRE(box != nullptr);
  if (!panel || !box) {
    return;
  }

  bool enter_called = false;
  float enter_local_x = 0.0f;
  float enter_local_y = 0.0f;
  box->on_hover_enter([&](PointerEvent& event) {
    enter_called = true;
    enter_local_x = event.local_x;
    enter_local_y = event.local_y;
  });

  bool leave_called = false;
  float leave_local_x = 0.0f;
  float leave_local_y = 0.0f;
  box->on_hover_leave([&](PointerEvent& event) {
    leave_called = true;
    leave_local_x = event.local_x;
    leave_local_y = event.local_y;
  });

  bool target_down_called = false;
  float target_down_x = 0.0f;
  float target_down_y = 0.0f;
  box->on_pointer_down([&](PointerEvent& event) {
    target_down_called = true;
    target_down_x = event.local_x;
    target_down_y = event.local_y;
  });

  bool bubble_move_called = false;
  float bubble_move_x = 0.0f;
  float bubble_move_y = 0.0f;
  panel->on_pointer_move([&](PointerEvent& event) {
    if (event.phase != EventPhase::Bubble) {
      return;
    }
    bubble_move_called = true;
    bubble_move_x = event.local_x;
    bubble_move_y = event.local_y;
  });

  bool bubble_down_called = false;
  float bubble_down_x = 0.0f;
  float bubble_down_y = 0.0f;
  panel->on_pointer_down([&](PointerEvent& event) {
    if (event.phase != EventPhase::Bubble) {
      return;
    }
    bubble_down_called = true;
    bubble_down_x = event.local_x;
    bubble_down_y = event.local_y;
  });

  instance->send_pointer_event(125.0f, 70.0f, false);

  REQUIRE(enter_called);
  REQUIRE(bubble_move_called);
  REQUIRE_THAT(enter_local_x, WithinAbs(5.0f, 0.001f));
  REQUIRE_THAT(enter_local_y, WithinAbs(10.0f, 0.001f));
  REQUIRE_THAT(bubble_move_x, WithinAbs(25.0f, 0.001f));
  REQUIRE_THAT(bubble_move_y, WithinAbs(20.0f, 0.001f));

  instance->send_pointer_event(125.0f, 70.0f, true);

  REQUIRE(target_down_called);
  REQUIRE(bubble_down_called);
  REQUIRE_THAT(target_down_x, WithinAbs(5.0f, 0.001f));
  REQUIRE_THAT(target_down_y, WithinAbs(10.0f, 0.001f));
  REQUIRE_THAT(bubble_down_x, WithinAbs(25.0f, 0.001f));
  REQUIRE_THAT(bubble_down_y, WithinAbs(20.0f, 0.001f));

  instance->send_pointer_event(10.0f, 10.0f, false);

  REQUIRE(leave_called);
  REQUIRE_THAT(leave_local_x, WithinAbs(-110.0f, 0.001f));
  REQUIRE_THAT(leave_local_y, WithinAbs(-50.0f, 0.001f));
}

TEST_CASE("Instance: focused node receives key text and composition input",
          "[instance][events][focus][input]") {
  auto definition = Definition::load(R"(
        scene InputTest {
          width: 200
          height: 120

          rect field {
            x: 10
            y: 10
            width: 50
            height: 20
          }

          rect other {
            x: 80
            y: 10
            width: 50
            height: 20
          }
        }
      )");
  REQUIRE(definition != nullptr);
  REQUIRE_FALSE(definition->has_error());

  auto instance = Instance::create(definition);
  REQUIRE(instance != nullptr);
  REQUIRE(instance->scene() != nullptr);

  auto* field = dynamic_cast<Shape*>(instance->scene()->find("field"));
  auto* other = dynamic_cast<Shape*>(instance->scene()->find("other"));
  REQUIRE(field != nullptr);
  REQUIRE(other != nullptr);
  if (!field || !other) {
    return;
  }

  REQUIRE_FALSE(instance->request_focus(field));

  field->set_focusable(true);
  other->set_focusable(true);

  int legacy_focus_count = 0;
  bool legacy_focused = false;
  field->on_focus([&](bool focused) {
    ++legacy_focus_count;
    legacy_focused = focused;
  });

  int field_focus_gained = 0;
  int field_focus_lost = 0;
  FocusChangeReason last_focus_reason = FocusChangeReason::Clear;
  Node* last_related_target = nullptr;
  field->on_focus_event([&](FocusEvent& event) {
    if (event.gained) {
      ++field_focus_gained;
    } else {
      ++field_focus_lost;
    }
    last_focus_reason = event.reason;
    last_related_target = event.related_target;
    REQUIRE(event.target == field);
    REQUIRE(event.current_target == field);
  });

  bool target_key_down = false;
  KeyCode target_key = KeyCode::Unknown;
  bool target_repeat = false;
  bool target_ctrl = false;
  field->on_key_down([&](KeyEvent& event) {
    if (event.phase != EventPhase::Target) {
      return;
    }
    target_key_down = true;
    target_key = event.key;
    target_repeat = event.repeat;
    target_ctrl = event.modifiers.ctrl;
    REQUIRE(event.target == field);
    REQUIRE(event.current_target == field);
  });

  bool bubble_key_down = false;
  instance->scene()->root()->on_key_down([&](KeyEvent& event) {
    if (event.phase != EventPhase::Bubble) {
      return;
    }
    bubble_key_down = true;
    REQUIRE(event.target == field);
    REQUIRE(event.current_target == instance->scene()->root());
  });

  std::string committed_text;
  bool committed_from_ime = false;
  field->on_text_input([&](TextInputEvent& event) {
    committed_text = event.text;
    committed_from_ime = event.from_ime;
    REQUIRE(event.target == field);
    REQUIRE(event.current_target == field);
  });

  CompositionEventType composition_type = CompositionEventType::Start;
  std::string composition_text;
  int composition_selection_start = 0;
  int composition_selection_end = 0;
  field->on_composition([&](CompositionEvent& event) {
    composition_type = event.type;
    composition_text = event.text;
    composition_selection_start = event.selection_start;
    composition_selection_end = event.selection_end;
    REQUIRE(event.target == field);
    REQUIRE(event.current_target == field);
  });

  REQUIRE(instance->request_focus(field, FocusChangeReason::Programmatic));
  REQUIRE(instance->focused_node() == field);
  REQUIRE(field->focused());
  check_int_eq(legacy_focus_count, 1);
  REQUIRE(legacy_focused);
  check_int_eq(field_focus_gained, 1);
  check_int_eq(field_focus_lost, 0);
  REQUIRE(last_focus_reason == FocusChangeReason::Programmatic);
  REQUIRE(last_related_target == nullptr);

  KeyModifiers modifiers;
  modifiers.ctrl = true;
  instance->send_key_event(KeyCode::A, true, modifiers, true);

  REQUIRE(target_key_down);
  REQUIRE(bubble_key_down);
  REQUIRE(target_key == KeyCode::A);
  REQUIRE(target_repeat);
  REQUIRE(target_ctrl);

  instance->send_text_input("text", true);
  check_eq(committed_text, std::string("text"));
  REQUIRE(committed_from_ime);

  instance->send_composition_event(CompositionEventType::Update, "preedit", 1, 3);
  REQUIRE(composition_type == CompositionEventType::Update);
  check_eq(composition_text, std::string("preedit"));
  check_int_eq(composition_selection_start, 1);
  check_int_eq(composition_selection_end, 3);

  int other_focus_gained = 0;
  other->on_focus_event([&](FocusEvent& event) {
    if (event.gained) {
      ++other_focus_gained;
    }
    REQUIRE(event.target == other);
  });

  instance->send_pointer_event(85.0f, 15.0f, true);
  instance->send_pointer_event(85.0f, 15.0f, false);

  REQUIRE(instance->focused_node() == other);
  REQUIRE_FALSE(field->focused());
  REQUIRE(other->focused());
  check_int_eq(field_focus_lost, 1);
  check_int_eq(other_focus_gained, 1);
  REQUIRE(last_focus_reason == FocusChangeReason::Pointer);
  REQUIRE(last_related_target == other);

  instance->send_pointer_event(180.0f, 100.0f, true);
  instance->send_pointer_event(180.0f, 100.0f, false);

  REQUIRE(instance->focused_node() == nullptr);
  REQUIRE_FALSE(other->focused());
}

}
