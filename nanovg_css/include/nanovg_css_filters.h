#pragma once

#include <nanovg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Filter types
typedef enum NVGCSSFilterType {
    NVGCSS_FILTER_NONE = 0,
    NVGCSS_FILTER_BLUR,
    NVGCSS_FILTER_BRIGHTNESS,
    NVGCSS_FILTER_CONTRAST,
    NVGCSS_FILTER_GRAYSCALE,
    NVGCSS_FILTER_HUE_ROTATE,
    NVGCSS_FILTER_INVERT,
    NVGCSS_FILTER_SATURATE,
    NVGCSS_FILTER_SEPIA,
    NVGCSS_FILTER_OPACITY
} NVGCSSFilterType;

// Filter parameters
typedef struct NVGCSSFilter {
    NVGCSSFilterType type;
    float value;  // Generic value (blur radius, brightness, etc.)
} NVGCSSFilter;

// Filter context (manages FBOs and shaders)
typedef struct NVGCSSFilterContext NVGCSSFilterContext;

// Create filter context
NVGCSSFilterContext* nvgcssCreateFilterContext(void);

// Delete filter context
void nvgcssDeleteFilterContext(NVGCSSFilterContext* ctx);

// Apply filters to a region
// Returns 1 on success, 0 on failure
int nvgcssApplyFilters(
    NVGCSSFilterContext* ctx,
    NVGcontext* vg,
    float x, float y,
    float width, float height,
    const NVGCSSFilter* filters,
    int filter_count,
    void (*render_callback)(NVGcontext*, void*),
    void* user_data
);

#ifdef __cplusplus
}
#endif
