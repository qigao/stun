#include "tinytest.h"
#include "flexchart/area/area_parser.h"
#include <string>

spec("flexchart_area_parser") {
    describe("area_chart_parse_ast") {
        it("should parse a minimal area chart") {
            const char* input =
                "area {\n"
                "  title: \"Coverage\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"month\"\n"
                "  y: \"value\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::area_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Coverage");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "area");
            check(mark->encodings.size() >= 2);
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "month");
            check_str_eq(y_field.c_str(), "value");
        }

        it("should parse area chart with data block") {
            const char* input =
                "area {\n"
                "  title: \"Trends\"\n"
                "  data {\n"
                "    { \"month\": \"Jan\", \"value\": 30 }\n"
                "    { \"month\": \"Feb\", \"value\": 45 }\n"
                "  }\n"
                "  x: \"month\"\n"
                "  y: \"value\"\n"
                "  opacity: 0.6\n"
                "  interpolate: monotone\n"
                "  stroke-width: 2\n"
                "}";
            std::string error;
            auto chart = flex::chart::area_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Trends");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            check(mark->styles.count("opacity") > 0);
            check(std::get<double>(mark->styles["opacity"]) > 0.59);
            check(mark->styles.count("interpolate") > 0);
            check_str_eq(std::get<std::string>(mark->styles["interpolate"]).c_str(), "monotone");
            check(mark->styles.count("stroke-width") > 0);
            check(std::get<double>(mark->styles["stroke-width"]) > 1.9);
        }

        it("should parse encoding block") {
            const char* input =
                "area {\n"
                "  encoding {\n"
                "    x: \"quarter\"\n"
                "    y: \"revenue\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::area_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 2);
        }

        it("should parse style block") {
            const char* input =
                "area {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    opacity: 0.8\n"
                "    interpolate: linear\n"
                "    stroke-width: 3\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::area_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["opacity"]) > 0.79);
            check_str_eq(std::get<std::string>(mark->styles["interpolate"]).c_str(), "linear");
            check(std::get<double>(mark->styles["stroke-width"]) > 2.9);
        }

        it("should parse expr and range") {
            const char* input =
                "area {\n"
                "  expr: \"sin(x) * 50\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::area_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "sin(x) * 50");
            check(mark->ranges.count("x") > 0);
            auto& r = mark->ranges["x"];
            check(r.size() == 3);
            check(r[0] < 0.01);    // 0
            check(r[1] > 6.27);    // 6.28
            check(r[2] > 0.09);    // 0.1
        }

        it("should parse data source reference") {
            const char* input =
                "area {\n"
                "  data: \"metrics.json\"\n"
                "  x: \"time\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::area_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "metrics.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::area_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
