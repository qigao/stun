#include "tinytest.h"
#include "flexchart/point/point_parser.h"
#include <string>

spec("flexchart_point_parser") {
    describe("point_chart_parse_ast") {
        it("should parse a minimal point chart") {
            const char* input =
                "point {\n"
                "  title: \"Scatter\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"x\"\n"
                "  y: \"y\"\n"
                "  size: \"size\"\n"
                "  color: \"category\"\n"
                "  shape: circle\n"
                "}";
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Scatter");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "point");
            check(mark->encodings.size() >= 4);
            // Find encodings
            std::string x_field, y_field, size_field, color_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
                if (enc->channel == "size") size_field = enc->field;
                if (enc->channel == "color") color_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "x");
            check_str_eq(y_field.c_str(), "y");
            check_str_eq(size_field.c_str(), "size");
            check_str_eq(color_field.c_str(), "category");
            // Check shape style
            check(mark->styles.count("shape") > 0);
            check_str_eq(std::get<std::string>(mark->styles["shape"]).c_str(), "circle");
        }

        it("should parse point chart with data block") {
            const char* input =
                "point {\n"
                "  title: \"Measurements\"\n"
                "  data {\n"
                "    { \"x\": 10, \"y\": 20, \"size\": 5 }\n"
                "    { \"x\": 30, \"y\": 40, \"size\": 8 }\n"
                "  }\n"
                "  x: \"x\"\n"
                "  y: \"y\"\n"
                "  size: \"size\"\n"
                "  shape: diamond\n"
                "}";
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Measurements");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            check(mark->styles.count("shape") > 0);
            check_str_eq(std::get<std::string>(mark->styles["shape"]).c_str(), "diamond");
        }

        it("should parse encoding block") {
            const char* input =
                "point {\n"
                "  encoding {\n"
                "    x: \"weight\"\n"
                "    y: \"height\"\n"
                "    size: \"age\"\n"
                "    color: \"gender\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 4);
        }

        it("should parse style block") {
            const char* input =
                "point {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    shape: square\n"
                "    filled: true\n"
                "    opacity: 0.8\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(std::get<std::string>(mark->styles["shape"]).c_str(), "square");
            check(std::get<bool>(mark->styles["filled"]) == true);
            check(std::get<double>(mark->styles["opacity"]) > 0.79);
        }

        it("should parse expr and range") {
            const char* input =
                "point {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast(input, error);
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
                "point {\n"
                "  data: \"scatter.json\"\n"
                "  x: \"a\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "scatter.json");
        }

        it("should parse shape as string") {
            const char* input =
                "point {\n"
                "  x: \"a\"\n"
                "  shape: \"triangle-up\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(std::get<std::string>(mark->styles["shape"]).c_str(), "triangle-up");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::point_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
