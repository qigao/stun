/*
 * Flex Engine - Cubic Bezier Mathematics
 */

#include "flex/core/cubic_bezier.h"

#include <cmath>
#include <iterator>
#include <stdexcept>

namespace flex {
namespace {

constexpr std::uint32_t kMaximumArcLengthDepth = 20;
constexpr double kGaussLegendreNodes[] = {
    0.0, -0.5384693101056831, 0.5384693101056831, -0.9061798459386640, 0.9061798459386640,
};
constexpr double kGaussLegendreWeights[] = {
    0.5688888888888889, 0.4786286704993665, 0.4786286704993665,
    0.2369268850561891, 0.2369268850561891,
};

struct AdaptiveArcLength {
  double length = 0.0;
  double estimated_error = 0.0;
  std::uint32_t interval_count = 0;
  bool converged = true;
};

double integrate_interval(const CubicBezier2D &curve, double from, double to) noexcept {
  const double midpoint = 0.5 * (from + to);
  const double half_width = 0.5 * (to - from);
  double weighted_speed = 0.0;
  for (std::size_t index = 0; index < std::size(kGaussLegendreNodes); ++index) {
    const double parameter = midpoint + half_width * kGaussLegendreNodes[index];
    weighted_speed += kGaussLegendreWeights[index] *
                      static_cast<double>(curve.speed(static_cast<float>(parameter)));
  }
  return half_width * weighted_speed;
}

AdaptiveArcLength integrate_adaptive(const CubicBezier2D &curve, double from, double to,
                                     double coarse, double tolerance, std::uint32_t depth,
                                     std::uint32_t max_depth) noexcept {
  const double midpoint = 0.5 * (from + to);
  const double left = integrate_interval(curve, from, midpoint);
  const double right = integrate_interval(curve, midpoint, to);
  const double fine = left + right;
  const double error = std::abs(fine - coarse);

  if (error <= tolerance || depth >= max_depth) {
    return {fine, error, 2, error <= tolerance};
  }

  const AdaptiveArcLength left_result =
      integrate_adaptive(curve, from, midpoint, left, tolerance * 0.5, depth + 1, max_depth);
  const AdaptiveArcLength right_result =
      integrate_adaptive(curve, midpoint, to, right, tolerance * 0.5, depth + 1, max_depth);
  return {left_result.length + right_result.length,
          left_result.estimated_error + right_result.estimated_error,
          left_result.interval_count + right_result.interval_count,
          left_result.converged && right_result.converged};
}

} // namespace

CubicBezier2D::CubicBezier2D(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2,
                             const Vec2 &p3) noexcept
    : a_{-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x, -p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y},
      b_{3.0f * p0.x - 6.0f * p1.x + 3.0f * p2.x, 3.0f * p0.y - 6.0f * p1.y + 3.0f * p2.y},
      c_{-3.0f * p0.x + 3.0f * p1.x, -3.0f * p0.y + 3.0f * p1.y}, d_(p0) {}

Vec2 CubicBezier2D::position(float parameter) const noexcept {
  return {((a_.x * parameter + b_.x) * parameter + c_.x) * parameter + d_.x,
          ((a_.y * parameter + b_.y) * parameter + c_.y) * parameter + d_.y};
}

Vec2 CubicBezier2D::derivative(float parameter) const noexcept {
  return {(3.0f * a_.x * parameter + 2.0f * b_.x) * parameter + c_.x,
          (3.0f * a_.y * parameter + 2.0f * b_.y) * parameter + c_.y};
}

Vec2 CubicBezier2D::second_derivative(float parameter) const noexcept {
  return {6.0f * a_.x * parameter + 2.0f * b_.x, 6.0f * a_.y * parameter + 2.0f * b_.y};
}

float CubicBezier2D::speed(float parameter) const noexcept {
  const Vec2 velocity = derivative(parameter);
  return std::hypot(velocity.x, velocity.y);
}

ArcLengthResult CubicBezier2D::arc_length(float from, float to,
                                          const ArcLengthOptions &options) const {
  if (!std::isfinite(from) || !std::isfinite(to) || from < 0.0f || to > 1.0f || from > to) {
    throw std::invalid_argument("cubic Bezier arc-length range must satisfy 0 <= from <= to <= 1");
  }
  if (!std::isfinite(options.absolute_tolerance) || options.absolute_tolerance <= 0.0f) {
    throw std::invalid_argument("cubic Bezier arc-length tolerance must be finite and positive");
  }
  if (options.max_depth > kMaximumArcLengthDepth) {
    throw std::invalid_argument("cubic Bezier arc-length max_depth exceeds the bounded limit");
  }
  if (from == to) {
    return {};
  }

  const double coarse = integrate_interval(*this, from, to);
  const AdaptiveArcLength result =
      integrate_adaptive(*this, from, to, coarse, options.absolute_tolerance, 0, options.max_depth);
  return {static_cast<float>(result.length), static_cast<float>(result.estimated_error),
          result.interval_count, result.converged};
}

} // namespace flex
