#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>

namespace flexui {

struct RatingStyle {
    NVGcolor fillColor = nvgRGB(255, 193, 7);  // Gold
    NVGcolor emptyColor = nvgRGB(189, 189, 189);
    NVGcolor hoverColor = nvgRGB(255, 235, 59);  // Lighter gold
    float starSize = 20.0f;
    float spacing = 4.0f;
    int maxRating = 5;
};

class Rating : public Widget {
public:
    using ChangeCallback = std::function<void(int)>;

    Rating(cssboxRenderer* renderer, const std::string& id, int initialRating = 0,
           const RatingStyle& style = RatingStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseMove(float mx, float my) override;

    void setRating(int rating);
    int getRating() const { return rating_; }

    void setChangeCallback(ChangeCallback cb) { change_callback_ = cb; }
    void setRatingStyle(const RatingStyle& style) { style_ = style; }

private:
    int rating_;
    int hover_rating_ = 0;
    RatingStyle style_;
    ChangeCallback change_callback_;

    void drawStar(NVGcontext* vg, float cx, float cy, float r, const NVGcolor& color);
};

} // namespace flexui
