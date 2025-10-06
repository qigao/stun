/*
    src/apple_effects.cpp -- Apple-style visual effects implementation
*/

#include <nanogui/apple_effects.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

// ============================================================================
// AppleMaterial
// ============================================================================

AppleMaterial::AppleMaterial(Type type)
    : m_type(type), m_blend_mode(BlendMode::WithinWindow), m_opacity(0.95f) {}

void AppleMaterial::apply(NVGcontext *ctx, float x, float y, float w, float h,
                          float corner_radius) {
    Color color = get_color(false); // TODO: Get actual dark mode state
    
    nvgBeginPath(ctx);
    if (corner_radius > 0.0f) {
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    } else {
        nvgRect(ctx, x, y, w, h);
    }
    
    // Apply translucent color
    nvgFillColor(ctx, Color(color.r(), color.g(), color.b(), m_opacity));
    nvgFill(ctx);
    
    // Add subtle gradient for depth
    NVGpaint gradient = nvgLinearGradient(ctx, x, y, x, y + h,
                                          nvgRGBA(255, 255, 255, 10),
                                          nvgRGBA(0, 0, 0, 10));
    nvgFillPaint(ctx, gradient);
    nvgFill(ctx);
}

Color AppleMaterial::get_color(bool dark_mode) const {
    if (dark_mode) {
        switch (m_type) {
            case Type::Regular:
            case Type::WindowBackground:
                return Color(28, 28, 30, 255);
            case Type::Thick:
                return Color(20, 20, 22, 255);
            case Type::Thin:
            case Type::Ultrathin:
                return Color(35, 35, 37, 255);
            case Type::Titlebar:
            case Type::HeaderView:
                return Color(40, 40, 42, 255);
            case Type::Sidebar:
                return Color(32, 32, 34, 255);
            case Type::Menu:
            case Type::Popover:
                return Color(44, 44, 46, 255);
            case Type::Sheet:
                return Color(28, 28, 30, 255);
            case Type::HUD:
                return Color(20, 20, 22, 230);
            default:
                return Color(28, 28, 30, 255);
        }
    } else {
        switch (m_type) {
            case Type::Regular:
            case Type::WindowBackground:
                return Color(255, 255, 255, 255);
            case Type::Thick:
                return Color(245, 245, 247, 255);
            case Type::Thin:
            case Type::Ultrathin:
                return Color(250, 250, 252, 255);
            case Type::Titlebar:
            case Type::HeaderView:
                return Color(242, 242, 247, 255);
            case Type::Sidebar:
                return Color(238, 238, 240, 255);
            case Type::Menu:
            case Type::Popover:
                return Color(252, 252, 254, 255);
            case Type::Sheet:
                return Color(255, 255, 255, 255);
            case Type::HUD:
                return Color(240, 240, 242, 230);
            default:
                return Color(255, 255, 255, 255);
        }
    }
}

// ============================================================================
// AppleVibrancy
// ============================================================================

AppleVibrancy::AppleVibrancy(Style style)
    : m_style(style), m_intensity(0.8f) {}

void AppleVibrancy::apply(NVGcontext *ctx, float x, float y, float w, float h,
                         float corner_radius) {
    Color color = get_color();
    
    nvgBeginPath(ctx);
    if (corner_radius > 0.0f) {
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    } else {
        nvgRect(ctx, x, y, w, h);
    }
    
    // Apply vibrancy color with intensity
    nvgFillColor(ctx, Color(color.r(), color.g(), color.b(), m_intensity));
    nvgFill(ctx);
    
    // Add subtle noise/texture effect (approximated with gradient)
    NVGpaint noise = nvgRadialGradient(ctx, x + w * 0.5f, y + h * 0.5f,
                                       w * 0.3f, w * 0.7f,
                                       nvgRGBA(255, 255, 255, 5),
                                       nvgRGBA(0, 0, 0, 5));
    nvgFillPaint(ctx, noise);
    nvgFill(ctx);
}

Color AppleVibrancy::get_color() const {
    switch (m_style) {
        case Style::Light:
        case Style::MediumLight:
            return Color(250, 250, 252, 200);
        case Style::Dark:
        case Style::UltraDark:
            return Color(28, 28, 30, 200);
        case Style::Titlebar:
        case Style::HeaderView:
            return Color(242, 242, 247, 220);
        case Style::Sidebar:
            return Color(238, 238, 240, 220);
        case Style::Menu:
        case Style::Popover:
            return Color(252, 252, 254, 240);
        case Style::Selection:
            return Color(0, 122, 255, 100);
        case Style::HUD:
            return Color(40, 40, 42, 230);
        default:
            return Color(250, 250, 252, 200);
    }
}

// ============================================================================
// AppleBlur
// ============================================================================

AppleBlur::AppleBlur(Style style, float radius)
    : m_style(style), m_radius(radius) {}

void AppleBlur::apply(NVGcontext *ctx, float x, float y, float w, float h,
                     float corner_radius) {
    // Approximate blur with layered box shadows
    Color base_color;
    
    switch (m_style) {
        case Style::Light:
        case Style::ExtraLight:
            base_color = Color(255, 255, 255, 200);
            break;
        case Style::Dark:
            base_color = Color(20, 20, 22, 200);
            break;
        case Style::Regular:
            base_color = Color(240, 240, 242, 200);
            break;
        case Style::Prominent:
            base_color = Color(230, 230, 232, 220);
            break;
    }
    
    // Draw multiple layers for blur effect
    for (int i = 0; i < 3; ++i) {
        float offset = i * (m_radius / 3.0f);
        float alpha = base_color.a() * (1.0f - i * 0.2f);
        
        NVGpaint shadow = nvgBoxGradient(ctx, x, y + offset, w, h,
                                         corner_radius, m_radius,
                                         nvgRGBA(base_color.r() * 255,
                                                base_color.g() * 255,
                                                base_color.b() * 255,
                                                alpha * 255),
                                         nvgRGBA(0, 0, 0, 0));
        
        nvgBeginPath(ctx);
        nvgRect(ctx, x - m_radius, y - m_radius, w + m_radius * 2, h + m_radius * 2);
        if (corner_radius > 0.0f) {
            nvgRoundedRect(ctx, x, y, w, h, corner_radius);
        } else {
            nvgRect(ctx, x, y, w, h);
        }
        nvgPathWinding(ctx, NVG_HOLE);
        nvgFillPaint(ctx, shadow);
        nvgFill(ctx);
    }
}

// ============================================================================
// AppleShadow
// ============================================================================

AppleShadow::AppleShadow(Elevation elevation)
    : m_elevation(elevation), m_color(0, 0, 0, 128) {}

void AppleShadow::apply(NVGcontext *ctx, float x, float y, float w, float h,
                       float corner_radius) {
    float blur_radius = 0.0f;
    float offset_y = 0.0f;
    float alpha = m_color.a();
    
    switch (m_elevation) {
        case Elevation::Level0:
            return; // No shadow
        case Elevation::Level1:
            blur_radius = 2.0f;
            offset_y = 1.0f;
            alpha *= 0.3f;
            break;
        case Elevation::Level2:
            blur_radius = 4.0f;
            offset_y = 2.0f;
            alpha *= 0.4f;
            break;
        case Elevation::Level3:
            blur_radius = 8.0f;
            offset_y = 4.0f;
            alpha *= 0.5f;
            break;
        case Elevation::Level4:
            blur_radius = 12.0f;
            offset_y = 6.0f;
            alpha *= 0.6f;
            break;
        case Elevation::Level5:
            blur_radius = 16.0f;
            offset_y = 8.0f;
            alpha *= 0.7f;
            break;
    }
    
    NVGpaint shadow = nvgBoxGradient(ctx, x, y + offset_y, w, h,
                                     corner_radius, blur_radius,
                                     nvgRGBA(m_color.r() * 255,
                                            m_color.g() * 255,
                                            m_color.b() * 255,
                                            alpha * 255),
                                     nvgRGBA(0, 0, 0, 0));
    
    nvgBeginPath(ctx);
    nvgRect(ctx, x - blur_radius, y - blur_radius, w + blur_radius * 2, h + blur_radius * 2);
    if (corner_radius > 0.0f) {
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    } else {
        nvgRect(ctx, x, y, w, h);
    }
    nvgPathWinding(ctx, NVG_HOLE);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);
}

NAMESPACE_END(nanogui)
