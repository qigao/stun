#include "tinytest.h"
#include "flexchart/errorband/errorband_parser.h"
#include <string>

spec("flexchart_errorband_parser") {
    describe("errorband_chart_parse_ast") {
        it("should parse a minimal errorband chart") {
            const char* input =
                "errorband {\n"
                "  title: \"Error Band\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"x_val\"\n"
                "  y: \"y_val\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorband_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Error Band");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "errorband");
            check(mark->encodings.size() >= 2);
            // Find x and y encodings
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "x_val");
            check_str_eq(y_field.c_str(), "y_val");
        }

        it("should parse errorband chart with all fields") {
            const char* input =
                "errorband {\n"
                "  title: \"Measurements\"\n"
                "  data {\n"
                "    { \"x_val\": 10, \"y_val\": 20, \"y_error\": 2, \"color\": \"red\" }\n"
                "    { \"x_val\": 15, \"y_val\": 25, \"y_error\": 3, \"color\": \"blue\" }\n"
                "  }\n"
                "  x: \"x_val\"\n"
                "  y: \"y_val\"\n"
                "  y-error: \"y_error\"\n"
                "  color: \"color\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorband_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Measurements");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Find y-error and color encodings
            std::string yerr_field, color_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "y-error") yerr_field = enc->field;
                if (enc->channel == "color") color_field = enc->field;
            }
            check_str_eq(yerr_field.c_str(), "y_error");
            check_str_eq(color_field.c_str(), "color");
        }

        it("should parse encoding block") {
            const char* input =
                "errorband {\n"
                "  encoding {\n"
                "    x: \"time\"\n"
                "    y: \"value\"\n"
                "    y-error: \"err\"\n"
                "    color: \"group\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorband_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 4);
        }

        it("should parse style block") {
            const char* input =
                "errorband {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    opacity: 0.5\n"
                "    interpolate: \"monotone\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorband_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["opacity"]) > 0.49);
            check_str_eq(std::get<std::string>(mark->styles["interpolate"]).c_str(), "monotone");
        }

        it("should parse expr and range") {
            const char* input =
                "errorband {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorband_chart_parse_ast(input, error);
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
                "errorband {\n"
                "  data: \"measurements.json\"\n"
                "  x: \"t\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorband_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "measurements.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::errorband_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
