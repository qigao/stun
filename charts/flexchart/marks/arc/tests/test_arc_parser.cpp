#include "tinytest.h"
#include "flexchart/arc/arc_parser.h"
#include <string>

spec("flexchart_arc_parser") {
    describe("arc_chart_parse_ast") {
        it("should parse a minimal arc chart") {
            const char* input =
                "arc {\n"
                "  title: \"Donut\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  theta: \"value\"\n"
                "  color: \"category\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::arc_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Donut");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "arc");
            check(mark->encodings.size() >= 2);
            // Find theta and color encodings
            std::string theta_field, color_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "theta") theta_field = enc->field;
                if (enc->channel == "color") color_field = enc->field;
            }
            check_str_eq(theta_field.c_str(), "value");
            check_str_eq(color_field.c_str(), "category");
        }

        it("should parse arc chart with data block") {
            const char* input =
                "arc {\n"
                "  title: \"Sales\"\n"
                "  data {\n"
                "    { \"category\": \"Software\", \"value\": 45 }\n"
                "    { \"category\": \"Hardware\", \"value\": 60 }\n"
                "  }\n"
                "  theta: \"value\"\n"
                "  color: \"category\"\n"
                "  inner-radius: 50\n"
                "  outer-radius: 120\n"
                "  pad-angle: 0.05\n"
                "}";
            std::string error;
            auto chart = flex::chart::arc_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Sales");
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check styles
            check(mark->styles.count("inner-radius") > 0);
            check(std::get<double>(mark->styles["inner-radius"]) > 49.9);
            check(mark->styles.count("outer-radius") > 0);
            check(std::get<double>(mark->styles["outer-radius"]) > 119.9);
            check(mark->styles.count("pad-angle") > 0);
            check(std::get<double>(mark->styles["pad-angle"]) > 0.04);
        }

        it("should parse encoding block") {
            const char* input =
                "arc {\n"
                "  encoding {\n"
                "    theta: \"revenue\"\n"
                "    color: \"region\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::arc_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 2);
        }

        it("should parse style block") {
            const char* input =
                "arc {\n"
                "  theta: \"a\"\n"
                "  style {\n"
                "    inner-radius: 40\n"
                "    outer-radius: 100\n"
                "    pad-angle: 0.03\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::arc_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["inner-radius"]) > 39.9);
            check(std::get<double>(mark->styles["outer-radius"]) > 99.9);
            check(std::get<double>(mark->styles["pad-angle"]) > 0.02);
        }

        it("should parse expr and range") {
            const char* input =
                "arc {\n"
                "  expr: \"sin(x) * 100\"\n"
                "  theta: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::arc_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "sin(x) * 100");
            check(mark->ranges.count("theta") > 0);
            auto& r = mark->ranges["theta"];
            check(r.size() == 3);
            check(r[0] < 0.01);    // 0
            check(r[1] > 6.27);    // 6.28
            check(r[2] > 0.09);    // 0.1
        }

        it("should parse data source reference") {
            const char* input =
                "arc {\n"
                "  data: \"sales.json\"\n"
                "  theta: \"q\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::arc_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check_str_eq(chart->datasets[0]->source.c_str(), "sales.json");
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::arc_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
