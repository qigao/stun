/*
 * Meta Editor - Align Panel
 *
 * Align, distribute, group/ungroup selected shapes.
 */

#pragma once

#include "panel.h"
#include <functional>

namespace meta_editor {

class Canvas;
class SelectionManager;

class AlignPanel : public Panel {
public:
    AlignPanel(Canvas* canvas, SelectionManager* selection);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    // Callbacks for group/ungroup (handled by MetaEditor)
    void set_group_callback(std::function<void()> cb) { on_group_ = cb; }
    void set_ungroup_callback(std::function<void()> cb) { on_ungroup_ = cb; }

protected:
    float content_height() const override;

private:
    enum class AlignType {
        Left, CenterH, Right,
        Top, CenterV, Bottom,
        DistributeH, DistributeV,
        Group, Ungroup
    };

    void render_button(flex::Renderer& renderer, float x, float y, AlignType type);
    void do_align(AlignType type);

    Canvas* canvas_;
    SelectionManager* selection_;

    std::function<void()> on_group_;
    std::function<void()> on_ungroup_;

    float button_size_ = 28;
    float gap_ = 4;
    float padding_ = 8;
};

} // namespace meta_editor
