#include "tinytest.h"

#include <flex.h>
#include <flexinfographic.h>
#include <flexui_infographic.h>
#include <infographic_component.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/widget.h>

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

    it("builds a Box-owned utility styled interactive infographic") {
        FlexInfographic api;
        auto parsed = api.parse(
            "infographic list-grid-badge-card\n"
            "data\n"
            "  items\n"
            "    - label Revenue\n");
        check_true(parsed.success);
        parsed.infographic->set_title("Metrics");

        flexUI::Box box(nullptr);
        InfographicViewOptions options;
        options.width = 640.0f;
        options.height = 360.0f;
        options.accessible_label = "Business metrics";
        auto result = create_flexui_infographic(
            box, *parsed.infographic, options);
        check_true(static_cast<bool>(result));
        check_string_eq(result.error, "");
        box.set_root(result.root);
        box.set_viewport(options.width, options.height);
        box.update();

        check_not_null(box.query_selector("[data-slot=infographic]"));
        check_not_null(box.query_selector("[data-slot=infographic-title]"));
        check(result.plot ==
              box.query_selector("[data-slot=infographic-plot]"));
        check_not_null(result.plot->widget);
        flexUI::RenderCommandList commands(flex::RendererCapabilities{});
        result.plot->widget->emit_render_commands(*result.plot, commands);
        check_false(commands.commands().empty());
        check(result.root->computed_style->display == flexUI::Display::Flex);
        check_true(box.missing_utility_tokens().empty());
    }

    it("does not expose a partial tree for invalid interactive dimensions") {
        UnifiedInfographic infographic;
        flexUI::Box box(nullptr);
        InfographicViewOptions options;
        options.height = 0.0f;
        const auto result = create_flexui_infographic(
            box, infographic, options);
        check_false(static_cast<bool>(result));
        check_null(result.root);
        check_false(result.error.empty());
        check_null(box.root());
    }
}
