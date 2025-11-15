#pragma once

#include <nanogui/widget.h>
#include <string>

namespace whiteboard {

class ToastNotification : public nanogui::Widget {
public:
    enum class Type {
        Info,
        Success,
        Warning,
        Error
    };
    
    ToastNotification(nanogui::Widget* parent);
    
    void show(const std::string& message, Type type = Type::Info, float duration = 3.0f);
    void hide();
    
    void draw(NVGcontext* ctx) override;
    
    bool is_visible() const { return m_target_alpha > 0.0f; }
    
private:
    std::string m_message;
    Type m_type;
    float m_alpha;              // Current alpha for animation
    float m_target_alpha;       // Target alpha (0 or 1)
    float m_duration;           // How long to show (seconds)
    float m_elapsed;            // Time elapsed since show
    
    void update_animation(float dt);
    nanogui::Color get_background_color() const;
    nanogui::Color get_text_color() const;
    int get_icon() const;
};

} // namespace whiteboard
