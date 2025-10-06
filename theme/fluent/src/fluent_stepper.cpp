#include <nanogui/fluent_stepper.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentStepper::FluentStepper(Widget *parent, Orientation orientation)
    : Widget(parent), m_current_step(0), m_orientation(orientation) {
}

void FluentStepper::add_step(const std::string &label, const std::string &description) {
    m_steps.emplace_back(label, description);
    if (m_steps.size() == 1) {
        m_steps[0].state = StepState::Active;
    }
}

void FluentStepper::set_current_step(int index) {
    if (index < 0 || index >= (int)m_steps.size())
        return;
    
    // Update states
    if (m_current_step >= 0 && m_current_step < (int)m_steps.size()) {
        if (m_steps[m_current_step].state == StepState::Active) {
            m_steps[m_current_step].state = StepState::Complete;
        }
    }
    
    m_current_step = index;
    m_steps[index].state = StepState::Active;
    
    if (m_callback)
        m_callback(m_current_step);
}

void FluentStepper::set_step_state(int index, StepState state) {
    if (index >= 0 && index < (int)m_steps.size()) {
        m_steps[index].state = state;
    }
}

void FluentStepper::next() {
    if (m_current_step < (int)m_steps.size() - 1) {
        set_current_step(m_current_step + 1);
    }
}

void FluentStepper::previous() {
    if (m_current_step > 0) {
        set_current_step(m_current_step - 1);
    }
}

Vector2i FluentStepper::preferred_size_impl(NVGcontext *ctx) const {
    if (m_orientation == Orientation::Horizontal) {
        return Vector2i(m_parent ? m_parent->width() : 600, 80);
    } else {
        return Vector2i(300, m_steps.size() * 80);
    }
}

bool FluentStepper::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    // Allow clicking on completed steps to go back
    if (m_orientation == Orientation::Horizontal) {
        int step_width = m_size.x() / m_steps.size();
        int clicked_step = (p.x() - m_pos.x()) / step_width;
        
        if (clicked_step >= 0 && clicked_step < (int)m_steps.size()) {
            if (m_steps[clicked_step].state == StepState::Complete) {
                set_current_step(clicked_step);
                return true;
            }
        }
    } else {
        int clicked_step = (p.y() - m_pos.y()) / 80;
        
        if (clicked_step >= 0 && clicked_step < (int)m_steps.size()) {
            if (m_steps[clicked_step].state == StepState::Complete) {
                set_current_step(clicked_step);
                return true;
            }
        }
    }
    
    return false;
}

void FluentStepper::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme || m_steps.empty()) return;
    
    if (m_orientation == Orientation::Horizontal) {
        // Horizontal stepper
        int step_width = m_size.x() / m_steps.size();
        
        for (size_t i = 0; i < m_steps.size(); ++i) {
            const auto &step = m_steps[i];
            float x = m_pos.x() + i * step_width + step_width / 2;
            float y = m_pos.y() + 40;
            
            // Connector line (except for last step)
            if (i < m_steps.size() - 1) {
                nvgBeginPath(ctx);
                nvgMoveTo(ctx, x + 16, y);
                nvgLineTo(ctx, x + step_width - 16, y);
                nvgStrokeWidth(ctx, 2.0f);
                nvgStrokeColor(ctx, step.state == StepState::Complete ? 
                              theme->primary_color() : Color(0.7f, 0.7f, 0.7f, 1.0f));
                nvgStroke(ctx);
            }
            
            // Step circle
            nvgBeginPath(ctx);
            nvgCircle(ctx, x, y, 16);
            
            Color circle_color;
            if (step.state == StepState::Active) {
                circle_color = theme->primary_color();
            } else if (step.state == StepState::Complete) {
                circle_color = theme->primary_color();
            } else if (step.state == StepState::Error) {
                circle_color = Color(0.8f, 0.2f, 0.2f, 1.0f);
            } else {
                circle_color = theme->surface_color();
            }
            
            nvgFillColor(ctx, circle_color);
            nvgFill(ctx);
            
            // Step number or checkmark
            nvgFontSize(ctx, 14.0f);
            nvgFontFace(ctx, "sans-bold");
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, step.state == StepState::Incomplete ? 
                        theme->on_surface_color() : Color(1.0f, 1.0f, 1.0f, 1.0f));
            
            if (step.state == StepState::Complete) {
                nvgText(ctx, x, y, "✓", nullptr);
            } else if (step.state == StepState::Error) {
                nvgText(ctx, x, y, "!", nullptr);
            } else {
                char num[8];
                snprintf(num, sizeof(num), "%zu", i + 1);
                nvgText(ctx, x, y, num, nullptr);
            }
            
            // Label
            nvgFontSize(ctx, 12.0f);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFillColor(ctx, step.state == StepState::Active ? 
                        theme->on_surface_color() : theme->on_surface_color());
            nvgText(ctx, x, y + 24, step.label.c_str(), nullptr);
        }
    } else {
        // Vertical stepper
        for (size_t i = 0; i < m_steps.size(); ++i) {
            const auto &step = m_steps[i];
            float x = m_pos.x() + 40;
            float y = m_pos.y() + i * 80 + 40;
            
            // Connector line (except for last step)
            if (i < m_steps.size() - 1) {
                nvgBeginPath(ctx);
                nvgMoveTo(ctx, x, y + 16);
                nvgLineTo(ctx, x, y + 64);
                nvgStrokeWidth(ctx, 2.0f);
                nvgStrokeColor(ctx, step.state == StepState::Complete ? 
                              theme->primary_color() : Color(0.7f, 0.7f, 0.7f, 1.0f));
                nvgStroke(ctx);
            }
            
            // Step circle
            nvgBeginPath(ctx);
            nvgCircle(ctx, x, y, 16);
            
            Color circle_color;
            if (step.state == StepState::Active) {
                circle_color = theme->primary_color();
            } else if (step.state == StepState::Complete) {
                circle_color = theme->primary_color();
            } else if (step.state == StepState::Error) {
                circle_color = Color(0.8f, 0.2f, 0.2f, 1.0f);
            } else {
                circle_color = theme->surface_color();
            }
            
            nvgFillColor(ctx, circle_color);
            nvgFill(ctx);
            
            // Step number or checkmark
            nvgFontSize(ctx, 14.0f);
            nvgFontFace(ctx, "sans-bold");
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, step.state == StepState::Incomplete ? 
                        theme->on_surface_color() : Color(1.0f, 1.0f, 1.0f, 1.0f));
            
            if (step.state == StepState::Complete) {
                nvgText(ctx, x, y, "✓", nullptr);
            } else if (step.state == StepState::Error) {
                nvgText(ctx, x, y, "!", nullptr);
            } else {
                char num[8];
                snprintf(num, sizeof(num), "%zu", i + 1);
                nvgText(ctx, x, y, num, nullptr);
            }
            
            // Label and description
            nvgFontSize(ctx, 14.0f);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, step.state == StepState::Active ? 
                        theme->on_surface_color() : theme->on_surface_color());
            nvgText(ctx, x + 24, y - 8, step.label.c_str(), nullptr);
            
            if (!step.description.empty()) {
                nvgFontSize(ctx, 12.0f);
                nvgFillColor(ctx, theme->on_surface_color());
                nvgText(ctx, x + 24, y + 8, step.description.c_str(), nullptr);
            }
        }
    }
}

NAMESPACE_END(nanogui)
