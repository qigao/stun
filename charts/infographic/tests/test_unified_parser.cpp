#include "tinytest.h"

#include <flex.h>
#include <flexinfographic.h>
#include <infographic_component.h>

using namespace flex::modules::infographic;

spec("infographic public parser") {
    it("preserves nested children") {
        FlexInfographic api;
        auto result = api.parse(R"(
infographic compare-binary-horizontal-underline-text-vs
data
  items
    - label Left
      children
        - label One
          children
            - label Nested
    - label Right
      children
        - label Two
)");

        check_true(result.success);
        check_size_eq(result.infographic->items.size(), 2);
        check_size_eq(result.infographic->items[0]->children.size(), 1);
        check_size_eq(result.infographic->items[0]->children[0]->children.size(), 1);
        check_string_eq(result.infographic->items[0]->children[0]->children[0]->label,
                        "Nested");
    }

    it("rejects unknown templates and invalid numeric expressions") {
        FlexInfographic api;
        auto unknown = api.parse("infographic not-a-template\ndata\n  items\n    - label X\n");
        check_false(unknown.success);
        check_string_contains(unknown.get_error(), "Unknown infographic template");

        auto invalid_number = api.parse(
            "infographic list-grid-badge-card\ndata\n  items\n"
            "    - label X\n      value 1 +\n");
        check_false(invalid_number.success);
        check_string_contains(invalid_number.get_error(), "Invalid numeric expression");
    }

    it("builds the registered component from public props") {
        InfographicComponent::register_component();
        flex::Props props;
        props["source"] = std::string(
            "infographic list-grid-badge-card\ndata\n  items\n    - label Metric\n");
        props["width"] = 720.0f;
        props["height"] = 480.0f;
        auto node = flex::create_component_instance("Infographic", props);
        check_not_null(node.get());
        check_float_eq(node->layout_width(), 720.0f, 0.001f);
        check_float_eq(node->layout_height(), 480.0f, 0.001f);
    }
}
