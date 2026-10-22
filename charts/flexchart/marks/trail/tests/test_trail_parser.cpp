#include "tinytest.h"
#include "flexchart/trail/trail_parser.h"
#include <string>

spec("flexchart_trail_parser") {
    describe("trail_chart_parse_ast") {
        it("should parse a minimal trail chart") {
            const char* input =
                "trail {\n"
                "  title: \"Trail\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"x_val\"\n"
                "  y: \"y_val\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::trail_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Trail"));
            check(std::string(chart->width.c_str()) == std::string("600"));
            check(std::string(chart->height.c_str()) == std::string("400"));
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check(std::string(mark->type.c_str()) == std::string("trail"));
            check(mark->encodings.size() >= 2);
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check(std::string(x_field.c_str()) == std::string("x_val"));
            check(std::string(y_field.c_str()) == std::string("y_val"));
        }

        it("should parse trail chart with all encoding channels") {
            const char* input =
                "trail {\n"
                "  title: \"Movement\"\n"
                "  data {\n"
                "    { \"x_val\": 10, \"y_val\": 20, \"sz\": 5, \"clr\": \"red\" }\n"
                "    { \"x_val\": 15, \"y_val\": 25, \"sz\": 6, \"clr\": \"blue\" }\n"
                "  }\n"
                "  x: \"x_val\"\n"
                "  y: \"y_val\"\n"
                "  size: \"sz\"\n"
                "  color: \"clr\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::trail_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Movement"));
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 4);
            std::string size_field, color_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "size") size_field = enc->field;
                if (enc->channel == "color") color_field = enc->field;
            }
            check(std::string(size_field.c_str()) == std::string("sz"));
            check(std::string(color_field.c_str()) == std::string("clr"));
        }

        it("should parse encoding block") {
            const char* input =
                "trail {\n"
                "  encoding {\n"
                "    x: \"time\"\n"
                "    y: \"value\"\n"
                "    size: \"weight\"\n"
                "    color: \"group\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::trail_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 4);
        }

        it("should parse style block") {
            const char* input =
                "trail {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    opacity: 0.8\n"
                "    interpolate: \"monotone\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::trail_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["opacity"]) > 0.79);
            check(std::string(std::get<std::string>(mark->styles["interpolate"]).c_str()) == std::string("monotone"));
        }

        it("should parse expr and range") {
            const char* input =
                "trail {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::trail_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::string(mark->expr.c_str()) == std::string("sin(x) * 100"));
            check(mark->ranges.count("x") > 0);
            auto& r = mark->ranges["x"];
            check(r.size() == 3);
            check(r[0] < 0.01);
            check(r[1] > 6.27);
            check(r[2] > 0.09);
        }

        it("should parse data source reference") {
            const char* input =
                "trail {\n"
                "  data: \"movement.json\"\n"
                "  x: \"t\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::trail_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check(std::string(chart->datasets[0]->source.c_str()) == std::string("movement.json"));
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::trail_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
