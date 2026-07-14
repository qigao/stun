#include "tinytest.h"
#include "flexchart/tick/tick_parser.h"
#include <string>

spec("flexchart_tick_parser") {
    describe("tick_chart_parse_ast") {
        it("should parse a minimal tick chart") {
            const char* input =
                "tick {\n"
                "  title: \"Ticks\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::tick_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Ticks");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "tick");
            check(mark->encodings.size() >= 2);
            // Find x and y encodings
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "category");
            check_str_eq(y_field.c_str(), "value");
        }

        it("should parse tick chart with data block") {
            const char* input =
                "tick {\n"
                "  title: \"Measurements\"\n"
                "  data {\n"
                "    { \"category\": \"A\", \"value\": 30 }\n"
                "    { \"category\": \"B\", \"value\": 50 }\n"
                "  }\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "  color: \"group\"\n"
                "  size: 15\n"
                "  orient: vertical\n"
                "}";
            std::string error;
            auto chart = flex::chart::tick_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Measurements");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check styles
            check(mark->styles.count("size") > 0);
            check(std::get<double>(mark->styles["size"]) > 14.9);
            check(mark->styles.count("orient") > 0);
            check_str_eq(std::get<std::string>(mark->styles["orient"]).c_str(), "vertical");
        }

        it("should parse encoding block") {
            const char* input =
                "tick {\n"
                "  encoding {\n"
                "    x: \"quarter\"\n"
                "    y: \"revenue\"\n"
                "    color: \"region\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::tick_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 3);
        }

        it("should parse style block") {
            const char* input =
                "tick {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    size: 20\n"
                "    orient: horizontal\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::tick_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["size"]) > 19.9);
            check_str_eq(std::get<std::string>(mark->styles["orient"]).c_str(), "horizontal");
        }

        it("should parse expr and range") {
            const char* input =
                "tick {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::tick_chart_parse_ast(input, error);
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
                "tick {\n"
                "  data: \"measurements.json\"\n"
                "  x: \"q\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::tick_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "measurements.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::tick_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
