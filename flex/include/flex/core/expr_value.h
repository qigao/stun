/*
 * Flex Engine - expression runtime values.
 *
 * Numeric buffers are row-major at the Flex boundary. Vendor miniblas is
 * column-major, so conversion stays inside the math adapter implementation.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace flex {

enum class NumericShape : unsigned char {
    Scalar,
    Vector,
    Matrix,
};

struct NumericValue {
    NumericShape shape = NumericShape::Scalar;
    std::size_t rows = 1;
    std::size_t cols = 1;
    std::vector<float> values;

    static NumericValue scalar(float value);
    static NumericValue vector(std::vector<float> values);
    static NumericValue matrix(std::size_t rows, std::size_t cols, std::vector<float> values);

    bool valid() const;
    bool is_scalar() const { return shape == NumericShape::Scalar; }
    bool is_vector() const { return shape == NumericShape::Vector; }
    bool is_matrix() const { return shape == NumericShape::Matrix; }
    std::size_t size() const { return values.size(); }
};

std::optional<float> numeric_dot(const NumericValue& a, const NumericValue& b);
std::optional<float> numeric_norm(const NumericValue& value);
std::optional<NumericValue> numeric_transpose(const NumericValue& value);
std::optional<NumericValue> numeric_matmul(const NumericValue& a, const NumericValue& b);

} // namespace flex
