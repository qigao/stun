#include "tinytest.h"
#include "flexchart/boxplot/boxplot_parser.h"
#include <string>

spec("flexchart_boxplot_parser") {
    describe("boxplot_chart_parse_ast") {
        it("should parse a minimal boxplot chart") {
            const char* input =
                "boxplot {\n"
                "  title: \"Distribution\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"category\"\n"
                "  min: \"min_val\"\n"
                "  q1: \"q1_val\"\n"
                "  median: \"med_val\"\n"
                "  q3: \"q3_val\"\n"
                "  max: \"max_val\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::boxplot_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Distribution"));
            check(std::string(chart->width.c_str()) == std::string("600"));
            check(std::string(chart->height.c_str()) == std::string("400"));
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check(std::string(mark->type.c_str()) == std::string("boxplot"));
            check(mark->encodings.size() >= 6);
            // Find all encoding channels
            std::string x_f, min_f, q1_f, med_f, q3_f, max_f;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_f = enc->field;
                if (enc->channel == "min") min_f = enc->field;
                if (enc->channel == "q1") q1_f = enc->field;
                if (enc->channel == "median") med_f = enc->field;
                if (enc->channel == "q3") q3_f = enc->field;
                if (enc->channel == "max") max_f = enc->field;
            }
            check(std::string(x_f.c_str()) == std::string("category"));
            check(std::string(min_f.c_str()) == std::string("min_val"));
            check(std::string(q1_f.c_str()) == std::string("q1_val"));
            check(std::string(med_f.c_str()) == std::string("med_val"));
            check(std::string(q3_f.c_str()) == std::string("q3_val"));
            check(std::string(max_f.c_str()) == std::string("max_val"));
        }

        it("should parse boxplot chart with data block") {
            const char* input =
                "boxplot {\n"
                "  title: \"Box Plot\"\n"
                "  data {\n"
                "    { \"category\": \"A\", \"min_val\": 10, \"q1_val\": 20, \"med_val\": 30, \"q3_val\": 40, \"max_val\": 50 }\n"
                "    { \"category\": \"B\", \"min_val\": 15, \"q1_val\": 25, \"med_val\": 35, \"q3_val\": 45, \"max_val\": 55 }\n"
                "  }\n"
                "  x: \"category\"\n"
                "  min: \"min_val\"\n"
                "  q1: \"q1_val\"\n"
                "  median: \"med_val\"\n"
                "  q3: \"q3_val\"\n"
                "  max: \"max_val\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::boxplot_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Box Plot"));
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            check(std::string(mark->type.c_str()) == std::string("boxplot"));
        }

        it("should parse encoding block") {
            const char* input =
                "boxplot {\n"
                "  encoding {\n"
                "    x: \"category\"\n"
                "    min: \"lo\"\n"
                "    q1: \"q1\"\n"
                "    median: \"med\"\n"
                "    q3: \"q3\"\n"
                "    max: \"hi\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::boxplot_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 6);
        }

        it("should parse style block") {
            const char* input =
                "boxplot {\n"
                "  x: \"a\"\n"
                "  style {\n"
                "    box-width: 30\n"
                "    fill-opacity: \"0.5\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::boxplot_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["box-width"]) > 29.9);
            check(std::string(std::get<std::string>(mark->styles["fill-opacity"]).c_str()) == std::string("0.5"));
        }

        it("should parse data source reference") {
            const char* input =
                "boxplot {\n"
                "  data: \"stats.json\"\n"
                "  x: \"group\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::boxplot_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check(std::string(chart->datasets[0]->source.c_str()) == std::string("stats.json"));
        }

        it("should parse expr and range") {
            const char* input =
                "boxplot {\n"
                "  expr: \"quantile(x)\"\n"
                "  x: [0, 10, 1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::boxplot_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::string(mark->expr.c_str()) == std::string("quantile(x)"));
            check(mark->ranges.count("x") > 0);
            auto& r = mark->ranges["x"];
            check(r.size() == 3);
            check(r[0] < 0.01);
            check(r[1] > 9.99);
            check(r[2] > 0.99);
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::boxplot_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
