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
            check_str_eq(chart->title.c_str(), "Distribution");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "boxplot");
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
            check_str_eq(x_f.c_str(), "category");
            check_str_eq(min_f.c_str(), "min_val");
            check_str_eq(q1_f.c_str(), "q1_val");
            check_str_eq(med_f.c_str(), "med_val");
            check_str_eq(q3_f.c_str(), "q3_val");
            check_str_eq(max_f.c_str(), "max_val");
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
            check_str_eq(chart->title.c_str(), "Box Plot");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "boxplot");
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
            check_str_eq(std::get<std::string>(mark->styles["fill-opacity"]).c_str(), "0.5");
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
            check_str_eq(chart->datasets[0]->source.c_str(), "stats.json");
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
            check_str_eq(mark->expr.c_str(), "quantile(x)");
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
