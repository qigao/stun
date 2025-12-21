/*
 * Grid System Implementation
 */

#include <editor/view/grid.h>
#include <editor/viewmodel/editor_vm.h>
#include <cmath>
#include <cstdio>

namespace editor {

// ============================================================================
// Grid
// ============================================================================

Grid::Grid() {}

Point Grid::snapPoint(const Point& p) const {
    if (!settings_.snap_enabled) return p;

    float snapSize = settings_.size / settings_.subdivisions;
    return {
        std::round(p.x / snapSize) * snapSize,
        std::round(p.y / snapSize) * snapSize
    };
}

float Grid::snapValue(float v) const {
    if (!settings_.snap_enabled) return v;

    float snapSize = settings_.size / settings_.subdivisions;
    return std::round(v / snapSize) * snapSize;
}

void Grid::render(flex::Renderer& renderer, const Rect& visibleArea, float zoom) {
    if (!settings_.visible) return;

    if (settings_.type == GridType::Rectangular) {
        renderRectangularGrid(renderer, visibleArea, zoom);
    } else {
        renderIsometricGrid(renderer, visibleArea, zoom);
    }
}

void Grid::renderRectangularGrid(flex::Renderer& renderer, const Rect& area, float zoom) {
    float majorSize = settings_.size;
    float minorSize = majorSize / settings_.subdivisions;

    // Adjust for zoom - don't draw grid lines that would be too close
    float minSpacing = 8.0f;  // Minimum pixels between lines
    while (minorSize * zoom < minSpacing && settings_.subdivisions > 1) {
        minorSize *= 2;
    }
    while (majorSize * zoom < minSpacing * 4) {
        majorSize *= 2;
        minorSize = majorSize / settings_.subdivisions;
    }

    // Calculate grid bounds
    float startX = std::floor(area.x / minorSize) * minorSize;
    float startY = std::floor(area.y / minorSize) * minorSize;
    float endX = area.x + area.width;
    float endY = area.y + area.height;

    // Draw minor grid lines
    for (float x = startX; x <= endX; x += minorSize) {
        bool isMajor = std::fmod(std::abs(x), majorSize) < 0.001f;
        if (isMajor) continue;  // Draw major lines separately

        std::string line = "M " + std::to_string(x) + " " + std::to_string(area.y) +
            " v " + std::to_string(area.height);
        renderer.stroke_path(line, flex::Paint::solid({settings_.minor_color.r,
            settings_.minor_color.g, settings_.minor_color.b, settings_.minor_color.a}), 1.0f);
    }

    for (float y = startY; y <= endY; y += minorSize) {
        bool isMajor = std::fmod(std::abs(y), majorSize) < 0.001f;
        if (isMajor) continue;

        std::string line = "M " + std::to_string(area.x) + " " + std::to_string(y) +
            " h " + std::to_string(area.width);
        renderer.stroke_path(line, flex::Paint::solid({settings_.minor_color.r,
            settings_.minor_color.g, settings_.minor_color.b, settings_.minor_color.a}), 1.0f);
    }

    // Draw major grid lines
    startX = std::floor(area.x / majorSize) * majorSize;
    startY = std::floor(area.y / majorSize) * majorSize;

    for (float x = startX; x <= endX; x += majorSize) {
        std::string line = "M " + std::to_string(x) + " " + std::to_string(area.y) +
            " v " + std::to_string(area.height);
        renderer.stroke_path(line, flex::Paint::solid({settings_.major_color.r,
            settings_.major_color.g, settings_.major_color.b, settings_.major_color.a}), 1.0f);
    }

    for (float y = startY; y <= endY; y += majorSize) {
        std::string line = "M " + std::to_string(area.x) + " " + std::to_string(y) +
            " h " + std::to_string(area.width);
        renderer.stroke_path(line, flex::Paint::solid({settings_.major_color.r,
            settings_.major_color.g, settings_.major_color.b, settings_.major_color.a}), 1.0f);
    }
}

void Grid::renderIsometricGrid(flex::Renderer& renderer, const Rect& area, float zoom) {
    float size = settings_.size;

    // Adjust for zoom
    float minSpacing = 20.0f;
    while (size * zoom < minSpacing) {
        size *= 2;
    }

    // Isometric angles: 30 degrees
    float angle = 30.0f * 3.14159f / 180.0f;
    float dx = size * std::cos(angle);
    float dy = size * std::sin(angle);

    // Calculate how many lines we need
    float diagonalLength = std::sqrt(area.width * area.width + area.height * area.height);
    int numLines = static_cast<int>(diagonalLength / dx) + 2;

    // Draw lines going down-right
    for (int i = -numLines; i <= numLines; ++i) {
        float startX = area.x + i * dx;
        float startY = area.y;
        float endX = startX + area.height / std::tan(angle);
        float endY = area.y + area.height;

        std::string line = "M " + std::to_string(startX) + " " + std::to_string(startY) +
            " L " + std::to_string(endX) + " " + std::to_string(endY);
        renderer.stroke_path(line, flex::Paint::solid({settings_.major_color.r,
            settings_.major_color.g, settings_.major_color.b, settings_.major_color.a}), 1.0f);
    }

    // Draw lines going down-left
    for (int i = -numLines; i <= numLines; ++i) {
        float startX = area.x + i * dx;
        float startY = area.y;
        float endX = startX - area.height / std::tan(angle);
        float endY = area.y + area.height;

        std::string line = "M " + std::to_string(startX) + " " + std::to_string(startY) +
            " L " + std::to_string(endX) + " " + std::to_string(endY);
        renderer.stroke_path(line, flex::Paint::solid({settings_.major_color.r,
            settings_.major_color.g, settings_.major_color.b, settings_.major_color.a}), 1.0f);
    }
}

// ============================================================================
// Grid Panel
// ============================================================================

GridPanel::GridPanel() {}

bool GridPanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + style_.width &&
           my >= y_ && my < y_ + style_.title_height;
}

void GridPanel::render(flex::Renderer& renderer) {
    if (!grid_) return;

    // Background with rounded corners
    std::string bg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.height - 8) +
        " a 4 4 0 0 1 -4 4" +
        " h " + std::to_string(-(style_.width - 8)) +
        " a 4 4 0 0 1 -4 -4" +
        " v " + std::to_string(-(style_.height - 8)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));

    // Title bar
    std::string titleBg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.title_height - 4) +
        " h " + std::to_string(-style_.width) +
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
    renderer.draw_text("Grid Settings", x_ + 22, y_ + 16, "Arial", 11, true,
        {style_.title_text.r, style_.title_text.g, style_.title_text.b, 1});

    float rowY = y_ + style_.title_height + style_.padding;

    // Show Grid toggle
    renderer.draw_text("Show Grid", x_ + style_.padding, rowY + 14, "Arial", 11, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});
    renderToggle(renderer, x_ + style_.width - 50, rowY + 2, grid_->isVisible(), hovered_element_ == 0);
    rowY += style_.row_height;

    // Snap to Grid toggle
    renderer.draw_text("Snap to Grid", x_ + style_.padding, rowY + 14, "Arial", 11, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});
    renderToggle(renderer, x_ + style_.width - 50, rowY + 2, grid_->isSnapEnabled(), hovered_element_ == 1);
    rowY += style_.row_height;

    // Grid Size
    renderer.draw_text("Size", x_ + style_.padding, rowY + 14, "Arial", 11, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    char sizeText[16];
    snprintf(sizeText, sizeof(sizeText), "%.0fpx", grid_->size());
    renderer.draw_text(sizeText, x_ + style_.width - 45, rowY + 14, "Arial", 11, false,
        {style_.value_text.r, style_.value_text.g, style_.value_text.b, 1});

    renderSlider(renderer, x_ + 60, rowY + 6, style_.width - 120,
                 (grid_->size() - 5) / 95.0f, 5, 100);
    rowY += style_.row_height;

    // Subdivisions
    renderer.draw_text("Subdivisions", x_ + style_.padding, rowY + 14, "Arial", 11, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    char subText[8];
    snprintf(subText, sizeof(subText), "%d", grid_->subdivisions());
    renderer.draw_text(subText, x_ + style_.width - 30, rowY + 14, "Arial", 11, false,
        {style_.value_text.r, style_.value_text.g, style_.value_text.b, 1});

    renderSlider(renderer, x_ + 85, rowY + 6, style_.width - 130,
                 (grid_->subdivisions() - 1) / 7.0f, 1, 8);
    rowY += style_.row_height;

    // Grid Type
    renderer.draw_text("Type", x_ + style_.padding, rowY + 14, "Arial", 11, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    const char* typeText = grid_->gridType() == GridType::Rectangular ? "Rectangular" : "Isometric";
    renderer.draw_text(typeText, x_ + 60, rowY + 14, "Arial", 11, false,
        {style_.value_text.r, style_.value_text.g, style_.value_text.b, 1});
}

void GridPanel::renderToggle(flex::Renderer& renderer, float x, float y, bool value, bool hovered) {
    float w = 36, h = 18;

    // Track
    std::string track = "M " + std::to_string(x + h/2) + " " + std::to_string(y) +
        " h " + std::to_string(w - h) +
        " a " + std::to_string(h/2) + " " + std::to_string(h/2) + " 0 0 1 0 " + std::to_string(h) +
        " h " + std::to_string(-(w - h)) +
        " a " + std::to_string(h/2) + " " + std::to_string(h/2) + " 0 0 1 0 " + std::to_string(-h);

    Color trackColor = value ? style_.toggle_on : style_.toggle_off;
    if (hovered) {
        trackColor.r = std::min(1.0f, trackColor.r + 0.1f);
        trackColor.g = std::min(1.0f, trackColor.g + 0.1f);
        trackColor.b = std::min(1.0f, trackColor.b + 0.1f);
    }
    renderer.fill_path(track, flex::Paint::solid({trackColor.r, trackColor.g, trackColor.b, 1}));

    // Thumb
    float thumbX = value ? (x + w - h/2 - 2) : (x + h/2 + 2);
    float thumbR = h/2 - 3;
    std::string thumb = "M " + std::to_string(thumbX - thumbR) + " " + std::to_string(y + h/2) +
        " a " + std::to_string(thumbR) + " " + std::to_string(thumbR) + " 0 1 1 " +
        std::to_string(thumbR * 2) + " 0 a " + std::to_string(thumbR) + " " + std::to_string(thumbR) +
        " 0 1 1 " + std::to_string(-thumbR * 2) + " 0";
    renderer.fill_path(thumb, flex::Paint::solid({1, 1, 1, 1}));
}

void GridPanel::renderSlider(flex::Renderer& renderer, float x, float y, float w, float value, float min, float max) {
    (void)min; (void)max;
    float h = 12;

    // Track
    std::string track = "M " + std::to_string(x) + " " + std::to_string(y + h/2 - 2) +
        " h " + std::to_string(w) + " v 4 h " + std::to_string(-w) + " Z";
    renderer.fill_path(track, flex::Paint::solid({0.3f, 0.3f, 0.3f, 1}));

    // Fill
    float fillW = value * w;
    std::string fill = "M " + std::to_string(x) + " " + std::to_string(y + h/2 - 2) +
        " h " + std::to_string(fillW) + " v 4 h " + std::to_string(-fillW) + " Z";
    renderer.fill_path(fill, flex::Paint::solid({0.4f, 0.6f, 0.9f, 1}));

    // Thumb
    float thumbX = x + fillW;
    std::string thumb = "M " + std::to_string(thumbX - 4) + " " + std::to_string(y) +
        " h 8 v " + std::to_string(h) + " h -8 Z";
    renderer.fill_path(thumb, flex::Paint::solid({0.9f, 0.9f, 0.9f, 1}));
}

int GridPanel::elementAt(float mx, float my) const {
    float rowY = y_ + style_.title_height + style_.padding;

    // Show Grid toggle (element 0)
    if (mx >= x_ + style_.width - 50 && mx < x_ + style_.width - 14 &&
        my >= rowY && my < rowY + style_.row_height) {
        return 0;
    }
    rowY += style_.row_height;

    // Snap toggle (element 1)
    if (mx >= x_ + style_.width - 50 && mx < x_ + style_.width - 14 &&
        my >= rowY && my < rowY + style_.row_height) {
        return 1;
    }
    rowY += style_.row_height;

    // Size slider (element 2)
    if (mx >= x_ + 60 && mx < x_ + style_.width - 60 &&
        my >= rowY && my < rowY + style_.row_height) {
        return 2;
    }
    rowY += style_.row_height;

    // Subdivisions slider (element 3)
    if (mx >= x_ + 85 && mx < x_ + style_.width - 45 &&
        my >= rowY && my < rowY + style_.row_height) {
        return 3;
    }
    rowY += style_.row_height;

    // Grid type (element 4)
    if (mx >= x_ + 60 && mx < x_ + style_.width - style_.padding &&
        my >= rowY && my < rowY + style_.row_height) {
        return 4;
    }

    return -1;
}

bool GridPanel::onMouseDown(float mx, float my, int button) {
    if (button != 0) return false;

    // Check for panel drag start on title bar
    if (draggable_ && isInTitleBar(mx, my)) {
        panel_dragging_ = true;
        panel_drag_offset_x_ = mx - x_;
        panel_drag_offset_y_ = my - y_;
        return true;
    }

    if (!grid_) return false;

    int elem = elementAt(mx, my);

    if (elem == 0) {
        grid_->setVisible(!grid_->isVisible());
        return true;
    }
    if (elem == 1) {
        grid_->setSnapEnabled(!grid_->isSnapEnabled());
        return true;
    }
    if (elem == 2) {
        dragging_slider_ = true;
        drag_slider_ = 2;
        float sliderX = x_ + 60;
        float sliderW = style_.width - 120;
        float t = (mx - sliderX) / sliderW;
        t = std::max(0.0f, std::min(1.0f, t));
        grid_->setSize(5 + t * 95);
        return true;
    }
    if (elem == 3) {
        dragging_slider_ = true;
        drag_slider_ = 3;
        float sliderX = x_ + 85;
        float sliderW = style_.width - 130;
        float t = (mx - sliderX) / sliderW;
        t = std::max(0.0f, std::min(1.0f, t));
        grid_->setSubdivisions(1 + static_cast<int>(t * 7 + 0.5f));
        return true;
    }
    if (elem == 4) {
        grid_->setGridType(grid_->gridType() == GridType::Rectangular ?
            GridType::Isometric : GridType::Rectangular);
        return true;
    }

    return false;
}

bool GridPanel::onMouseMove(float mx, float my) {
    // Handle panel dragging
    if (panel_dragging_) {
        x_ = mx - panel_drag_offset_x_;
        y_ = my - panel_drag_offset_y_;
        return true;
    }

    if (dragging_slider_ && grid_) {
        if (drag_slider_ == 2) {
            float sliderX = x_ + 60;
            float sliderW = style_.width - 120;
            float t = (mx - sliderX) / sliderW;
            t = std::max(0.0f, std::min(1.0f, t));
            grid_->setSize(5 + t * 95);
        } else if (drag_slider_ == 3) {
            float sliderX = x_ + 85;
            float sliderW = style_.width - 130;
            float t = (mx - sliderX) / sliderW;
            t = std::max(0.0f, std::min(1.0f, t));
            grid_->setSubdivisions(1 + static_cast<int>(t * 7 + 0.5f));
        }
        return true;
    }

    hovered_element_ = elementAt(mx, my);
    return hovered_element_ >= 0 || isInTitleBar(mx, my);
}

bool GridPanel::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    if (panel_dragging_) {
        panel_dragging_ = false;
        return true;
    }
    dragging_slider_ = false;
    drag_slider_ = -1;
    return false;
}

} // namespace editor
