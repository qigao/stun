#include <flexchart/flexchart.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderLiquidfillSeries(NVGcontext* vg, const SeriesData& series,
                            float cx, float cy, float radius,
                            Color color) {
    double value = std::max(0.0, std::min(1.0, series.liquidValue));
    
    // Draw outer circle
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius);
    nvgStrokeColor(vg, color.toNVG());
    nvgStrokeWidth(vg, 3.0f);
    nvgStroke(vg);
    
    // Clip to circle
    nvgSave(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius - 3);
    nvgPathWinding(vg, NVG_HOLE);
    
    // Draw water level
    float waterY = cy + radius - 2 * radius * value;
    
    // Static time for animation effect
    static float time = 0;
    time += 0.05f;
    
    // Draw waves
    for (int w = 0; w < series.liquidWaves; w++) {
        float waveOffset = w * M_PI / series.liquidWaves;
        float amplitude = 5.0f + 3.0f * w;
        float frequency = 0.03f + 0.01f * w;
        
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - radius, cy + radius);
        nvgLineTo(vg, cx - radius, waterY);
        
        for (float px = cx - radius; px <= cx + radius; px += 2) {
            float py = waterY + std::sin((px - cx) * frequency + time + waveOffset) * amplitude;
            nvgLineTo(vg, px, py);
        }
        
        nvgLineTo(vg, cx + radius, cy + radius);
        nvgClosePath(vg);
        
        uint8_t alpha = 150 - w * 30;
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, alpha));
        nvgFill(vg);
    }
    
    nvgRestore(vg);
    
    // Draw percentage text
    char text[32];
    snprintf(text, sizeof(text), "%.0f%%", value * 100);
    
    nvgFontSize(vg, radius * 0.4f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, value > 0.5 ? nvgRGB(255, 255, 255) : nvgRGB(60, 60, 60));
    nvgText(vg, cx, cy, text, nullptr);
    
    // Name below
    if (!series.name.empty()) {
        nvgFontSize(vg, radius * 0.15f);
        nvgFillColor(vg, nvgRGB(100, 100, 100));
        nvgText(vg, cx, cy + radius + 15, series.name.c_str(), nullptr);
    }
}

} // namespace flexchart
