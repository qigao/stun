#include "tinytest.h"
#include "flexchart/rect/rect_parser.h"
#include <string>

spec("flexchart_rect_parser") {
    describe("rect_chart_parse_ast") {
        it("should parse a minimal rect chart") {
            const char* input =
                "rect {\n"
                "  title: \"Heatmap\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"col\"\n"
                "  y: \"row\"\n"
                "  color: \"value\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::rect_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Heatmap");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "rect");
            check(mark->encodings.size() >= 3);
            // Find x, y, color encodings
            std::string x_field, y_field, color_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
                if (enc->channel == "color") color_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "col");
            check_str_eq(y_field.c_str(), "row");
            check_str_eq(color_field.c_str(), "value");
        }

        it("should parse rect chart with data block") {
            const char* input =
                "rect {\n"
                "  title: \"Grid\"\n"
                "  data {\n"
                "    { \"col\": \"A\", \"row\": \"1\", \"value\": 80 }\n"
                "    { \"col\": \"B\", \"row\": \"2\", \"value\": 45 }\n"
                "  }\n"
                "  x: \"col\"\n"
                "  y: \"row\"\n"
                "  color: \"value\"\n"
                "  cell-width: 40\n"
                "  cell-height: 25\n"
                "}";
            std::string error;
            auto chart = flex::chart::rect_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Grid");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check styles
            check(mark->styles.count("cell-width") > 0);
            check(std::get<double>(mark->styles["cell-width"]) > 39.9);
            check(mark->styles.count("cell-height") > 0);
            check(std::get<double>(mark->styles["cell-height"]) > 24.9);
        }

        it("should parse encoding block") {
            const char* input =
                "rect {\n"
                "  encoding {\n"
                "    x: \"col\"\n"
                "    y: \"row\"\n"
                "    color: \"intensity\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::rect_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 3);
        }

        it("should parse style block") {
            const char* input =
                "rect {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    cell-width: 50\n"
                "    cell-height: 35\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::rect_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["cell-width"]) > 49.9);
            check(std::get<double>(mark->styles["cell-height"]) > 34.9);
        }

        it("should parse expr and range") {
            const char* input =
                "rect {\n"
                "  expr: \"x * y\"\n"
                "  x: [0, 10, 1]\n"
                "  y: [0, 10, 1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::rect_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "x * y");
            check(mark->ranges.count("x") > 0);
            auto& rx = mark->ranges["x"];
            check(rx.size() == 3);
            check(rx[0] < 0.01);    // 0
            check(rx[1] > 9.99);    // 10
            check(rx[2] > 0.99);    // 1
            check(mark->ranges.count("y") > 0);
            auto& ry = mark->ranges["y"];
            check(ry.size() == 3);
        }

        it("should parse data source reference") {
            const char* input =
                "rect {\n"
                "  data: \"heatmap.json\"\n"
                "  x: \"col\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::rect_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "heatmap.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::rect_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
