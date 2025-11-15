/*
 * NanoVG CSS - Background Images (Sprint 31)
 *
 * Data structures for background image support.
 */

#ifndef NANOVG_CSS_BACKGROUND_H
#define NANOVG_CSS_BACKGROUND_H

// ============================================================================
// Background Images (Sprint 31)
// ============================================================================

/**
 * @brief Background size modes
 */
enum BackgroundSize {
    BG_SIZE_AUTO,
    BG_SIZE_COVER,
    BG_SIZE_CONTAIN,
    BG_SIZE_EXPLICIT  // Explicit width/height
};

/**
 * @brief Background position
 */
struct BackgroundPosition {
    float x;  // 0.0 = left, 0.5 = center, 1.0 = right (or px value)
    float y;  // 0.0 = top, 0.5 = center, 1.0 = bottom (or px value)
    bool x_is_percent;  // True if x is percentage, false if pixels
    bool y_is_percent;  // True if y is percentage, false if pixels

    BackgroundPosition() : x(0.0f), y(0.0f), x_is_percent(true), y_is_percent(true) {}
};

/**
 * @brief Background repeat modes
 */
enum BackgroundRepeat {
    BG_REPEAT,
    BG_NO_REPEAT,
    BG_REPEAT_X,
    BG_REPEAT_Y
};

#endif // NANOVG_CSS_BACKGROUND_H
