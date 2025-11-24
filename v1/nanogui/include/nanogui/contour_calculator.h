/*
    nanogui/contour_calculator.h -- Calculate precise contours for selection frames
*/

#pragma once

#include <cmath>
#include <nanogui/common.h>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

NAMESPACE_BEGIN(nanogui)

struct Point2D {
  float x, y;
  Point2D(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
};

class NANOGUI_EXPORT ContourCalculator {
public:
  /**
   * Calculate contour for a rounded rectangle (like text fields, buttons)
   * Returns a series of points that trace the outline
   */
  static std::vector<Point2D> rounded_rect_contour(float x, float y, float w, float h, float radius,
                                                   int segments_per_corner = 8) {

    std::vector<Point2D> contour;
    contour.reserve(segments_per_corner * 4 + 4);

    // Clamp radius to half of smallest dimension
    radius = std::min(radius, std::min(w, h) / 2.0f);

    // Top-left corner (180° to 270°)
    for (int i = 0; i <= segments_per_corner; i++) {
      float angle = M_PI + i * (M_PI / 2.0f) / segments_per_corner;
      contour.push_back(
          Point2D(x + radius + radius * std::cos(angle), y + radius + radius * std::sin(angle)));
    }

    // Top-right corner (270° to 0°)
    for (int i = 0; i <= segments_per_corner; i++) {
      float angle = -M_PI / 2.0f + i * (M_PI / 2.0f) / segments_per_corner;
      contour.push_back(Point2D(x + w - radius + radius * std::cos(angle),
                                y + radius + radius * std::sin(angle)));
    }

    // Bottom-right corner (0° to 90°)
    for (int i = 0; i <= segments_per_corner; i++) {
      float angle = i * (M_PI / 2.0f) / segments_per_corner;
      contour.push_back(Point2D(x + w - radius + radius * std::cos(angle),
                                y + h - radius + radius * std::sin(angle)));
    }

    // Bottom-left corner (90° to 180°)
    for (int i = 0; i <= segments_per_corner; i++) {
      float angle = M_PI / 2.0f + i * (M_PI / 2.0f) / segments_per_corner;
      contour.push_back(Point2D(x + radius + radius * std::cos(angle),
                                y + h - radius + radius * std::sin(angle)));
    }

    return contour;
  }

  /**
   * Calculate contour for a circle
   */
  static std::vector<Point2D> circle_contour(float cx, float cy, float radius, int segments = 64) {

    std::vector<Point2D> contour;
    contour.reserve(segments);

    for (int i = 0; i < segments; i++) {
      float angle = 2.0f * M_PI * i / segments;
      contour.push_back(Point2D(cx + radius * std::cos(angle), cy + radius * std::sin(angle)));
    }

    return contour;
  }

  /**
   * Simplify contour using Douglas-Peucker algorithm
   * Reduces number of points while maintaining shape
   */
  static std::vector<Point2D> simplify_contour(const std::vector<Point2D> &contour,
                                               float epsilon = 1.0f) {

    if (contour.size() <= 2)
      return contour;

    // Find point with maximum distance from line
    float dmax = 0;
    size_t argmax = 0;

    Point2D p0 = contour[0];
    Point2D p1 = contour[contour.size() - 1];

    for (size_t i = 1; i < contour.size() - 1; i++) {
      float d = point_to_line_distance(contour[i], p0, p1);
      if (d > dmax) {
        dmax = d;
        argmax = i;
      }
    }

    // If max distance is greater than epsilon, recursively simplify
    std::vector<Point2D> result;
    if (dmax > epsilon) {
      // Simplify left segment
      std::vector<Point2D> left(contour.begin(), contour.begin() + argmax + 1);
      auto left_simplified = simplify_contour(left, epsilon);

      // Simplify right segment
      std::vector<Point2D> right(contour.begin() + argmax, contour.end());
      auto right_simplified = simplify_contour(right, epsilon);

      // Combine results (remove duplicate middle point)
      result.insert(result.end(), left_simplified.begin(), left_simplified.end() - 1);
      result.insert(result.end(), right_simplified.begin(), right_simplified.end());
    } else {
      // Base case: just keep endpoints
      result.push_back(contour[0]);
      result.push_back(contour[contour.size() - 1]);
    }

    return result;
  }

  /**
   * Calculate perpendicular distance from point to line segment
   */
  static float point_to_line_distance(const Point2D &p, const Point2D &p0, const Point2D &p1) {
    float x = p.x, y = p.y;
    float x1 = p0.x, y1 = p0.y;
    float x2 = p1.x, y2 = p1.y;

    float A = x - x1, B = y - y1;
    float C = x2 - x1, D = y2 - y1;

    float dot = A * C + B * D;
    float len_sq = C * C + D * D;

    if (len_sq < 1e-6f) {
      // Degenerate case: p0 and p1 are the same point
      return std::sqrt(A * A + B * B);
    }

    float param = dot / len_sq;

    float xx, yy;
    if (param < 0) {
      xx = x1;
      yy = y1;
    } else if (param > 1) {
      xx = x2;
      yy = y2;
    } else {
      xx = x1 + param * C;
      yy = y1 + param * D;
    }

    float dx = x - xx;
    float dy = y - yy;
    return std::sqrt(dx * dx + dy * dy);
  }

  /**
   * Offset a contour outward (or inward if offset is negative)
   * Used to create glow effects or padding
   */
  static std::vector<Point2D> offset_contour(const std::vector<Point2D> &contour, float offset) {

    if (contour.size() < 3)
      return contour;

    std::vector<Point2D> result;
    result.reserve(contour.size());
    size_t n = contour.size();

    for (size_t i = 0; i < n; i++) {
      Point2D prev = contour[(i - 1 + n) % n];
      Point2D curr = contour[i];
      Point2D next = contour[(i + 1) % n];

      // Calculate edge vectors
      float dx1 = curr.x - prev.x;
      float dy1 = curr.y - prev.y;
      float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);

      float dx2 = next.x - curr.x;
      float dy2 = next.y - curr.y;
      float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

      if (len1 > 1e-6f && len2 > 1e-6f) {
        // Perpendicular normals (pointing outward)
        float nx1 = -dy1 / len1;
        float ny1 = dx1 / len1;

        float nx2 = -dy2 / len2;
        float ny2 = dx2 / len2;

        // Average and normalize
        float nx = (nx1 + nx2);
        float ny = (ny1 + ny2);
        float nlen = std::sqrt(nx * nx + ny * ny);

        if (nlen > 1e-6f) {
          nx /= nlen;
          ny /= nlen;

          // Apply offset
          result.push_back(Point2D(curr.x + nx * offset, curr.y + ny * offset));
        } else {
          result.push_back(curr);
        }
      } else {
        result.push_back(curr);
      }
    }

    return result;
  }

  /**
   * Draw a contour using NanoVG
   */
  static void draw_contour(NVGcontext *ctx, const std::vector<Point2D> &contour,
                           const Color &stroke_color, float stroke_width, bool closed = true) {
    if (contour.empty())
      return;

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, contour[0].x, contour[0].y);

    for (size_t i = 1; i < contour.size(); i++) {
      nvgLineTo(ctx, contour[i].x, contour[i].y);
    }

    if (closed) {
      nvgClosePath(ctx);
    }

    nvgStrokeColor(ctx,
                   nvgRGBA(stroke_color.r(), stroke_color.g(), stroke_color.b(), stroke_color.a()));
    nvgStrokeWidth(ctx, stroke_width);
    nvgStroke(ctx);
  }

  /**
   * Draw contour with glow effect
   */
  static void draw_contour_with_glow(NVGcontext *ctx, const std::vector<Point2D> &base_contour,
                                     const Color &color, int glow_layers = 3,
                                     float glow_spacing = 3.0f) {
    // Draw glow layers (back to front)
    for (int i = glow_layers; i > 0; i--) {
      float offset = i * glow_spacing;
      auto glow_contour = offset_contour(base_contour, offset);

      int alpha = 60 / i; // Decreasing opacity
      // Color values are 0.0-1.0, need to scale to 0-255 for int constructor
      Color glow_color(static_cast<int>(color.r() * 255.0f), static_cast<int>(color.g() * 255.0f),
                       static_cast<int>(color.b() * 255.0f), alpha);

      draw_contour(ctx, glow_contour, glow_color, 2.0f);
    }

    // Draw main contour
    draw_contour(ctx, base_contour, color, 2.5f);
  }
};

NAMESPACE_END(nanogui)
