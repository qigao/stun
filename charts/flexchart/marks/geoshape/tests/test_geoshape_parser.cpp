#include "tinytest.h"
#include "flexchart/geoshape/geoshape_parser.h"
#include <string>

spec("flexchart_geoshape_parser") {
    describe("geoshape_chart_parse_ast") {
        it("should parse a minimal geoshape chart") {
            const char* input =
                "geoshape {\n"
                "  title: \"Geo Shape\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  shape: \"shape_data\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Geo Shape");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "geoshape");
            check(mark->encodings.size() >= 1);
            std::string shape_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "shape") shape_field = enc->field;
            }
            check_str_eq(shape_field.c_str(), "shape_data");
        }

        it("should parse geoshape chart with data block") {
            const char* input =
                "geoshape {\n"
                "  title: \"World Map\"\n"
                "  data {\n"
                "    { \"shape_data\": \"USA\", \"color\": \"blue\" }\n"
                "    { \"shape_data\": \"Canada\", \"color\": \"red\" }\n"
                "  }\n"
                "  shape: \"shape_data\"\n"
                "  projection: mercator\n"
                "  color: \"color\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "World Map");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check encodings
            check(mark->encodings.size() >= 2);
            std::string shape_f, color_f;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "shape") shape_f = enc->field;
                if (enc->channel == "color") color_f = enc->field;
            }
            check_str_eq(shape_f.c_str(), "shape_data");
            check_str_eq(color_f.c_str(), "color");
            // Check projection style
            check(mark->styles.count("projection") > 0);
            check_str_eq(std::get<std::string>(mark->styles["projection"]).c_str(), "mercator");
        }

        it("should parse encoding block") {
            const char* input =
                "geoshape {\n"
                "  encoding {\n"
                "    shape: \"region\"\n"
                "    color: \"population\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 2);
        }

        it("should parse style block") {
            const char* input =
                "geoshape {\n"
                "  shape: \"a\"\n"
                "  style {\n"
                "    projection: mercator\n"
                "    fill-opacity: 0.8\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(std::get<std::string>(mark->styles["projection"]).c_str(), "mercator");
            check(std::get<double>(mark->styles["fill-opacity"]) > 0.79);
        }

        it("should parse expr and range") {
            const char* input =
                "geoshape {\n"
                "  expr: \"geo(lat, lon)\"\n"
                "  shape: [0, 100, 1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "geo(lat, lon)");
            check(mark->ranges.count("shape") > 0);
            auto& r = mark->ranges["shape"];
            check(r.size() == 3);
            check(r[0] < 0.01);    // 0
            check(r[1] > 99.9);    // 100
            check(r[2] > 0.9);     // 1
        }

        it("should parse data source reference") {
            const char* input =
                "geoshape {\n"
                "  data: \"world.json\"\n"
                "  shape: \"region\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "world.json");
        }

        it("should parse projection as string") {
            const char* input =
                "geoshape {\n"
                "  shape: \"a\"\n"
                "  projection: \"albers-usa\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->styles.count("projection") > 0);
            check_str_eq(std::get<std::string>(mark->styles["projection"]).c_str(), "albers-usa");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::geoshape_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
