/*
 * Flex Engine - Cubic Bezier Mathematics
 */

#pragma once

#include "flex/core/types.h"

#include <cstdint>

namespace flex {

struct ArcLengthOptions {
  float absolute_tolerance = 1.0e-4f;
  std::uint32_t max_depth = 12;
};

struct ArcLengthResult {
  float length = 0.0f;
  float estimated_error = 0.0f;
  std::uint32_t interval_count = 0;
  bool converged = true;
};

// Cubic polynomial with coefficients cached at construction time. Evaluation,
// differentiation, and acceleration are O(1) time and O(1) space.
class CubicBezier2D {
public:
  CubicBezier2D(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3) noexcept;

  // The parameter is intentionally not clamped so callers can choose whether
  // extrapolation is valid for their domain.
  Vec2 position(float parameter) const noexcept;
  Vec2 derivative(float parameter) const noexcept;
  Vec2 second_derivative(float parameter) const noexcept;
  float speed(float parameter) const noexcept;

  // Integrates |B'(u)| over [from, to]. Invalid ranges and unbounded options
  // fail fast with std::invalid_argument. Worst-case time is O(2^max_depth),
  // with O(max_depth) stack space and no dynamic allocation.
  ArcLengthResult arc_length(float from = 0.0f, float to = 1.0f,
                             const ArcLengthOptions &options = {}) const;

private:
  Vec2 a_;
  Vec2 b_;
  Vec2 c_;
  Vec2 d_;
};

} // namespace flex
