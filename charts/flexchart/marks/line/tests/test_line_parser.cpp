#include "tinytest.h"
#include "flexchart/line/line_parser.h"

spec("flexchart_line_parser") {
    describe("line_chart_parse_ast") {
        it("should parse a minimal line chart") {
            const char* input =
                "line {\n"
                "  title: \"Trend\"\n"
                "  width: 600\n"
                "  height: 400\n"
                "  x: \"month\"\n"
                "  y: \"value\"\n"
                "  stroke-width: 2\n"
                "  interpolate: monotone\n"
                "  point: true\n"
                "  dash: \"4 2\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::line_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(chart->title.c_str(), "Trend");
            check_str_eq(chart->width.c_str(), "600");
            check_str_eq(chart->height.c_str(), "400");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "line");
            check(mark->encodings.size() == 2);
            // Check styles
            auto it_sw = mark->styles.find("stroke-width");
            check(it_sw != mark->styles.end());
            check(std::get<double>(it_sw->second) == 2.0);
            auto it_interp = mark->styles.find("interpolate");
            check(it_interp != mark->styles.end());
            check_str_eq(std::get<std::string>(it_interp->second).c_str(), "monotone");
            auto it_pt = mark->styles.find("point");
            check(it_pt != mark->styles.end());
            check(std::get<bool>(it_pt->second) == true);
            auto it_dash = mark->styles.find("dash");
            check(it_dash != mark->styles.end());
            check_str_eq(std::get<std::string>(it_dash->second).c_str(), "4 2");
        }

        it("should parse encoding and style blocks") {
            const char* input =
                "line {\n"
                "  encoding {\n"
                "    x: \"date\"\n"
                "    y: \"price\"\n"
                "  }\n"
                "  style {\n"
                "    stroke-width: 3\n"
                "    interpolate: linear\n"
                "    point: false\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::line_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check(mark->encodings.size() == 2);
            check_str_eq(mark->encodings[0]->channel.c_str(), "x");
            check_str_eq(mark->encodings[0]->field.c_str(), "date");
            auto it_sw = mark->styles.find("stroke-width");
            check(it_sw != mark->styles.end());
            check(std::get<double>(it_sw->second) == 3.0);
            auto it_pt = mark->styles.find("point");
            check(it_pt != mark->styles.end());
            check(std::get<bool>(it_pt->second) == false);
        }

        it("should parse inline data") {
            const char* input =
                "line {\n"
                "  x: \"a\"\n"
                "  y: \"b\"\n"
                "  data {\n"
                "    { \"a\": \"1\", \"b\": \"2\" }\n"
                "    { \"a\": \"3\", \"b\": \"4\" }\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = flex::chart::line_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(chart->datasets.size() == 1);
        }

        it("should parse data source reference") {
            const char* input =
                "line {\n"
                "  x: \"a\"\n"
                "  y: \"b\"\n"
                "  data: \"stocks.csv\"\n"
                "}";
            std::string error;
            auto chart = flex::chart::line_chart_parse_ast(input, error);
            check(chart != nullptr);
            check(chart->datasets.size() == 1);
            check_str_eq(chart->datasets[0]->source.c_str(), "stocks.csv");
        }

        it("should parse expr with ranges") {
            const char* input =
                "line {\n"
                "  expr: \"sin(x)\"\n"
                "  x: [0, 6.28, 0.1]\n"
                "}";
            std::string error;
            auto chart = flex::chart::line_chart_parse_ast(input, error);
            check(chart != nullptr);
            auto& mark = chart->marks[0];
            check_str_eq(mark->expr.c_str(), "sin(x)");
            check(mark->ranges.count("x") == 1);
            auto& r = mark->ranges["x"];
            check(r[0] == 0.0);
            check(r[2] == 0.1);
        }

        it("should return nullptr on invalid input") {
            std::string error;
            auto chart = flex::chart::line_chart_parse_ast("invalid", error);
            check(chart == nullptr);
            check(!error.empty());
        }

        it("should parse empty body") {
            std::string error;
            auto chart = flex::chart::line_chart_parse_ast("line {}", error);
            check(chart != nullptr);
            check(chart->marks.size() == 1);
            check_str_eq(chart->marks[0]->type.c_str(), "line");
        }
    }
}
