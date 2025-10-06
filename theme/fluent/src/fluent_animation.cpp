/*
    src/fluent_animation.cpp -- Fluent Design Animation System implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <algorithm>
#include <cmath>
#include <nanogui/fluent_animation.h>

NAMESPACE_BEGIN(nanogui)

float FluentAnimation::ease(Curve curve, float t) {
  t = std::clamp(t, 0.0f, 1.0f);

  switch (curve) {
  case Curve::Linear:
    return t;

  case Curve::Accelerate:
    // cubic-bezier(0.7, 0.0, 1.0, 0.5)
    return cubic_bezier(t, 0.7f, 0.0f, 1.0f, 0.5f);

  case Curve::Decelerate:
    // cubic-bezier(0.1, 0.9, 0.2, 1.0)
    return cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f);

  case Curve::Standard:
    // cubic-bezier(0.8, 0.0, 0.2, 1.0)
    return cubic_bezier(t, 0.8f, 0.0f, 0.2f, 1.0f);

  case Curve::Express:
    // cubic-bezier(0.9, 0.1, 0.2, 1.0)
    return cubic_bezier(t, 0.9f, 0.1f, 0.2f, 1.0f);

  case Curve::Entrance:
    // cubic-bezier(0.0, 0.0, 0.2, 1.0)
    return cubic_bezier(t, 0.0f, 0.0f, 0.2f, 1.0f);

  case Curve::Exit:
    // cubic-bezier(0.7, 0.0, 1.0, 1.0)
    return cubic_bezier(t, 0.7f, 0.0f, 1.0f, 1.0f);

  default:
    return t;
  }
}

float FluentAnimation::bezier_component(float t, float p1, float p2) {
  // Cubic bezier with fixed start (0) and end (1) points
  float t2 = t * t;
  float t3 = t2 * t;
  float mt = 1.0f - t;
  float mt2 = mt * mt;
  float mt3 = mt2 * mt;

  return 3.0f * mt2 * t * p1 + 3.0f * mt * t2 * p2 + t3;
}

float FluentAnimation::cubic_bezier(float t, float p1x, float p1y, float p2x,
                                    float p2y) {
  // Newton-Raphson iteration to solve for x
  // This is a simplified version - production code would use more iterations
  const int iterations = 8;
  float x = t;

  for (int i = 0; i < iterations; ++i) {
    float x_for_t = bezier_component(x, p1x, p2x);
    float error = x_for_t - t;

    if (std::abs(error) < 0.001f)
      break;

    // Derivative of bezier curve
    float dx = 3.0f * (1.0f - x) * (1.0f - x) * p1x +
               6.0f * (1.0f - x) * x * (p2x - p1x) +
               3.0f * x * x * (1.0f - p2x);

    if (std::abs(dx) < 0.000001f)
      break;

    x -= error / dx;
    x = std::clamp(x, 0.0f, 1.0f);
  }

  // Calculate y for the solved x
  return bezier_component(x, p1y, p2y);
}

NAMESPACE_END(nanogui)
