/*
    nanogui/fluent_animation.h -- Fluent Design Animation System

    Fluent Design uses specific animation curves and timings to create
    natural, responsive motion.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <chrono>
#include <functional>
#include <nanogui/common.h>


NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentAnimation fluent_animation.h nanogui/fluent_animation.h
 *
 * \brief Fluent Design animation system
 *
 * Provides animation curves and timings that match Microsoft's Fluent Design:
 * - Standard easing curves (Linear, Accelerate, Decelerate, Standard)
 * - Fluent-specific durations
 * - Connected animations support
 */
class FluentAnimation {
public:
  /// Fluent Design easing curves
  enum class Curve {
    Linear,     ///< Linear motion (no easing)
    Accelerate, ///< Accelerate (ease-in) - cubic-bezier(0.7, 0.0, 1.0, 0.5)
    Decelerate, ///< Decelerate (ease-out) - cubic-bezier(0.1, 0.9, 0.2, 1.0)
    Standard,   ///< Standard (ease-in-out) - cubic-bezier(0.8, 0.0, 0.2, 1.0)
    Express,    ///< Express (fast) - cubic-bezier(0.9, 0.1, 0.2, 1.0)
    Entrance,   ///< Entrance animation - cubic-bezier(0.0, 0.0, 0.2, 1.0)
    Exit        ///< Exit animation - cubic-bezier(0.7, 0.0, 1.0, 1.0)
  };

  /// Fluent Design animation durations (in milliseconds)
  enum class Duration {
    Instant = 0,   ///< No animation
    Fast = 150,    ///< Fast animations (hover, press)
    Normal = 250,  ///< Normal animations (most UI)
    Slow = 350,    ///< Slow animations (page transitions)
    VerySlow = 500 ///< Very slow animations (connected animations)
  };

  /**
   * \brief Evaluate easing curve at time t
   *
   * \param curve
   *     The easing curve to use
   *
   * \param t
   *     Time value (0.0 to 1.0)
   *
   * \return
   *     Eased value (0.0 to 1.0)
   */
  static float ease(Curve curve, float t);

  /**
   * \brief Cubic bezier easing function
   *
   * \param t
   *     Time value (0.0 to 1.0)
   *
   * \param p1x, p1y
   *     First control point
   *
   * \param p2x, p2y
   *     Second control point
   *
   * \return
   *     Eased value (0.0 to 1.0)
   */
  static float cubic_bezier(float t, float p1x, float p1y, float p2x,
                            float p2y);

  /**
   * \brief Get duration in seconds
   *
   * \param duration
   *     Duration enum value
   *
   * \return
   *     Duration in seconds
   */
  static float duration_seconds(Duration duration) {
    return static_cast<int>(duration) / 1000.0f;
  }

  /**
   * \brief Get duration in milliseconds
   *
   * \param duration
   *     Duration enum value
   *
   * \return
   *     Duration in milliseconds
   */
  static int duration_ms(Duration duration) {
    return static_cast<int>(duration);
  }

private:
  // Helper for cubic bezier calculation
  static float bezier_component(float t, float p1, float p2);
};

NAMESPACE_END(nanogui)
