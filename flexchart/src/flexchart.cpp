#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

Color Color::fromHex(const std::string& hex) {
    Color c;
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    
    if (h.size() == 6) {
        c.r = (uint8_t)std::stoi(h.substr(0, 2), nullptr, 16);
        c.g = (uint8_t)std::stoi(h.substr(2, 2), nullptr, 16);
        c.b = (uint8_t)std::stoi(h.substr(4, 2), nullptr, 16);
        c.a = 255;
    } else if (h.size() == 8) {
        c.r = (uint8_t)std::stoi(h.substr(0, 2), nullptr, 16);
        c.g = (uint8_t)std::stoi(h.substr(2, 2), nullptr, 16);
        c.b = (uint8_t)std::stoi(h.substr(4, 2), nullptr, 16);
        c.a = (uint8_t)std::stoi(h.substr(6, 2), nullptr, 16);
    }
    return c;
}

FlexChart::FlexChart(cssboxRenderer* renderer, const std::string& id)
    : renderer_(renderer), id_(id) {
    option_.color = defaultColorPalette();
}

FlexChart::~FlexChart() = default;

void FlexChart::setOption(const ChartOption& option) {
    option_ = option;
    if (option_.color.empty()) {
        option_.color = defaultColorPalette();
    }
}

void FlexChart::resize(float width, float height) {
    width_ = width;
    height_ = height;
}

void FlexChart::on(const std::string& event, EventCallback callback) {
    callbacks_[event] = std::move(callback);
}

void FlexChart::off(const std::string& event) {
    callbacks_.erase(event);
}

void FlexChart::emitEvent(const std::string& type, int seriesIdx, int dataIdx) {
    auto it = callbacks_.find(type);
    if (it == callbacks_.end()) return;
    
    ChartEvent event;
    event.type = type;
    event.seriesIndex = seriesIdx;
    event.dataIndex = dataIdx;
    event.x = mouseX_;
    event.y = mouseY_;
    
    if (seriesIdx >= 0 && seriesIdx < (int)option_.series.size()) {
        const auto& s = option_.series[seriesIdx];
        event.name = s.name;
        if (dataIdx >= 0 && dataIdx < (int)s.data.size()) {
            event.value = s.data[dataIdx];
            if (dataIdx < (int)option_.xAxis.data.size()) {
                event.name = option_.xAxis.data[dataIdx];
            }
        }
    }
    
    it->second(event);
}

Color FlexChart::getSeriesColor(size_t index) const {
    if (option_.color.empty()) return {91, 143, 249, 255};
    return option_.color[index % option_.color.size()];
}

void FlexChart::getChartArea(float& cx, float& cy, float& cw, float& ch) const {
    cx = x_ + option_.grid.left;
    cy = y_ + option_.grid.top;
    cw = width_ - option_.grid.left - option_.grid.right;
    ch = height_ - option_.grid.top - option_.grid.bottom;
    
    if (option_.legend.show && option_.legend.top == "bottom") {
        ch -= 30.0f;
    }
}

void FlexChart::getValueRange(double& minVal, double& maxVal) const {
    minVal = 0;
    maxVal = 0;
    
    for (const auto& s : option_.series) {
        if (s.type == SeriesType::Pie) continue;
        for (double v : s.data) {
            maxVal = std::max(maxVal, v);
            minVal = std::min(minVal, v);
        }
    }
    
    if (option_.yAxis.min) minVal = *option_.yAxis.min;
    if (option_.yAxis.max) maxVal = *option_.yAxis.max;
    
    if (maxVal <= minVal) maxVal = minVal + 100.0;
    
    double range = maxVal - minVal;
    double magnitude = std::pow(10.0, std::floor(std::log10(range)));
    maxVal = std::ceil(maxVal / magnitude) * magnitude;
    if (minVal < 0) {
        minVal = std::floor(minVal / magnitude) * magnitude;
    }
}

void FlexChart::draw(NVGcontext* vg) {
    if (!vg) return;
    
    nvgSave(vg);
    
    nvgBeginPath(vg);
    nvgRect(vg, x_, y_, width_, height_);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
    nvgFill(vg);
    
    drawTitle(vg);
    drawGrid(vg);
    drawAxis(vg);
    
    float cx, cy, cw, ch;
    getChartArea(cx, cy, cw, ch);
    
    double minVal, maxVal;
    getValueRange(minVal, maxVal);
    
    bool hasPie = false;
    size_t barCount = 0;
    for (const auto& s : option_.series) {
        if (s.type == SeriesType::Pie) hasPie = true;
        if (s.type == SeriesType::Bar) barCount++;
    }
    
    size_t barIdx = 0;
    for (size_t i = 0; i < option_.series.size(); i++) {
        const auto& s = option_.series[i];
        Color color = s.color ? *s.color : getSeriesColor(i);
        int hovIdx = (hoveredSeries_ == (int)i) ? hoveredIndex_ : -1;
        
        switch (s.type) {
        case SeriesType::Line:
        case SeriesType::Area:
            renderLineSeries(vg, s, cx, cy, cw, ch, 
                           option_.xAxis.data, minVal, maxVal, color, hovIdx);
            break;
        case SeriesType::Bar:
            renderBarSeries(vg, s, cx, cy, cw, ch,
                          option_.xAxis.data, minVal, maxVal, color,
                          barIdx++, barCount, hovIdx);
            break;
        case SeriesType::Pie: {
            float radius = std::min(cw, ch) * 0.4f;
            float pcx = cx + cw / 2;
            float pcy = cy + ch / 2;
            renderPieSeries(vg, s, pcx, pcy, radius, color, hovIdx, option_.color);
            break;
        }
        case SeriesType::Scatter:
            renderScatterSeries(vg, s, cx, cy, cw, ch,
                              option_.xAxis.data, minVal, maxVal, color, hovIdx);
            break;
        case SeriesType::Radar: {
            float radius = std::min(cw, ch) * 0.35f;
            float rcx = cx + cw / 2;
            float rcy = cy + ch / 2;
            renderRadarSeries(vg, s, rcx, rcy, radius, color, hovIdx, option_.color);
            break;
        }
        case SeriesType::Gauge: {
            float radius = std::min(cw, ch) * 0.4f;
            float gcx = cx + cw / 2;
            float gcy = cy + ch / 2 + radius * 0.1f;
            renderGaugeSeries(vg, s, gcx, gcy, radius, color);
            break;
        }
        case SeriesType::Funnel:
            renderFunnelSeries(vg, s, cx, cy, cw, ch,
                             option_.xAxis.data, hovIdx, option_.color);
            break;
        case SeriesType::Candlestick: {
            double cminVal = 1e9, cmaxVal = -1e9;
            for (const auto& c : s.candlestickData) {
                cminVal = std::min(cminVal, c.low);
                cmaxVal = std::max(cmaxVal, c.high);
            }
            if (cmaxVal > cminVal) {
                double padding = (cmaxVal - cminVal) * 0.1;
                cminVal -= padding;
                cmaxVal += padding;
            }
            renderCandlestickSeries(vg, s, cx, cy, cw, ch,
                                   option_.xAxis.data, cminVal, cmaxVal, hovIdx);
            break;
        }
        case SeriesType::Heatmap:
            renderHeatmapSeries(vg, s, cx, cy, cw, ch,
                              option_.xAxis.data, hovIdx);
            break;
        case SeriesType::Treemap:
            renderTreemapSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::BoxPlot: {
            double bpMin = 1e9, bpMax = -1e9;
            for (const auto& bp : s.boxPlotData) {
                bpMin = std::min(bpMin, bp.min);
                bpMax = std::max(bpMax, bp.max);
                for (double o : bp.outliers) {
                    bpMin = std::min(bpMin, o);
                    bpMax = std::max(bpMax, o);
                }
            }
            if (bpMax > bpMin) {
                double padding = (bpMax - bpMin) * 0.1;
                bpMin -= padding;
                bpMax += padding;
            }
            renderBoxPlotSeries(vg, s, cx, cy, cw, ch,
                              option_.xAxis.data, bpMin, bpMax, color, hovIdx);
            break;
        }
        case SeriesType::Waterfall: {
            double wfMin = 0, wfMax = 0, wfCum = 0;
            for (double v : s.data) {
                wfCum += v;
                wfMin = std::min(wfMin, std::min(wfCum - v, wfCum));
                wfMax = std::max(wfMax, std::max(wfCum - v, wfCum));
            }
            double padding = (wfMax - wfMin) * 0.1;
            wfMin -= padding;
            wfMax += padding;
            renderWaterfallSeries(vg, s, cx, cy, cw, ch,
                                option_.xAxis.data, wfMin, wfMax, hovIdx, option_.color);
            break;
        }
        case SeriesType::Sunburst: {
            float radius = std::min(cw, ch) * 0.4f;
            float scx = cx + cw / 2;
            float scy = cy + ch / 2;
            renderSunburstSeries(vg, s, scx, scy, radius, hovIdx, option_.color);
            break;
        }
        case SeriesType::Polar: {
            float radius = std::min(cw, ch) * 0.35f;
            float pcx = cx + cw / 2;
            float pcy = cy + ch / 2;
            renderPolarSeries(vg, s, pcx, pcy, radius, option_.xAxis.data, color, hovIdx);
            break;
        }
        case SeriesType::Ring: {
            float radius = std::min(cw, ch) * 0.4f;
            float rcx = cx + cw / 2;
            float rcy = cy + ch / 2;
            renderRingSeries(vg, s, rcx, rcy, radius, color);
            break;
        }
        case SeriesType::Parallel:
            renderParallelSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Sankey:
            renderSankeySeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Graph:
            renderGraphSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Calendar:
            renderCalendarSeries(vg, s, cx, cy, cw, ch, hovIdx);
            break;
        case SeriesType::ThemeRiver:
            renderThemeRiverSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::PictorialBar:
            renderPictorialBarSeries(vg, s, cx, cy, cw, ch, option_.xAxis.data,
                                     minVal, maxVal, color, hovIdx);
            break;
        case SeriesType::Liquidfill: {
            float radius = std::min(cw, ch) * 0.4f;
            float lcx = cx + cw / 2;
            float lcy = cy + ch / 2;
            renderLiquidfillSeries(vg, s, lcx, lcy, radius, color);
            break;
        }
        case SeriesType::Tree:
            renderTreeSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Bullet:
            renderBulletSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Nightingale: {
            float radius = std::min(cw, ch) * 0.4f;
            float ncx = cx + cw / 2;
            float ncy = cy + ch / 2;
            renderNightingaleSeries(vg, s, ncx, ncy, radius, option_.xAxis.data, hovIdx, option_.color);
            break;
        }
        case SeriesType::Histogram:
            renderHistogramSeries(vg, s, cx, cy, cw, ch, hovIdx, color);
            break;
        case SeriesType::Step:
            renderStepSeries(vg, s, cx, cy, cw, ch, option_.xAxis.data, minVal, maxVal, color, hovIdx);
            break;
        case SeriesType::BarStack:
            renderBarStackSeries(vg, option_.series, cx, cy, cw, ch, option_.xAxis.data, (int)i, hovIdx, option_.color);
            break;
        case SeriesType::AreaStack:
            renderAreaStackSeries(vg, option_.series, cx, cy, cw, ch, option_.xAxis.data, (int)i, hovIdx, option_.color);
            break;
        case SeriesType::Bubble:
            renderBubbleSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Lollipop:
            renderLollipopSeries(vg, s, cx, cy, cw, ch, option_.xAxis.data, minVal, maxVal, color, hovIdx);
            break;
        case SeriesType::Dumbbell:
            renderDumbbellSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::RangeBar:
            renderRangeBarSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Gantt:
            renderGanttSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Waffle:
            renderWaffleSeries(vg, s, cx, cy, cw, ch, color);
            break;
        case SeriesType::RadialBar: {
            float radius = std::min(cw, ch) * 0.45f;
            float rbcx = cx + cw / 2;
            float rbcy = cy + ch / 2;
            renderRadialBarSeries(vg, s, rbcx, rbcy, radius, option_.xAxis.data, hovIdx, option_.color);
            break;
        }
        case SeriesType::Pyramid:
            renderPyramidSeries(vg, s, cx, cy, cw, ch, option_.xAxis.data, hovIdx, option_.color);
            break;
        case SeriesType::Violin:
            renderViolinSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::ErrorBar:
            renderErrorBarSeries(vg, s, cx, cy, cw, ch, option_.xAxis.data, minVal, maxVal, color, hovIdx);
            break;
        case SeriesType::Slope:
            renderSlopeSeries(vg, s, cx, cy, cw, ch, hovIdx, option_.color);
            break;
        case SeriesType::Dot:
            renderDotSeries(vg, s, cx, cy, cw, ch, option_.xAxis.data, hovIdx, color);
            break;
        case SeriesType::MapChina:
            renderMapChinaSeries(vg, s, cx, cy, cw, ch, hovIdx);
            break;
        }
    }
    
    drawLegend(vg);
    drawTooltip(vg);
    
    nvgRestore(vg);
}

void FlexChart::drawTitle(NVGcontext* vg) {
    if (option_.title.text.empty()) return;
    
    nvgFontSize(vg, option_.title.fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(51, 51, 51));
    nvgText(vg, x_ + width_ / 2, y_ + 10, option_.title.text.c_str(), nullptr);
    
    if (!option_.title.subtext.empty()) {
        nvgFontSize(vg, option_.title.subtextFontSize);
        nvgFillColor(vg, nvgRGB(113, 113, 113));
        nvgText(vg, x_ + width_ / 2, y_ + 10 + option_.title.fontSize + 4,
                option_.title.subtext.c_str(), nullptr);
    }
}

void FlexChart::drawGrid(NVGcontext* vg) {
    bool hasPie = false;
    for (const auto& s : option_.series) {
        if (s.type == SeriesType::Pie) hasPie = true;
    }
    if (hasPie) return;
    
    float cx, cy, cw, ch;
    getChartArea(cx, cy, cw, ch);
    
    if (!option_.yAxis.splitLine) return;
    
    nvgStrokeColor(vg, nvgRGBA(228, 228, 228, 128));
    nvgStrokeWidth(vg, 1.0f);
    
    for (int i = 0; i <= 5; i++) {
        float gy = cy + ch * (1.0f - i / 5.0f);
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, gy);
        nvgLineTo(vg, cx + cw, gy);
        nvgStroke(vg);
    }
}

void FlexChart::drawAxis(NVGcontext* vg) {
    bool hasPie = false;
    for (const auto& s : option_.series) {
        if (s.type == SeriesType::Pie) hasPie = true;
    }
    if (hasPie) return;
    
    float cx, cy, cw, ch;
    getChartArea(cx, cy, cw, ch);
    
    double minVal, maxVal;
    getValueRange(minVal, maxVal);
    
    nvgFontSize(vg, 11.0f);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, nvgRGB(113, 113, 122));
    
    if (option_.yAxis.show) {
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        for (int i = 0; i <= 5; i++) {
            float gy = cy + ch * (1.0f - i / 5.0f);
            double val = minVal + (maxVal - minVal) * (i / 5.0);
            
            char label[32];
            if (std::abs(val) >= 1000) {
                snprintf(label, sizeof(label), "%.0fK", val / 1000.0);
            } else if (std::abs(val) < 1 && val != 0) {
                snprintf(label, sizeof(label), "%.2f", val);
            } else {
                snprintf(label, sizeof(label), "%.0f", val);
            }
            nvgText(vg, cx - 8, gy, label, nullptr);
        }
    }
    
    if (option_.xAxis.show && !option_.xAxis.data.empty()) {
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        size_t n = option_.xAxis.data.size();
        for (size_t i = 0; i < n; i++) {
            float lx = cx + (cw * (i + 0.5f)) / n;
            nvgText(vg, lx, cy + ch + 8, option_.xAxis.data[i].c_str(), nullptr);
        }
    }
}

void FlexChart::drawLegend(NVGcontext* vg) {
    if (!option_.legend.show || option_.series.empty()) return;
    
    float lx = x_ + width_ / 2;
    float ly = y_ + height_ - 20;
    
    nvgFontSize(vg, 11.0f);
    nvgFontFace(vg, "sans-serif");
    
    float totalWidth = 0;
    for (size_t i = 0; i < option_.series.size(); i++) {
        float bounds[4];
        nvgTextBounds(vg, 0, 0, option_.series[i].name.c_str(), nullptr, bounds);
        totalWidth += (bounds[2] - bounds[0]) + 24;
    }
    
    float startX = lx - totalWidth / 2;
    
    for (size_t i = 0; i < option_.series.size(); i++) {
        const auto& s = option_.series[i];
        Color color = s.color ? *s.color : getSeriesColor(i);
        
        nvgBeginPath(vg);
        nvgRoundedRect(vg, startX, ly - 5, 12, 12, 2);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(113, 113, 122));
        nvgText(vg, startX + 16, ly + 1, s.name.c_str(), nullptr);
        
        float bounds[4];
        nvgTextBounds(vg, 0, 0, s.name.c_str(), nullptr, bounds);
        startX += (bounds[2] - bounds[0]) + 30;
    }
}

void FlexChart::drawTooltip(NVGcontext* vg) {
    if (!option_.tooltip.show) return;
    if (hoveredSeries_ < 0 || hoveredIndex_ < 0) return;
    if (hoveredSeries_ >= (int)option_.series.size()) return;
    
    const auto& s = option_.series[hoveredSeries_];
    if (hoveredIndex_ >= (int)s.data.size()) return;
    
    std::string label;
    if (hoveredIndex_ < (int)option_.xAxis.data.size()) {
        label = option_.xAxis.data[hoveredIndex_];
    }
    
    char text[128];
    if (s.type == SeriesType::Pie) {
        double total = 0;
        for (double v : s.data) total += v;
        double pct = (s.data[hoveredIndex_] / total) * 100.0;
        snprintf(text, sizeof(text), "%s: %.0f (%.1f%%)", 
                 label.c_str(), s.data[hoveredIndex_], pct);
    } else {
        snprintf(text, sizeof(text), "%s: %s = %.2f",
                 s.name.c_str(), label.c_str(), s.data[hoveredIndex_]);
    }
    
    nvgFontSize(vg, 12.0f);
    nvgFontFace(vg, "sans-serif");
    float bounds[4];
    nvgTextBounds(vg, 0, 0, text, nullptr, bounds);
    
    float tw = bounds[2] - bounds[0] + 16;
    float th = bounds[3] - bounds[1] + 10;
    float tx = mouseX_ + 10;
    float ty = mouseY_ - th - 5;
    
    if (tx + tw > x_ + width_) tx = mouseX_ - tw - 10;
    if (ty < y_) ty = mouseY_ + 15;
    
    nvgBeginPath(vg);
    nvgRoundedRect(vg, tx, ty, tw, th, 4);
    nvgFillColor(vg, nvgRGBA(30, 30, 30, 230));
    nvgFill(vg);
    
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgText(vg, tx + tw / 2, ty + th / 2, text, nullptr);
}

bool FlexChart::handleMouseMove(float mx, float my) {
    mouseX_ = mx;
    mouseY_ = my;
    
    float cx, cy, cw, ch;
    getChartArea(cx, cy, cw, ch);
    
    int oldSeries = hoveredSeries_;
    int oldIndex = hoveredIndex_;
    hoveredSeries_ = -1;
    hoveredIndex_ = -1;
    
    for (size_t si = 0; si < option_.series.size(); si++) {
        const auto& s = option_.series[si];
        
        if (s.type == SeriesType::MapChina) {
            int provinceIdx = findHoveredProvinceIndex(mx, my, cx, cy, cw, ch);
            if (provinceIdx >= 0) {
                hoveredSeries_ = (int)si;
                hoveredIndex_ = provinceIdx;
            }
        } else if (s.type == SeriesType::Pie) {
            float radius = std::min(cw, ch) * 0.4f;
            float pcx = cx + cw / 2;
            float pcy = cy + ch / 2;
            float innerR = radius * s.innerRadius;
            
            float dx = mx - pcx;
            float dy = my - pcy;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist >= innerR && dist <= radius) {
                double total = 0;
                for (double v : s.data) total += v;
                
                float angle = std::atan2(dy, dx);
                float startAngle = -M_PI / 2;
                float checkAngle = angle - startAngle;
                while (checkAngle < 0) checkAngle += 2 * M_PI;
                while (checkAngle >= 2 * M_PI) checkAngle -= 2 * M_PI;
                
                float cumAngle = 0;
                for (size_t i = 0; i < s.data.size(); i++) {
                    float sweep = (s.data[i] / total) * 2 * M_PI;
                    if (checkAngle >= cumAngle && checkAngle < cumAngle + sweep) {
                        hoveredSeries_ = (int)si;
                        hoveredIndex_ = (int)i;
                        break;
                    }
                    cumAngle += sweep;
                }
            }
        } else {
            size_t n = s.data.size();
            if (n == 0) continue;
            
            double minVal, maxVal;
            getValueRange(minVal, maxVal);
            
            for (size_t i = 0; i < n; i++) {
                float px, py;
                
                if (s.type == SeriesType::Bar) {
                    float barW = (cw / n) * 0.6f;
                    px = cx + (cw * (i + 0.5f)) / n;
                    float barH = ch * (s.data[i] - minVal) / (maxVal - minVal);
                    py = cy + ch - barH;
                    
                    if (mx >= px - barW/2 && mx <= px + barW/2 &&
                        my >= py && my <= cy + ch) {
                        hoveredSeries_ = (int)si;
                        hoveredIndex_ = (int)i;
                    }
                } else {
                    px = cx + (cw * i) / (n - 1);
                    py = cy + ch * (1.0f - (s.data[i] - minVal) / (maxVal - minVal));
                    
                    float dx = mx - px;
                    float dy = my - py;
                    if (dx*dx + dy*dy < 100) {
                        hoveredSeries_ = (int)si;
                        hoveredIndex_ = (int)i;
                    }
                }
            }
        }
    }
    
    return (hoveredSeries_ != oldSeries || hoveredIndex_ != oldIndex);
}

bool FlexChart::handleMouseDown(float mx, float my) {
    if (hoveredSeries_ >= 0 && hoveredIndex_ >= 0) {
        emitEvent("click", hoveredSeries_, hoveredIndex_);
        return true;
    }
    return false;
}

bool FlexChart::handleMouseUp(float mx, float my) {
    return false;
}

} // namespace flexchart
