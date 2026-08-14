#include "flex/core/cubic_bezier.h"
#include "flex/core/path.h"
#include "tinytest.h"

#include <stdexcept>

using namespace flex;

spec("Flex cubic Bezier mathematics") {
  group("analytic evaluation") {
    it("evaluates endpoints and midpoint") {
      const CubicBezier2D curve({0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f});

      const Vec2 start = curve.position(0.0f);
      const Vec2 midpoint = curve.position(0.5f);
      const Vec2 end = curve.position(1.0f);
      check_float_within_abs(start.x, 0.0f, 1.0e-6f);
      check_float_within_abs(start.y, 0.0f, 1.0e-6f);
      check_float_within_abs(midpoint.x, 0.5f, 1.0e-6f);
      check_float_within_abs(midpoint.y, 0.75f, 1.0e-6f);
      check_float_within_abs(end.x, 1.0f, 1.0e-6f);
      check_float_within_abs(end.y, 0.0f, 1.0e-6f);
    }

    it("computes first and second derivatives") {
      const CubicBezier2D curve({0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f});

      const Vec2 start_velocity = curve.derivative(0.0f);
      const Vec2 midpoint_velocity = curve.derivative(0.5f);
      const Vec2 midpoint_acceleration = curve.second_derivative(0.5f);
      check_float_within_abs(start_velocity.x, 0.0f, 1.0e-6f);
      check_float_within_abs(start_velocity.y, 3.0f, 1.0e-6f);
      check_float_within_abs(midpoint_velocity.x, 1.5f, 1.0e-6f);
      check_float_within_abs(midpoint_velocity.y, 0.0f, 1.0e-6f);
      check_float_within_abs(midpoint_acceleration.x, 0.0f, 1.0e-6f);
      check_float_within_abs(midpoint_acceleration.y, -6.0f, 1.0e-6f);
    }
  }

  group("arc length integration") {
    it("integrates a straight cubic exactly within tolerance") {
      const CubicBezier2D line({0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f});

      const ArcLengthResult result = line.arc_length();
      check_float_within_abs(result.length, 3.0f, 1.0e-5f);
      check_true(result.converged);
      check_true(result.interval_count > 0);
    }

    it("exposes bounded non-convergence instead of hiding it") {
      const CubicBezier2D curve({0.0f, 0.0f}, {0.0f, 1000.0f}, {1.0f, -1000.0f}, {1.0f, 0.0f});

      const ArcLengthResult result = curve.arc_length(0.0f, 1.0f, {1.0e-7f, 0});
      check_false(result.converged);
      check_true(result.estimated_error > 1.0e-7f);
    }

    it("reports invalid integration options") {
      const CubicBezier2D curve({0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f});
      check_throws_as(curve.arc_length(-0.1f, 1.0f), std::invalid_argument);
      check_throws_as(curve.arc_length(0.0f, 1.0f, {0.0f, 4}), std::invalid_argument);
      check_throws_as(curve.arc_length(0.0f, 1.0f, {1.0e-4f, 21}), std::invalid_argument);
    }
  }

  group("Path Bezier mode") {
    it("uses explicit cubic control points instead of Catmull-Rom fallback") {
      Path path;
      path.set_interpolation_mode(Path::InterpolationMode::Bezier);
      path.add_point(0.0f, 0.0f, 0.0f);
      path.add_point(0.0f, 1.0f);
      path.add_point(1.0f, 1.0f);
      path.add_point(1.0f, 0.0f, 1.0f);

      check_true(path.is_valid());
      const PathPoint midpoint = path.interpolate(0.5f);
      check_float_within_abs(midpoint.x, 0.5f, 1.0e-6f);
      check_float_within_abs(midpoint.y, 0.75f, 1.0e-6f);
      check_float_within_abs(midpoint.time, 0.5f, 1.0e-6f);
    }

    it("rejects incomplete and non-monotonic cubic segments") {
      Path incomplete;
      incomplete.set_interpolation_mode(Path::InterpolationMode::Bezier);
      incomplete.add_point(0.0f, 0.0f, 0.0f);
      incomplete.add_point(0.0f, 1.0f);
      incomplete.add_point(1.0f, 1.0f);
      check_false(incomplete.is_valid());

      Path non_monotonic;
      non_monotonic.set_interpolation_mode(Path::InterpolationMode::Bezier);
      non_monotonic.add_point(0.0f, 0.0f, 1.0f);
      non_monotonic.add_point(0.0f, 1.0f);
      non_monotonic.add_point(1.0f, 1.0f);
      non_monotonic.add_point(1.0f, 0.0f, 0.0f);
      check_false(non_monotonic.is_valid());
    }

    it("samples consecutive cubic segments using endpoint times") {
      Path path;
      path.set_interpolation_mode(Path::InterpolationMode::Bezier);
      path.add_point(0.0f, 0.0f, 0.0f);
      path.add_point(0.0f, 1.0f);
      path.add_point(1.0f, 1.0f);
      path.add_point(1.0f, 0.0f, 0.5f);
      path.add_point(1.0f, -1.0f);
      path.add_point(2.0f, -1.0f);
      path.add_point(2.0f, 0.0f, 1.0f);

      check_true(path.is_valid());
      const PathPoint join = path.interpolate(0.5f);
      const PathPoint second_midpoint = path.interpolate(0.75f);
      check_float_within_abs(join.x, 1.0f, 1.0e-6f);
      check_float_within_abs(join.y, 0.0f, 1.0e-6f);
      check_float_within_abs(second_midpoint.x, 1.5f, 1.0e-6f);
      check_float_within_abs(second_midpoint.y, -0.75f, 1.0e-6f);
    }
  }
}
