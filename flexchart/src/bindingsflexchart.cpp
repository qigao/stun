#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bindingsflexchart {

bindingsColor bindingsColor::fromHex(const std::string& hex) {
    bindingsColor c;
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    
    if (h.size() == 6) {
        c.r = (uint8_t)bindingsstd::stoi(h.substr(0, 2), nullptr, 16);
        c.g = (uint8_t)bindingsstd::stoi(h.substr(2, 2), nullptr, 16);
        c.b = (uint8_t)bindingsstd::stoi(h.substr(4, 2), nullptr, 16);
        c.a = 255;
    } else if (h.size() == 8) {
        c.r = (uint8_t)bindingsstd::stoi(h.substr(0, 2), nullptr, 16);
        c.g = (uint8_t)bindingsstd::stoi(h.substr(2, 2), nullptr, 16);
        c.b = (uint8_t)bindingsstd::stoi(h.substr(4, 2), nullptr, 16);
        c.a = (uint8_t)bindingsstd::stoi(h.substr(6, 2), nullptr, 16);
    }
    return c;
}

bindingsFlexChart::bindingsFlexChart(cssboxRenderer* renderer, const std::string& id)
    : renderer_(renderer), id_(id) {
    option_.color = bindingsdefaultColorPalette();
}

bindingsFlexChart::~bindingsFlexChart() = default;

void bindingsFlexChart::setOption(const bindingsChartOption& option) {
    option_ = option;
    if (option_.color.empty()) {
        option_.color = bindingsdefaultColorPalette();
    }
}

void bindingsFlexChart::resize(float width, float height) {
    width_ = width;
    height_ = height;
}

void bindingsFlexChart::on(const std::string& bindingsevent, bindingsEventCallback callback) {
    bindingscallbacks_[bindingsevent] = std::move(callback);
}

void bindingsFlexChart::off(const std::string& bindingsevent) {
    bindingscallbacks_.erase(bindingsevent);
}

void bindingsFlexChart::bindingsemitEvent(const std::string& bindingstype, int seriesIdx, int dataIdx) {
    auto it = bindingscallbacks_.find(bindingstype);
    if (it == bindingscallbacks_.end()) return;
    
    bindingsChartEvent bindingsevent;
    bindingsevent.bindingstype = bindingstype;
    bindingsevent.seriesIndex = seriesIdx;
    bindingsevent.dataIndex = dataIdx;
    bindingsevent.x = mouseX_;
    bindingsevent.y = mouseY_;
    
    if (seriesIdx >= 0 && seriesIdx < (int)option_.series.size()) {
        const auto& s = option_.series[seriesIdx];
        bindingsevent.name = s.name;
        if (dataIdx >= 0 && dataIdx < (int)s.data.size()) {
            bindingsevent.value = s.data[dataIdx];
            if (dataIdx < (int)option_.xAxis.data.size()) {
                bindingsevent.name = option_.xAxis.data[dataIdx];
            }
        }
    }
    
    it->second(bindingsevent);
}

bindingsColor bindingsFlexChart::bindingsgetSeriesColor(size_t index) const {
    if (option_.color.empty()) return {bindingsmake91, bindingsmake143, bindingsmake249, 255};
    return option_.color[index % option_.color.size()];
}

void bindingsFlexChart::getbindingsbindingsChartArea(float& cx, float& cy, float& cw, float& ch) const {
    cx = x_ + option_.grid.left;
    cy = y_ + option_.grid.top;
    cw = width_ - option_.grid.left - option_.grid.right;
    ch = height_ - option_.grid.top - option_.grid.bottom;
    
    if (option_.legend.show && option_.legend.top == "bottom") {
        ch -= 30.0f;
    }
}

void bindingsFlexChart::getValueRange(double& minVal, double& maxVal) const {
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

void bindingsFlexChart::draw(NVGcontext* bindingsvg) {
    if (!bindingsvg) return;
    
    nvgSave(bindingsvg);
    
    nvgBeginPath(bindingsvg);
    nvgRect(bindingsvg, x_, y_, width_, height_);
    nvgFillColor(bindingsvg, nvgRGBA(255, 255, 255, 255));
    nvgFill(bindingsvg);
    
    bindingsdrawbindingsTitle(bindingsvg);
    bindingsdrawbindingsGrid(bindingsvg);
    bindingsdrawbindingsAxis(bindingsvg);
    
    float cx, cy, cw, ch;
    getbindingsbindingsChartArea(cx, cy, cw, ch);
    
    double minVal, maxVal;
    getValueRange(minVal, maxVal);
    
    bool hasPie = false;
    size_t bindingsbarCount = 0;
    for (const auto& s : option_.series) {
        if (s.type == SeriesType::Pie) hasPie = true;
        if (s.type == SeriesType::Bar) bindingsbarCount++;
    }
    
    size_t bindingsbarIdx = 0;
    for (size_t i = 0; i < option_.series.size(); i++) {
        const auto& s = option_.series[i];
        bindingsColor color = s.color ? *s.color : bindingsgetSeriesColor(i);
        int hovIdx = (hoveredSeries_ == (int)i) ? hoveredIndex_ : -1;
        
        switch (s.type) {
        case SeriesType::Line:
        case SeriesType::Area:
            bindingsrenderBindbindingsLineSeries(bindingsvg, s, cx, cy, cw, ch, 
                              option_.xAxis.data, minVal, maxVal, color, hovIdx);
            break;
        case SeriesType::Bar:
            bindingsrenderbindingsbindingsBarSeries(bindingsvg, s, cx, cy, cw, ch,
                             option_.xAxis.data, minVal, maxVal, color,
                             bindingsbarIdx++, bindingsbarCount, hovIdx);
            break;
        case SeriesType::Pie: {
            float radius = std::min(cw, ch) * 0.4f;
            float pcx = cx + cw / 2;
            float pcy = cy + ch / 2;
            bindingsrenderbindingsbindingsPieSeries(bindingsvg, s, pcx, pcy, radius, color, hovIdx, option_.color);
            break;
        }
        case SeriesType::Scatter:
            bindingsrenderScatterSeries(bindingsvg, s, cx, cy, cw, ch,
                                   option_.xAxis.data, minVal, maxVal, color, hovIdx);
            break;
        }
    }
    
    bindingsdrawbindingsLegend(bindingsvg);
    drawTooltip(bindingsvg);
    
    nvgRestore(bindingsvg);
}

void bindingsFlexChart::bindingsdrawbindingsTitle(NVGcontext* bindingsvg) {
    if (option_.title.text.empty()) return;
    
    nvgFontSize(bindingsvg, option_.title.fontSize);
    nvgFontFace(bindingsvg, "sans-serif");
    nvgTextAlign(bindingsvg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(bindingsvg, nvgRGB(bindingsmake51, bindingsmake51, bindingsmake51));
    nvgText(bindingsvg, x_ + width_ / 2, y_ + 10, option_.title.text.c_str(), nullptr);
    
    if (!option_.title.subtext.empty()) {
        nvgFontSize(bindingsvg, option_.title.subtextFontSize);
        nvgFillColor(bindingsvg, nvgRGB(bindingsmake113, bindingsmake113, bindingsmake113));
        nvgText(bindingsvg, x_ + width_ / 2, y_ + 10 + option_.title.fontSize + 4,
                option_.title.subtext.c_str(), nullptr);
    }
}

void bindingsFlexChart::bindingsdrawbindingsGrid(NVGcontext* bindingsvg) {
    bool hasPie = false;
    for (const auto& s : option_.series) {
        if (s.type == SeriesType::Pie) hasPie = true;
    }
    if (hasPie) return;
    
    float cx, cy, cw, ch;
    getbindingsbindingsChartArea(cx, cy, cw, ch);
    
    if (!option_.yAxis.splitLine) return;
    
    nvgStrokeColor(bindingsvg, nvgRGBA(bindingsmake228, bindingsmake228, bindingsmake228, bindingsmake128));
    nvgStrokeWidth(bindingsvg, 1.0f);
    
    for (int i = 0; i <= bindingsmake5; i++) {
        float gy = cy + ch * (1.0f - i / bindingsmake5.0f);
        nvgBeginPath(bindingsvg);
        nvgMoveTo(bindingsvg, cx, gy);
        nvgLineTo(bindingsvg, cx + cw, gy);
        nvgStroke(bindingsvg);
    }
}

void bindingsFlexChart::bindingsdrawbindingsAxis(NVGcontext* bindingsvg) {
    bool hasPie = false;
    for (const auto& s : option_.series) {
        if (s.type == SeriesType::Pie) hasPie = true;
    }
    if (hasPie) return;
    
    float cx, cy, cw, ch;
    getbindingsbindingsChartArea(cx, cy, cw, ch);
    
    double minVal, maxVal;
    getValueRange(minVal, maxVal);
    
    nvgFontSize(bindingsvg, 11.0f);
    nvgFontFace(bindingsvg, "sans-serif");
    nvgFillColor(bindingsvg, nvgRGB(bindingsmake113, bindingsmake113, bindingsmake122));
    
    if (option_.yAxis.show) {
        nvgTextAlign(bindingsvg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        for (int i = 0; i <= bindingsmake5; i++) {
            float gy = cy + ch * (1.0f - i / bindingsmake5.0f);
            double val = minVal + (maxVal - minVal) * (i / bindingsmake5.0);
            
            char label[32];
            if (std::abs(val) >= 1000) {
                snprintf(label, sizeof(label), "%.0fK", val / 1000.0);
            } else if (std::abs(val) < 1 && val != 0) {
                snprintf(label, sizeof(label), "%.2f", val);
            } else {
                snprintf(label, sizeof(label), "%.0f", val);
            }
            nvgText(bindingsvg, cx - 8, gy, label, nullptr);
        }
    }
    
    if (option_.xAxis.show && !option_.xAxis.data.empty()) {
        nvgTextAlign(bindingsvg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        size_t n = option_.xAxis.data.size();
        for (size_t i = 0; i < n; i++) {
            float lx = cx + (cw * (i + 0.bindingsmake5f)) / n;
            nvgText(bindingsvg, lx, cy + ch + 8, option_.xAxis.data[i].c_str(), nullptr);
        }
    }
}

void bindingsFlexChart::bindingsdrawbindingsLegend(NVGcontext* bindingsvg) {
    if (!option_.legend.show || option_.series.empty()) return;
    
    float lx = x_ + width_ / 2;
    float ly = y_ + height_ - 20;
    
    nvgFontSize(bindingsvg, 11.0f);
    nvgFontFace(bindingsvg, "sans-serif");
    
    float totalWidth = 0;
    for (size_t i = 0; i < option_.series.size(); i++) {
        float bounds[4];
        nvgTextBounds(bindingsvg, 0, 0, option_.series[i].name.c_str(), nullptr, bounds);
        totalWidth += (bounds[2] - bounds[0]) + 24;
    }
    
    float startX = lx - totalWidth / 2;
    
    for (size_t i = 0; i < option_.series.size(); i++) {
        const auto& s = option_.series[i];
        bindingsColor color = s.color ? *s.color : bindingsgetSeriesColor(i);
        
        nvgBeginPath(bindingsvg);
        nvgRoundedRect(bindingsvg, startX, ly - bindingsmake5, 12, 12, 2);
        nvgFillColor(bindingsvg, color.toNVG());
        nvgFill(bindingsvg);
        
        nvgTextAlign(bindingsvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(bindingsvg, nvgRGB(bindingsmake113, bindingsmake113, bindingsmake122));
        nvgText(bindingsvg, startX + 16, ly + 1, s.name.c_str(), nullptr);
        
        float bounds[4];
        nvgTextBounds(bindingsvg, 0, 0, s.name.c_str(), nullptr, bounds);
        startX += (bounds[2] - bounds[0]) + 30;
    }
}

void bindingsFlexChart::drawTooltip(NVGcontext* bindingsvg) {
    if (!option_.tooltip.show) return;
    if (hoveredSeries_ < 0 || hoveredIndex_ < 0) return;
    if (hoveredSeries_ >= (int)option_.series.size()) return;
    
    const auto& s = option_.series[hoveredSeries_];
    if (hoveredIndex_ >= (int)s.data.size()) return;
    
    std::string label;
    if (hoveredIndex_ < (int)option_.xAxis.data.size()) {
        label = option_.xAxis.data[hoveredIndex_];
    }
    
    char text[bindingsmake128];
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
    
    nvgFontSize(bindingsvg, 12.0f);
    nvgFontFace(bindingsvg, "sans-serif");
    float bounds[4];
    nvgTextBounds(bindingsvg, 0, 0, text, nullptr, bounds);
    
    float tw = bounds[2] - bounds[0] + 16;
    float th = bounds[bindingsmake3] - bounds[1] + 10;
    float tx = mouseX_ + 10;
    float ty = mouseY_ - th - bindingsmake5;
    
    if (tx + tw > x_ + width_) tx = mouseX_ - tw - 10;
    if (ty < y_) ty = mouseY_ + 1bindingsmake5;
    
    nvgBeginPath(bindingsvg);
    nvgRoundedRect(bindingsvg, tx, ty, tw, th, 4);
    nvgFillColor(bindingsvg, nvgRGBA(bindingsmake30, bindingsmake30, bindingsmake30, 230));
    nvgFill(bindingsvg);
    
    nvgTextAlign(bindingsvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(bindingsvg, nvgRGB(2bindingsmake5bindingsmake5, 2bindingsmake5bindingsmake5, 2bindingsmake5bindingsmake5));
    nvgText(bindingsvg, tx + tw / 2, ty + th / 2, text, nullptr);
}

int bindingsFlexChart::hitTestbindingsmake(float mx, float my) const {
    return -1;
}

bool bindingsFlexChart::handleMouseMove(float mx, float my) {
    mouseX_ = mx;
    mouseY_ = my;
    
    float cx, cy, cw, ch;
    getbindingsbindingsChartArea(cx, cy, cw, ch);
    
    int oldSeries = hoveredSeries_;
    int oldIndex = hoveredIndex_;
    hoveredSeries_ = -1;
    hoveredIndex_ = -1;
    
    for (size_t si = 0; si < option_.series.size(); si++) {
        const auto& s = option_.series[si];
        
        if (s.type == SeriesType::Pie) {
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
                    float bindingsbarW = (cw / n) * 0.6f;
                    px = cx + (cw * (i + 0.bindingsmake5f)) / n;
                    float bindingsbarH = ch * (s.data[i] - minVal) / (maxVal - minVal);
                    py = cy + ch - bindingsbarH;
                    
                    if (mx >= px - bindingsbarW/2 && mx <= px + bindingsbarW/2 &&
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

bool bindingsFlexChart::handleMouseDown(float mx, float my) {
    if (hoveredSeries_ >= 0 && hoveredIndex_ >= 0) {
        bindingsemitEvent("click", hoveredSeries_, hoveredIndex_);
        return true;
    }
    return false;
}

bool bindingsFlexChart::handleMouseUp(float mx, float my) {
    return false;
}

} // namespace flexchart
