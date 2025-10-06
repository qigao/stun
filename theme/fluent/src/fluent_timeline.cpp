#include <nanogui/fluent_timeline.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentTimeline::FluentTimeline(Widget *parent, Orientation orientation)
    : Widget(parent), m_orientation(orientation) {
}

void FluentTimeline::add_event(const std::string &title, const std::string &description,
                                 const std::string &time, int icon) {
    m_events.emplace_back(title, description, time, icon);
}

void FluentTimeline::clear_events() {
    m_events.clear();
}

Vector2i FluentTimeline::preferred_size_impl(NVGcontext *ctx) const {
    if (m_orientation == Orientation::Vertical) {
        return Vector2i(400, m_events.size() * 100);
    } else {
        return Vector2i(m_events.size() * 200, 150);
    }
}

void FluentTimeline::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme || m_events.empty()) return;
    
    if (m_orientation == Orientation::Vertical) {
        // Vertical timeline
        float x = m_pos.x() + 40;
        
        for (size_t i = 0; i < m_events.size(); ++i) {
            const auto &event = m_events[i];
            float y = m_pos.y() + i * 100 + 50;
            
            // Timeline line
            if (i < m_events.size() - 1) {
                nvgBeginPath(ctx);
                nvgMoveTo(ctx, x, y + 16);
                nvgLineTo(ctx, x, y + 84);
                nvgStrokeWidth(ctx, 2.0f);
                nvgStrokeColor(ctx, Color(0.7f, 0.7f, 0.7f, 1.0f));
                nvgStroke(ctx);
            }
            
            // Event marker
            nvgBeginPath(ctx);
            nvgCircle(ctx, x, y, 12);
            nvgFillColor(ctx, theme->primary_color());
            nvgFill(ctx);
            
            // Icon (if provided)
            if (event.icon != 0) {
                nvgFontSize(ctx, 16.0f);
                nvgFontFace(ctx, "icons");
                nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(ctx, theme->on_primary_color());
                
                char icon_str[8];
                snprintf(icon_str, sizeof(icon_str), "%c", (char)event.icon);
                nvgText(ctx, x, y, icon_str, nullptr);
            }
            
            // Time
            nvgFontSize(ctx, 12.0f);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, theme->on_surface_color());
            nvgText(ctx, x - 20, y, event.time.c_str(), nullptr);
            
            // Title
            nvgFontSize(ctx, 16.0f);
            nvgFontFace(ctx, "sans-bold");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgFillColor(ctx, theme->on_surface_color());
            nvgText(ctx, x + 24, y - 8, event.title.c_str(), nullptr);
            
            // Description
            if (!event.description.empty()) {
                nvgFontSize(ctx, 14.0f);
                nvgFontFace(ctx, "sans");
                nvgFillColor(ctx, theme->on_surface_color());
                nvgTextBox(ctx, x + 24, y + 12, m_size.x() - x - 48, 
                          event.description.c_str(), nullptr);
            }
        }
    } else {
        // Horizontal timeline
        float y = m_pos.y() + 60;
        
        for (size_t i = 0; i < m_events.size(); ++i) {
            const auto &event = m_events[i];
            float x = m_pos.x() + i * 200 + 100;
            
            // Timeline line
            if (i < m_events.size() - 1) {
                nvgBeginPath(ctx);
                nvgMoveTo(ctx, x + 16, y);
                nvgLineTo(ctx, x + 184, y);
                nvgStrokeWidth(ctx, 2.0f);
                nvgStrokeColor(ctx, Color(0.7f, 0.7f, 0.7f, 1.0f));
                nvgStroke(ctx);
            }
            
            // Event marker
            nvgBeginPath(ctx);
            nvgCircle(ctx, x, y, 12);
            nvgFillColor(ctx, theme->primary_color());
            nvgFill(ctx);
            
            // Icon (if provided)
            if (event.icon != 0) {
                nvgFontSize(ctx, 16.0f);
                nvgFontFace(ctx, "icons");
                nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(ctx, theme->on_primary_color());
                
                char icon_str[8];
                snprintf(icon_str, sizeof(icon_str), "%c", (char)event.icon);
                nvgText(ctx, x, y, icon_str, nullptr);
            }
            
            // Time
            nvgFontSize(ctx, 12.0f);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
            nvgFillColor(ctx, theme->on_surface_color());
            nvgText(ctx, x, y - 20, event.time.c_str(), nullptr);
            
            // Title
            nvgFontSize(ctx, 14.0f);
            nvgFontFace(ctx, "sans-bold");
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFillColor(ctx, theme->on_surface_color());
            nvgText(ctx, x, y + 20, event.title.c_str(), nullptr);
            
            // Description
            if (!event.description.empty()) {
                nvgFontSize(ctx, 12.0f);
                nvgFontFace(ctx, "sans");
                nvgFillColor(ctx, theme->on_surface_color());
                nvgTextBox(ctx, x - 80, y + 40, 160, event.description.c_str(), nullptr);
            }
        }
    }
}

NAMESPACE_END(nanogui)
