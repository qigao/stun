#include "tinytest.h"
#include "flexchart/radar/radar_parser.h"
#include <string>

spec("flexchart_radar_parser") {
    describe("radar_chart_parse_ast") {
        it("should parse a minimal radar chart") {
            const char* input =
                "radar {\n"
                "  title: \"Attributes\"\n"
                "  width: 600\n"
                "  height: 500\n"
                "  category: \"attr\"\n"
                "  value: \"score\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::radar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Attributes");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "500");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "radar");
            check(mark->encodings.size() >= 2);
            // Find category and value encodings
            std::string cat_field, val_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "category") cat_field = enc->field;
                if (enc->channel == "value") val_field = enc->field;
            }
            check_str_eq(cat_field.c_str(), "attr");
            check_str_eq(val_field.c_str(), "score");
        }

        it("should parse radar chart with data block") {
            const char* input =
                "radar {\n"
                "  title: \"Skills\"\n"
                "  data {\n"
                "    { \"attr\": \"Speed\", \"score\": 80 }\n"
                "    { \"attr\": \"Power\", \"score\": 65 }\n"
                "  }\n"
                "  category: \"attr\"\n"
                "  value: \"score\"\n"
                "  max: 100\n"
                "  fill-opacity: 0.3\n"
                "  grid-lines: 5\n"
                "}";
            std::string error;
            auto chart = flex::chart::radar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Skills");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check styles
            check(mark->styles.count("max") > 0);
            check(std::get<double>(mark->styles["max"]) > 99.9);
            check(mark->styles.count("fill-opacity") > 0);
            check(std::get<double>(mark->styles["fill-opacity"]) > 0.29);
            check(mark->styles.count("grid-lines") > 0);
            check(std::get<double>(mark->styles["grid-lines"]) > 4.9);
        }

        it("should parse encoding block") {
            const char* input =
                "radar {\n"
                "  encoding {\n"
                "    category: \"attr\"\n"
                "    value: \"score\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::radar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 2);
        }

        it("should parse style block") {
            const char* input =
                "radar {\n"
                "  category: \"a\"\n"
                "  style {\n"
                "    max: 100\n"
                "    fill-opacity: 0.5\n"
                "    grid-lines: 4\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::radar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["max"]) > 99.9);
            check(std::get<double>(mark->styles["fill-opacity"]) > 0.49);
            check(std::get<double>(mark->styles["grid-lines"]) > 3.9);
        }

        it("should parse expr and range") {
            const char* input =
                "radar {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  category: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::radar_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "sin(x) * 100");
            check(mark->ranges.count("category") > 0);
            auto& r = mark->ranges["category"];
            check(r.size() == 3);
            check(r[0] < 0.01);    // 0
            check(r[1] > 6.27);    // 6.28
            check(r[2] > 0.09);    // 0.1
        }

        it("should parse data source reference") {
            const char* input =
                "radar {\n"
                "  data: \"stats.json\"\n"
                "  category: \"attr\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::radar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "stats.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::radar_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
