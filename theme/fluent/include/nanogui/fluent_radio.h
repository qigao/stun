#pragma once

#include <nanogui/widget.h>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Radio Button
 * 
 * Radio buttons allow users to select one option from a set.
 * Follows Fluent Design 3 specifications.
 */
class NANOGUI_EXPORT FluentRadio : public Widget {
public:
    FluentRadio(Widget *parent, const std::string &caption = "");

    const std::string &caption() const { return m_caption; }
    void set_caption(const std::string &caption) { m_caption = caption; }

    bool checked() const { return m_checked; }
    void set_checked(bool checked);

    /// Callback when radio button is selected
    std::function<void(bool)> callback() const { return m_callback; }
    void set_callback(const std::function<void(bool)> &callback) { m_callback = callback; }

    /// Radio group - only one radio in a group can be selected
    int group() const { return m_group; }
    void set_group(int group) { m_group = group; }

    bool enabled() const { return m_enabled; }
    void set_enabled(bool enabled) { m_enabled = enabled; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;

protected:
    std::string m_caption;
    bool m_checked;
    bool m_enabled;
    int m_group;
    std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
