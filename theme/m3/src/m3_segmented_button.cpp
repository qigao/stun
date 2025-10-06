/*
    src/m3_segmented_button.cpp -- M3 Segmented Button implementation
*/

#include <nanogui/m3_segmented_button.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3SegmentedButton::M3SegmentedButton(Widget *parent, const std::vector<std::string> &items, bool multi_select)
    : Widget(parent), m_items(items), m_multi_select(multi_select) {
    m_selected.resize(items.size(), false);
    set_fixed_height(40);
}

M3Theme *M3SegmentedButton::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3SegmentedButton::set_selected(int index, bool selected) {
    if (index >= 0 && index < static_cast<int>(m_selected.size())) {
        if (!m_multi_select && selected) {
            for (size_t i = 0; i < m_selected.size(); ++i) {
                m_selected[i] = false;
            }
        }
        m_selected[index] = selected;
        if (m_callback) m_callback(index, selected);
    }
}

bool M3SegmentedButton::is_selected(int index) const {
    return index >= 0 && index < static_cast<int>(m_selected.size()) && m_selected[index];
}

int M3SegmentedButton::segment_at_position(const Vector2i &p) const {
    if (m_items.empty()) return -1;
    Vector2i local = p - m_pos;
    float seg_width = m_size.x() / static_cast<float>(m_items.size());
    int idx = static_cast<int>(local.x() / seg_width);
    return (idx >= 0 && idx < static_cast<int>(m_items.size())) ? idx : -1;
}

bool M3SegmentedButton::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT || !down) return false;
    
    int idx = segment_at_position(p);
    if (idx >= 0) {
        set_selected(idx, !m_selected[idx]);
        return true;
    }
    return false;
}

Vector2i M3SegmentedButton::preferred_size(NVGcontext *ctx) const {
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans-bold");
    
    float total_width = 0;
    for (const auto &item : m_items) {
        float tw = nvgTextBounds(ctx, 0, 0, item.c_str(), nullptr, nullptr);
        total_width += tw + 48;
    }
    return Vector2i(static_cast<int>(total_width), 40);
}

void M3SegmentedButton::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Widget::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();
    float seg_width = w / static_cast<float>(m_items.size());

    nvgSave(ctx);

    for (size_t i = 0; i < m_items.size(); ++i) {
        float sx = x + i * seg_width;
        bool selected = m_selected[i];
        bool hovered = (static_cast<int>(i) == m_hover_index);

        Color bg = selected ? theme->secondary_container() : Color(0,0,0,0);
        Color fg = selected ? theme->on_secondary_container() : theme->on_surface();

        if (bg.a() > 0) {
            nvgBeginPath(ctx);
            nvgRect(ctx, sx, y, seg_width, h);
            nvgFillColor(ctx, bg);
            nvgFill(ctx);
        }

        if (hovered && m_enabled) {
            nvgBeginPath(ctx);
            nvgRect(ctx, sx, y, seg_width, h);
            nvgFillColor(ctx, theme->state_layer(fg, 0.08f));
            nvgFill(ctx);
        }

        nvgFontSize(ctx, 14);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, fg);
        nvgText(ctx, sx + seg_width*0.5f, y + h*0.5f, m_items[i].c_str(), nullptr);

        if (i < m_items.size() - 1) {
            nvgBeginPath(ctx);
            nvgRect(ctx, sx + seg_width, y + 8, 1, h - 16);
            nvgFillColor(ctx, theme->outline());
            nvgFill(ctx);
        }
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x+0.5f, y+0.5f, w-1, h-1, 8);
    nvgStrokeWidth(ctx, 1.0f);
    nvgStrokeColor(ctx, theme->outline());
    nvgStroke(ctx);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
