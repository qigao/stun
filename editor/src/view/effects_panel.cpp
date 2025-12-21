/*
 * Effects Panel Implementation
 */

#include <editor/view/effects_panel.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace editor {

void EffectsPanel::setNode(EditorNode* node) {
    node_ = node;
    if (node) {
        // Load current shadow settings
        if (node->hasShadow()) {
            shadow_enabled_ = true;
            auto s = node->shadow();
            shadow_ox_ = s.offset_x;
            shadow_oy_ = s.offset_y;
            shadow_blur_ = s.blur;
            shadow_color_ = {s.color.r, s.color.g, s.color.b, s.color.a};
        } else {
            shadow_enabled_ = false;
        }

        // Load current blur settings
        if (node->hasBlur()) {
            blur_enabled_ = true;
            blur_radius_ = node->blur().radius;
        } else {
            blur_enabled_ = false;
        }
    }
}

void EffectsPanel::render(flex::Renderer& renderer) {
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
    renderer.draw_text("Effects", x + pad, y + 20, "Arial", 12, true, {0.9f, 0.9f, 0.9f, 1.0f});

    float contentY = y + 40;

    // Shadow section
    renderShadowSection(renderer, x + pad, contentY);

    contentY += 15;

    // Blur section
    renderBlurSection(renderer, x + pad, contentY);

    // Embedded color picker
    if (editing_shadow_color_) {
        color_picker_.render(renderer);
    }
}

void EffectsPanel::renderShadowSection(flex::Renderer& renderer, float x, float& y) {
    float w = width_ - 20;

    // Section header with checkbox
    renderCheckbox(renderer, x, y, shadow_enabled_, "Shadow", 0);
    y += 25;

    if (!shadow_enabled_) return;

    // Offset X slider
    renderSlider(renderer, x, y, w, shadow_ox_, -50, 50, "Offset X", 0);
    y += 30;

    // Offset Y slider
    renderSlider(renderer, x, y, w, shadow_oy_, -50, 50, "Offset Y", 1);
    y += 30;

    // Blur slider
    renderSlider(renderer, x, y, w, shadow_blur_, 0, 50, "Blur", 2);
    y += 30;

    // Color preview
    renderer.draw_text("Color", x, y + 12, "Arial", 10, false, {0.7f, 0.7f, 0.7f, 1});

    float colorX = x + 50;
    float colorW = w - 50;
    std::string colorBg = "M " + std::to_string(colorX) + " " + std::to_string(y) +
        " h " + std::to_string(colorW) + " v 18 h " + std::to_string(-colorW) + " Z";
    renderer.fill_path(colorBg, flex::Paint::solid({shadow_color_.r, shadow_color_.g, shadow_color_.b, shadow_color_.a}));
    renderer.stroke_path(colorBg, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}), 1.0f);

    y += 25;
}

void EffectsPanel::renderBlurSection(flex::Renderer& renderer, float x, float& y) {
    float w = width_ - 20;

    // Section header with checkbox
    renderCheckbox(renderer, x, y, blur_enabled_, "Blur", 1);
    y += 25;

    if (!blur_enabled_) return;

    // Radius slider
    renderSlider(renderer, x, y, w, blur_radius_, 0, 50, "Radius", 3);
    y += 30;
}

void EffectsPanel::renderSlider(flex::Renderer& renderer, float x, float y, float w,
                                float value, float minVal, float maxVal, const char* label, int id) {
    // Label
    renderer.draw_text(label, x, y + 12, "Arial", 10, false, {0.7f, 0.7f, 0.7f, 1});

    // Track
    float trackX = x + 60;
    float trackW = w - 100;
    float trackY = y + 6;
    float trackH = 6;

    std::string track = "M " + std::to_string(trackX) + " " + std::to_string(trackY) +
        " h " + std::to_string(trackW) + " v " + std::to_string(trackH) +
        " h " + std::to_string(-trackW) + " Z";
    renderer.fill_path(track, flex::Paint::solid({0.3f, 0.3f, 0.3f, 1}));

    // Thumb
    float t = (value - minVal) / (maxVal - minVal);
    float thumbX = trackX + t * trackW;
    float thumbR = 6;

    std::string thumb = "M " + std::to_string(thumbX - thumbR) + " " + std::to_string(trackY + trackH / 2) +
        " a " + std::to_string(thumbR) + " " + std::to_string(thumbR) + " 0 1 0 " + std::to_string(2 * thumbR) + " 0" +
        " a " + std::to_string(thumbR) + " " + std::to_string(thumbR) + " 0 1 0 " + std::to_string(-2 * thumbR) + " 0";
    renderer.fill_path(thumb, flex::Paint::solid({0.9f, 0.9f, 0.9f, 1}));

    // Value text
    char valStr[16];
    snprintf(valStr, sizeof(valStr), "%.1f", value);
    renderer.draw_text(valStr, trackX + trackW + 5, y + 12, "Arial", 10, false, {0.6f, 0.6f, 0.6f, 1});

    // Store rect for hit testing
    slider_rects_[id] = {trackX, trackY - 4, trackW, trackH + 8};
}

void EffectsPanel::renderCheckbox(flex::Renderer& renderer, float x, float y, bool checked, const char* label, int id) {
    float boxSize = 14;

    std::string box = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(boxSize) + " v " + std::to_string(boxSize) +
        " h " + std::to_string(-boxSize) + " Z";
    renderer.fill_path(box, flex::Paint::solid(checked ? flex::Color{0.3f, 0.6f, 0.9f, 1} : flex::Color{0.25f, 0.25f, 0.25f, 1}));
    renderer.stroke_path(box, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}), 1.0f);

    if (checked) {
        // Checkmark
        std::string check = "M " + std::to_string(x + 3) + " " + std::to_string(y + 7) +
            " l 3 3 l 5 -6";
        renderer.stroke_path(check, flex::Paint::solid({1, 1, 1, 1}), 2.0f);
    }

    // Label
    renderer.draw_text(label, x + boxSize + 8, y + 11, "Arial", 11, true, {0.85f, 0.85f, 0.85f, 1});

    checkbox_rects_[id] = {x, y, boxSize + 60, boxSize};
}

bool EffectsPanel::onMouseDown(float mx, float my, int button) {
    if (!visible_) return false;

    // Forward to color picker
    if (editing_shadow_color_ && color_picker_.onMouseDown(mx, my, button)) {
        return true;
    }

    if (button != 0) return false;

    // Check checkboxes
    for (int i = 0; i < 2; ++i) {
        if (checkbox_rects_[i].contains({mx, my})) {
            if (i == 0) {
                shadow_enabled_ = !shadow_enabled_;
                if (node_) {
                    if (shadow_enabled_) {
                        node_->setShadow(shadow_ox_, shadow_oy_, shadow_blur_, shadow_color_);
                    } else {
                        node_->clearShadow();
                    }
                }
            } else {
                blur_enabled_ = !blur_enabled_;
                if (node_) {
                    if (blur_enabled_) {
                        node_->setBlur(blur_radius_);
                    } else {
                        node_->clearBlur();
                    }
                }
            }
            notifyChange();
            return true;
        }
    }

    // Check sliders
    for (int i = 0; i < 4; ++i) {
        if (slider_rects_[i].contains({mx, my})) {
            dragging_slider_ = i;
            onMouseMove(mx, my);  // Update immediately
            return true;
        }
    }

    // Check shadow color area
    if (shadow_enabled_) {
        float colorX = x_ + 60;
        float colorY = y_ + 40 + 25 + 30 * 3;  // After 3 sliders
        float colorW = width_ - 70;
        Rect colorRect = {colorX, colorY, colorW, 18};

        if (colorRect.contains({mx, my})) {
            color_picker_.setColor(shadow_color_);
            color_picker_.setPosition(x_ + width_ + 10, y_);
            color_picker_.setVisible(true);
            color_picker_.onChange([this](const Color& c) {
                shadow_color_ = c;
                if (node_ && shadow_enabled_) {
                    node_->setShadow(shadow_ox_, shadow_oy_, shadow_blur_, shadow_color_);
                }
                notifyChange();
            });
            color_picker_.onClose([this]() {
                editing_shadow_color_ = false;
            });
            editing_shadow_color_ = true;
            return true;
        }
    }

    // Check if outside panel
    if (mx < x_ || mx > x_ + width_ || my < y_ || my > y_ + height_) {
        return false;
    }

    return true;
}

bool EffectsPanel::onMouseMove(float mx, float my) {
    if (!visible_) return false;

    if (editing_shadow_color_ && color_picker_.onMouseMove(mx, my)) {
        return true;
    }

    if (dragging_slider_ >= 0) {
        const auto& rect = slider_rects_[dragging_slider_];
        float t = std::clamp((mx - rect.x) / rect.width, 0.0f, 1.0f);

        float minVal = 0, maxVal = 50;
        if (dragging_slider_ == 0 || dragging_slider_ == 1) {
            minVal = -50;
            maxVal = 50;
        }

        float value = minVal + t * (maxVal - minVal);

        switch (dragging_slider_) {
            case 0: shadow_ox_ = value; break;
            case 1: shadow_oy_ = value; break;
            case 2: shadow_blur_ = value; break;
            case 3: blur_radius_ = value; break;
        }

        if (node_) {
            if (dragging_slider_ < 3 && shadow_enabled_) {
                node_->setShadow(shadow_ox_, shadow_oy_, shadow_blur_, shadow_color_);
            } else if (dragging_slider_ == 3 && blur_enabled_) {
                node_->setBlur(blur_radius_);
            }
        }

        notifyChange();
        return true;
    }

    return false;
}

bool EffectsPanel::onMouseUp(float mx, float my, int button) {
    if (editing_shadow_color_) {
        color_picker_.onMouseUp(mx, my, button);
    }

    dragging_slider_ = -1;
    return false;
}

void EffectsPanel::notifyChange() {
    if (onChange_) onChange_();
}

} // namespace editor
