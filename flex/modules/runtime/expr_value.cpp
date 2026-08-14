/*
 * Flex Engine - expression runtime values.
 */

#include "flex/core/expr_value.h"

#include <algorithm>
#include <cmath>
#include <limits>

extern "C" {
#include "linalg.h"
}

namespace flex {
namespace {

bool fits_int(std::size_t value) {
    return value <= static_cast<std::size_t>((std::numeric_limits<int>::max)());
}

std::vector<float> to_column_major(const NumericValue& value) {
    std::vector<float> out(value.values.size(), 0.0f);
    for (std::size_t r = 0; r < value.rows; ++r) {
        for (std::size_t c = 0; c < value.cols; ++c) {
            out[r + c * value.rows] = value.values[r * value.cols + c];
        }
    }
    return out;
}

std::vector<float> from_column_major(const std::vector<float>& values,
                                     std::size_t rows,
                                     std::size_t cols) {
    std::vector<float> out(values.size(), 0.0f);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            out[r * cols + c] = values[r + c * rows];
        }
    }
    return out;
}

} // namespace

NumericValue NumericValue::scalar(float value) {
    NumericValue out;
    out.shape = NumericShape::Scalar;
    out.rows = 1;
    out.cols = 1;
    out.values = {value};
    return out;
}

NumericValue NumericValue::vector(std::vector<float> values) {
    NumericValue out;
    out.shape = NumericShape::Vector;
    out.rows = values.size();
    out.cols = 1;
    out.values = std::move(values);
    return out;
}

NumericValue NumericValue::matrix(std::size_t rows, std::size_t cols, std::vector<float> values) {
    NumericValue out;
    out.shape = NumericShape::Matrix;
    out.rows = rows;
    out.cols = cols;
    out.values = std::move(values);
    return out;
}

bool NumericValue::valid() const {
    if (rows == 0 || cols == 0) {
        return false;
    }
    return values.size() == rows * cols;
}

std::optional<float> numeric_dot(const NumericValue& a, const NumericValue& b) {
    if (!a.valid() || !b.valid() || a.size() != b.size()) {
        return std::nullopt;
    }

    float result = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        result += a.values[i] * b.values[i];
    }
    return result;
}

std::optional<float> numeric_norm(const NumericValue& value) {
    if (!value.valid()) {
        return std::nullopt;
    }
    auto dot = numeric_dot(value, value);
    if (!dot) {
        return std::nullopt;
    }
    return std::sqrt(*dot);
}

std::optional<NumericValue> numeric_transpose(const NumericValue& value) {
    if (!value.valid()) {
        return std::nullopt;
    }

    std::vector<float> out(value.values.size(), 0.0f);
    for (std::size_t r = 0; r < value.rows; ++r) {
        for (std::size_t c = 0; c < value.cols; ++c) {
            out[c * value.rows + r] = value.values[r * value.cols + c];
        }
    }

    return NumericValue::matrix(value.cols, value.rows, std::move(out));
}

std::optional<NumericValue> numeric_matmul(const NumericValue& a, const NumericValue& b) {
    if (!a.valid() || !b.valid() || a.cols != b.rows ||
        !fits_int(a.rows) || !fits_int(a.cols) || !fits_int(b.cols)) {
        return std::nullopt;
    }

    std::vector<float> a_col_major = to_column_major(a);
    std::vector<float> b_col_major = to_column_major(b);
    std::vector<float> c_col_major(a.rows * b.cols, 0.0f);

    ::matmul("N", "N",
             static_cast<int>(a.rows),
             static_cast<int>(b.cols),
             static_cast<int>(a.cols),
             1.0f,
             a_col_major.data(),
             b_col_major.data(),
             0.0f,
             c_col_major.data());

    return NumericValue::matrix(
        a.rows,
        b.cols,
        from_column_major(c_col_major, a.rows, b.cols));
}

} // namespace flex
