#pragma once

#include <nanogui/widget.h>
#include <nanogui/fluent_web_theme.h>

#include <chrono>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 toast / snackbar notification.
 *
 * Appears near the bottom edge of the screen, optionally auto-dismisses after a
 * timeout, and can expose an action button.
 */
class NANOGUI_EXPORT FluentWebToast : public Widget {
public:
    enum class Severity {
        Informational,
        Success,
        Warning,
        Error
    };

    FluentWebToast(Widget *parent,
                   const std::string &message = "",
                   Severity severity = Severity::Informational);

    void set_message(const std::string &message);
    const std::string &message() const { return m_message; }

    void set_severity(Severity severity);
    Severity severity() const { return m_severity; }

    void set_action(const std::string &label, const std::function<void()> &callback);
    void clear_action();

    void set_duration(std::chrono::milliseconds duration) { m_duration = duration; }
    std::chrono::milliseconds duration() const { return m_duration; }

    /// Show toast. Repositions near bottom center of parent screen.
    void show();
    /// Hide toast immediately.
    void hide();
    bool showing() const { return m_visible; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

private:
    struct Palette {
        Color background;
        Color text;
        Color icon;
        Color action;
    };

    Palette resolve_palette(const FluentWebTheme &theme) const;
    void reposition();

    std::string m_message;
    std::string m_action_label;
    Severity m_severity;
    bool m_visible;

    std::function<void()> m_action_callback;

    std::chrono::milliseconds m_duration;
    std::chrono::steady_clock::time_point m_show_time;

    float m_animation_progress;

    Vector2f m_action_min;
    Vector2f m_action_max;
};

NAMESPACE_END(nanogui)

