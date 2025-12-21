/*
 * Gradient Picker Implementation
 */

#include <editor/view/gradient_picker.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace editor {

void GradientPicker::setGradientType(GradientType t) {
    type_ = t;
    notifyChange();
}

void GradientPicker::setLinearGradient(const flex::LinearGradient& g) {
    linear_ = g;
    if (linear_.stops.empty()) {
        linear_.add_stop(0.0f, flex::Color{1, 1, 1, 1});
        linear_.add_stop(1.0f, flex::Color{0, 0, 0, 1});
    }
    type_ = GradientType::Linear;
    selected_stop_ = 0;
}

void GradientPicker::setRadialGradient(const flex::RadialGradient& g) {
    radial_ = g;
    if (radial_.stops.empty()) {
        radial_.add_stop(0.0f, flex::Color{1, 1, 1, 1});
        radial_.add_stop(1.0f, flex::Color{0, 0, 0, 1});
    }
    type_ = GradientType::Radial;
    selected_stop_ = 0;
}

void GradientPicker::render(flex::Renderer& renderer) {
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
    renderer.draw_text("Gradient", x + pad, y + 20, "Arial", 12, true, {0.9f, 0.9f, 0.9f, 1.0f});

    // Type selector
    renderTypeSelector(renderer, x + pad, y + 35);

    // Gradient bar
    float barX = x + pad;
    float barY = y + 70;
    float barW = width_ - 2 * pad;
    float barH = 30;
    bar_rect_ = {barX, barY, barW, barH};
    renderGradientBar(renderer, barX, barY, barW, barH);

    // Color stops
    float stopsY = barY + barH + 5;
    renderColorStops(renderer, barX, stopsY, barW, 20);

    // Direction control
    float dirY = stopsY + 35;
    float dirSize = 80;
    direction_rect_ = {x + width_ / 2 - dirSize / 2, dirY, dirSize, dirSize};
    renderDirectionControl(renderer, x + width_ / 2 - dirSize / 2, dirY, dirSize);

    // Selected stop color preview
    float prevY = dirY + dirSize + 15;
    auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    if (selected_stop_ >= 0 && selected_stop_ < static_cast<int>(stops.size())) {
        const auto& stop = stops[selected_stop_];
        std::string prevBg = "M " + std::to_string(barX) + " " + std::to_string(prevY) +
            " h " + std::to_string(barW) + " v 25 h " + std::to_string(-barW) + " Z";
        renderer.fill_path(prevBg, flex::Paint::solid({stop.color.r, stop.color.g, stop.color.b, stop.color.a}));
        renderer.stroke_path(prevBg, flex::Paint::solid({0.5f, 0.5f, 0.5f, 1}), 1.0f);

        char label[32];
        snprintf(label, sizeof(label), "Stop %d: %.0f%%", selected_stop_ + 1, stop.offset * 100);
        renderer.draw_text(label, barX, prevY + 40, "Arial", 10, false, {0.7f, 0.7f, 0.7f, 1});
    }

    // Embedded color picker for selected stop
    if (editing_color_) {
        color_picker_.render(renderer);
    }
}

void GradientPicker::renderGradientBar(flex::Renderer& renderer, float x, float y, float w, float h) {
    auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    if (stops.empty()) return;

    // Draw gradient approximation
    int segments = 32;
    float segW = w / segments;

    for (int i = 0; i < segments; ++i) {
        float t = (i + 0.5f) / segments;

        // Find surrounding stops
        flex::Color c = stops[0].color;
        for (size_t j = 0; j < stops.size() - 1; ++j) {
            if (t >= stops[j].offset && t <= stops[j + 1].offset) {
                float localT = (t - stops[j].offset) / (stops[j + 1].offset - stops[j].offset);
                const auto& c1 = stops[j].color;
                const auto& c2 = stops[j + 1].color;
                c.r = c1.r + (c2.r - c1.r) * localT;
                c.g = c1.g + (c2.g - c1.g) * localT;
                c.b = c1.b + (c2.b - c1.b) * localT;
                c.a = c1.a + (c2.a - c1.a) * localT;
                break;
            }
        }
        if (t > stops.back().offset) {
            c = stops.back().color;
        }

        std::string rect = "M " + std::to_string(x + i * segW) + " " + std::to_string(y) +
            " h " + std::to_string(segW + 0.5f) +
            " v " + std::to_string(h) +
            " h " + std::to_string(-(segW + 0.5f)) + " Z";
        renderer.fill_path(rect, flex::Paint::solid({c.r, c.g, c.b, c.a}));
    }

    // Border
    std::string border = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(w) + " v " + std::to_string(h) +
        " h " + std::to_string(-w) + " Z";
    renderer.stroke_path(border, flex::Paint::solid({0.3f, 0.3f, 0.3f, 1}), 1.0f);
}

void GradientPicker::renderColorStops(flex::Renderer& renderer, float x, float y, float w, float h) {
    auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    stop_rects_.clear();

    for (size_t i = 0; i < stops.size(); ++i) {
        float stopX = x + stops[i].offset * w;
        float handleW = 12;
        float handleH = h;

        Rect r = {stopX - handleW / 2, y, handleW, handleH};
        stop_rects_.push_back(r);

        // Draw handle
        std::string handle = "M " + std::to_string(stopX) + " " + std::to_string(y) +
            " l -6 " + std::to_string(handleH / 2) +
            " l 6 " + std::to_string(handleH / 2) +
            " l 6 " + std::to_string(-handleH / 2) + " Z";

        bool selected = (static_cast<int>(i) == selected_stop_);
        renderer.fill_path(handle, flex::Paint::solid({stops[i].color.r, stops[i].color.g, stops[i].color.b, 1}));
        renderer.stroke_path(handle, flex::Paint::solid(selected ? flex::Color{1, 1, 1, 1} : flex::Color{0.3f, 0.3f, 0.3f, 1}),
                           selected ? 2.0f : 1.0f);
    }
}

void GradientPicker::renderDirectionControl(flex::Renderer& renderer, float x, float y, float size) {
    // Background circle
    float cx = x + size / 2;
    float cy = y + size / 2;
    float r = size / 2 - 5;

    std::string circle = "M " + std::to_string(cx - r) + " " + std::to_string(cy) +
        " a " + std::to_string(r) + " " + std::to_string(r) + " 0 1 0 " + std::to_string(2 * r) + " 0" +
        " a " + std::to_string(r) + " " + std::to_string(r) + " 0 1 0 " + std::to_string(-2 * r) + " 0";
    renderer.fill_path(circle, flex::Paint::solid({0.15f, 0.15f, 0.15f, 1}));
    renderer.stroke_path(circle, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}), 1.0f);

    if (type_ == GradientType::Linear) {
        // Direction line
        float dx = linear_.x2 - linear_.x1;
        float dy = linear_.y2 - linear_.y1;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0) {
            dx /= len;
            dy /= len;
        } else {
            dx = 1;
            dy = 0;
        }

        float lineR = r - 5;
        float x1 = cx - dx * lineR;
        float y1 = cy - dy * lineR;
        float x2 = cx + dx * lineR;
        float y2 = cy + dy * lineR;

        std::string line = "M " + std::to_string(x1) + " " + std::to_string(y1) +
            " L " + std::to_string(x2) + " " + std::to_string(y2);
        renderer.stroke_path(line, flex::Paint::solid({1, 1, 1, 1}), 2.0f);

        // Arrow head
        float arrowSize = 8;
        float ax = x2 - dx * arrowSize - dy * arrowSize / 2;
        float ay = y2 - dy * arrowSize + dx * arrowSize / 2;
        float bx = x2 - dx * arrowSize + dy * arrowSize / 2;
        float by = y2 - dy * arrowSize - dx * arrowSize / 2;

        std::string arrow = "M " + std::to_string(x2) + " " + std::to_string(y2) +
            " L " + std::to_string(ax) + " " + std::to_string(ay) +
            " L " + std::to_string(bx) + " " + std::to_string(by) + " Z";
        renderer.fill_path(arrow, flex::Paint::solid({1, 1, 1, 1}));
    } else {
        // Radial indicator - concentric circles
        for (int i = 1; i <= 3; ++i) {
            float rr = r * i / 4;
            std::string rc = "M " + std::to_string(cx - rr) + " " + std::to_string(cy) +
                " a " + std::to_string(rr) + " " + std::to_string(rr) + " 0 1 0 " + std::to_string(2 * rr) + " 0" +
                " a " + std::to_string(rr) + " " + std::to_string(rr) + " 0 1 0 " + std::to_string(-2 * rr) + " 0";
            renderer.stroke_path(rc, flex::Paint::solid({1, 1, 1, 0.3f}), 1.0f);
        }

        // Center point
        float centerR = 4;
        std::string centerPt = "M " + std::to_string(cx - centerR) + " " + std::to_string(cy) +
            " a " + std::to_string(centerR) + " " + std::to_string(centerR) + " 0 1 0 " + std::to_string(2 * centerR) + " 0" +
            " a " + std::to_string(centerR) + " " + std::to_string(centerR) + " 0 1 0 " + std::to_string(-2 * centerR) + " 0";
        renderer.fill_path(centerPt, flex::Paint::solid({1, 1, 1, 1}));
    }

    // Label
    const char* label = (type_ == GradientType::Linear) ? "Direction" : "Center";
    renderer.draw_text(label, x, y + size + 12, "Arial", 10, false, {0.6f, 0.6f, 0.6f, 1});
}

void GradientPicker::renderTypeSelector(flex::Renderer& renderer, float x, float y) {
    float btnW = 60;
    float btnH = 22;
    float gap = 5;

    // Linear button
    bool linearSelected = (type_ == GradientType::Linear);
    std::string linearBg = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(btnW) + " v " + std::to_string(btnH) +
        " h " + std::to_string(-btnW) + " Z";
    renderer.fill_path(linearBg, flex::Paint::solid(linearSelected ? flex::Color{0.3f, 0.5f, 0.8f, 1} : flex::Color{0.25f, 0.25f, 0.25f, 1}));
    renderer.stroke_path(linearBg, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}), 1.0f);
    renderer.draw_text("Linear", x + 8, y + 15, "Arial", 10, false, {0.9f, 0.9f, 0.9f, 1});

    // Radial button
    bool radialSelected = (type_ == GradientType::Radial);
    float rx = x + btnW + gap;
    std::string radialBg = "M " + std::to_string(rx) + " " + std::to_string(y) +
        " h " + std::to_string(btnW) + " v " + std::to_string(btnH) +
        " h " + std::to_string(-btnW) + " Z";
    renderer.fill_path(radialBg, flex::Paint::solid(radialSelected ? flex::Color{0.3f, 0.5f, 0.8f, 1} : flex::Color{0.25f, 0.25f, 0.25f, 1}));
    renderer.stroke_path(radialBg, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}), 1.0f);
    renderer.draw_text("Radial", rx + 8, y + 15, "Arial", 10, false, {0.9f, 0.9f, 0.9f, 1});
}

int GradientPicker::hitTestStop(float mx, float my) const {
    for (size_t i = 0; i < stop_rects_.size(); ++i) {
        if (stop_rects_[i].contains({mx, my})) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool GradientPicker::onMouseDown(float mx, float my, int button) {
    if (!visible_) return false;

    // Forward to color picker if active
    if (editing_color_ && color_picker_.onMouseDown(mx, my, button)) {
        return true;
    }

    if (button != 0) return false;

    // Type selector
    float pad = 10;
    float btnW = 60;
    float btnH = 22;
    float btnY = y_ + 35;
    if (my >= btnY && my <= btnY + btnH) {
        if (mx >= x_ + pad && mx <= x_ + pad + btnW) {
            setGradientType(GradientType::Linear);
            return true;
        }
        if (mx >= x_ + pad + btnW + 5 && mx <= x_ + pad + 2 * btnW + 5) {
            setGradientType(GradientType::Radial);
            return true;
        }
    }

    // Color stops
    int stop = hitTestStop(mx, my);
    if (stop >= 0) {
        selected_stop_ = stop;
        if (button == 0) {
            dragging_stop_ = true;
        }
        return true;
    }

    // Double-click on gradient bar to add stop
    if (bar_rect_.contains({mx, my})) {
        float t = std::clamp((mx - bar_rect_.x) / bar_rect_.width, 0.0f, 1.0f);
        auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;

        // Interpolate color at position
        flex::Color c = stops[0].color;
        for (size_t j = 0; j < stops.size() - 1; ++j) {
            if (t >= stops[j].offset && t <= stops[j + 1].offset) {
                float localT = (t - stops[j].offset) / (stops[j + 1].offset - stops[j].offset);
                const auto& c1 = stops[j].color;
                const auto& c2 = stops[j + 1].color;
                c.r = c1.r + (c2.r - c1.r) * localT;
                c.g = c1.g + (c2.g - c1.g) * localT;
                c.b = c1.b + (c2.b - c1.b) * localT;
                c.a = c1.a + (c2.a - c1.a) * localT;
                break;
            }
        }

        stops.emplace_back(t, c);
        std::sort(stops.begin(), stops.end(),
                 [](const flex::ColorStop& a, const flex::ColorStop& b) { return a.offset < b.offset; });

        // Find new stop index
        for (size_t i = 0; i < stops.size(); ++i) {
            if (std::abs(stops[i].offset - t) < 0.001f) {
                selected_stop_ = static_cast<int>(i);
                break;
            }
        }
        notifyChange();
        return true;
    }

    // Direction control
    if (direction_rect_.contains({mx, my})) {
        dragging_direction_ = true;
        return true;
    }

    // Selected stop color preview - open color picker
    float prevY = direction_rect_.y + direction_rect_.height + 15;
    if (mx >= bar_rect_.x && mx <= bar_rect_.x + bar_rect_.width &&
        my >= prevY && my <= prevY + 25) {
        auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
        if (selected_stop_ >= 0 && selected_stop_ < static_cast<int>(stops.size())) {
            const auto& stopColor = stops[selected_stop_].color;
            color_picker_.setColor(Color{stopColor.r, stopColor.g, stopColor.b, stopColor.a});
            color_picker_.setPosition(x_ + width_ + 10, y_);
            color_picker_.setVisible(true);
            color_picker_.onChange([this](const Color& c) {
                updateSelectedStopColor(c);
            });
            color_picker_.onClose([this]() {
                editing_color_ = false;
            });
            editing_color_ = true;
        }
        return true;
    }

    // Click outside closes picker
    if (mx < x_ || mx > x_ + width_ || my < y_ || my > y_ + height_) {
        if (onClose_) onClose_();
        return true;
    }

    return false;
}

bool GradientPicker::onMouseMove(float mx, float my) {
    if (!visible_) return false;

    if (editing_color_ && color_picker_.onMouseMove(mx, my)) {
        return true;
    }

    if (dragging_stop_) {
        auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
        if (selected_stop_ >= 0 && selected_stop_ < static_cast<int>(stops.size())) {
            float t = std::clamp((mx - bar_rect_.x) / bar_rect_.width, 0.0f, 1.0f);

            // Don't allow dragging past adjacent stops
            if (selected_stop_ > 0) {
                t = std::max(t, stops[selected_stop_ - 1].offset + 0.01f);
            }
            if (selected_stop_ < static_cast<int>(stops.size()) - 1) {
                t = std::min(t, stops[selected_stop_ + 1].offset - 0.01f);
            }

            stops[selected_stop_].offset = t;
            notifyChange();
        }
        return true;
    }

    if (dragging_direction_ && type_ == GradientType::Linear) {
        float cx = direction_rect_.x + direction_rect_.width / 2;
        float cy = direction_rect_.y + direction_rect_.height / 2;
        float dx = mx - cx;
        float dy = my - cy;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0) {
            dx /= len;
            dy /= len;
            linear_.x1 = 0.5f - dx * 0.5f;
            linear_.y1 = 0.5f - dy * 0.5f;
            linear_.x2 = 0.5f + dx * 0.5f;
            linear_.y2 = 0.5f + dy * 0.5f;
            notifyChange();
        }
        return true;
    }

    return false;
}

bool GradientPicker::onMouseUp(float mx, float my, int button) {
    if (editing_color_) {
        color_picker_.onMouseUp(mx, my, button);
    }

    dragging_stop_ = false;
    dragging_direction_ = false;
    return false;
}

void GradientPicker::updateSelectedStopColor(const Color& c) {
    auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    if (selected_stop_ >= 0 && selected_stop_ < static_cast<int>(stops.size())) {
        stops[selected_stop_].color = flex::Color{c.r, c.g, c.b, c.a};
        notifyChange();
    }
}

void GradientPicker::notifyChange() {
    if (onChange_) onChange_();
}

} // namespace editor
