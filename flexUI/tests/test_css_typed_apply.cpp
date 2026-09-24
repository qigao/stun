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
};
