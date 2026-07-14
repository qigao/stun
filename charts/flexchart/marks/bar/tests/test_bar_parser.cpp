#include "tinytest.h"
#include "flexchart/bar/bar_parser.h"
#include <string>

spec("flexchart_bar_parser") {
    describe("bar_chart_parse_ast") {
        it("should parse a minimal bar chart") {
            const char* input =
                "bar {\n"
                "  title: \"Revenue\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"category\"\n"
                "  y: \"revenue\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::bar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Revenue");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "bar");
            check(mark->encodings.size() >= 2);
            // Find x and y encodings
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "category");
            check_str_eq(y_field.c_str(), "revenue");
        }

        it("should parse bar chart with data block") {
            const char* input =
                "bar {\n"
                "  title: \"Sales\"\n"
                "  data {\n"
                "    { \"category\": \"Software\", \"revenue\": 45 }\n"
                "    { \"category\": \"Hardware\", \"revenue\": 60 }\n"
                "  }\n"
                "  x: \"category\"\n"
                "  y: \"revenue\"\n"
                "  stack: true\n"
                "  corner-radius: 6\n"
                "  orientation: vertical\n"
                "}";
            std::string error;
            auto chart = flex::chart::bar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Sales");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check styles
            check(mark->styles.count("stack") > 0);
            check(std::get<bool>(mark->styles["stack"]) == true);
            check(mark->styles.count("corner-radius") > 0);
            check(std::get<double>(mark->styles["corner-radius"]) > 5.9);
            check(mark->styles.count("orientation") > 0);
            check_str_eq(std::get<std::string>(mark->styles["orientation"]).c_str(), "vertical");
        }

        it("should parse encoding block") {
            const char* input =
                "bar {\n"
                "  encoding {\n"
                "    x: \"quarter\"\n"
                "    y: \"revenue\"\n"
                "    color: \"region\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::bar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 3);
        }

        it("should parse style block") {
            const char* input =
                "bar {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    corner-radius: 8\n"
                "    stack: true\n"
                "    orientation: vertical\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::bar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["corner-radius"]) > 7.9);
            check(std::get<bool>(mark->styles["stack"]) == true);
        }

        it("should parse expr and range") {
            const char* input =
                "bar {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::bar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "sin(x) * 100");
            check(mark->ranges.count("x") > 0);
            auto& r = mark->ranges["x"];
            check(r.size() == 3);
            check(r[0] < 0.01);    // 0
            check(r[1] > 6.27);    // 6.28
            check(r[2] > 0.09);    // 0.1
        }

        it("should parse data source reference") {
            const char* input =
                "bar {\n"
                "  data: \"sales.json\"\n"
                "  x: \"q\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::bar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "sales.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::bar_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
