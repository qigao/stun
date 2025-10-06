#pragma once

#include <nanogui/common.h>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design easing curves for natural animations
 * 
 * Implements the official Fluent Design motion system with
 * cubic bezier easing curves for smooth, natural animations.
 */
class NANOGUI_EXPORT FluentEasing {
public:
    enum class Curve {
        Standard,      ///< Default easing for most animations
        Emphasized,    ///< Attention-grabbing, expressive motion
        Decelerated,   ///< Incoming elements (enter screen)
        Accelerated    ///< Outgoing elements (exit screen)
    };
    
    /// Apply easing curve to linear progress (0.0 to 1.0)
    static float ease(Curve curve, float t);
    
    /// Cubic bezier interpolation with control points
    static float cubic_bezier(float t, float p1, float p2, float p3, float p4);
};

/// Fluent Design duration constants (milliseconds)
namespace FluentDuration {
    constexpr int SHORT1 = 50;
    constexpr int SHORT2 = 100;
    constexpr int SHORT3 = 150;
    constexpr int SHORT4 = 200;
    constexpr int MEDIUM1 = 250;
    constexpr int MEDIUM2 = 300;
    constexpr int MEDIUM3 = 350;
    constexpr int MEDIUM4 = 400;
    constexpr int LONG1 = 450;
    constexpr int LONG2 = 500;
    constexpr int LONG3 = 550;
    constexpr int LONG4 = 600;
}

NAMESPACE_END(nanogui)
