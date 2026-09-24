#include <flexUI/box.h>
#include <flexUI/detail/css_typed_value.h>

#include <tinytest.hpp>

using namespace flexUI;

namespace {

Element* make_target(Box& box, const char* class_name = nullptr) {
    auto* root = box.create("div", "root");
    auto* target = box.create("div", "target");
    if (class_name) target->add_class(class_name);
    root->append(target);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
    return target;
}

void check_color(const Color& actual, const Color& expected,
                 float epsilon = 0.001f) {
    check_float_eq(actual.r, expected.r, epsilon);
    check_float_eq(actual.g, expected.g, epsilon);
    check_float_eq(actual.b, expected.b, epsilon);
    check_float_eq(actual.a, expected.a, epsilon);
}

} // namespace

suite("FlexUI typed CSS opacity apply") {
    it("applies a compiled literal through the registry") {
        Box box(nullptr);
        auto* target = make_target(box);

        box.load_css("#target { opacity: 0.625; }");
        box.update();

        check_float_eq(target->computed_style->opacity, 0.625f, 0.0001f);
    }

    it("preserves source order for typed opacity declarations") {
        Box box(nullptr);
        auto* target = make_target(box);

        box.load_css(R"(
          #target { opacity: 0.2; }
          #target { opacity: 0.75; }
        )");
        box.update();

        check_float_eq(target->computed_style->opacity, 0.75f, 0.0001f);
    }

    it("preserves important and specificity ordering") {
        Box box(nullptr);
        auto* target = make_target(box, "item");

        box.load_css(R"(
          #target { opacity: 0.1; }
          .item { opacity: 0.8 !important; }
          #target { opacity: 0.4; }
        )");
        box.update();

        check_float_eq(target->computed_style->opacity, 0.8f, 0.0001f);
    }

    it("falls back for variable-backed opacity") {
        Box box(nullptr);
        auto* target = make_target(box);

        box.load_css(R"(
          #target {
            --target-opacity: 0.375;
            opacity: var(--target-opacity);
          }
        )");
        box.update();

        check_float_eq(target->computed_style->opacity, 0.375f, 0.0001f);
    }

    it("keeps deferred calc opacity on the fallback path") {
        const auto* property = detail::style_property_find("opacity");
        check_not_null(property);
        check_false(
            detail::compile_css_literal(property, "calc(1 - 0.25)").has_value());
    }

    it("applies safe literal Color properties through the registry") {
        Box box(nullptr);
        auto* target = make_target(box);

        box.load_css(R"(
          #target {
            background-color: #336699;
            outline-color: rgb(255 0 128 / 50%);
            ring-color: hsl(120 100% 25%);
            ring-offset-color: #112233;
          }
        )");
        box.update();

        check_color(target->computed_style->background_color,
                    Color{0.2f, 0.4f, 0.6f, 1.0f});
        check_color(target->computed_style->outline_color,
                    Color{1.0f, 0.0f, 128.0f / 255.0f, 0.5f});
        check_color(target->computed_style->ring_color,
                    Color{0.0f, 0.5f, 0.0f, 1.0f});
        check_color(target->computed_style->ring_offset_color,
                    Color{0x11 / 255.0f, 0x22 / 255.0f, 0x33 / 255.0f, 1.0f});
    }

    it("preserves Color source order and important semantics") {
        Box box(nullptr);
        auto* target = make_target(box, "item");

        box.load_css(R"(
          #target { background-color: #ff0000; }
          .item { background-color: #00ff00 !important; }
          #target { background-color: #0000ff; }
        )");
        box.update();

        check_color(target->computed_style->background_color,
                    Color{0.0f, 1.0f, 0.0f, 1.0f});
    }

    it("falls back for variable-backed and currentColor values") {
        Box box(nullptr);
        auto* target = make_target(box);

        box.load_css(R"(
          #target {
            --surface: #123456;
            color: #336699;
            background-color: var(--surface);
            outline-color: currentColor;
          }
        )");
        box.update();

        check_color(target->computed_style->background_color,
                    Color{0x12 / 255.0f, 0x34 / 255.0f, 0x56 / 255.0f, 1.0f});
        check_color(target->computed_style->outline_color,
                    target->computed_style->text_color);
    }

    it("keeps border-color on the legacy side-projection path") {
        Box box(nullptr);
        auto* target = make_target(box);

        box.load_css("#target { border-color: #112233; }");
        box.update();

        check_true(target->computed_style->has_border_side_colors);
        for (const auto& side : target->computed_style->border_colors) {
            check_color(side,
                        Color{0x11 / 255.0f, 0x22 / 255.0f,
                              0x33 / 255.0f, 1.0f});
        }
    }
};
