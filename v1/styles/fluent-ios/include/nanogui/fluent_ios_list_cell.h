#pragma once

#include <nanogui/widget.h>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT FluentIOSListCell : public Widget {
public:
    enum class Accessory {
        None,
        Chevron
    };

    FluentIOSListCell(Widget *parent,
                      std::string title,
                      std::string subtitle = std::string(),
                      Accessory accessory = Accessory::None);

    void set_title(const std::string &title);
    const std::string &title() const { return m_title; }

    void set_subtitle(const std::string &subtitle);
    const std::string &subtitle() const { return m_subtitle; }

    void set_leading_icon(int icon) { m_leading_icon = icon; }
    int leading_icon() const { return m_leading_icon; }

    void set_accessory(Accessory accessory) { m_accessory = accessory; }
    Accessory accessory() const { return m_accessory; }

    void set_selected(bool selected) { m_selected = selected; }
    bool selected() const { return m_selected; }

    void set_callback(const std::function<void()> &cb) { m_callback = cb; }

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

private:
    std::string m_title;
    std::string m_subtitle;
    int m_leading_icon = 0;
    Accessory m_accessory;
    bool m_selected = false;
    std::function<void()> m_callback;
};

NAMESPACE_END(nanogui)
