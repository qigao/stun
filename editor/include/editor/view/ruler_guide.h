/*
 * Rulers and Guides
 *
 * Ruler displays along canvas edges with tick marks.
 * Guides are draggable reference lines for alignment.
 */

#pragma once

#include "../core/types.h"
#include <flex/flex.h>
#include <vector>
#include <functional>

namespace editor {

// Forward declaration
class EditorViewModel;

// Guide orientation
enum class GuideOrientation {
    Horizontal,
    Vertical
};

// Single guide line
struct Guide {
    float position = 0;
    GuideOrientation orientation = GuideOrientation::Horizontal;
    bool locked = false;

    Guide() = default;
    Guide(float pos, GuideOrientation orient) : position(pos), orientation(orient) {}
};

// Ruler display settings
struct RulerStyle {
    float thickness = 20;
    Color background = {0.15f, 0.15f, 0.15f, 1.0f};
    Color tick_color = {0.6f, 0.6f, 0.6f, 1.0f};
    Color text_color = {0.7f, 0.7f, 0.7f, 1.0f};
    Color cursor_color = {1.0f, 0.3f, 0.3f, 0.8f};
    float font_size = 9;
};

// Guide display settings
struct GuideStyle {
    Color color = {0.0f, 0.7f, 1.0f, 0.8f};
    Color hover_color = {0.0f, 0.9f, 1.0f, 1.0f};
    Color locked_color = {0.5f, 0.5f, 0.5f, 0.6f};
    float line_width = 1.0f;
    float hit_tolerance = 5.0f;
};

// Rulers and Guides Manager
class RulerGuideManager {
public:
    RulerGuideManager() = default;

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Visibility
    bool rulersVisible() const { return rulers_visible_; }
    void setRulersVisible(bool v) { rulers_visible_ = v; }

    bool guidesVisible() const { return guides_visible_; }
    void setGuidesVisible(bool v) { guides_visible_ = v; }

    bool guidesLocked() const { return guides_locked_; }
    void setGuidesLocked(bool v) { guides_locked_ = v; }

    // Guide management
    const std::vector<Guide>& guides() const { return guides_; }

    void addGuide(const Guide& guide);
    void removeGuide(size_t index);
    void clearGuides();

    Guide* guideAt(float x, float y);
    int guideIndexAt(float x, float y) const;

    // Snap to guides
    bool snapEnabled() const { return snap_enabled_; }
    void setSnapEnabled(bool v) { snap_enabled_ = v; }
    float snapTolerance() const { return snap_tolerance_; }
    void setSnapTolerance(float t) { snap_tolerance_ = t; }

    // Returns snapped position (or original if no snap)
    Point snapPoint(const Point& p) const;
    float snapX(float x) const;
    float snapY(float y) const;

    // Rendering
    void render(flex::Renderer& renderer, const Rect& viewport, float zoom);
    void renderRulers(flex::Renderer& renderer, const Rect& viewport, float zoom);
    void renderGuides(flex::Renderer& renderer, const Rect& viewport, float zoom);

    // Input handling
    bool onMouseDown(float x, float y, int button);
    bool onMouseMove(float x, float y);
    bool onMouseUp(float x, float y, int button);

    // Set cursor position for ruler indicator
    void setCursorPosition(float x, float y) { cursor_x_ = x; cursor_y_ = y; }

    // Styles
    RulerStyle& rulerStyle() { return ruler_style_; }
    GuideStyle& guideStyle() { return guide_style_; }

private:
    void renderHorizontalRuler(flex::Renderer& renderer, const Rect& viewport, float zoom);
    void renderVerticalRuler(flex::Renderer& renderer, const Rect& viewport, float zoom);
    float calculateTickSpacing(float zoom) const;

    EditorViewModel* vm_ = nullptr;

    bool rulers_visible_ = true;
    bool guides_visible_ = true;
    bool guides_locked_ = false;
    bool snap_enabled_ = true;
    float snap_tolerance_ = 5.0f;

    std::vector<Guide> guides_;
    int hovered_guide_ = -1;
    int dragging_guide_ = -1;
    bool creating_guide_ = false;
    GuideOrientation creating_orientation_ = GuideOrientation::Horizontal;

    float cursor_x_ = 0, cursor_y_ = 0;

    RulerStyle ruler_style_;
    GuideStyle guide_style_;
};

} // namespace editor
