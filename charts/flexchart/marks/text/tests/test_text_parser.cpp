#include "tinytest.h"
#include "flexchart/text/text_parser.h"
#include <string>

spec("flexchart_text_parser") {
    describe("text_chart_parse_ast") {
        it("should parse a minimal text chart") {
            const char* input =
                "text {\n"
                "  title: \"Labels\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::text_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Labels"));
            check(std::string(chart->width.c_str()) == std::string("600"));
            check(std::string(chart->height.c_str()) == std::string("400"));
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check(std::string(mark->type.c_str()) == std::string("text"));
            check(mark->encodings.size() >= 2);
            // Find x and y encodings
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check(std::string(x_field.c_str()) == std::string("category"));
            check(std::string(y_field.c_str()) == std::string("value"));
        }

        it("should parse text chart with data block and text encoding") {
            const char* input =
                "text {\n"
                "  title: \"Annotations\"\n"
                "  data {\n"
                "    { \"category\": \"A\", \"value\": 30, \"label\": \"Alpha\" }\n"
                "    { \"category\": \"B\", \"value\": 50, \"label\": \"Beta\" }\n"
                "  }\n"
                "  x: \"category\"\n"
                "  y: \"value\"\n"
                "  text-field: \"label\"\n"
                "  color: \"group\"\n"
                "  font-size: 16\n"
                "}";
            std::string error;
            auto chart = flex::chart::text_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Annotations"));
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check text encoding
            std::string text_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "text") text_field = enc->field;
            }
            check(std::string(text_field.c_str()) == std::string("label"));
            // Check font-size style
            check(mark->styles.count("font-size") > 0);
            check(std::get<double>(mark->styles["font-size"]) > 15.9);
        }

        it("should parse encoding block") {
            const char* input =
                "text {\n"
                "  encoding {\n"
                "    x: \"quarter\"\n"
                "    y: \"revenue\"\n"
                "    color: \"region\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::text_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 3);
        }

        it("should parse style block") {
            const char* input =
                "text {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    font-size: 24\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::text_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["font-size"]) > 23.9);
        }

        it("should parse expr and range") {
            const char* input =
                "text {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::text_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::string(mark->expr.c_str()) == std::string("sin(x) * 100"));
            check(mark->ranges.count("x") > 0);
            auto& r = mark->ranges["x"];
            check(r.size() == 3);
            check(r[0] < 0.01);    // 0
            check(r[1] > 6.27);    // 6.28
            check(r[2] > 0.09);    // 0.1
        }

        it("should parse data source reference") {
            const char* input =
                "text {\n"
                "  data: \"labels.json\"\n"
                "  x: \"q\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::text_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check(std::string(chart->datasets[0]->source.c_str()) == std::string("labels.json"));
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::text_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
