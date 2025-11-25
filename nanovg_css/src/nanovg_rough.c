/*
 * NanoVG Rough - Hand-drawn style rendering implementation
 * 
 * C implementation inspired by rough.js (https://github.com/rough-stuff/rough)
 */

#include "nanovg_rough.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Random number generator using seed
typedef struct {
    unsigned int state;
} RoughRandom;

static void rough_random_init(RoughRandom* rng, unsigned int seed) {
    rng->state = seed;
}

static float rough_random_float(RoughRandom* rng) {
    // Simple LCG (Linear Congruential Generator)
    rng->state = rng->state * 1103515245 + 12345;
    return (float)(rng->state & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float rough_random_range(RoughRandom* rng, float min, float max) {
    return min + rough_random_float(rng) * (max - min);
}

// Helper to offset a point perpendicular to direction
static void offset_point(float x, float y, float dx, float dy, float offset, float* out_x, float* out_y) {
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) {
        *out_x = x;
        *out_y = y;
        return;
    }
    // Perpendicular vector
    float px = -dy / len;
    float py = dx / len;
    *out_x = x + px * offset;
    *out_y = y + py * offset;
}

// Create default options
NVGRoughOptions nvgRoughDefaultOptions(void) {
    NVGRoughOptions opts;
    opts.roughness = 1.0f;
    opts.bowing = 1.0f;
    opts.stroke_count = 1;
    opts.stroke_width = 1.0f;
    opts.stroke_color = nvgRGBA(0, 0, 0, 255);
    opts.fill_color = nvgRGBA(255, 255, 255, 0);
    opts.fill_enabled = 0;
    opts.stroke_enabled = 1;
    opts.seed = 0;
    return opts;
}

// Draw rough line with bowing and roughness
static void draw_rough_line(NVGcontext* vg, float x1, float y1, float x2, float y2, 
                           NVGRoughOptions opts, RoughRandom* rng) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);
    
    if (length < 1e-6f) return;
    
    // Calculate bowing (curvature in the middle)
    float bow_amount = opts.bowing * 0.15f * length;
    float bow = rough_random_range(rng, -bow_amount, bow_amount);
    
    // Middle point with bow
    float mx = (x1 + x2) / 2.0f;
    float my = (y1 + y2) / 2.0f;
    
    // Offset perpendicular to line direction
    float bow_x, bow_y;
    offset_point(mx, my, dx, dy, bow, &bow_x, &bow_y);
    
    // Roughness - subdivide line and add noise
    int segments = (int)(length / 10.0f) + 3;
    if (segments > 50) segments = 50;
    
    nvgBeginPath(vg);
    nvgMoveTo(vg, x1, y1);
    
    for (int i = 1; i < segments; i++) {
        float t = (float)i / (float)segments;
        
        // Quadratic bezier through bow point
        float px = (1-t)*(1-t)*x1 + 2*(1-t)*t*bow_x + t*t*x2;
        float py = (1-t)*(1-t)*y1 + 2*(1-t)*t*bow_y + t*t*y2;
        
        // Add roughness
        float roughness = opts.roughness * 2.0f;
        float offset_x = rough_random_range(rng, -roughness, roughness);
        float offset_y = rough_random_range(rng, -roughness, roughness);
        
        nvgLineTo(vg, px + offset_x, py + offset_y);
    }
    
    nvgLineTo(vg, x2, y2);
    nvgStroke(vg);
}

// Draw rough line
void nvgRoughLine(NVGcontext* vg, float x1, float y1, float x2, float y2, NVGRoughOptions opts) {
    if (!opts.stroke_enabled) return;
    
    RoughRandom rng;
    rough_random_init(&rng, opts.seed);
    
    nvgStrokeWidth(vg, opts.stroke_width);
    nvgStrokeColor(vg, opts.stroke_color);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    
    for (int i = 0; i < opts.stroke_count; i++) {
        draw_rough_line(vg, x1, y1, x2, y2, opts, &rng);
    }
}

// Draw rough circle
void nvgRoughCircle(NVGcontext* vg, float cx, float cy, float radius, NVGRoughOptions opts) {
    nvgRoughEllipse(vg, cx, cy, radius, radius, opts);
}

// Draw rough ellipse
void nvgRoughEllipse(NVGcontext* vg, float cx, float cy, float rx, float ry, NVGRoughOptions opts) {
    RoughRandom rng;
    rough_random_init(&rng, opts.seed);
    
    // Fill first if enabled
    if (opts.fill_enabled) {
        // Hachure fill (parallel lines)
        float angle = rough_random_range(&rng, 0, M_PI);
        float gap = 4.0f;
        
        nvgSave(vg);
        nvgScissor(vg, cx - rx, cy - ry, rx * 2, ry * 2);
        
        nvgStrokeWidth(vg, 1.0f);
        NVGcolor fill_stroke = opts.fill_color;
        fill_stroke.a *= 0.5f;
        nvgStrokeColor(vg, fill_stroke);
        
        float max_dim = sqrtf(rx * rx + ry * ry) * 2;
        for (float d = -max_dim; d < max_dim; d += gap) {
            float x1 = cx + cosf(angle) * d - sinf(angle) * max_dim;
            float y1 = cy + sinf(angle) * d + cosf(angle) * max_dim;
            float x2 = cx + cosf(angle) * d + sinf(angle) * max_dim;
            float y2 = cy + sinf(angle) * d - cosf(angle) * max_dim;
            
            nvgBeginPath(vg);
            nvgMoveTo(vg, x1, y1);
            nvgLineTo(vg, x2, y2);
            nvgStroke(vg);
        }
        
        nvgRestore(vg);
    }
    
    // Stroke
    if (opts.stroke_enabled) {
        int segments = 32;
        
        nvgStrokeWidth(vg, opts.stroke_width);
        nvgStrokeColor(vg, opts.stroke_color);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        
        for (int stroke = 0; stroke < opts.stroke_count; stroke++) {
            nvgBeginPath(vg);
            
            for (int i = 0; i <= segments; i++) {
                float angle = (float)i / (float)segments * 2.0f * M_PI;
                float x = cx + cosf(angle) * rx;
                float y = cy + sinf(angle) * ry;
                
                // Add roughness
                float roughness = opts.roughness * 2.0f;
                x += rough_random_range(&rng, -roughness, roughness);
                y += rough_random_range(&rng, -roughness, roughness);
                
                if (i == 0) {
                    nvgMoveTo(vg, x, y);
                } else {
                    nvgLineTo(vg, x, y);
                }
            }
            
            nvgStroke(vg);
        }
    }
}

// Draw rough rectangle
void nvgRoughRect(NVGcontext* vg, float x, float y, float w, float h, NVGRoughOptions opts) {
    RoughRandom rng;
    rough_random_init(&rng, opts.seed);
    
    // Fill
    if (opts.fill_enabled) {
        // Hachure fill
        float angle = rough_random_range(&rng, -M_PI / 4, M_PI / 4);
        float gap = 4.0f;
        
        nvgSave(vg);
        nvgScissor(vg, x, y, w, h);
        
        nvgStrokeWidth(vg, 1.0f);
        NVGcolor fill_stroke = opts.fill_color;
        fill_stroke.a *= 0.5f;
        nvgStrokeColor(vg, fill_stroke);
        
        float diagonal = sqrtf(w * w + h * h);
        for (float d = -diagonal; d < diagonal; d += gap) {
            float x1 = x + w/2 + cosf(angle) * d - sinf(angle) * diagonal;
            float y1 = y + h/2 + sinf(angle) * d + cosf(angle) * diagonal;
            float x2 = x + w/2 + cosf(angle) * d + sinf(angle) * diagonal;
            float y2 = y + h/2 + sinf(angle) * d - cosf(angle) * diagonal;
            
            nvgBeginPath(vg);
            nvgMoveTo(vg, x1, y1);
            nvgLineTo(vg, x2, y2);
            nvgStroke(vg);
        }
        
        nvgRestore(vg);
    }
    
    // Stroke
    if (opts.stroke_enabled) {
        nvgStrokeWidth(vg, opts.stroke_width);
        nvgStrokeColor(vg, opts.stroke_color);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        
        for (int stroke = 0; stroke < opts.stroke_count; stroke++) {
            // Draw 4 sides with roughness
            draw_rough_line(vg, x, y, x + w, y, opts, &rng);           // Top
            draw_rough_line(vg, x + w, y, x + w, y + h, opts, &rng);   // Right
            draw_rough_line(vg, x + w, y + h, x, y + h, opts, &rng);   // Bottom
            draw_rough_line(vg, x, y + h, x, y, opts, &rng);           // Left
        }
    }
}

// Draw rough path
void nvgRoughPath(NVGcontext* vg, const float* points, int npoints, int closed, NVGRoughOptions opts) {
    if (npoints < 2 || !opts.stroke_enabled) return;
    
    RoughRandom rng;
    rough_random_init(&rng, opts.seed);
    
    nvgStrokeWidth(vg, opts.stroke_width);
    nvgStrokeColor(vg, opts.stroke_color);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    
    for (int stroke = 0; stroke < opts.stroke_count; stroke++) {
        for (int i = 0; i < npoints - 1; i++) {
            float x1 = points[i * 2];
            float y1 = points[i * 2 + 1];
            float x2 = points[(i + 1) * 2];
            float y2 = points[(i + 1) * 2 + 1];
            
            draw_rough_line(vg, x1, y1, x2, y2, opts, &rng);
        }
        
        if (closed && npoints > 2) {
            float x1 = points[(npoints - 1) * 2];
            float y1 = points[(npoints - 1) * 2 + 1];
            float x2 = points[0];
            float y2 = points[1];
            
            draw_rough_line(vg, x1, y1, x2, y2, opts, &rng);
        }
    }
}
