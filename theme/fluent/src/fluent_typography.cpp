#include <nanogui/fluent_typography.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentTypography::TypeSpec FluentTypography::get_spec(Style style) {
    switch (style) {
        // Display styles
        case Style::DisplayLarge:
            return {57.f, 1.12f, -0.25f, 400};
        case Style::DisplayMedium:
            return {45.f, 1.16f, 0.f, 400};
        case Style::DisplaySmall:
            return {36.f, 1.22f, 0.f, 400};
        
        // Headline styles
        case Style::HeadlineLarge:
            return {32.f, 1.25f, 0.f, 400};
        case Style::HeadlineMedium:
            return {28.f, 1.29f, 0.f, 400};
        case Style::HeadlineSmall:
            return {24.f, 1.33f, 0.f, 400};
        
        // Title styles
        case Style::TitleLarge:
            return {22.f, 1.27f, 0.f, 400};
        case Style::TitleMedium:
            return {16.f, 1.5f, 0.15f, 500};
        case Style::TitleSmall:
            return {14.f, 1.43f, 0.1f, 500};
        
        // Body styles
        case Style::BodyLarge:
            return {16.f, 1.5f, 0.5f, 400};
        case Style::BodyMedium:
            return {14.f, 1.43f, 0.25f, 400};
        case Style::BodySmall:
            return {12.f, 1.33f, 0.4f, 400};
        
        // Label styles
        case Style::LabelLarge:
            return {14.f, 1.43f, 0.1f, 500};
        case Style::LabelMedium:
            return {12.f, 1.33f, 0.5f, 500};
        case Style::LabelSmall:
            return {11.f, 1.45f, 0.5f, 500};
    }
    
    return {14.f, 1.43f, 0.25f, 400}; // Default to BodyMedium
}

void FluentTypography::apply(NVGcontext *ctx, Style style, const char *font_face) {
    TypeSpec spec = get_spec(style);
    
    nvgFontSize(ctx, spec.size);
    nvgFontFace(ctx, font_face);
    nvgTextLetterSpacing(ctx, spec.letter_spacing);
    
    // Note: NanoVG doesn't directly support font weight selection
    // In production, you'd load different font files for different weights
    // and select the appropriate font face based on spec.weight
}

NAMESPACE_END(nanogui)
