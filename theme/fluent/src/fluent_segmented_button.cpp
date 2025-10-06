#include <nanogui/fluent_segmented_button.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentSegmentedButton::FluentSegmentedButton(Widget *parent, SelectMode mode)
    : Widget(parent), m_mode(mode) {
}

void FluentSegmentedButton::add_segment(const std::string &label, int icon) {
    m_segments.emplace_back(label, icon);
}

std::vector<int> FluentSegmentedButton::selected_indices() const {
    std::vector<int> indices;
    for (size_t i = 0; i < m_segments.size(); ++i) {
        if (m_segments[i].selected)
            indices.push_back(i);
    }
    return indices;
}

void FluentSegmentedButton::set_selected(int index, bool selected) {
    if (index < 0 || index >= (int)m_segments.size())
        return;
    
    if (m_mode == SelectMode::Single && selected) {
        // Deselect all others
        for (auto &seg : m_segments)
            seg.selected = false;
    }
    
    m_segments[index].selected = selected;
    
    if (m_callback)
        m_callback(selected_indices());
}

Vector2i FluentSegmentedButton::preferred_size_impl(NVGcontext *ctx) const {
    if (m_segments.empty())
        return Vector2i(0, 40);
    
    nvgFontSize(ctx, 14.f);
    nvgFontFace(ctx, "sans");
    
    float total_width = 0.f;
    for (const auto &seg : m_segments) {
        float w = nvgTextBounds(ctx, 0, 0, seg.label.c_str(), nullptr, nullptr);
        total_width += w + 24; // padding
    }
    
    return Vector2i(total_width, 40);
}

bool FluentSegmentedButton::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1 || m_segments.empty())
        return false;
    
    float segment_width = (float)m_size.x() / m_segments.size();
    int index = (p.x() - m_pos.x()) / segment_width;
    
    if (index >= 0 && index < (int)m_segments.size()) {
        set_selected(index, !m_segments[index].selected);
        return true;
    }
    
    return false;
}

void FluentSegmentedButton::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme || m_segments.empty()) return;
    
    float segment_width = (float)m_size.x() / m_segments.size();
    
    for (size_t i = 0; i < m_segments.size(); ++i) {
        const auto &seg = m_segments[i];
        float x = m_pos.x() + i * segment_width;
        
        // Background
        nvgBeginPath(ctx);
        if (i == 0) {
            nvgRoundedRectVarying(ctx, x, m_pos.y(), segment_width, m_size.y(), 
                                  20, 0, 0, 20);
        } else if (i == m_segments.size() - 1) {
            nvgRoundedRectVarying(ctx, x, m_pos.y(), segment_width, m_size.y(), 
                                  0, 20, 20, 0);
        } else {
            nvgRect(ctx, x, m_pos.y(), segment_width, m_size.y());
        }
        
        nvgFillColor(ctx, seg.selected ? Color(0.9f, 0.9f, 1.0f, 1.0f) : theme->surface_color());
        nvgFill(ctx);
        
        // Border
        nvgStrokeWidth(ctx, 1.f);
        nvgStrokeColor(ctx, Color(0.7f, 0.7f, 0.7f, 1.0f));
        nvgStroke(ctx);
        
        // Label
        nvgFontSize(ctx, 14.f);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, seg.selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
        nvgText(ctx, x + segment_width * 0.5f, m_pos.y() + m_size.y() * 0.5f, 
                seg.label.c_str(), nullptr);
    }
}

NAMESPACE_END(nanogui)
