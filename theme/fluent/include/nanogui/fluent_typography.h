#pragma once

#include <nanogui/common.h>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design 3 Typography Scale
 * 
 * Provides the complete Fluent Design type system with
 * 15 semantic text styles for consistent hierarchy.
 */
class NANOGUI_EXPORT FluentTypography {
public:
    enum class Style {
        // Display - Large, short, important text
        DisplayLarge,    // 57sp
        DisplayMedium,   // 45sp
        DisplaySmall,    // 36sp
        
        // Headline - High-emphasis text
        HeadlineLarge,   // 32sp
        HeadlineMedium,  // 28sp
        HeadlineSmall,   // 24sp
        
        // Title - Medium-emphasis text
        TitleLarge,      // 22sp
        TitleMedium,     // 16sp
        TitleSmall,      // 14sp
        
        // Body - Main content text
        BodyLarge,       // 16sp
        BodyMedium,      // 14sp
        BodySmall,       // 12sp
        
        // Label - UI elements
        LabelLarge,      // 14sp
        LabelMedium,     // 12sp
        LabelSmall       // 11sp
    };
    
    struct TypeSpec {
        float size;          // Font size in pixels
        float line_height;   // Line height multiplier
        float letter_spacing; // Letter spacing in pixels
        int weight;          // Font weight (400=regular, 500=medium, 700=bold)
    };
    
    /// Get type specifications for a style
    static TypeSpec get_spec(Style style);
    
    /// Apply typography style to NanoVG context
    static void apply(NVGcontext *ctx, Style style, const char *font_face = "sans");
    
    /// Get font size for a style
    static float get_size(Style style) { return get_spec(style).size; }
    
    /// Get line height for a style
    static float get_line_height(Style style) { return get_spec(style).line_height; }
};

NAMESPACE_END(nanogui)
