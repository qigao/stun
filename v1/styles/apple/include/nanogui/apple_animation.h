/*
    nanogui/apple_animation.h -- Apple-style animation system

    Implements spring animations and easing curves matching Apple's design.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/common.h>
#include <nanogui/vector.h>
#include <functional>
#include <vector>
#include <memory>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleAnimation apple_animation.h nanogui/apple_animation.h
 *
 * \brief Animation system with Apple-style spring curves and easing
 */
class NANOGUI_EXPORT AppleAnimation {
public:
    /// Easing function types
    enum class Easing {
        Linear,           ///< Linear interpolation
        EaseIn,          ///< Ease in (slow start)
        EaseOut,         ///< Ease out (slow end)
        EaseInOut,       ///< Ease in and out
        Spring,          ///< Spring animation (Apple default)
        SpringBouncy,    ///< Bouncy spring
        SpringSnappy     ///< Snappy spring
    };

    /// Animation state
    enum class State {
        Idle,
        Running,
        Paused,
        Completed
    };

    /**
     * \brief Create an animation
     * 
     * \param duration Animation duration in seconds
     * \param easing Easing function type
     */
    AppleAnimation(float duration = 0.3f, Easing easing = Easing::Spring);

    /// Set animation duration
    void set_duration(float duration) { m_duration = duration; }
    float duration() const { return m_duration; }

    /// Set easing function
    void set_easing(Easing easing) { m_easing = easing; }
    Easing easing() const { return m_easing; }

    /// Set spring parameters (for spring easing)
    void set_spring_params(float damping, float stiffness, float mass = 1.0f);

    /// Set delay before animation starts
    void set_delay(float delay) { m_delay = delay; }
    float delay() const { return m_delay; }

    /// Set callback for animation updates (progress 0.0 to 1.0)
    void set_update_callback(const std::function<void(float)> &callback) {
        m_update_callback = callback;
    }

    /// Set callback for animation completion
    void set_completion_callback(const std::function<void()> &callback) {
        m_completion_callback = callback;
    }

    /// Start the animation
    void start();

    /// Pause the animation
    void pause();

    /// Resume the animation
    void resume();

    /// Stop and reset the animation
    void stop();

    /// Update animation (call each frame)
    void update(float delta_time);

    /// Get current progress (0.0 to 1.0)
    float progress() const { return m_progress; }

    /// Get eased progress (0.0 to 1.0)
    float eased_progress() const;

    /// Get current state
    State state() const { return m_state; }

    /// Check if animation is running
    bool is_running() const { return m_state == State::Running; }

    /// Check if animation is completed
    bool is_completed() const { return m_state == State::Completed; }

    /// Static easing functions
    static float ease_linear(float t);
    static float ease_in(float t);
    static float ease_out(float t);
    static float ease_in_out(float t);
    static float ease_spring(float t, float damping, float stiffness, float mass);

protected:
    float m_duration;
    float m_delay;
    float m_elapsed_time;
    float m_progress;
    Easing m_easing;
    State m_state;

    // Spring parameters
    float m_spring_damping;
    float m_spring_stiffness;
    float m_spring_mass;

    std::function<void(float)> m_update_callback;
    std::function<void()> m_completion_callback;
};

/**
 * \class AppleAnimator apple_animation.h nanogui/apple_animation.h
 *
 * \brief Animation manager for coordinating multiple animations
 */
class NANOGUI_EXPORT AppleAnimator {
public:
    /// Get singleton instance
    static AppleAnimator &instance();

    /// Add animation to be managed
    void add_animation(std::shared_ptr<AppleAnimation> animation);

    /// Remove animation
    void remove_animation(std::shared_ptr<AppleAnimation> animation);

    /// Update all animations
    void update(float delta_time);

    /// Clear all animations
    void clear();

    /// Animate a float value
    static std::shared_ptr<AppleAnimation> animate_float(
        float &value, float target, float duration = 0.3f,
        AppleAnimation::Easing easing = AppleAnimation::Easing::Spring,
        const std::function<void()> &completion = nullptr);

    /// Animate a color
    static std::shared_ptr<AppleAnimation> animate_color(
        Color &color, const Color &target, float duration = 0.3f,
        AppleAnimation::Easing easing = AppleAnimation::Easing::Spring,
        const std::function<void()> &completion = nullptr);

    /// Animate a Vector2f
    static std::shared_ptr<AppleAnimation> animate_vector2f(
        Vector2f &vec, const Vector2f &target, float duration = 0.3f,
        AppleAnimation::Easing easing = AppleAnimation::Easing::Spring,
        const std::function<void()> &completion = nullptr);

private:
    AppleAnimator() = default;
    std::vector<std::shared_ptr<AppleAnimation>> m_animations;
};

/**
 * \class AppleTransition apple_animation.h nanogui/apple_animation.h
 *
 * \brief Transition effects for widgets
 */
class NANOGUI_EXPORT AppleTransition {
public:
    enum class Type {
        Fade,           ///< Fade in/out
        Slide,          ///< Slide in/out
        Scale,          ///< Scale in/out
        SlideAndFade,   ///< Slide + fade
        ScaleAndFade    ///< Scale + fade
    };

    enum class Direction {
        Up,
        Down,
        Left,
        Right
    };

    /**
     * \brief Create a transition
     * 
     * \param type Transition type
     * \param duration Duration in seconds
     */
    AppleTransition(Type type = Type::Fade, float duration = 0.3f);

    /// Set transition direction (for slide transitions)
    void set_direction(Direction dir) { m_direction = dir; }

    /// Apply transition in
    void transition_in(Widget *widget, const std::function<void()> &completion = nullptr);

    /// Apply transition out
    void transition_out(Widget *widget, const std::function<void()> &completion = nullptr);

protected:
    Type m_type;
    Direction m_direction;
    float m_duration;
};

NAMESPACE_END(nanogui)
