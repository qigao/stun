#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT FluentIOSNavigationBar : public Widget {
public:
    FluentIOSNavigationBar(Widget *parent, const std::string &title = "");

    void set_title(const std::string &title);
    const std::string &title() const { return m_title; }

    void set_subtitle(const std::string &subtitle);
    const std::string &subtitle() const { return m_subtitle; }

    void set_show_large_title(bool value) { m_show_large_title = value; }
    bool show_large_title() const { return m_show_large_title; }

    void set_back_button_visible(bool visible) { m_back_button = visible; }
    bool back_button_visible() const { return m_back_button; }

    void set_back_callback(const std::function<void()> &cb) { m_back_callback = cb; }

    Widget *trailing_container() const { return m_trailing_container; }

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void perform_layout(NVGcontext *ctx) override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

private:
    std::string m_title;
    std::string m_subtitle;
    bool m_show_large_title;
    bool m_back_button;
    std::function<void()> m_back_callback;
    Widget *m_trailing_container;
};

NAMESPACE_END(nanogui)
