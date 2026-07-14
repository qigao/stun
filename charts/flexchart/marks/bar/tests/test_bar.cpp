#include "tinytest.h"
#include "flexchart/bar/bar_parser.h"
#include "flexchart/mark_renderer_registry.h"
#include "chart_component_internal.h"
#include <string>
#include <cmath>

extern "C" void flexchart_bar_force_link(void);
static auto _fl = (flexchart_bar_force_link(), 0);

using namespace flex::chart;

spec("flexchart_bar") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = MarkRendererRegistry::instance().create("bar");
            check(r != nullptr);
        }
    }

    describe("parse + data pipeline") {
        it("should parse inline data and produce correct records") {
            const char* input =
                "bar {\n"
                "  title: \"Sales\"\n"
                "  x: \"month\"\n"
                "  y: \"revenue\"\n"
                "  data {\n"
                "    { \"month\": \"Jan\", \"revenue\": 35 }\n"
                "    { \"month\": \"Feb\", \"revenue\": 84 }\n"
                "    { \"month\": \"Mar\", \"revenue\": 55 }\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = bar_chart_parse_ast(input, error);
            check(chart != nullptr);
            check_str_eq(error.c_str(), "");

            // Verify chart metadata
            check_str_eq(chart->title.c_str(), "Sales");
            check(chart->marks.size() == 1);
            auto& mark = chart->marks[0];
            check_str_eq(mark->type.c_str(), "bar");
            check_str_eq(mark->data_ref.c_str(), "default");

            // Verify encodings
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
            check_str_eq(x_field.c_str(), "month");
            check_str_eq(y_field.c_str(), "revenue");

            // Verify dataset exists
            check(chart->datasets.size() == 1);
            auto& ds = chart->datasets[0];
            check_str_eq(ds->name.c_str(), "default");

            // Verify inline_values: 3 rows × 2 fields × 2 (key+val) = 12
            check(ds->inline_values.size() == 12);

            // Check actual values in inline_values
            // Row 0: "month", "Jan", "revenue", "35"
            check(std::holds_alternative<std::string>(ds->inline_values[0]));
            check_str_eq(std::get<std::string>(ds->inline_values[0]).c_str(), "month");
            check(std::holds_alternative<std::string>(ds->inline_values[1]));
            check_str_eq(std::get<std::string>(ds->inline_values[1]).c_str(), "Jan");
            check(std::holds_alternative<std::string>(ds->inline_values[2]));
            check_str_eq(std::get<std::string>(ds->inline_values[2]).c_str(), "revenue");
            check(std::holds_alternative<std::string>(ds->inline_values[3]));
            check_str_eq(std::get<std::string>(ds->inline_values[3]).c_str(), "35");

            // Verify find_dataset
            auto found = find_dataset(chart, "default");
            check(found != nullptr);
            check(found == ds);

            // Verify get_records produces correct records
            auto records = get_records(found);
            check(records.size() == 3);

            // Record 0: month=Jan, revenue=35
            check_string_eq(get_string_val(records[0].get("month")), "Jan");
            check(get_double_val(records[0].get("revenue")) > 34.9);
            check(get_double_val(records[0].get("revenue")) < 35.1);

            // Record 1: month=Feb, revenue=84
            check_string_eq(get_string_val(records[1].get("month")), "Feb");
            check(get_double_val(records[1].get("revenue")) > 83.9);
            check(get_double_val(records[1].get("revenue")) < 84.1);

            // Record 2: month=Mar, revenue=55
            check_string_eq(get_string_val(records[2].get("month")), "Mar");
            check(get_double_val(records[2].get("revenue")) > 54.9);
            check(get_double_val(records[2].get("revenue")) < 55.1);
        }

        it("should build record_map matching x_labels") {
            const char* input =
                "bar {\n"
                "  x: \"cat\"\n"
                "  y: \"val\"\n"
                "  data {\n"
                "    { \"cat\": \"A\", \"val\": 10 }\n"
                "    { \"cat\": \"B\", \"val\": 20 }\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = bar_chart_parse_ast(input, error);
            check(chart != nullptr);

            auto& mark = chart->marks[0];
            std::string x_field, y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }

            auto ds = find_dataset(chart, mark->data_ref);
            check(ds != nullptr);
            auto records = get_records(ds);
            check(records.size() == 2);

            // Build record_map the same way the renderer does
            std::map<std::string, const Record*> record_map;
            for (const auto& rec : records) {
                record_map[get_string_val(rec.get(x_field))] = &rec;
            }

            // Simulate x_labels (built from data scan)
            std::vector<std::string> x_labels;
            for (const auto& rec : records) {
                x_labels.push_back(get_string_val(rec.get(x_field)));
            }
            check(x_labels.size() == 2);
            check_str_eq(x_labels[0].c_str(), "A");
            check_str_eq(x_labels[1].c_str(), "B");

            // Verify record_map lookup works for each label
            for (const auto& label : x_labels) {
                auto it = record_map.find(label);
                check(it != record_map.end());
                float val = (float)get_double_val(it->second->get(y_field));
                check(val > 0.0f);
            }

            // Verify specific values
            check(get_double_val(record_map["A"]->get(y_field)) > 9.9);
            check(get_double_val(record_map["A"]->get(y_field)) < 10.1);
            check(get_double_val(record_map["B"]->get(y_field)) > 19.9);
            check(get_double_val(record_map["B"]->get(y_field)) < 20.1);
        }

        it("should compute y_scale correctly") {
            const char* input =
                "bar {\n"
                "  x: \"item\"\n"
                "  y: \"amount\"\n"
                "  data {\n"
                "    { \"item\": \"X\", \"amount\": 50 }\n"
                "    { \"item\": \"Y\", \"amount\": 100 }\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = bar_chart_parse_ast(input, error);
            check(chart != nullptr);

            auto& mark = chart->marks[0];
            std::string y_field;
            for (auto& enc : mark->encodings) {
                if (enc->channel == "y") y_field = enc->field;
            }

            auto ds = find_dataset(chart, mark->data_ref);
            auto records = get_records(ds);

            // Compute data_y_max the same way chart_component does
            double data_y_max = 0;
            for (const auto& rec : records) {
                double v = get_double_val(rec.get(y_field));
                if (v > data_y_max) data_y_max = v;
            }
            check(data_y_max > 99.9);
            check(data_y_max < 100.1);

            // y_scale = estimated_plot_h / data_y_max
            float estimated_plot_h = 300.0f; // typical
            float y_scale = (data_y_max > 0) ? (estimated_plot_h / (float)data_y_max) : 1.0f;
            check(y_scale > 2.9f);
            check(y_scale < 3.1f);

            // Bar heights should be proportional
            float bar_h_x = (float)get_double_val(records[0].get(y_field)) * y_scale;
            float bar_h_y = (float)get_double_val(records[1].get(y_field)) * y_scale;
            check(bar_h_x > 149.0f);
            check(bar_h_x < 151.0f);
            check(bar_h_y > 299.0f);
            check(bar_h_y < 301.0f);
        }

        it("should handle numeric string values in records") {
            const char* input =
                "bar {\n"
                "  x: \"name\"\n"
                "  y: \"score\"\n"
                "  data {\n"
                "    { \"name\": \"Alice\", \"score\": 95 }\n"
                "  }\n"
                "}";
            std::string error;
            auto chart = bar_chart_parse_ast(input, error);
            check(chart != nullptr);

            auto ds = find_dataset(chart, "default");
            auto records = get_records(ds);
            check(records.size() == 1);

            // The value "95" is stored as string in inline_values
            // get_double_val should convert it
            auto& val = records[0].fields["score"];
            check(std::holds_alternative<std::string>(val));
            check_str_eq(std::get<std::string>(val).c_str(), "95");
            check(get_double_val(val) > 94.9);
            check(get_double_val(val) < 95.1);
        }
    }
}
