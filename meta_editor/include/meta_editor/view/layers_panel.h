/*
 * Meta Editor - Layers Panel
 *
 * Show/hide, select, and manage layers.
 */

#pragma once

#include "panel.h"
#include <string>
#include <vector>

namespace flex { class Group; }

namespace meta_editor {

class Canvas;
class SelectionManager;

class LayersPanel : public Panel {
public:
    LayersPanel(Canvas* canvas, SelectionManager* selection);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

protected:
    float content_height() const override;

private:
    void render_layer_row(flex::Renderer& renderer, flex::Group* layer, int index, float y);
    int hit_test_layer(float local_y) const;
    bool hit_test_visibility(float local_x) const;

    Canvas* canvas_;
    SelectionManager* selection_;

    float row_height_ = 32;
    float header_height_ = 28;
    float icon_size_ = 20;
    int selected_layer_ = 0;
};

} // namespace meta_editor
