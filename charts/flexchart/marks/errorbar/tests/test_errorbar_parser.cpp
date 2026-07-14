#include "tinytest.h"
#include "flexchart/errorbar/errorbar_parser.h"
#include <string>

spec("flexchart_errorbar_parser") {
    describe("errorbar_chart_parse_ast") {
        it("should parse a minimal errorbar chart") {
            const char* input =
                "errorbar {\n"
                "  title: \"Errors\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "  y-error: \"err\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorbar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Errors");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "errorbar");
            check(mark->encodings.size() >= 3);
            // Find x, y, y-error encodings
            std::string x_field, y_field, y_error_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
                if (enc->channel == "y-error") y_error_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "category");
            check_str_eq(y_field.c_str(), "value");
            check_str_eq(y_error_field.c_str(), "err");
        }

        it("should parse errorbar chart with data block and min/max") {
            const char* input =
                "errorbar {\n"
                "  title: \"Measurements\"\n"
                "  data {\n"
                "    { \"category\": \"A\", \"value\": 30, \"lo\": 25, \"hi\": 35 }\n"
                "    { \"category\": \"B\", \"value\": 50, \"lo\": 42, \"hi\": 58 }\n"
                "  }\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "  y-error-min: \"lo\"\n"
                "  y-error-max: \"hi\"\n"
                "  cap-size: 12\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorbar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Measurements");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check encodings
            std::string y_err_min, y_err_max;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "y-error-min") y_err_min = enc->field;
                if (enc->channel == "y-error-max") y_err_max = enc->field;
            }
            check_str_eq(y_err_min.c_str(), "lo");
            check_str_eq(y_err_max.c_str(), "hi");
            // Check styles
            check(mark->styles.count("cap-size") > 0);
            check(std::get<double>(mark->styles["cap-size"]) > 11.9);
        }

        it("should parse encoding block") {
            const char* input =
                "errorbar {\n"
                "  encoding {\n"
                "    x: \"quarter\"\n"
                "    y: \"revenue\"\n"
                "    y-error: \"err\"\n"
                "    color: \"region\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorbar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 4);
        }

        it("should parse style block") {
            const char* input =
                "errorbar {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    cap-size: 10\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorbar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["cap-size"]) > 9.9);
        }

        it("should parse expr and range") {
            const char* input =
                "errorbar {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorbar_chart_parse_ast(input, error);
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
                "errorbar {\n"
                "  data: \"measurements.json\"\n"
                "  x: \"q\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::errorbar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "measurements.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::errorbar_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
