#include "tinytest.h"
#include "flexchart/rule/rule_parser.h"
#include <string>

spec("flexchart_rule_parser") {
    describe("rule_chart_parse_ast") {
        it("should parse a minimal rule chart") {
            const char* input =
                "rule {\n"
                "  title: \"Reference\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Reference");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "rule");
            check(mark->encodings.size() >= 2);
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "category");
            check_str_eq(y_field.c_str(), "value");
        }

        it("should parse rule chart with data block") {
            const char* input =
                "rule {\n"
                "  title: \"Thresholds\"\n"
                "  data {\n"
                "    { \"label\": \"min\", \"value\": 10 }\n"
                "    { \"label\": \"max\", \"value\": 90 }\n"
                "  }\n"
                "  x: \"label\"\n"
                "  y: \"value\"\n"
                "  stroke-width: 3\n"
                "}";
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Thresholds");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            check(mark->styles.count("stroke-width") > 0);
            check(std::get<double>(mark->styles["stroke-width"]) > 2.9);
        }

        it("should parse x2 and y2 encodings") {
            const char* input =
                "rule {\n"
                "  x: \"start\"\n"
                "  y: \"level\"\n"
                "  x2: \"end\"\n"
                "  y2: \"level2\"\n"
                "  color: \"group\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 5);
            std::string x2_field, y2_field, color_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x2") x2_field = enc->field;
                if (enc->channel == "y2") y2_field = enc->field;
                if (enc->channel == "color") color_field = enc->field;
            }
            check_str_eq(x2_field.c_str(), "end");
            check_str_eq(y2_field.c_str(), "level2");
            check_str_eq(color_field.c_str(), "group");
        }

        it("should parse encoding block") {
            const char* input =
                "rule {\n"
                "  encoding {\n"
                "    x: \"start\"\n"
                "    y: \"value\"\n"
                "    x2: \"end\"\n"
                "    color: \"type\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 4);
        }

        it("should parse style block") {
            const char* input =
                "rule {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    stroke-width: 4\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["stroke-width"]) > 3.9);
        }

        it("should parse expr and range") {
            const char* input =
                "rule {\n"
                "  expr: \"threshold(x)\"\n"
                "  x: [0, 100, 10]\n"
                "}";
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "threshold(x)");
            check(mark->ranges.count("x") > 0);
            auto& r = mark->ranges["x"];
            check(r.size() == 3);
            check(r[0] < 0.01);
            check(r[1] > 99.9);
            check(r[2] > 9.9);
        }

        it("should parse data source reference") {
            const char* input =
                "rule {\n"
                "  data: \"thresholds.json\"\n"
                "  y: \"level\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "thresholds.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::rule_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
