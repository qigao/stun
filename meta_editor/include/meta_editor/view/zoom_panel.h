/*
 * Meta Editor - Zoom Panel
 *
 * Floating panel with zoom controls:
 * - Zoom percentage stepper
 * - Fit to view button
 * - Reset (100%) button
 */

#pragma once

#include <flexUI.h>
#include <functional>

namespace meta_editor {

class Canvas;

class ZoomPanel {
public:
    explicit ZoomPanel(Canvas* canvas);

    // Create UI elements within the given box
    void setup_ui(flexUI::Box* box, flexUI::Element* parent);

    // Update zoom display from canvas state
    void update();

private:
    void on_zoom_changed(int percent);
    void on_fit_clicked();
    void on_reset_clicked();

    Canvas* canvas_;
    flexUI::Box* box_ = nullptr;
    flexUI::Element* zoom_label_ = nullptr;

    // Zoom presets
    static constexpr int ZOOM_PRESETS[] = {25, 50, 75, 100, 125, 150, 200, 400};
    static constexpr int NUM_PRESETS = 8;
};

} // namespace meta_editor
