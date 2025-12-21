/*
 * Color Picker Implementation
 */

#include <editor/view/color_picker.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace editor {

// HSV implementation
HSV HSV::fromRGB(const Color& rgb) {
    HSV hsv;
    hsv.a = rgb.a;

    float max = std::max({rgb.r, rgb.g, rgb.b});
    float min = std::min({rgb.r, rgb.g, rgb.b});
    float delta = max - min;

    hsv.v = max;

    if (max > 0) {
        hsv.s = delta / max;
    } else {
        hsv.s = 0;
        hsv.h = 0;
        return hsv;
    }

    if (delta < 0.00001f) {
        hsv.h = 0;
        return hsv;
    }

    if (rgb.r >= max) {
        hsv.h = (rgb.g - rgb.b) / delta;
    } else if (rgb.g >= max) {
        hsv.h = 2.0f + (rgb.b - rgb.r) / delta;
    } else {
        hsv.h = 4.0f + (rgb.r - rgb.g) / delta;
    }

    hsv.h *= 60.0f;
    if (hsv.h < 0) hsv.h += 360.0f;

    return hsv;
}

Color HSV::toRGB() const {
    Color rgb;
    rgb.a = a;

    if (s <= 0) {
        rgb.r = rgb.g = rgb.b = v;
        return rgb;
    }

    float hh = h;
    if (hh >= 360.0f) hh = 0;
    hh /= 60.0f;

    int i = static_cast<int>(hh);
    float ff = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * ff));
    float t = v * (1.0f - (s * (1.0f - ff)));

    switch (i) {
        case 0: rgb.r = v; rgb.g = t; rgb.b = p; break;
        case 1: rgb.r = q; rgb.g = v; rgb.b = p; break;
        case 2: rgb.r = p; rgb.g = v; rgb.b = t; break;
        case 3: rgb.r = p; rgb.g = q; rgb.b = v; break;
        case 4: rgb.r = t; rgb.g = p; rgb.b = v; break;
        default: rgb.r = v; rgb.g = p; rgb.b = q; break;
    }

    return rgb;
}

// ColorPicker implementation
void ColorPicker::setColor(const Color& c) {
    color_ = c;
    hsv_ = HSV::fromRGB(c);
}

void ColorPicker::render(flex::Renderer& renderer) {
    if (!visible_) return;

    float x = x_;
    float y = y_;
    float pad = 10;

    // Background
    std::string bg = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(width_) +
        " v " + std::to_string(height_) +
        " h " + std::to_string(-width_) + " Z";
    renderer.fill_path(bg, flex::Paint::solid({0.2f, 0.2f, 0.2f, 0.95f}));
    renderer.stroke_path(bg, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1.0f}), 1.0f);

    // Title
    renderer.draw_text("Color Picker", x + pad, y + 20, "Arial", 12, true, {0.9f, 0.9f, 0.9f, 1.0f});

    // SV square (saturation-value)
    float svX = x + pad;
    float svY = y + 35;
    float svSize = width_ - 40;
    sv_rect_ = {svX, svY, svSize, svSize};

    // Draw SV gradient
    renderSVGradient(renderer, svX, svY, svSize, svSize);

    // SV cursor
    float svCursorX = svX + hsv_.s * svSize;
    float svCursorY = svY + (1.0f - hsv_.v) * svSize;
    std::string cursor = "M " + std::to_string(svCursorX - 5) + " " + std::to_string(svCursorY) +
        " a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0";
    renderer.stroke_path(cursor, flex::Paint::solid({1, 1, 1, 1}), 2.0f);
    renderer.stroke_path(cursor, flex::Paint::solid({0, 0, 0, 1}), 1.0f);

    // Hue bar
    float hueX = x + width_ - 25;
    float hueY = y + 35;
    float hueW = 15;
    float hueH = svSize;
    hue_rect_ = {hueX, hueY, hueW, hueH};

    // Draw hue gradient
    renderHueGradient(renderer, hueX, hueY, hueW, hueH);

    // Hue cursor
    float hueCursorY = hueY + (hsv_.h / 360.0f) * hueH;
    std::string hueCursor = "M " + std::to_string(hueX - 2) + " " + std::to_string(hueCursorY - 3) +
        " h " + std::to_string(hueW + 4) +
        " v 6 h " + std::to_string(-(hueW + 4)) + " Z";
    renderer.fill_path(hueCursor, flex::Paint::solid({1, 1, 1, 1}));
    renderer.stroke_path(hueCursor, flex::Paint::solid({0, 0, 0, 1}), 1.0f);

    // Alpha bar
    float alphaY = svY + svSize + 15;
    float alphaW = width_ - 2 * pad;
    float alphaH = 15;
    alpha_rect_ = {svX, alphaY, alphaW, alphaH};

    // Draw alpha gradient
    renderAlphaGradient(renderer, svX, alphaY, alphaW, alphaH);

    // Alpha cursor
    float alphaCursorX = svX + hsv_.a * alphaW;
    std::string alphaCursor = "M " + std::to_string(alphaCursorX - 3) + " " + std::to_string(alphaY - 2) +
        " v " + std::to_string(alphaH + 4) +
        " h 6 v " + std::to_string(-(alphaH + 4)) + " Z";
    renderer.fill_path(alphaCursor, flex::Paint::solid({1, 1, 1, 1}));
    renderer.stroke_path(alphaCursor, flex::Paint::solid({0, 0, 0, 1}), 1.0f);

    // Current color preview
    float prevY = alphaY + 25;
    float prevH = 30;
    std::string prevBg = "M " + std::to_string(svX) + " " + std::to_string(prevY) +
        " h " + std::to_string(alphaW) +
        " v " + std::to_string(prevH) +
        " h " + std::to_string(-alphaW) + " Z";

    // Checkerboard for alpha
    renderCheckerboard(renderer, svX, prevY, alphaW, prevH);
    renderer.fill_path(prevBg, flex::Paint::solid({color_.r, color_.g, color_.b, color_.a}));
    renderer.stroke_path(prevBg, flex::Paint::solid({0.5f, 0.5f, 0.5f, 1}), 1.0f);

    // Hex value
    char hex[10];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X",
        static_cast<int>(color_.r * 255),
        static_cast<int>(color_.g * 255),
        static_cast<int>(color_.b * 255));
    renderer.draw_text(hex, svX, prevY + prevH + 20, "Arial", 12, false, {0.8f, 0.8f, 0.8f, 1});
}

bool ColorPicker::onMouseDown(float mx, float my, int button) {
    if (!visible_ || button != 0) return false;

    // Check SV area
    if (sv_rect_.contains({mx, my})) {
        dragging_sv_ = true;
        updateSV(mx, my);
        return true;
    }

    // Check hue bar
    if (hue_rect_.contains({mx, my})) {
        dragging_hue_ = true;
        updateHue(mx, my);
        return true;
    }

    // Check alpha bar
    if (alpha_rect_.contains({mx, my})) {
        dragging_alpha_ = true;
        updateAlpha(mx, my);
        return true;
    }

    // Click outside closes picker
    if (mx < x_ || mx > x_ + width_ || my < y_ || my > y_ + height_) {
        if (onClose_) onClose_();
        return true;
    }

    return false;
}

bool ColorPicker::onMouseMove(float mx, float my) {
    if (!visible_) return false;

    if (dragging_sv_) {
        updateSV(mx, my);
        return true;
    }
    if (dragging_hue_) {
        updateHue(mx, my);
        return true;
    }
    if (dragging_alpha_) {
        updateAlpha(mx, my);
        return true;
    }
    return false;
}

bool ColorPicker::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    dragging_sv_ = false;
    dragging_hue_ = false;
    dragging_alpha_ = false;
    return false;
}

void ColorPicker::updateSV(float mx, float my) {
    hsv_.s = std::clamp((mx - sv_rect_.x) / sv_rect_.width, 0.0f, 1.0f);
    hsv_.v = 1.0f - std::clamp((my - sv_rect_.y) / sv_rect_.height, 0.0f, 1.0f);
    color_ = hsv_.toRGB();
    if (onChange_) onChange_(color_);
}

void ColorPicker::updateHue(float mx, float my) {
    (void)mx;
    hsv_.h = std::clamp((my - hue_rect_.y) / hue_rect_.height, 0.0f, 1.0f) * 360.0f;
    color_ = hsv_.toRGB();
    if (onChange_) onChange_(color_);
}

void ColorPicker::updateAlpha(float mx, float my) {
    (void)my;
    hsv_.a = std::clamp((mx - alpha_rect_.x) / alpha_rect_.width, 0.0f, 1.0f);
    color_.a = hsv_.a;
    if (onChange_) onChange_(color_);
}

void ColorPicker::renderSVGradient(flex::Renderer& renderer, float x, float y, float w, float h) {
    // Draw in strips for gradient approximation
    int steps = 16;
    float stepW = w / steps;
    float stepH = h / steps;

    HSV temp = hsv_;
    for (int i = 0; i < steps; ++i) {
        for (int j = 0; j < steps; ++j) {
            temp.s = (i + 0.5f) / steps;
            temp.v = 1.0f - (j + 0.5f) / steps;
            Color c = temp.toRGB();

            std::string rect = "M " + std::to_string(x + i * stepW) + " " + std::to_string(y + j * stepH) +
                " h " + std::to_string(stepW + 0.5f) +
                " v " + std::to_string(stepH + 0.5f) +
                " h " + std::to_string(-(stepW + 0.5f)) + " Z";
            renderer.fill_path(rect, flex::Paint::solid({c.r, c.g, c.b, 1.0f}));
        }
    }

    // Border
    std::string border = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(w) + " v " + std::to_string(h) +
        " h " + std::to_string(-w) + " Z";
    renderer.stroke_path(border, flex::Paint::solid({0.3f, 0.3f, 0.3f, 1}), 1.0f);
}

void ColorPicker::renderHueGradient(flex::Renderer& renderer, float x, float y, float w, float h) {
    int steps = 12;
    float stepH = h / steps;

    HSV temp;
    temp.s = 1.0f;
    temp.v = 1.0f;

    for (int i = 0; i < steps; ++i) {
        temp.h = (i + 0.5f) / steps * 360.0f;
        Color c = temp.toRGB();

        std::string rect = "M " + std::to_string(x) + " " + std::to_string(y + i * stepH) +
            " h " + std::to_string(w) +
            " v " + std::to_string(stepH + 0.5f) +
            " h " + std::to_string(-w) + " Z";
        renderer.fill_path(rect, flex::Paint::solid({c.r, c.g, c.b, 1.0f}));
    }

    std::string border = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(w) + " v " + std::to_string(h) +
        " h " + std::to_string(-w) + " Z";
    renderer.stroke_path(border, flex::Paint::solid({0.3f, 0.3f, 0.3f, 1}), 1.0f);
}

void ColorPicker::renderAlphaGradient(flex::Renderer& renderer, float x, float y, float w, float h) {
    // Checkerboard background
    renderCheckerboard(renderer, x, y, w, h);

    // Alpha gradient overlay
    int steps = 16;
    float stepW = w / steps;

    for (int i = 0; i < steps; ++i) {
        float a = (i + 0.5f) / steps;
        std::string rect = "M " + std::to_string(x + i * stepW) + " " + std::to_string(y) +
            " h " + std::to_string(stepW + 0.5f) +
            " v " + std::to_string(h) +
            " h " + std::to_string(-(stepW + 0.5f)) + " Z";
        renderer.fill_path(rect, flex::Paint::solid({color_.r, color_.g, color_.b, a}));
    }

    std::string border = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(w) + " v " + std::to_string(h) +
        " h " + std::to_string(-w) + " Z";
    renderer.stroke_path(border, flex::Paint::solid({0.3f, 0.3f, 0.3f, 1}), 1.0f);
}

void ColorPicker::renderCheckerboard(flex::Renderer& renderer, float x, float y, float w, float h) {
    float cs = 6;
    int cols = static_cast<int>(w / cs) + 1;
    int rows = static_cast<int>(h / cs) + 1;

    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            bool dark = (i + j) % 2 == 0;
            float rx = x + i * cs;
            float ry = y + j * cs;
            float rw = std::min(cs, x + w - rx);
            float rh = std::min(cs, y + h - ry);
            if (rw <= 0 || rh <= 0) continue;

            std::string sq = "M " + std::to_string(rx) + " " + std::to_string(ry) +
                " h " + std::to_string(rw) + " v " + std::to_string(rh) +
                " h " + std::to_string(-rw) + " Z";
            renderer.fill_path(sq, flex::Paint::solid(dark ? flex::Color{0.4f, 0.4f, 0.4f, 1} : flex::Color{0.6f, 0.6f, 0.6f, 1}));
        }
    }
}

} // namespace editor
