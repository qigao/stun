/*
    nanogui/m3_segmented_button.h -- M3 Segmented Button

    Based on: https://m3.material.io/components/segmented-buttons
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3SegmentedButton : public Widget {
public:
    M3SegmentedButton(Widget *parent, const std::vector<std::string> &items = {},
                      bool multi_select = false);

    void set_items(const std::vector<std::string> &items) { m_items = items; }
    const std::vector<std::string> &items() const { return m_items; }

    void set_selected(int index, bool selected);
    bool is_selected(int index) const;
    
    void set_callback(const std::function<void(int, bool)> &callback) { m_callback = callback; }

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;
    int segment_at_position(const Vector2i &p) const;

    std::vector<std::string> m_items;
    std::vector<bool> m_selected;
    bool m_multi_select;
    int m_hover_index = -1;
    std::function<void(int, bool)> m_callback;
};

NAMESPACE_END(nanogui)
