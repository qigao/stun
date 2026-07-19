#include "tinytest.h"
#include "flexchart/flexchart.h"
#include "flexchart/chart_ast.h"
#include "../src/chart_component_internal.h"
#include "flexchart/flex_chart.h"
#include "flexchart/flexui_chart.h"
#include "flexchart/mark_renderer_registry.h"
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/widget.h>
#include <stdexcept>

#define REQUIRE(expr) check(expr)

using namespace flex::chart;

spec("flexchart_parser") {

    describe("basic properties") {
        it("should parse title, width, height, margin, theme") {
            const char* source = R"chart(
                bar {
                    title: "Test Chart"
                    width: 800
                    height: 500
                    x: "month"
                    y: "revenue"
                }
            )chart";

            AstProgram program;
            std::string error;
            bool success = parse_chart(source, &program, error);

            REQUIRE(success);
            REQUIRE(error.empty());
            REQUIRE(program.views.size() > 0);
            auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
            REQUIRE(chart != nullptr);
            check_string_eq(chart->title, "Test Chart");
            check_string_eq(chart->width, "800");
            check_string_eq(chart->height, "500");
        }

        it("should reject an unexpected character inside a valid chart") {
            AstProgram program;
            std::string error;

            const bool success = parse_chart(
                "bar { title: \"Test Chart\" @ x: \"month\" }",
                &program, error);

            check_false(success);
            check_string_contains(error, "unexpected character '@'");
        }
    }

    describe("data blocks") {
        it("should parse inline data through the chart dispatcher") {
            const char* source = R"chart(
                bar {
                    data {
                        { "month": "Jan", "revenue": 10 }
                        { "month": "Feb", "revenue": 20 }
                    }
                    x: "month"
                    y: "revenue"
                }
            )chart";

            AstProgram program;
            std::string error;
            bool success = parse_chart(source, &program, error);

            REQUIRE(success);
            REQUIRE(program.views.size() == 1);
            auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
            REQUIRE(chart != nullptr);
            REQUIRE(chart->datasets.size() == 1);
            REQUIRE(!chart->datasets[0]->inline_values.empty());
        }
    }

    describe("marks and encodings") {
        it("should parse mark type, data ref, encodings, and styles") {
            const char* source = R"chart(
                bar {
                    data: "sales.json"
                    x: "month"
                    y: "revenue"
                    style {
                        corner-radius: 6
                    }
                }
            )chart";

            AstProgram program;
            std::string error;
            bool success = parse_chart(source, &program, error);

            REQUIRE(success);
            auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
            REQUIRE(chart != nullptr);
            REQUIRE(chart->marks.size() == 1);
            auto mark = chart->marks[0];
            check_string_eq(mark->type, "bar");
            REQUIRE(mark->encodings.size() == 2);
            check_string_eq(mark->encodings[0]->channel, "x");
            check_string_eq(mark->encodings[0]->field, "month");
            check_string_eq(mark->encodings[1]->channel, "y");
            check_string_eq(mark->encodings[1]->field, "revenue");
            REQUIRE(mark->styles.count("corner-radius") > 0);
        }
    }

    describe("signals") {
        it("should reject unsupported top-level chart blocks") {
            const char* source = R"chart(
                signal hover_id {
                    value: 0
                    on: [
                        { events: "rect:mouseover", update: "datum.id" },
                        { events: "rect:mouseout", update: "0" }
                    ]
                }
                signal active = true
            )chart";

            AstProgram program;
            std::string error;
            bool success = parse_chart(source, &program, error);

            REQUIRE(!success);
            REQUIRE(!error.empty());
        }
    }

    describe("expression data blocks") {
        it("should parse expr and named ranges") {
            const char* source = R"chart(
                line {
                    expr: "sin(x) * 2 + cos(x / 3)"
                    x: [0, 6.28, 0.1]
                }
            )chart";

            AstProgram program;
            std::string error;
            bool success = parse_chart(source, &program, error);

            REQUIRE(success);
            REQUIRE(program.views.size() == 1);
            auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
            REQUIRE(chart != nullptr);
            REQUIRE(chart->marks.size() == 1);
            auto mark = chart->marks[0];
            check_string_eq(mark->expr, "sin(x) * 2 + cos(x / 3)");
            REQUIRE(mark->ranges.count("x") == 1);
            auto& r = mark->ranges["x"];
            REQUIRE(r.size() == 3);
            check_float_eq(r[0], 0.0, 0.001);
            check_float_eq(r[1], 6.28, 0.001);
            check_float_eq(r[2], 0.1, 0.001);
        }

        it("should evaluate x*x over range") {
            auto data = std::make_shared<AstData>();
            data->expr = "x * x";
            data->ranges["x"] = {0, 3, 1};

            auto records = get_records(data);
            REQUIRE(records.size() == 4);
            check_float_eq(get_double_val(records[0].get("y")), 0.0, 0.001);
            check_float_eq(get_double_val(records[1].get("y")), 1.0, 0.001);
            check_float_eq(get_double_val(records[2].get("y")), 4.0, 0.001);
            check_float_eq(get_double_val(records[3].get("y")), 9.0, 0.001);
            check_float_eq(get_double_val(records[2].get("x")), 2.0, 0.001);
        }

        it("should apply filter transform") {
            auto data = std::make_shared<AstData>();
            data->expr = "x * x";
            data->ranges["x"] = {0, 4, 1};

            auto filter_t = std::make_shared<AstTransform>();
            filter_t->type = "filter";
            filter_t->properties["filter"] = std::string("y > 5");
            data->transforms.push_back(filter_t);

            auto records = get_records(data);
            // x=0->0, x=1->1, x=2->4, x=3->9, x=4->16 => y>5 keeps x=3,x=4
            REQUIRE(records.size() == 2);
            check_float_eq(get_double_val(records[0].get("x")), 3.0, 0.001);
            check_float_eq(get_double_val(records[1].get("x")), 4.0, 0.001);
        }

        it("should apply formula transform") {
            auto data = std::make_shared<AstData>();
            data->expr = "x";
            data->ranges["x"] = {1, 3, 1};

            auto formula_t = std::make_shared<AstTransform>();
            formula_t->type = "formula";
            formula_t->properties["formula"] = std::string("x * 2 + 1");
            formula_t->properties["as"] = std::string("scaled");
            data->transforms.push_back(formula_t);

            auto records = get_records(data);
            REQUIRE(records.size() == 3);
            check_float_eq(get_double_val(records[0].get("scaled")), 3.0, 0.001);
            check_float_eq(get_double_val(records[1].get("scaled")), 5.0, 0.001);
            check_float_eq(get_double_val(records[2].get("scaled")), 7.0, 0.001);
        }
    }

    describe("inline mark expressions") {
        it("should parse expr and ranges inside a mark") {
            const char* source = R"chart(
                line {
                    expr: "sin(x)"
                    x: [0, 6.28, 0.1]
                }
            )chart";

            AstProgram program;
            std::string error;
            bool success = parse_chart(source, &program, error);

            REQUIRE(success);
            auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
            REQUIRE(chart != nullptr);
            REQUIRE(chart->marks.size() == 1);
            auto mark = chart->marks[0];
            check_string_eq(mark->type, "line");
            check_string_eq(mark->expr, "sin(x)");
            REQUIRE(mark->ranges.count("x") == 1);
            auto& r = mark->ranges["x"];
            REQUIRE(r.size() == 3);
            check_float_eq(r[0], 0.0, 0.001);
            check_float_eq(r[1], 6.28, 0.001);
            check_float_eq(r[2], 0.1, 0.001);
        }

        it("should generate records from inline mark expr") {
            auto records = generate_expr_records("t * t", {{"t", {0, 3, 1}}});
            REQUIRE(records.size() == 4);
            check_float_eq(get_double_val(records[0].get("t")), 0.0, 0.001);
            check_float_eq(get_double_val(records[0].get("y")), 0.0, 0.001);
            check_float_eq(get_double_val(records[2].get("t")), 2.0, 0.001);
            check_float_eq(get_double_val(records[2].get("y")), 4.0, 0.001);
            check_float_eq(get_double_val(records[3].get("y")), 9.0, 0.001);
        }

        it("should support descending ranges") {
            auto records = generate_expr_records("t * 2", {{"t", {3, 0, -1}}});
            check_size_eq(records.size(), 4);
            check_float_eq(get_double_val(records.front().get("t")), 3.0, 0.001);
            check_float_eq(get_double_val(records.back().get("t")), 0.0, 0.001);
        }

        it("should reject unsafe ranges") {
            check_throws_as(generate_expr_records("t", {{"t", {0, 1, 0}}}), std::invalid_argument);
            check_throws_as(generate_expr_records("t", {{"t", {0, 1, -1}}}), std::invalid_argument);
            check_throws_as(generate_expr_records("t", {{"t", {0, 1, 0.0000001}}}), std::length_error);
        }

        it("should evaluate inline mark expr with MIR functions and logical operators") {
            auto records = generate_expr_records(
                "clamp(pow(t, 2), 0, 4) + (t >= 2 and not (t > 2))",
                {{"t", {0, 3, 1}}});
            REQUIRE(records.size() == 4);
            check_float_eq(get_double_val(records[0].get("y")), 0.0, 0.001);
            check_float_eq(get_double_val(records[1].get("y")), 1.0, 0.001);
            check_float_eq(get_double_val(records[2].get("y")), 5.0, 0.001);
            check_float_eq(get_double_val(records[3].get("y")), 4.0, 0.001);
        }

        it("should keep compiled chart expressions when records have extra fields") {
            std::vector<Record> records;
            Record rec;
            rec.fields["x"] = 2.0;
            rec.fields["y"] = 10.0;
            rec.fields["label"] = std::string("ignored");
            records.push_back(std::move(rec));

            auto enc = std::make_shared<AstEncoding>();
            enc->channel = "size";
            enc->field = "pow(x, 2) + clamp(y, 0, 3)";
            apply_computed_fields(records, {enc});

            check_float_eq(get_double_val(records[0].get("__expr_size")), 7.0, 0.001);
        }
    }

    describe("public rendering contract") {
        it("should link built-in mark renderers without application anchors") {
            ensure_builtin_mark_renderers_linked();
            check_not_null(MarkRendererRegistry::instance().create("bar").get());
            check_not_null(MarkRendererRegistry::instance().create("line").get());
        }

        it("should fail explicitly while SVG export is unavailable") {
            flex::modules::chart::FlexChart chart;
            AstChart ast;
            check_throws_as(chart.to_svg(ast), std::logic_error);
            check_throws_as(chart.to_svg("not a chart"), std::invalid_argument);
        }

        it("builds a Box-owned utility styled interactive chart") {
            AstProgram program;
            std::string error;
            check_true(parse_chart(R"chart(
                bar {
                    title: "Revenue"
                    data {
                        { "month": "Jan", "revenue": 10 }
                        { "month": "Feb", "revenue": 20 }
                    }
                    x: "month"
                    y: "revenue"
                }
            )chart", &program, error));
            auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
            check_not_null(chart.get());

            flexUI::Box box(nullptr);
            flex::modules::chart::ChartViewOptions options;
            options.width = 640.0f;
            options.height = 360.0f;
            options.accessible_label = "Monthly revenue";
            auto result = flex::modules::chart::create_flexui_chart(
                box, *chart, options);
            check_true(static_cast<bool>(result));
            check_string_eq(result.error, "");
            box.set_root(result.root);
            box.set_viewport(options.width, options.height);
            box.update();

            check_not_null(box.query_selector("[data-slot=chart]"));
            check_not_null(box.query_selector("[data-slot=chart-title]"));
            check(result.plot == box.query_selector("[data-slot=chart-plot]"));
            check_not_null(result.plot->widget);
            flexUI::RenderCommandList commands(flex::RendererCapabilities{});
            result.plot->widget->emit_render_commands(*result.plot, commands);
            check_false(commands.commands().empty());
            check(result.root->computed_style->display == flexUI::Display::Flex);
            check_true(box.missing_utility_tokens().empty());
        }

        it("does not expose a partial chart tree for invalid view options") {
            flexUI::Box box(nullptr);
            AstChart chart;
            flex::modules::chart::ChartViewOptions options;
            options.width = 0.0f;
            const auto result = flex::modules::chart::create_flexui_chart(
                box, chart, options);
            check_false(static_cast<bool>(result));
            check_null(result.root);
            check_false(result.error.empty());
            check_null(box.root());
        }
    }
}
