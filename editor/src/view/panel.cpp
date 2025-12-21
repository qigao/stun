/*
 * Panel Implementation
 */

#include <editor/view/panel.h>
#include <algorithm>
#include <cstdio>
#include <cfloat>

namespace editor {

// =============================================================================
// DraggablePanel Implementation
// =============================================================================

DraggablePanel::DraggablePanel(const std::string& title)
    : title_(title) {}

bool DraggablePanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + width_ &&
           my >= y_ && my < y_ + panel_style_.title_height;
}

void DraggablePanel::renderTitleBar(flex::Renderer& renderer) {
    // Title bar background
    std::string titleBg = "M " + std::to_string(x_) + " " + std::to_string(y_) +
        " h " + std::to_string(width_) +
        " v " + std::to_string(panel_style_.title_height) +
        " h " + std::to_string(-width_) + " Z";
    renderer.fill_path(titleBg, flex::Paint::solid({panel_style_.title_bg.r,
        panel_style_.title_bg.g, panel_style_.title_bg.b, panel_style_.title_bg.a}));

    // Drag grip indicator (three horizontal lines)
    if (draggable_) {
        float gripX = x_ + 8;
        float gripY = y_ + panel_style_.title_height / 2 - 4;
        for (int i = 0; i < 3; ++i) {
            std::string line = "M " + std::to_string(gripX) + " " + std::to_string(gripY + i * 3) +
                " h 8";
            renderer.stroke_path(line, flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.6f}), 1.0f);
        }
    }

    // Title text
    renderer.draw_text(title_, x_ + 22, y_ + 16, "Arial", 11, true,
        {panel_style_.title_text.r, panel_style_.title_text.g, panel_style_.title_text.b, 1});
}

bool DraggablePanel::handleDragStart(float mx, float my, int button) {
    if (!draggable_ || button != 0) return false;

    if (isInTitleBar(mx, my)) {
        dragging_ = true;
        drag_offset_x_ = mx - x_;
        drag_offset_y_ = my - y_;
        return true;
    }
    return false;
}

bool DraggablePanel::handleDragMove(float mx, float my) {
    if (dragging_) {
        x_ = mx - drag_offset_x_;
        y_ = my - drag_offset_y_;
        return true;
    }
    return false;
}

bool DraggablePanel::handleDragEnd(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    if (dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

// LabelWidget
void LabelWidget::render(flex::Renderer& renderer, float x, float y) {
    renderer.draw_text(text_, x, y + 16, "Arial", 12, false, {0.2f, 0.2f, 0.2f, 1.0f});
}

// NumberWidget
void NumberWidget::setValue(float v) {
    v = std::clamp(v, min_, max_);
    if (v != value_) {
        value_ = v;
        if (onChange_) onChange_(value_);
    }
}

void NumberWidget::render(flex::Renderer& renderer, float x, float y) {
    // Draw background
    std::string bg = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(width_) +
        " v " + std::to_string(height_) +
        " h " + std::to_string(-width_) + " Z";
    renderer.fill_path(bg, flex::Paint::solid({1.0f, 1.0f, 1.0f, 1.0f}));
    renderer.stroke_path(bg, flex::Paint::solid({0.7f, 0.7f, 0.7f, 1.0f}), 1.0f);

    // Draw value
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", value_);
    renderer.draw_text(buf, x + 4, y + 16, "Arial", 12, false, {0.1f, 0.1f, 0.1f, 1.0f});
}

bool NumberWidget::onMouseDown(float x, float y, int button) {
    (void)y;
    if (button == 0) {
        dragging_ = true;
        drag_start_x_ = x;
        drag_start_value_ = value_;
        return true;
    }
    return false;
}

bool NumberWidget::onMouseUp(float x, float y, int button) {
    (void)x; (void)y; (void)button;
    dragging_ = false;
    return false;
}

bool NumberWidget::onMouseMove(float x, float y) {
    (void)y;
    if (dragging_) {
        float delta = (x - drag_start_x_) * 0.5f;
        setValue(drag_start_value_ + delta);
        return true;
    }
    return false;
}

// ColorSwatchWidget
void ColorSwatchWidget::setColor(const Color& c) {
    color_ = c;
    if (onChange_) onChange_(color_);
}

void ColorSwatchWidget::render(flex::Renderer& renderer, float x, float y) {
    // Draw checkerboard background (for alpha)
    float cs = 6;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            bool dark = (i + j) % 2 == 0;
            std::string sq = "M " + std::to_string(x + i * cs) + " " + std::to_string(y + j * cs) +
                " h " + std::to_string(cs) + " v " + std::to_string(cs) +
                " h " + std::to_string(-cs) + " Z";
            renderer.fill_path(sq, flex::Paint::solid(dark ? flex::Color{0.8f, 0.8f, 0.8f, 1} : flex::Color{1, 1, 1, 1}));
        }
    }

    // Draw color
    std::string rect = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(width_) +
        " v " + std::to_string(height_) +
        " h " + std::to_string(-width_) + " Z";
    renderer.fill_path(rect, flex::Paint::solid({color_.r, color_.g, color_.b, color_.a}));
    renderer.stroke_path(rect, flex::Paint::solid({0.5f, 0.5f, 0.5f, 1.0f}), 1.0f);
}

bool ColorSwatchWidget::onMouseDown(float x, float y, int button) {
    (void)x; (void)y;
    if (button == 0 && onClick_) {
        onClick_();
        return true;
    }
    return false;
}

// PropertyPanel
void PropertyPanel::clear() {
    rows_.clear();
    sections_.clear();
}

void PropertyPanel::addSection(const std::string& title) {
    sections_.push_back({title, rows_.size()});
}

void PropertyPanel::addProperty(const std::string& label, std::unique_ptr<Widget> widget) {
    rows_.push_back({label, std::move(widget)});
}

void PropertyPanel::addNumberProperty(const std::string& label, float value, std::function<void(float)> onChange) {
    auto widget = std::make_unique<NumberWidget>(value);
    widget->setSize(width_ - 80, 24);
    widget->onChange(std::move(onChange));
    addProperty(label, std::move(widget));
}

void PropertyPanel::addColorProperty(const std::string& label, const Color& color,
                                     std::function<void(const Color&)> onChange,
                                     std::function<void()> onClick) {
    auto widget = std::make_unique<ColorSwatchWidget>(color);
    widget->onChange(std::move(onChange));
    if (onClick) widget->onClick(std::move(onClick));
    addProperty(label, std::move(widget));
}

bool PropertyPanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + width_ &&
           my >= y_ && my < y_ + style_.title_height;
}

void PropertyPanel::render(flex::Renderer& renderer) {
    float py = y_ + style_.title_height + style_.padding;
    size_t sectionIdx = 0;

    // Background with rounded corners
    std::string bg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(width_ - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(height_ - 8) +
        " a 4 4 0 0 1 -4 4" +
        " h " + std::to_string(-(width_ - 8)) +
        " a 4 4 0 0 1 -4 -4" +
        " v " + std::to_string(-(height_ - 8)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));

    // Title bar
    std::string titleBg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(width_ - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.title_height - 4) +
        " h " + std::to_string(-width_) +
        " v " + std::to_string(-(style_.title_height - 4)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(titleBg, flex::Paint::solid({style_.title_bg.r, style_.title_bg.g,
        style_.title_bg.b, style_.title_bg.a}));

    // Drag grip
    if (draggable_) {
        float gripX = x_ + 8;
        float gripY = y_ + style_.title_height / 2 - 4;
        for (int i = 0; i < 3; ++i) {
            std::string line = "M " + std::to_string(gripX) + " " + std::to_string(gripY + i * 3) +
                " h 8";
            renderer.stroke_path(line, flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.5f}), 1.0f);
        }
    }

    // Title
    renderer.draw_text("Properties", x_ + 22, y_ + 16, "Arial", 11, true,
        {style_.title_text.r, style_.title_text.g, style_.title_text.b, 1});

    for (size_t i = 0; i < rows_.size(); ++i) {
        // Check for section header
        while (sectionIdx < sections_.size() && sections_[sectionIdx].startIndex == i) {
            // Section background
            std::string secBg = "M " + std::to_string(x_) + " " + std::to_string(py) +
                " h " + std::to_string(width_) +
                " v " + std::to_string(style_.section_height) +
                " h " + std::to_string(-width_) + " Z";
            renderer.fill_path(secBg, flex::Paint::solid({style_.section_bg.r, style_.section_bg.g,
                style_.section_bg.b, style_.section_bg.a}));

            renderer.draw_text(sections_[sectionIdx].title, x_ + style_.padding, py + 16, "Arial", 11, true,
                {style_.section_text.r, style_.section_text.g, style_.section_text.b, 1});
            py += style_.section_height;
            ++sectionIdx;
        }

        auto& row = rows_[i];

        // Draw label
        renderer.draw_text(row.label, x_ + style_.padding, py + 16, "Arial", 10, false,
            {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

        // Draw widget
        if (row.widget) {
            row.widget->render(renderer, x_ + row.labelWidth + style_.padding, py);
        }

        py += row.widget ? row.widget->height() + 6 : style_.row_height;
    }

    height_ = py - y_ + style_.padding;
}

bool PropertyPanel::onMouseDown(float mx, float my, int button) {
    if (button != 0) return false;

    // Check for panel drag start on title bar
    if (draggable_ && isInTitleBar(mx, my)) {
        dragging_ = true;
        drag_offset_x_ = mx - x_;
        drag_offset_y_ = my - y_;
        return true;
    }

    float wy = y_ + style_.title_height + style_.padding;
    size_t sectionIdx = 0;

    for (size_t i = 0; i < rows_.size(); ++i) {
        // Skip section headers
        while (sectionIdx < sections_.size() && sections_[sectionIdx].startIndex == i) {
            wy += style_.section_height;
            ++sectionIdx;
        }

        auto& row = rows_[i];
        if (row.widget) {
            float wx = x_ + row.labelWidth + style_.padding;
            if (mx >= wx && mx < wx + row.widget->width() &&
                my >= wy && my < wy + row.widget->height()) {
                return row.widget->onMouseDown(mx - wx, my - wy, button);
            }
            wy += row.widget->height() + 6;
        } else {
            wy += style_.row_height;
        }
    }

    return mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_;
}

bool PropertyPanel::onMouseMove(float mx, float my) {
    // Handle panel dragging
    if (dragging_) {
        x_ = mx - drag_offset_x_;
        y_ = my - drag_offset_y_;
        return true;
    }

    // Forward to widgets that are being dragged
    for (auto& row : rows_) {
        if (row.widget && row.widget->onMouseMove(mx - x_ - row.labelWidth - style_.padding, my - y_)) {
            return true;
        }
    }

    return isInTitleBar(mx, my);
}

bool PropertyPanel::onMouseUp(float mx, float my, int button) {
    if (dragging_) {
        dragging_ = false;
        return true;
    }

    for (auto& row : rows_) {
        if (row.widget) {
            row.widget->onMouseUp(mx - x_ - row.labelWidth - style_.padding, my - y_, button);
        }
    }
    return false;
}

} // namespace editor
