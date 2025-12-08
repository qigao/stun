#include <flexui/sparkline.h>
#include <cssbox_internal.h>
#include <algorithm>

namespace flexui {

Sparkline::Sparkline(cssboxRenderer* renderer, const std::string& id,
                     const std::vector<float>& values, const SparklineStyle& style)
    : Widget(renderer, id, "sparkline"), values_(values), style_(style) {
    setInlineStyle("width", "100%");
    setInlineStyle("height", "100%");
    updateRange();
}

void Sparkline::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    // If dimensions are zero, try to use parent's dimensions
    if ((w <= 0 || h <= 0) && el->parent_internal_id >= 0) {
        auto* parent = cssboxGetParent(renderer(), el);
        if (parent) {
            x = parent->layout.x;
            y = parent->layout.y;
            w = parent->layout.width;
            h = parent->layout.height;
            // Update element layout so spatial index can find us
            el->layout.x = x;
            el->layout.y = y;
            el->layout.width = w;
            el->layout.height = h;
        }
    }

    if (values_.empty() || w <= 0 || h <= 0) return;

    float padding = 2.0f;
    float chartX = x + padding;
    float chartY = y + padding;
    float chartW = w - padding * 2;
    float chartH = h - padding * 2;

    size_t n = values_.size();
    float range = maxValue_ - minValue_;
    if (range <= 0) range = 1.0f;

    // Calculate points
    std::vector<float> px(n), py(n);
    for (size_t i = 0; i < n; i++) {
        px[i] = chartX + (chartW * i) / (n - 1);
        py[i] = chartY + chartH * (1.0f - (values_[i] - minValue_) / range);
    }

    // Draw fill
    if (style_.showFill) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, px[0], chartY + chartH);
        
        if (style_.smooth && n > 2) {
            nvgLineTo(vg, px[0], py[0]);
            for (size_t i = 0; i < n - 1; i++) {
                float cx = (px[i] + px[i+1]) / 2.0f;
                nvgBezierTo(vg, cx, py[i], cx, py[i+1], px[i+1], py[i+1]);
            }
        } else {
            for (size_t i = 0; i < n; i++) {
                nvgLineTo(vg, px[i], py[i]);
            }
        }
        
        nvgLineTo(vg, px[n-1], chartY + chartH);
        nvgClosePath(vg);
        nvgFillColor(vg, style_.fillColor);
        nvgFill(vg);
    }

    // Draw line
    nvgBeginPath(vg);
    nvgMoveTo(vg, px[0], py[0]);
    
    if (style_.smooth && n > 2) {
        for (size_t i = 0; i < n - 1; i++) {
            float cx = (px[i] + px[i+1]) / 2.0f;
            nvgBezierTo(vg, cx, py[i], cx, py[i+1], px[i+1], py[i+1]);
        }
    } else {
        for (size_t i = 1; i < n; i++) {
            nvgLineTo(vg, px[i], py[i]);
        }
    }
    
    nvgStrokeColor(vg, style_.lineColor);
    nvgStrokeWidth(vg, style_.lineWidth);
    nvgStroke(vg);

    // Draw end point
    if (style_.showEndPoint && n > 0) {
        nvgBeginPath(vg);
        nvgCircle(vg, px[n-1], py[n-1], 3.0f);
        nvgFillColor(vg, style_.pointColor);
        nvgFill(vg);
    }
}

void Sparkline::setValues(const std::vector<float>& values) {
    values_ = values;
    if (values_.size() > maxPoints_) {
        values_.erase(values_.begin(), values_.begin() + (values_.size() - maxPoints_));
    }
    updateRange();
}

void Sparkline::addValue(float value) {
    values_.push_back(value);
    if (values_.size() > maxPoints_) {
        values_.erase(values_.begin());
    }
    updateRange();
}

void Sparkline::clearValues() {
    values_.clear();
    minValue_ = 0.0f;
    maxValue_ = 0.0f;
}

void Sparkline::updateRange() {
    if (values_.empty()) {
        minValue_ = 0.0f;
        maxValue_ = 0.0f;
        return;
    }
    
    minValue_ = *std::min_element(values_.begin(), values_.end());
    maxValue_ = *std::max_element(values_.begin(), values_.end());
    
    // Add some padding to range
    float padding = (maxValue_ - minValue_) * 0.1f;
    if (padding < 0.1f) padding = 0.1f;
    minValue_ -= padding;
    maxValue_ += padding;
}

} // namespace flexui
