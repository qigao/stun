/*
 * Meta Editor - Zoom Panel Implementation
 */

#include "meta_editor/view/zoom_panel.h"
#include "meta_editor/canvas.h"
#include "flexUI/box.h"
#include "flexUI/element.h"
#include "flexUI/widgets/stepper_widget.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace meta_editor {

ZoomPanel::ZoomPanel(Canvas* canvas) : canvas_(canvas) {}

void ZoomPanel::setup_ui(flexUI::Box* box, flexUI::Element* parent) {
    box_ = box;

    // Create container
    auto* container = box->create("div", "zoom_panel");
    container->add_class("floating_panel");
    parent->append(container);

    // Zoom out button
    auto* zoom_out = box->create("button", "zoom_out");
    zoom_out->set_text("−");
    zoom_out->add_class("zoom_btn");
    zoom_out->on_click([this]() {
        float current = canvas_->camera_zoom();
        // Find next lower preset
        for (int i = NUM_PRESETS - 1; i >= 0; --i) {
            float preset = ZOOM_PRESETS[i] / 100.0f;
            if (preset < current - 0.01f) {
                canvas_->zoom(preset / current);
                update();
                return;
            }
        }
    });
    container->append(zoom_out);

    // Zoom label
    zoom_label_ = box->create("span", "zoom_label");
    zoom_label_->set_text("100%");
    zoom_label_->add_class("zoom_text");
    container->append(zoom_label_);

    // Zoom in button
    auto* zoom_in = box->create("button", "zoom_in");
    zoom_in->set_text("+");
    zoom_in->add_class("zoom_btn");
    zoom_in->on_click([this]() {
        float current = canvas_->camera_zoom();
        // Find next higher preset
        for (int i = 0; i < NUM_PRESETS; ++i) {
            float preset = ZOOM_PRESETS[i] / 100.0f;
            if (preset > current + 0.01f) {
                canvas_->zoom(preset / current);
                update();
                return;
            }
        }
    });
    container->append(zoom_in);

    // Fit button
    auto* fit_btn = box->create("button", "zoom_fit");
    fit_btn->set_text("⊡");
    fit_btn->add_class("zoom_btn");
    fit_btn->on_click([this]() {
        on_fit_clicked();
    });
    container->append(fit_btn);

    // Reset button (100%)
    auto* reset_btn = box->create("button", "zoom_reset");
    reset_btn->set_text("1:1");
    reset_btn->add_class("zoom_btn");
    reset_btn->on_click([this]() {
        on_reset_clicked();
    });
    container->append(reset_btn);
}

void ZoomPanel::update() {
    if (!zoom_label_) return;

    int percent = static_cast<int>(std::round(canvas_->camera_zoom() * 100));
    std::stringstream ss;
    ss << percent << "%";
    zoom_label_->set_text(ss.str());
}

void ZoomPanel::on_zoom_changed(int percent) {
    float target = percent / 100.0f;
    float current = canvas_->camera_zoom();
    if (current > 0) {
        canvas_->zoom(target / current);
    }
    update();
}

void ZoomPanel::on_fit_clicked() {
    canvas_->fit_to_view();
    update();
}

void ZoomPanel::on_reset_clicked() {
    canvas_->reset_camera();
    update();
}

// Static member definition
constexpr int ZoomPanel::ZOOM_PRESETS[];

} // namespace meta_editor
