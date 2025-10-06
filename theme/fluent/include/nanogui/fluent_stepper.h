#pragma once

#include <nanogui/widget.h>
#include <vector>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Stepper
 * 
 * Displays progress through a sequence of logical steps.
 * Supports horizontal and vertical orientations.
 */
class NANOGUI_EXPORT FluentStepper : public Widget {
public:
    enum class Orientation {
        Horizontal,
        Vertical
    };
    
    enum class StepState {
        Incomplete,
        Active,
        Complete,
        Error
    };
    
    struct Step {
        std::string label;
        std::string description;
        StepState state;
        
        Step(const std::string &l, const std::string &d = "")
            : label(l), description(d), state(StepState::Incomplete) {}
    };
    
    FluentStepper(Widget *parent, Orientation orientation = Orientation::Horizontal);
    
    /// Add step
    void add_step(const std::string &label, const std::string &description = "");
    
    /// Current step
    int current_step() const { return m_current_step; }
    void set_current_step(int index);
    
    /// Step state
    void set_step_state(int index, StepState state);
    
    /// Navigate
    void next();
    void previous();
    
    /// Orientation
    Orientation orientation() const { return m_orientation; }
    void set_orientation(Orientation orientation) { m_orientation = orientation; }
    
    /// Callback when step changes
    std::function<void(int)> callback() const { return m_callback; }
    void set_callback(const std::function<void(int)> &callback) { 
        m_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    std::vector<Step> m_steps;
    int m_current_step;
    Orientation m_orientation;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
