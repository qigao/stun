#include "tinytest.h"
#include "flexchart/pie/pie_parser.h"
#include <string>

spec("flexchart_pie_parser") {
    describe("pie_chart_parse_ast") {
        it("should parse a minimal pie chart") {
            const char* input =
                "pie {\n"
                "  title: \"Sales\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  theta: \"value\"\n"
                "  color: \"category\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::pie_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Sales"));
            check(std::string(chart->width.c_str()) == std::string("600"));
            check(std::string(chart->height.c_str()) == std::string("400"));
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check(std::string(mark->type.c_str()) == std::string("pie"));
            check(mark->encodings.size() >= 2);
            // Find theta and color encodings
            std::string theta_field, color_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "theta") theta_field = enc->field;
                if (enc->channel == "color") color_field = enc->field;
            }
            check(std::string(theta_field.c_str()) == std::string("value"));
            check(std::string(color_field.c_str()) == std::string("category"));
        }

        it("should parse pie chart with data block and styles") {
            const char* input =
                "pie {\n"
                "  title: \"Revenue\"\n"
                "  data {\n"
                "    { \"category\": \"Software\", \"value\": 45 }\n"
                "    { \"category\": \"Hardware\", \"value\": 60 }\n"
                "  }\n"
                "  theta: \"value\"\n"
                "  color: \"category\"\n"
                "  inner-radius: 50\n"
                "  outer-radius: 200\n"
                "  pad-angle: 0.02\n"
                "}";
            std::string error;
            auto chart = flex::chart::pie_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(std::string(chart->title.c_str()) == std::string("Revenue"));
            check(!chart->datasets.empty());
            check(!chart->datasets[0]->inline_values.empty());
            auto& mark = chart->marks[0];
            // Check styles
            check(mark->styles.count("inner-radius") > 0);
            check(std::get<double>(mark->styles["inner-radius"]) > 49.9);
            check(mark->styles.count("outer-radius") > 0);
            check(std::get<double>(mark->styles["outer-radius"]) > 199.9);
            check(mark->styles.count("pad-angle") > 0);
            check(std::get<double>(mark->styles["pad-angle"]) > 0.019);
        }

        it("should parse encoding block") {
            const char* input =
                "pie {\n"
                "  encoding {\n"
                "    theta: \"amount\"\n"
                "    color: \"region\"\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::pie_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 2);
        }

        it("should parse style block") {
            const char* input =
                "pie {\n"
                "  theta: \"v\"\n"
                "  style {\n"
                "    inner-radius: 30\n"
                "    outer-radius: 150\n"
                "    pad-angle: 0.05\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::pie_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::get<double>(mark->styles["inner-radius"]) > 29.9);
            check(std::get<double>(mark->styles["outer-radius"]) > 149.9);
            check(std::get<double>(mark->styles["pad-angle"]) > 0.049);
        }

        it("should parse expr and range") {
            const char* input =
                "pie {\n"
                "  expr: \"abs(x)\"\n"
                "  theta: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::pie_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(std::string(mark->expr.c_str()) == std::string("abs(x)"));
            check(mark->ranges.count("theta") > 0);
            auto& r = mark->ranges["theta"];
            check(r.size() == 3);
            check(r[0] < 0.01);    // 0
            check(r[1] > 6.27);    // 6.28
            check(r[2] > 0.09);    // 0.1
        }

        it("should parse data source reference") {
            const char* input =
                "pie {\n"
                "  data: \"sales.json\"\n"
                "  theta: \"amount\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::pie_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(!chart->datasets.empty());
            check(std::string(chart->datasets[0]->source.c_str()) == std::string("sales.json"));
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::pie_chart_parse_ast("invalid garbage", error);
            check(chart == nullptr);
            check(!error.empty());
        }
    }
}
