#pragma once

#include <nanovg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Filter types
typedef enum cssboxFilterType {
    cssbox_FILTER_NONE = 0,
    cssbox_FILTER_BLUR,
    cssbox_FILTER_BRIGHTNESS,
    cssbox_FILTER_CONTRAST,
    cssbox_FILTER_GRAYSCALE,
    cssbox_FILTER_HUE_ROTATE,
    cssbox_FILTER_INVERT,
    cssbox_FILTER_SATURATE,
    cssbox_FILTER_SEPIA,
    cssbox_FILTER_OPACITY
} cssboxFilterType;

// Filter parameters
typedef struct cssboxFilter {
    cssboxFilterType type;
    float value;  // Generic value (blur radius, brightness, etc.)
} cssboxFilter;

// Filter context (manages FBOs and shaders)
typedef struct cssboxFilterContext cssboxFilterContext;

// Create filter context
cssboxFilterContext* cssboxCreateFilterContext(void);

// Delete filter context
void cssboxDeleteFilterContext(cssboxFilterContext* ctx);

// Apply filters to a region
// Returns 1 on success, 0 on failure
int cssboxApplyFilters(
    cssboxFilterContext* ctx,
    NVGcontext* vg,
    float x, float y,
    float width, float height,
    const cssboxFilter* filters,
    int filter_count,
    void (*render_callback)(NVGcontext*, void*),
    void* user_data
);

#ifdef __cplusplus
}
#endif
