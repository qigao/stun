#pragma once

#include <nanogui/widget.h>
#include <vector>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT FluentIOSSegmentedControl : public Widget {
public:
    FluentIOSSegmentedControl(Widget *parent, std::vector<std::string> items = {});

    void set_items(const std::vector<std::string> &items);
    const std::vector<std::string> &items() const { return m_items; }

    void set_selected_index(int index, bool emit = true);
    int selected_index() const { return m_selected; }

    void set_callback(const std::function<void(int)> &cb) { m_callback = cb; }

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

private:
    std::vector<std::string> m_items;
    int m_selected = 0;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
