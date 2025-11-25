/*
 * NanoVG Rough - Hand-drawn style rendering
 * 
 * C implementation inspired by rough.js (https://github.com/rough-stuff/rough)
 * Provides hand-drawn, sketchy appearance for shapes
 */

#ifndef NANOVG_ROUGH_H
#define NANOVG_ROUGH_H

#include <nanovg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Rough drawing options
typedef struct NVGRoughOptions {
    float roughness;           // 0-10, default 1
    float bowing;              // 0-10, default 1
    int stroke_count;          // 1-5, default 1 (number of strokes per shape)
    float stroke_width;        // Line width
    NVGcolor stroke_color;     // Stroke color
    NVGcolor fill_color;       // Fill color
    int fill_enabled;          // Whether to fill
    int stroke_enabled;        // Whether to stroke
    unsigned int seed;         // Random seed for reproducibility
} NVGRoughOptions;

// Create default options
NVGRoughOptions nvgRoughDefaultOptions(void);

// Draw rough shapes
void nvgRoughCircle(NVGcontext* vg, float cx, float cy, float radius, NVGRoughOptions opts);
void nvgRoughEllipse(NVGcontext* vg, float cx, float cy, float rx, float ry, NVGRoughOptions opts);
void nvgRoughRect(NVGcontext* vg, float x, float y, float w, float h, NVGRoughOptions opts);
void nvgRoughLine(NVGcontext* vg, float x1, float y1, float x2, float y2, NVGRoughOptions opts);
void nvgRoughPath(NVGcontext* vg, const float* points, int npoints, int closed, NVGRoughOptions opts);

#ifdef __cplusplus
}
#endif

#endif // NANOVG_ROUGH_H
