#pragma once

#include <nanogui/widget.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 Message Bar (inline notification).
 *
 * Supports informational, success, warning, and error severities along with an
 * optional action button and close affordance.
 */
class NANOGUI_EXPORT FluentWebMessageBar : public Widget {
public:
    enum class Severity {
        Informational,
        Success,
        Warning,
        Error
    };

    FluentWebMessageBar(Widget *parent,
                        const std::string &message = "",
                        Severity severity = Severity::Informational);

    void set_message(const std::string &message);
    const std::string &message() const { return m_message; }

    void set_severity(Severity severity);
    Severity severity() const { return m_severity; }

    void set_action(const std::string &label, const std::function<void()> &callback);
    void clear_action();

    void set_closable(bool closable);
    bool closable() const { return m_closable; }

    void set_close_callback(const std::function<void()> &callback) { m_close_callback = callback; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

private:
    struct RectF {
        Vector2f min{0.f, 0.f};
        Vector2f max{0.f, 0.f};
        bool contains(const Vector2i &p) const {
            return p.x() >= min.x() && p.x() <= max.x() &&
                   p.y() >= min.y() && p.y() <= max.y();
        }
    };

    struct Palette {
        Color background;
        Color accent;
        Color border;
        Color text;
        Color icon;
        Color action;
    };

    Palette resolve_palette(const FluentWebTheme &theme) const;

    std::string m_message;
    std::string m_action_label;
    Severity m_severity;
    bool m_closable;

    std::function<void()> m_action_callback;
    std::function<void()> m_close_callback;

    RectF m_action_bounds;
    RectF m_close_bounds;
};

NAMESPACE_END(nanogui)

