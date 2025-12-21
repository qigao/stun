/*
 * TvgUiLayer - Bridge between Editor and tvgbox2
 */

#pragma once

#include <tvgbox2/box.h>
#include <tvgbox2/event.h>
#include <tvgbox2/style_engine.h>
#include <tvgbox2/layout_engine.h>
#include <tvgbox2/renderer.h>
#include <thorvg.h>
#include <memory>
#include <string>

namespace editor {

class TvgUiLayer {
public:
    explicit TvgUiLayer(tvg::Canvas* canvas) {
        box_ = std::make_unique<tvgbox2::Box>(canvas);
    }

    void init(int width, int height) {
        resize(width, height);
        
        // Create root element
        auto* root = box_->create("div", "root");
        box_->set_root(root);
        
        // Basic layout for root
        root->computed_style->width = static_cast<float>(width);
        root->computed_style->height = static_cast<float>(height);
    }

    void resize(int width, int height) {
        box_->set_viewport(static_cast<float>(width), static_cast<float>(height));
        
        if (auto* root = box_->root()) {
            root->computed_style->width = static_cast<float>(width);
            root->computed_style->height = static_cast<float>(height);
            root->mark_layout_dirty();
        }
    }

    void update(float delta_ms) {
        box_->update_time(delta_ms);
        box_->update();
    }

    void render(tvg::Canvas* canvas) {
        (void)canvas; // unused here as box has it
    }

    // Direct helpers acting as platform abstraction
    bool onMouseDown(float x, float y, int button) {
        tvgbox2::Event te = tvgbox2::Event::mouse_down(x, y, static_cast<tvgbox2::MouseButton>(button));
        box_->dispatch_event(te);
        
        // Hack: Check if mouse is in PropertyInspector area (right 250px)
        // Ideally Box should expose hit_test or hovered element
        float w = box_->viewport_width();
        if (x > w - 250) return true;
        return false;
    }

    bool onMouseUp(float x, float y, int button) {
         tvgbox2::Event te = tvgbox2::Event::mouse_up(x, y, static_cast<tvgbox2::MouseButton>(button));
         box_->dispatch_event(te);
         
         float w = box_->viewport_width();
         if (x > w - 250) return true;
         return false;
    }

    bool onMouseMove(float x, float y) {
        tvgbox2::Event te = tvgbox2::Event::mouse_move(x, y);
        box_->dispatch_event(te);
        
        // Hack: Check if mouse is in PropertyInspector area (right 250px)
        float w = box_->viewport_width();
        if (x > w - 250) return true;
        
        return false;
    }
    
    tvgbox2::Box* box() { return box_.get(); }
    tvgbox2::Element* root() { return box_->root(); }

private:
    std::unique_ptr<tvgbox2::Box> box_;
};

} // namespace editor
