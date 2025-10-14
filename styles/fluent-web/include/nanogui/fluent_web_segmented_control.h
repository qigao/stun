#pragma once

#include <nanogui/widget.h>
#include <nanogui/fluent_web_theme.h>

#include <functional>
#include <string>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebSegmentedControl : public Widget {
public:
    struct Segment {
        std::string label;
        int icon;
        bool selected;
    };

    enum class SelectMode { Single, Multi };

    FluentWebSegmentedControl(Widget *parent, SelectMode mode = SelectMode::Single);

    void add_segment(const std::string &label, int icon = 0);
    void set_selected(int index, bool selected);
    std::vector<int> selected_indices() const;

    void set_mode(SelectMode mode);
    SelectMode mode() const { return m_mode; }
    void set_callback(const std::function<void(const std::vector<int>&)> &callback) { m_callback = callback; }


    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

private:
    std::vector<Segment> m_segments;
    SelectMode m_mode;
    std::function<void(const std::vector<int>&)> m_callback;
    float m_segment_padding;
};

NAMESPACE_END(nanogui)
