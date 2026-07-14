#include "chart_component_internal.h"
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <tlog.h>

namespace flex {
namespace chart {
namespace {

constexpr size_t kMaxGeneratedRecords = 1'000'000;

void set_record_variables(Expr& evaluator,
                          const std::vector<std::string>& variables,
                          const Record& rec) {
    for (const auto& name : variables) {
        evaluator.set(name, static_cast<float>(get_double_val(rec.get(name))));
    }
}

} // namespace

std::string get_string_val(const AstValue& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<double>(v)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", std::get<double>(v));
        return buf;
    }
    return "";
}

double get_double_val(const AstValue& v) {
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<std::string>(v)) {
        try { return std::stod(std::get<std::string>(v)); } catch(...) {}
    }
    return 0.0;
}

std::shared_ptr<AstData> find_dataset(const std::shared_ptr<AstChart>& chart, const std::string& name) {
    for (const auto& ds : chart->datasets) {
        if (ds->name == name) return ds;
    }
    return nullptr;
}

std::shared_ptr<AstAxis> find_axis(const std::shared_ptr<AstChart>& chart, const std::string& name) {
    for (const auto& ax : chart->axes) {
        if (ax->name == name) return ax;
    }
    return nullptr;
}

bool is_expression(const std::string& s) {
    for (char c : s) {
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(')
            return true;
    }
    return false;
}

std::vector<Record> generate_expr_records(
    const std::string& expr,
    const std::map<std::string, std::vector<double>>& ranges) {
    std::vector<Record> records;

    // Find the primary range variable (first one with 3 elements: start, end, step)
    std::string primary_var;
    double start = 0, end = 0, step = 1;
    for (const auto& [name, range] : ranges) {
        if (range.size() >= 3) {
            primary_var = name;
            start = range[0];
            end = range[1];
            step = range[2];
            break;
        }
    }
    if (primary_var.empty()) return records;

    if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(step)) {
        throw std::invalid_argument("chart range values must be finite");
    }
    if (step == 0.0) {
        throw std::invalid_argument("chart range step must not be zero");
    }
    if ((end > start && step < 0.0) || (end < start && step > 0.0)) {
        throw std::invalid_argument("chart range step does not advance toward the end value");
    }

    const double distance = std::abs(end - start);
    const double estimated_count = std::floor(distance / std::abs(step)) + 1.0;
    if (!std::isfinite(estimated_count) ||
        estimated_count > static_cast<double>(kMaxGeneratedRecords)) {
        throw std::length_error("chart range exceeds the maximum generated record count");
    }

    records.reserve(static_cast<size_t>(estimated_count));

    Expr evaluator;
    evaluator.add_variable(primary_var);

    // Add any additional range variables
    for (const auto& [name, range] : ranges) {
        if (name != primary_var) evaluator.add_variable(name);
    }

    const auto in_range = [end, step](double value) {
        return step > 0.0 ? value <= end + step * 0.5
                          : value >= end + step * 0.5;
    };
    for (double v = start; in_range(v); v += step) {
        evaluator.set(primary_var, static_cast<float>(v));
        double result = evaluator.eval(expr);

        Record rec;
        rec.fields[primary_var] = v;
        rec.fields["y"] = result;
        records.push_back(std::move(rec));
    }
    return records;
}

std::vector<Record> get_records(const std::shared_ptr<AstData>& data) {
    std::vector<Record> records;
    if (!data) return records;

    // Expression-driven data generation
    if (!data->expr.empty() && !data->ranges.empty()) {
        records = generate_expr_records(data->expr, data->ranges);

        // Apply transforms if present
        if (!data->transforms.empty()) {
            records = apply_transforms(std::move(records), data->transforms);
        }
        return records;
    }

    // Inline data values: stored as alternating key/value pairs
    // Each row was flattened as [k1, v1, k2, v2, ...] by MarkParserContext
    if (!data->inline_values.empty()) {
        // First pass: determine field count from first row
        // We detect row boundaries by seeing the first key repeat
        std::string first_key;
        int fields_per_row = 0;
        for (size_t i = 0; i < data->inline_values.size(); i += 2) {
            const auto& key = std::get<std::string>(data->inline_values[i]);
            if (i == 0) {
                first_key = key;
            } else if (key == first_key) {
                fields_per_row = (int)(i / 2);
                break;
            }
        }
        if (fields_per_row == 0) fields_per_row = (int)(data->inline_values.size() / 2);

        for (size_t i = 0; i + 1 < data->inline_values.size(); i += fields_per_row * 2) {
            Record rec;
            for (int f = 0; f < fields_per_row && (i + f * 2 + 1) < data->inline_values.size(); f++) {
                const auto& key = std::get<std::string>(data->inline_values[i + f * 2]);
                const auto& val = data->inline_values[i + f * 2 + 1];
                rec.fields[key] = val;
            }
            records.push_back(std::move(rec));
        }

        if (!data->transforms.empty()) {
            records = apply_transforms(std::move(records), data->transforms);
        }
        return records;
    }

    // Existing behavior: treat transforms as records
    for (const auto& t : data->transforms) {
        records.push_back({t->properties});
    }
    return records;
}

std::vector<Record> apply_transforms(
    std::vector<Record> records,
    const std::vector<std::shared_ptr<AstTransform>>& transforms) {

    for (const auto& t : transforms) {
        if (t->type == "filter") {
            // Get filter expression from properties
            std::string filter_expr;
            auto it = t->properties.find("filter");
            if (it != t->properties.end())
                filter_expr = get_string_val(it->second);
            if (filter_expr.empty()) continue;

            Expr evaluator;
            // Discover variables from the expression
            auto variables = evaluator.compile(filter_expr);

            std::vector<Record> filtered;
            for (const auto& rec : records) {
                set_record_variables(evaluator, variables, rec);
                double result = evaluator.eval_compiled(filter_expr);
                if (result != 0.0) {
                    filtered.push_back(rec);
                }
            }
            records = std::move(filtered);

        } else if (t->type == "formula") {
            // Get formula expression and target field name
            std::string formula_expr, as_field;
            auto fit = t->properties.find("formula");
            if (fit != t->properties.end())
                formula_expr = get_string_val(fit->second);
            auto ait = t->properties.find("as");
            if (ait != t->properties.end())
                as_field = get_string_val(ait->second);
            if (formula_expr.empty() || as_field.empty()) continue;

            Expr evaluator;
            auto variables = evaluator.compile(formula_expr);

            for (auto& rec : records) {
                set_record_variables(evaluator, variables, rec);
                double result = evaluator.eval_compiled(formula_expr);
                rec.fields[as_field] = result;
            }
        }
    }
    return records;
}

void apply_computed_fields(
    std::vector<Record>& records,
    const std::vector<std::shared_ptr<AstEncoding>>& encodings) {

    for (const auto& enc : encodings) {
        if (!is_expression(enc->field)) continue;

        std::string synthetic = "__expr_" + enc->channel;
        Expr evaluator;
        auto variables = evaluator.compile(enc->field);

        for (auto& rec : records) {
            set_record_variables(evaluator, variables, rec);
            double result = evaluator.eval_compiled(enc->field);
            rec.fields[synthetic] = result;
        }

        // Record the synthetic field name inside each record so callers can
        // retrieve it without touching the shared AstEncoding object.
        // Callers that need the resolved field name should call
        // resolved_field_name(enc) instead of reading enc->field directly.
        for (auto& rec : records) {
            rec.fields["__synthetic_channel_" + enc->channel] = synthetic;
        }
    }
}


} // namespace chart
} // namespace flex
