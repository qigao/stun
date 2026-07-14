#include "tinytest.h"
#include "flexchart/image/image_parser.h"
#include <string>

spec("flexchart_image_parser") {
    describe("image_chart_parse_ast") {
        it("should parse a minimal image chart") {
            const char* input =
                "image {\n"
                "  title: \"Icons\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "  url: \"icon\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::image_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Icons");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "image");
            check(mark->encodings.size() >= 3);
            // Find x, y, url encodings
            std::string x_field, y_field, url_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
                if (enc->channel == "url") url_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "category");
            check_str_eq(y_field.c_str(), "value");
            check_str_eq(url_field.c_str(), "icon");
        }

        it("should parse image chart with data block") {
            const char* input =
                "image {\n"
                "  title: \"Logos\"\n"
                "  data {\n"
                "    { \"category\": \"A\", \"value\": 30, \"icon\": \"a.png\" }\n"
                "    { \"category\": \"B\", \"value\": 50, \"icon\": \"b.png\" }\n"
                "  }\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "  url: \"icon\"\n"
                "  img-width: 32\n"
                "  img-height: 32\n"
                "}";
            std::string error;
            auto chart = flex::chart::image_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Logos");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check styles
            check(mark->styles.count("img-width") > 0);
            check(std::get<double>(mark->styles["img-width"]) > 31.9);
            check(mark->styles.count("img-height") > 0);
            check(std::get<double>(mark->styles["img-height"]) > 31.9);
        }

        it("should parse encoding block") {
            const char* input =
                "image {\n"
                "  encoding {\n"
                "    x: \"px\"\n"
                "    y: \"py\"\n"
                "    url: \"src\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::image_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 3);
        }

        it("should parse style block") {
            const char* input =
                "image {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    img-width: 64\n"
                "    img-height: 48\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::image_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["img-width"]) > 63.9);
            check(std::get<double>(mark->styles["img-height"]) > 47.9);
        }

        it("should parse expr and range") {
            const char* input =
                "image {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::image_chart_parse_ast(input, error);
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
                "image {\n"
                "  data: \"icons.json\"\n"
                "  x: \"q\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::image_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "icons.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::image_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
