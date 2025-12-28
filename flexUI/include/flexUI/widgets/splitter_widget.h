/*
 * flexUI - SplitterWidget
 *
 * A bar that can be dragged to resize adjacent elements.
 */

#ifndef FLEXUI_SPLITTER_WIDGET_H
#define FLEXUI_SPLITTER_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include <iostream>

namespace flexUI {

class SplitterWidget : public Widget {
public:
    enum class Orientation {
        Horizontal, // Vertical bar, resizes horizontally
        Vertical    // Horizontal bar, resizes vertically
    };

    enum class ResizeTarget {
        Previous,
        Next
    };

    explicit SplitterWidget(Orientation orientation = Orientation::Horizontal, 
                           ResizeTarget target = ResizeTarget::Previous)
        : orientation_(orientation), target_(target) {}

    void set_target(ResizeTarget target) { target_ = target; }

    void render(const Element& elem, Renderer& renderer) override {
        // Render simple bar
        Color col = {0.2f, 0.2f, 0.2f, 1.0f}; // Darker default
        if (hovered_) col = {0.0f, 0.48f, 0.8f, 1.0f}; // Active blue
        if (dragging_) col = {0.0f, 0.58f, 0.9f, 1.0f};

        renderer.draw_rect(0, 0, elem.width(), elem.height(), 0,
                          Paint::solid(col), Paint::none(), 0);
    }

    bool handle_event(const Event& event, Element& elem) override {
        if (event.type == EventType::MouseDown) {
            dragging_ = true;
            last_pos_ = (orientation_ == Orientation::Horizontal) ? event.x : event.y;
            return true;
        } else if (event.type == EventType::MouseMove) {
            if (dragging_) {
                float current_pos = (orientation_ == Orientation::Horizontal) ? event.x : event.y;
                float delta = current_pos - last_pos_;
                
                Element* sibling = (target_ == ResizeTarget::Previous) ? 
                                  find_previous_sibling(elem) : 
                                  find_next_sibling(elem);

                if (sibling) {
                    float current_size = (orientation_ == Orientation::Horizontal) ? 
                                        sibling->layout_width() : 
                                        sibling->layout_height();
                    
                    float new_size = (target_ == ResizeTarget::Previous) ? 
                                    current_size + delta : 
                                    current_size - delta;
                    
                    if (new_size < 100) new_size = 100; // Min size
                    if (new_size > 800) new_size = 800; // Max size

                    if (orientation_ == Orientation::Horizontal) {
                        sibling->computed_style->width = new_size;
                        sibling->computed_style->width_is_percent = false;
                    } else {
                        sibling->computed_style->height = new_size;
                        sibling->computed_style->height_is_percent = false;
                    }
                    sibling->mark_layout_dirty();
                }
                
                last_pos_ = current_pos;
                return true;
            }
            hovered_ = true;
            return false;
        } else if (event.type == EventType::MouseUp) {
            dragging_ = false;
            return true;
        } else if (event.type == EventType::MouseEnter) {
            hovered_ = true;
            return false;
        } else if (event.type == EventType::MouseLeave) {
            hovered_ = false;
            return false;
        }
        return false;
    }

    bool wants_mouse_capture() const override { return dragging_; }

    const char* type_name() const override { return "SplitterWidget"; }

private:
    Element* find_previous_sibling(Element& elem) {
        if (!elem.parent_elem()) return nullptr;
        auto& children = elem.parent_elem()->children();
        for (size_t i = 1; i < children.size(); ++i) {
            if (children[i] == &elem) {
                return static_cast<Element*>(children[i-1]);
            }
        }
        return nullptr;
    }

    Element* find_next_sibling(Element& elem) {
        if (!elem.parent_elem()) return nullptr;
        auto& children = elem.parent_elem()->children();
        for (size_t i = 0; i < children.size() - 1; ++i) {
            if (children[i] == &elem) {
                return static_cast<Element*>(children[i+1]);
            }
        }
        return nullptr;
    }

    Orientation orientation_;
    ResizeTarget target_;
    bool dragging_ = false;
    bool hovered_ = false;
    float last_pos_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_SPLITTER_WIDGET_H
