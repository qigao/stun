/*
 * Rulers and Guides Implementation
 */

#include <editor/view/ruler_guide.h>
#include <editor/viewmodel/editor_vm.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace editor {

void RulerGuideManager::addGuide(const Guide& guide) {
    guides_.push_back(guide);
}

void RulerGuideManager::removeGuide(size_t index) {
    if (index < guides_.size()) {
        guides_.erase(guides_.begin() + index);
    }
}

void RulerGuideManager::clearGuides() {
    guides_.clear();
}

Guide* RulerGuideManager::guideAt(float x, float y) {
    int idx = guideIndexAt(x, y);
    return idx >= 0 ? &guides_[idx] : nullptr;
}

int RulerGuideManager::guideIndexAt(float x, float y) const {
    float tol = guide_style_.hit_tolerance;
    for (size_t i = 0; i < guides_.size(); ++i) {
        const auto& g = guides_[i];
        if (g.orientation == GuideOrientation::Horizontal) {
            if (std::abs(y - g.position) < tol) return static_cast<int>(i);
        } else {
            if (std::abs(x - g.position) < tol) return static_cast<int>(i);
        }
    }
    return -1;
}

Point RulerGuideManager::snapPoint(const Point& p) const {
    return {snapX(p.x), snapY(p.y)};
}

float RulerGuideManager::snapX(float x) const {
    if (!snap_enabled_) return x;

    for (const auto& g : guides_) {
        if (g.orientation == GuideOrientation::Vertical) {
            if (std::abs(x - g.position) < snap_tolerance_) {
                return g.position;
            }
        }
    }
    return x;
}

float RulerGuideManager::snapY(float y) const {
    if (!snap_enabled_) return y;

    for (const auto& g : guides_) {
        if (g.orientation == GuideOrientation::Horizontal) {
            if (std::abs(y - g.position) < snap_tolerance_) {
                return g.position;
            }
        }
    }
    return y;
}

void RulerGuideManager::render(flex::Renderer& renderer, const Rect& viewport, float zoom) {
    if (guides_visible_) {
        renderGuides(renderer, viewport, zoom);
    }
    if (rulers_visible_) {
        renderRulers(renderer, viewport, zoom);
    }
}

void RulerGuideManager::renderRulers(flex::Renderer& renderer, const Rect& viewport, float zoom) {
    renderHorizontalRuler(renderer, viewport, zoom);
    renderVerticalRuler(renderer, viewport, zoom);

    // Corner square
    float t = ruler_style_.thickness;
    std::string corner = "M 0 0 h " + std::to_string(t) + " v " + std::to_string(t) +
                        " h " + std::to_string(-t) + " Z";
    renderer.fill_path(corner, flex::Paint::solid({ruler_style_.background.r,
        ruler_style_.background.g, ruler_style_.background.b, 1.0f}));
}

void RulerGuideManager::renderHorizontalRuler(flex::Renderer& renderer, const Rect& viewport, float zoom) {
    float t = ruler_style_.thickness;
    float w = viewport.width;

    // Background
    std::string bg = "M " + std::to_string(t) + " 0 h " + std::to_string(w - t) +
                    " v " + std::to_string(t) + " h " + std::to_string(-(w - t)) + " Z";
    renderer.fill_path(bg, flex::Paint::solid({ruler_style_.background.r,
        ruler_style_.background.g, ruler_style_.background.b, ruler_style_.background.a}));

    // Calculate tick spacing based on zoom
    float spacing = calculateTickSpacing(zoom);
    float majorSpacing = spacing * 5;

    // Get world coordinates
    float worldStartX = -viewport.x / zoom;
    float worldEndX = worldStartX + w / zoom;

    // Align to tick spacing
    float firstTick = std::floor(worldStartX / spacing) * spacing;

    for (float worldX = firstTick; worldX <= worldEndX; worldX += spacing) {
        float screenX = t + (worldX - worldStartX) * zoom;
        if (screenX < t || screenX > w) continue;

        bool isMajor = std::fmod(std::abs(worldX), majorSpacing) < 0.01f;
        float tickHeight = isMajor ? t * 0.6f : t * 0.3f;

        std::string tick = "M " + std::to_string(screenX) + " " + std::to_string(t - tickHeight) +
                          " v " + std::to_string(tickHeight);
        renderer.stroke_path(tick, flex::Paint::solid({ruler_style_.tick_color.r,
            ruler_style_.tick_color.g, ruler_style_.tick_color.b, 1.0f}), 1.0f);

        // Label for major ticks
        if (isMajor) {
            char label[16];
            snprintf(label, sizeof(label), "%.0f", worldX);
            renderer.draw_text(label, screenX + 2, t - 4, "Arial",
                ruler_style_.font_size, false,
                {ruler_style_.text_color.r, ruler_style_.text_color.g,
                 ruler_style_.text_color.b, 1.0f});
        }
    }

    // Cursor indicator
    float cursorScreenX = t + (cursor_x_ - worldStartX) * zoom;
    if (cursorScreenX >= t && cursorScreenX <= w) {
        std::string cursor = "M " + std::to_string(cursorScreenX) + " 0 v " + std::to_string(t);
        renderer.stroke_path(cursor, flex::Paint::solid({ruler_style_.cursor_color.r,
            ruler_style_.cursor_color.g, ruler_style_.cursor_color.b, ruler_style_.cursor_color.a}), 1.0f);
    }
}

void RulerGuideManager::renderVerticalRuler(flex::Renderer& renderer, const Rect& viewport, float zoom) {
    float t = ruler_style_.thickness;
    float h = viewport.height;

    // Background
    std::string bg = "M 0 " + std::to_string(t) + " h " + std::to_string(t) +
                    " v " + std::to_string(h - t) + " h " + std::to_string(-t) + " Z";
    renderer.fill_path(bg, flex::Paint::solid({ruler_style_.background.r,
        ruler_style_.background.g, ruler_style_.background.b, ruler_style_.background.a}));

    // Calculate tick spacing based on zoom
    float spacing = calculateTickSpacing(zoom);
    float majorSpacing = spacing * 5;

    // Get world coordinates
    float worldStartY = -viewport.y / zoom;
    float worldEndY = worldStartY + h / zoom;

    // Align to tick spacing
    float firstTick = std::floor(worldStartY / spacing) * spacing;

    for (float worldY = firstTick; worldY <= worldEndY; worldY += spacing) {
        float screenY = t + (worldY - worldStartY) * zoom;
        if (screenY < t || screenY > h) continue;

        bool isMajor = std::fmod(std::abs(worldY), majorSpacing) < 0.01f;
        float tickWidth = isMajor ? t * 0.6f : t * 0.3f;

        std::string tick = "M " + std::to_string(t - tickWidth) + " " + std::to_string(screenY) +
                          " h " + std::to_string(tickWidth);
        renderer.stroke_path(tick, flex::Paint::solid({ruler_style_.tick_color.r,
            ruler_style_.tick_color.g, ruler_style_.tick_color.b, 1.0f}), 1.0f);

        // Label for major ticks (rotated text not supported, so skip or use small numbers)
        if (isMajor && worldY != 0) {
            char label[16];
            snprintf(label, sizeof(label), "%.0f", worldY);
            // Position text sideways (limited without rotation)
            renderer.draw_text(label, 2, screenY + 3, "Arial",
                ruler_style_.font_size, false,
                {ruler_style_.text_color.r, ruler_style_.text_color.g,
                 ruler_style_.text_color.b, 1.0f});
        }
    }

    // Cursor indicator
    float cursorScreenY = t + (cursor_y_ - worldStartY) * zoom;
    if (cursorScreenY >= t && cursorScreenY <= h) {
        std::string cursor = "M 0 " + std::to_string(cursorScreenY) + " h " + std::to_string(t);
        renderer.stroke_path(cursor, flex::Paint::solid({ruler_style_.cursor_color.r,
            ruler_style_.cursor_color.g, ruler_style_.cursor_color.b, ruler_style_.cursor_color.a}), 1.0f);
    }
}

void RulerGuideManager::renderGuides(flex::Renderer& renderer, const Rect& viewport, float zoom) {
    float t = ruler_style_.thickness;

    for (size_t i = 0; i < guides_.size(); ++i) {
        const auto& g = guides_[i];

        Color c = g.locked ? guide_style_.locked_color :
                  (static_cast<int>(i) == hovered_guide_ ? guide_style_.hover_color : guide_style_.color);

        if (g.orientation == GuideOrientation::Horizontal) {
            float worldStartY = -viewport.y / zoom;
            float screenY = t + (g.position - worldStartY) * zoom;

            std::string line = "M " + std::to_string(t) + " " + std::to_string(screenY) +
                              " h " + std::to_string(viewport.width - t);
            renderer.stroke_path(line, flex::Paint::solid({c.r, c.g, c.b, c.a}),
                guide_style_.line_width);
        } else {
            float worldStartX = -viewport.x / zoom;
            float screenX = t + (g.position - worldStartX) * zoom;

            std::string line = "M " + std::to_string(screenX) + " " + std::to_string(t) +
                              " v " + std::to_string(viewport.height - t);
            renderer.stroke_path(line, flex::Paint::solid({c.r, c.g, c.b, c.a}),
                guide_style_.line_width);
        }
    }
}

float RulerGuideManager::calculateTickSpacing(float zoom) const {
    // Base spacing in world units
    float baseSpacing = 10.0f;

    // Adjust based on zoom to keep screen spacing reasonable
    float screenSpacing = baseSpacing * zoom;

    if (screenSpacing < 5) {
        baseSpacing *= 10;
    } else if (screenSpacing < 10) {
        baseSpacing *= 5;
    } else if (screenSpacing < 20) {
        baseSpacing *= 2;
    } else if (screenSpacing > 100) {
        baseSpacing /= 2;
    } else if (screenSpacing > 200) {
        baseSpacing /= 5;
    }

    return baseSpacing;
}

bool RulerGuideManager::onMouseDown(float x, float y, int button) {
    if (!rulers_visible_ && !guides_visible_) return false;
    if (button != 0) return false;

    float t = ruler_style_.thickness;

    // Check if clicking on ruler to create guide
    if (rulers_visible_) {
        if (y < t && x >= t) {
            // Horizontal ruler - create vertical guide
            if (!guides_locked_) {
                creating_guide_ = true;
                creating_orientation_ = GuideOrientation::Vertical;
                guides_.push_back(Guide(cursor_x_, GuideOrientation::Vertical));
                dragging_guide_ = static_cast<int>(guides_.size()) - 1;
                return true;
            }
        } else if (x < t && y >= t) {
            // Vertical ruler - create horizontal guide
            if (!guides_locked_) {
                creating_guide_ = true;
                creating_orientation_ = GuideOrientation::Horizontal;
                guides_.push_back(Guide(cursor_y_, GuideOrientation::Horizontal));
                dragging_guide_ = static_cast<int>(guides_.size()) - 1;
                return true;
            }
        }
    }

    // Check if clicking on existing guide
    if (guides_visible_ && !guides_locked_) {
        int idx = guideIndexAt(cursor_x_, cursor_y_);
        if (idx >= 0 && !guides_[idx].locked) {
            dragging_guide_ = idx;
            return true;
        }
    }

    return false;
}

bool RulerGuideManager::onMouseMove(float x, float y) {
    cursor_x_ = x;
    cursor_y_ = y;

    if (dragging_guide_ >= 0) {
        auto& g = guides_[dragging_guide_];
        if (g.orientation == GuideOrientation::Horizontal) {
            g.position = y;
        } else {
            g.position = x;
        }
        return true;
    }

    // Update hover state
    hovered_guide_ = guideIndexAt(x, y);

    return false;
}

bool RulerGuideManager::onMouseUp(float x, float y, int button) {
    (void)x; (void)y; (void)button;

    if (creating_guide_ && dragging_guide_ >= 0) {
        // Remove guide if dragged back onto ruler
        float t = ruler_style_.thickness;
        auto& g = guides_[dragging_guide_];

        bool remove = false;
        if (g.orientation == GuideOrientation::Horizontal) {
            // Check if back on vertical ruler
            // Use screen position logic - simplified check
            remove = false;  // Keep for now
        } else {
            // Check if back on horizontal ruler
            remove = false;  // Keep for now
        }

        if (remove) {
            removeGuide(dragging_guide_);
        }
    }

    creating_guide_ = false;
    dragging_guide_ = -1;
    return false;
}

} // namespace editor
