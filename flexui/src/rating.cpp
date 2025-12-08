#include <flexui/rating.h>
#include <cmath>
#include <cssbox_internal.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexui {

Rating::Rating(cssboxRenderer* renderer, const std::string& id, int initialRating,
               const RatingStyle& style)
    : Widget(renderer, id, "rating"), rating_(initialRating), style_(style) {
    // Set text_content for layout engine to calculate intrinsic size
    // Use star symbols as placeholder for width estimation
    element()->text_content = std::string(style.maxRating, '*');

    setClickCallback([this](Widget* w) {
        if (hover_rating_ > 0) {
            rating_ = hover_rating_;
            if (change_callback_) {
                change_callback_(rating_);
            }
        }
        return true;
    });
}

void Rating::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    if (w == 0 || h == 0) return;

    NVGcolor fillColor = cssColor(style_.fillColor);

    float cx = x + style_.starSize / 2;
    float cy = y + h / 2;

    for (int i = 0; i < style_.maxRating; ++i) {
        NVGcolor color;
        if (hover_rating_ > 0 && i < hover_rating_) {
            color = style_.hoverColor;
        } else if (i < rating_) {
            color = fillColor;
        } else {
            color = style_.emptyColor;
        }

        drawStar(vg, cx, cy, style_.starSize / 2, color);
        cx += style_.starSize + style_.spacing;
    }
}

void Rating::drawStar(NVGcontext* vg, float cx, float cy, float r, const NVGcolor& color) {
    // Draw 5-pointed star
    nvgBeginPath(vg);
    for (int i = 0; i < 5; i++) {
        float angle = M_PI / 2 + i * 4 * M_PI / 5;  // Outer points
        float x = cx + r * cos(angle);
        float y = cy - r * sin(angle);

        if (i == 0) {
            nvgMoveTo(vg, x, y);
        } else {
            nvgLineTo(vg, x, y);
        }

        // Inner point
        angle += 2 * M_PI / 5;
        x = cx + r * 0.38f * cos(angle);
        y = cy - r * 0.38f * sin(angle);
        nvgLineTo(vg, x, y);
    }
    nvgClosePath(vg);
    nvgFillColor(vg, color);
    nvgFill(vg);
}

bool Rating::handleMouseMove(float mx, float my) {
    float x, y;
    getVisualPosition(x, y);
    float w = element()->layout.width;
    float h = element()->layout.height;

    if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
        float offsetX = mx - x;
        int star = (int)(offsetX / (style_.starSize + style_.spacing)) + 1;
        hover_rating_ = std::min(star, style_.maxRating);
        return true;
    } else {
        hover_rating_ = 0;
        return false;
    }
}

void Rating::setRating(int rating) {
    if (rating >= 0 && rating <= style_.maxRating) {
        rating_ = rating;
    }
}

} // namespace flexui
