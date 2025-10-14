/*
    src/apple_animation.cpp -- Apple-style animation implementation
*/

#include <nanogui/apple_animation.h>
#include <nanogui/widget.h>
#include <cmath>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

// ============================================================================
// AppleAnimation
// ============================================================================

AppleAnimation::AppleAnimation(float duration, Easing easing)
    : m_duration(duration), m_delay(0.0f), m_elapsed_time(0.0f),
      m_progress(0.0f), m_easing(easing), m_state(State::Idle),
      m_spring_damping(0.7f), m_spring_stiffness(300.0f), m_spring_mass(1.0f) {}

void AppleAnimation::set_spring_params(float damping, float stiffness, float mass) {
    m_spring_damping = damping;
    m_spring_stiffness = stiffness;
    m_spring_mass = mass;
}

void AppleAnimation::start() {
    m_state = State::Running;
    m_elapsed_time = 0.0f;
    m_progress = 0.0f;
}

void AppleAnimation::pause() {
    if (m_state == State::Running) {
        m_state = State::Paused;
    }
}

void AppleAnimation::resume() {
    if (m_state == State::Paused) {
        m_state = State::Running;
    }
}

void AppleAnimation::stop() {
    m_state = State::Idle;
    m_elapsed_time = 0.0f;
    m_progress = 0.0f;
}

void AppleAnimation::update(float delta_time) {
    if (m_state != State::Running)
        return;

    m_elapsed_time += delta_time;

    // Handle delay
    if (m_elapsed_time < m_delay)
        return;

    float adjusted_time = m_elapsed_time - m_delay;
    m_progress = std::min(1.0f, adjusted_time / m_duration);

    // Call update callback
    if (m_update_callback) {
        m_update_callback(eased_progress());
    }

    // Check completion
    if (m_progress >= 1.0f) {
        m_state = State::Completed;
        if (m_completion_callback) {
            m_completion_callback();
        }
    }
}

float AppleAnimation::eased_progress() const {
    switch (m_easing) {
        case Easing::Linear:
            return ease_linear(m_progress);
        case Easing::EaseIn:
            return ease_in(m_progress);
        case Easing::EaseOut:
            return ease_out(m_progress);
        case Easing::EaseInOut:
            return ease_in_out(m_progress);
        case Easing::Spring:
            return ease_spring(m_progress, 0.7f, 300.0f, 1.0f);
        case Easing::SpringBouncy:
            return ease_spring(m_progress, 0.5f, 200.0f, 1.0f);
        case Easing::SpringSnappy:
            return ease_spring(m_progress, 0.9f, 400.0f, 1.0f);
        default:
            return m_progress;
    }
}

// Static easing functions
float AppleAnimation::ease_linear(float t) {
    return t;
}

float AppleAnimation::ease_in(float t) {
    return t * t;
}

float AppleAnimation::ease_out(float t) {
    return t * (2.0f - t);
}

float AppleAnimation::ease_in_out(float t) {
    if (t < 0.5f) {
        return 2.0f * t * t;
    } else {
        return -1.0f + (4.0f - 2.0f * t) * t;
    }
}

float AppleAnimation::ease_spring(float t, float damping, float stiffness, float mass) {
    // Spring physics simulation
    // Based on: https://webkit.org/demos/spring/spring.js
    
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    float omega = std::sqrt(stiffness / mass);
    float zeta = damping / (2.0f * std::sqrt(stiffness * mass));

    if (zeta < 1.0f) {
        // Underdamped spring (oscillates)
        float omega_d = omega * std::sqrt(1.0f - zeta * zeta);
        float A = 1.0f;
        float B = (zeta * omega) / omega_d;
        
        float envelope = std::exp(-zeta * omega * t);
        float oscillation = std::cos(omega_d * t) + B * std::sin(omega_d * t);
        
        return 1.0f - envelope * oscillation;
    } else if (zeta == 1.0f) {
        // Critically damped spring (no oscillation)
        float envelope = std::exp(-omega * t);
        return 1.0f - envelope * (1.0f + omega * t);
    } else {
        // Overdamped spring (slow, no oscillation)
        float r1 = -omega * (zeta - std::sqrt(zeta * zeta - 1.0f));
        float r2 = -omega * (zeta + std::sqrt(zeta * zeta - 1.0f));
        float A = 1.0f / (r1 - r2);
        float B = -A;
        
        return 1.0f - (A * std::exp(r1 * t) + B * std::exp(r2 * t));
    }
}

// ============================================================================
// AppleAnimator
// ============================================================================

AppleAnimator &AppleAnimator::instance() {
    static AppleAnimator instance;
    return instance;
}

void AppleAnimator::add_animation(std::shared_ptr<AppleAnimation> animation) {
    m_animations.push_back(animation);
}

void AppleAnimator::remove_animation(std::shared_ptr<AppleAnimation> animation) {
    m_animations.erase(
        std::remove(m_animations.begin(), m_animations.end(), animation),
        m_animations.end());
}

void AppleAnimator::update(float delta_time) {
    // Update all animations
    for (auto it = m_animations.begin(); it != m_animations.end();) {
        (*it)->update(delta_time);
        
        // Remove completed animations
        if ((*it)->is_completed()) {
            it = m_animations.erase(it);
        } else {
            ++it;
        }
    }
}

void AppleAnimator::clear() {
    m_animations.clear();
}

std::shared_ptr<AppleAnimation> AppleAnimator::animate_float(
    float &value, float target, float duration,
    AppleAnimation::Easing easing,
    const std::function<void()> &completion) {
    
    float start_value = value;
    float delta = target - start_value;
    
    auto animation = std::make_shared<AppleAnimation>(duration, easing);
    animation->set_update_callback([&value, start_value, delta](float progress) {
        value = start_value + delta * progress;
    });
    
    if (completion) {
        animation->set_completion_callback(completion);
    }
    
    animation->start();
    instance().add_animation(animation);
    
    return animation;
}

std::shared_ptr<AppleAnimation> AppleAnimator::animate_color(
    Color &color, const Color &target, float duration,
    AppleAnimation::Easing easing,
    const std::function<void()> &completion) {
    
    Color start_color = color;
    
    auto animation = std::make_shared<AppleAnimation>(duration, easing);
    animation->set_update_callback([&color, start_color, target](float progress) {
        color = Color(
            start_color.r() + (target.r() - start_color.r()) * progress,
            start_color.g() + (target.g() - start_color.g()) * progress,
            start_color.b() + (target.b() - start_color.b()) * progress,
            start_color.a() + (target.a() - start_color.a()) * progress
        );
    });
    
    if (completion) {
        animation->set_completion_callback(completion);
    }
    
    animation->start();
    instance().add_animation(animation);
    
    return animation;
}

std::shared_ptr<AppleAnimation> AppleAnimator::animate_vector2f(
    Vector2f &vec, const Vector2f &target, float duration,
    AppleAnimation::Easing easing,
    const std::function<void()> &completion) {
    
    Vector2f start_vec = vec;
    
    auto animation = std::make_shared<AppleAnimation>(duration, easing);
    animation->set_update_callback([&vec, start_vec, target](float progress) {
        vec = Vector2f(
            start_vec.x() + (target.x() - start_vec.x()) * progress,
            start_vec.y() + (target.y() - start_vec.y()) * progress
        );
    });
    
    if (completion) {
        animation->set_completion_callback(completion);
    }
    
    animation->start();
    instance().add_animation(animation);
    
    return animation;
}

// ============================================================================
// AppleTransition
// ============================================================================

AppleTransition::AppleTransition(Type type, float duration)
    : m_type(type), m_direction(Direction::Up), m_duration(duration) {}

void AppleTransition::transition_in(Widget *widget,
                                    const std::function<void()> &completion) {
    if (!widget)
        return;

    widget->set_visible(true);

    switch (m_type) {
        case Type::Fade: {
            // Fade in from 0 to 1
            float opacity = 0.0f;
            AppleAnimator::animate_float(opacity, 1.0f, m_duration,
                                        AppleAnimation::Easing::EaseOut,
                                        completion);
            break;
        }
        case Type::Scale: {
            // Scale from 0.8 to 1.0
            Vector2f scale(0.8f, 0.8f);
            AppleAnimator::animate_vector2f(scale, Vector2f(1.0f, 1.0f),
                                           m_duration,
                                           AppleAnimation::Easing::Spring,
                                           completion);
            break;
        }
        case Type::Slide: {
            // Slide in based on direction
            // Implementation depends on widget position
            if (completion)
                completion();
            break;
        }
        default:
            if (completion)
                completion();
            break;
    }
}

void AppleTransition::transition_out(Widget *widget,
                                     const std::function<void()> &completion) {
    if (!widget)
        return;

    auto hide_widget = [widget, completion]() {
        widget->set_visible(false);
        if (completion)
            completion();
    };

    switch (m_type) {
        case Type::Fade: {
            // Fade out from 1 to 0
            float opacity = 1.0f;
            AppleAnimator::animate_float(opacity, 0.0f, m_duration,
                                        AppleAnimation::Easing::EaseIn,
                                        hide_widget);
            break;
        }
        case Type::Scale: {
            // Scale from 1.0 to 0.8
            Vector2f scale(1.0f, 1.0f);
            AppleAnimator::animate_vector2f(scale, Vector2f(0.8f, 0.8f),
                                           m_duration,
                                           AppleAnimation::Easing::EaseIn,
                                           hide_widget);
            break;
        }
        default:
            hide_widget();
            break;
    }
}

NAMESPACE_END(nanogui)
