/*
 * Layer Panel Implementation
 */

#include <editor/view/layer_panel.h>
#include <sstream>
#include <algorithm>

namespace editor {

bool LayerPanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + width_ &&
           my >= y_ && my < y_ + TITLE_HEIGHT;
}

void LayerPanel::render(flex::Renderer& renderer) {
    if (!document_) return;

    auto& layers = document_->layers();
    float contentHeight = TITLE_HEIGHT + layers.size() * ROW_HEIGHT + PADDING;
    height_ = contentHeight;

    // Background with rounded corners
    std::ostringstream bg;
    bg << "M " << (x_ + 4) << " " << y_
       << " h " << (width_ - 8)
       << " a 4 4 0 0 1 4 4"
       << " v " << (height_ - 8)
       << " a 4 4 0 0 1 -4 4"
       << " h " << -(width_ - 8)
       << " a 4 4 0 0 1 -4 -4"
       << " v " << -(height_ - 8)
       << " a 4 4 0 0 1 4 -4";
    renderer.fill_path(bg.str(), flex::Paint::solid({0.18f, 0.18f, 0.18f, 0.95f}));

    // Title bar
    std::ostringstream titleBg;
    titleBg << "M " << (x_ + 4) << " " << y_
            << " h " << (width_ - 8)
            << " a 4 4 0 0 1 4 4"
            << " v " << (TITLE_HEIGHT - 4)
            << " h " << -width_
            << " v " << -(TITLE_HEIGHT - 4)
            << " a 4 4 0 0 1 4 -4";
    renderer.fill_path(titleBg.str(), flex::Paint::solid({0.22f, 0.22f, 0.22f, 1.0f}));

    // Drag grip
    if (draggable_) {
        float gripX = x_ + 8;
        float gripY = y_ + TITLE_HEIGHT / 2 - 4;
        for (int i = 0; i < 3; ++i) {
            std::ostringstream line;
            line << "M " << gripX << " " << (gripY + i * 3) << " h 8";
            renderer.stroke_path(line.str(), flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.5f}), 1.0f);
        }
    }

    // Title
    renderer.draw_text("Layers", x_ + 22, y_ + 22, "Arial", 11, true,
                       {0.9f, 0.9f, 0.9f, 1.0f});

    // Add button [+]
    float btnX = x_ + width_ - BUTTON_SIZE * 2 - PADDING - 4;
    drawButton(renderer, btnX, y_ + 8, "+", hovered_button_ == 0);

    // Delete button [-]
    btnX = x_ + width_ - BUTTON_SIZE - PADDING;
    drawButton(renderer, btnX, y_ + 8, "-", hovered_button_ == 1);

    // Separator line
    std::ostringstream sep;
    sep << "M " << x_ << " " << (y_ + TITLE_HEIGHT)
        << " h " << width_;
    renderer.stroke_path(sep.str(), flex::Paint::solid({0.3f, 0.3f, 0.3f, 1.0f}), 1.0f);

    // Layer rows (top = last layer, bottom = first layer for visual stacking)
    float rowY = y_ + TITLE_HEIGHT;
    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        auto& layer = layers[i];
        bool isActive = (layer == document_->activeLayer());
        int rowIndex = static_cast<int>(layers.size()) - 1 - i;

        // Skip drawing the dragged row in its original position
        bool isDragSource = layer_dragging_ && (rowIndex == drag_source_row_);

        // Row background
        if (!isDragSource) {
            if (isActive) {
                std::ostringstream rowBg;
                rowBg << "M " << x_ << " " << rowY
                      << " h " << width_
                      << " v " << ROW_HEIGHT
                      << " h " << -width_ << " Z";
                renderer.fill_path(rowBg.str(), flex::Paint::solid({0.25f, 0.35f, 0.5f, 1.0f}));
            } else if (hovered_row_ == rowIndex && !layer_dragging_) {
                std::ostringstream rowBg;
                rowBg << "M " << x_ << " " << rowY
                      << " h " << width_
                      << " v " << ROW_HEIGHT
                      << " h " << -width_ << " Z";
                renderer.fill_path(rowBg.str(), flex::Paint::solid({0.25f, 0.25f, 0.25f, 1.0f}));
            }

            // Eye icon (visibility)
            drawEyeIcon(renderer, x_ + PADDING, rowY + 6, layer->visible());

            // Lock icon
            drawLockIcon(renderer, x_ + PADDING + 24, rowY + 6, layer->locked());

            // Layer name
            float textAlpha = layer->visible() ? 1.0f : 0.5f;
            renderer.draw_text(layer->name(), x_ + 56, rowY + 18, "Arial", 11, false,
                               {0.9f, 0.9f, 0.9f, textAlpha});

            // Up arrow (if not first visible row)
            if (i < static_cast<int>(layers.size()) - 1) {
                drawArrow(renderer, x_ + width_ - 36, rowY + 6, true);
            }

            // Down arrow (if not last row)
            if (i > 0) {
                drawArrow(renderer, x_ + width_ - 20, rowY + 6, false);
            }
        }

        rowY += ROW_HEIGHT;
    }

    // Draw drop indicator during drag
    if (layer_dragging_ && drop_target_row_ >= 0) {
        float indicatorY = y_ + TITLE_HEIGHT + drop_target_row_ * ROW_HEIGHT;
        drawDropIndicator(renderer, x_ + 4, indicatorY - 2, width_ - 8);
    }

    // Draw dragged layer ghost
    if (layer_dragging_ && drag_source_layer_ >= 0 && drag_source_layer_ < static_cast<int>(layers.size())) {
        auto& layer = layers[drag_source_layer_];

        // Ghost background (semi-transparent)
        float ghostY = drag_current_y_ - ROW_HEIGHT / 2;
        std::ostringstream ghostBg;
        ghostBg << "M " << (x_ + 4) << " " << ghostY
                << " h " << (width_ - 8)
                << " v " << ROW_HEIGHT
                << " h " << -(width_ - 8) << " Z";
        renderer.fill_path(ghostBg.str(), flex::Paint::solid({0.3f, 0.5f, 0.8f, 0.9f}));
        renderer.stroke_path(ghostBg.str(), flex::Paint::solid({0.4f, 0.6f, 0.9f, 1.0f}), 2.0f);

        // Ghost layer name
        renderer.draw_text(layer->name(), x_ + 56, ghostY + 18, "Arial", 11, true,
                           {1.0f, 1.0f, 1.0f, 1.0f});
    }
}

void LayerPanel::drawEyeIcon(flex::Renderer& renderer, float x, float y, bool visible) {
    float cx = x + 8;
    float cy = y + 8;

    if (visible) {
        // Eye shape
        std::ostringstream eye;
        eye << "M " << (cx - 7) << " " << cy
            << " Q " << cx << " " << (cy - 5) << " " << (cx + 7) << " " << cy
            << " Q " << cx << " " << (cy + 5) << " " << (cx - 7) << " " << cy;
        renderer.stroke_path(eye.str(), flex::Paint::solid({0.3f, 0.3f, 0.3f, 1.0f}), 1.5f);

        // Pupil
        std::ostringstream pupil;
        pupil << "M " << cx << " " << (cy - 2.5f)
              << " a 2.5 2.5 0 1 1 0 5 a 2.5 2.5 0 1 1 0 -5";
        renderer.fill_path(pupil.str(), flex::Paint::solid({0.3f, 0.3f, 0.3f, 1.0f}));
    } else {
        // Closed eye (line)
        std::ostringstream eye;
        eye << "M " << (cx - 7) << " " << cy
            << " Q " << cx << " " << (cy + 4) << " " << (cx + 7) << " " << cy;
        renderer.stroke_path(eye.str(), flex::Paint::solid({0.6f, 0.6f, 0.6f, 1.0f}), 1.5f);

        // Strike through
        std::ostringstream strike;
        strike << "M " << (cx - 5) << " " << (cy - 5)
               << " L " << (cx + 5) << " " << (cy + 5);
        renderer.stroke_path(strike.str(), flex::Paint::solid({0.6f, 0.6f, 0.6f, 1.0f}), 1.5f);
    }
}

void LayerPanel::drawLockIcon(flex::Renderer& renderer, float x, float y, bool locked) {
    float bx = x + 3;
    float by = y + 8;

    // Lock body
    std::ostringstream body;
    body << "M " << bx << " " << by
         << " h 10 v 8 h -10 Z";

    if (locked) {
        renderer.fill_path(body.str(), flex::Paint::solid({0.4f, 0.4f, 0.4f, 1.0f}));

        // Closed shackle
        std::ostringstream shackle;
        shackle << "M " << (bx + 2) << " " << by
                << " v -3 a 3 3 0 0 1 6 0 v 3";
        renderer.stroke_path(shackle.str(), flex::Paint::solid({0.4f, 0.4f, 0.4f, 1.0f}), 1.5f);
    } else {
        renderer.stroke_path(body.str(), flex::Paint::solid({0.6f, 0.6f, 0.6f, 1.0f}), 1.0f);

        // Open shackle
        std::ostringstream shackle;
        shackle << "M " << (bx + 2) << " " << by
                << " v -3 a 3 3 0 0 1 6 0 v 1";
        renderer.stroke_path(shackle.str(), flex::Paint::solid({0.6f, 0.6f, 0.6f, 1.0f}), 1.5f);
    }
}

void LayerPanel::drawButton(flex::Renderer& renderer, float x, float y, const char* label, bool hovered) {
    std::ostringstream btn;
    btn << "M " << x << " " << y
        << " h " << BUTTON_SIZE
        << " v " << BUTTON_SIZE
        << " h " << -BUTTON_SIZE << " Z";

    if (hovered) {
        renderer.fill_path(btn.str(), flex::Paint::solid({0.35f, 0.35f, 0.35f, 1.0f}));
    } else {
        renderer.fill_path(btn.str(), flex::Paint::solid({0.25f, 0.25f, 0.25f, 1.0f}));
    }

    renderer.draw_text(label, x + 6, y + 15, "Arial", 14, true,
                       {0.9f, 0.9f, 0.9f, 1.0f});
}

void LayerPanel::drawArrow(flex::Renderer& renderer, float x, float y, bool up) {
    float cx = x + 6;
    float cy = y + 8;

    std::ostringstream arrow;
    if (up) {
        arrow << "M " << (cx - 4) << " " << (cy + 3)
              << " L " << cx << " " << (cy - 3)
              << " L " << (cx + 4) << " " << (cy + 3);
    } else {
        arrow << "M " << (cx - 4) << " " << (cy - 3)
              << " L " << cx << " " << (cy + 3)
              << " L " << (cx + 4) << " " << (cy - 3);
    }
    renderer.stroke_path(arrow.str(), flex::Paint::solid({0.5f, 0.5f, 0.5f, 1.0f}), 1.5f);
}

void LayerPanel::drawDropIndicator(flex::Renderer& renderer, float x, float y, float w) {
    // Horizontal line
    std::ostringstream line;
    line << "M " << x << " " << y << " h " << w;
    renderer.stroke_path(line.str(), flex::Paint::solid({0.2f, 0.5f, 0.9f, 1.0f}), 2.0f);

    // Left circle
    std::ostringstream leftCircle;
    leftCircle << "M " << (x - 3) << " " << y
               << " a 3 3 0 1 1 6 0 a 3 3 0 1 1 -6 0";
    renderer.fill_path(leftCircle.str(), flex::Paint::solid({0.2f, 0.5f, 0.9f, 1.0f}));

    // Right circle
    std::ostringstream rightCircle;
    rightCircle << "M " << (x + w - 3) << " " << y
                << " a 3 3 0 1 1 6 0 a 3 3 0 1 1 -6 0";
    renderer.fill_path(rightCircle.str(), flex::Paint::solid({0.2f, 0.5f, 0.9f, 1.0f}));
}

bool LayerPanel::onMouseDown(float mx, float my, int button) {
    if (!document_ || button != 0) return false;

    float localX = mx - x_;
    float localY = my - y_;

    // Check bounds
    if (localX < 0 || localX > width_ || localY < 0 || localY > height_) {
        return false;
    }

    auto& layers = document_->layers();

    // Check for panel drag start on title bar (excluding buttons)
    if (draggable_ && localY < TITLE_HEIGHT) {
        float btnX = width_ - BUTTON_SIZE * 2 - PADDING - 4;
        if (localX < btnX) {
            panel_dragging_ = true;
            panel_drag_offset_x_ = mx - x_;
            panel_drag_offset_y_ = my - y_;
            return true;
        }
    }

    // Check title bar buttons
    if (localY < TITLE_HEIGHT) {
        float btnX = width_ - BUTTON_SIZE * 2 - PADDING - 4;
        if (localX >= btnX && localX < btnX + BUTTON_SIZE) {
            addNewLayer();
            return true;
        }
        btnX = width_ - BUTTON_SIZE - PADDING;
        if (localX >= btnX && localX < btnX + BUTTON_SIZE) {
            deleteActiveLayer();
            return true;
        }
        return true;
    }

    // Determine which layer row was clicked
    int rowIndex = static_cast<int>((localY - TITLE_HEIGHT) / ROW_HEIGHT);
    int layerIndex = static_cast<int>(layers.size()) - 1 - rowIndex;

    if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size())) {
        return false;
    }

    auto layer = layers[layerIndex];

    // Check which element was clicked
    if (localX < 24) {
        // Eye icon - toggle visibility
        layer->setVisible(!layer->visible());
        return true;
    } else if (localX < 48) {
        // Lock icon - toggle lock
        layer->setLocked(!layer->locked());
        return true;
    } else if (localX > width_ - 40) {
        // Arrow buttons area
        if (localX < width_ - 20) {
            // Up arrow
            if (layerIndex < static_cast<int>(layers.size()) - 1) {
                moveLayerUp(layerIndex);
            }
        } else {
            // Down arrow
            if (layerIndex > 0) {
                moveLayerDown(layerIndex);
            }
        }
        return true;
    } else {
        // Layer name area - start drag or select
        document_->setActiveLayer(layer);

        // Initiate layer drag
        layer_dragging_ = true;
        drag_source_row_ = rowIndex;
        drag_source_layer_ = layerIndex;
        drag_start_y_ = my;
        drag_current_y_ = my;
        drop_target_row_ = -1;

        return true;
    }
}

bool LayerPanel::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;

    // Handle panel drag end
    if (panel_dragging_) {
        panel_dragging_ = false;
        return true;
    }

    // Handle layer drag end
    if (layer_dragging_) {
        // Complete the drop if we have a valid target
        if (drop_target_row_ >= 0 && drop_target_row_ != drag_source_row_) {
            auto& layers = document_->layers();
            int numLayers = static_cast<int>(layers.size());

            // Convert visual row indices to layer indices
            // Visual: row 0 = top = highest layer index
            // Layer: index 0 = bottom, index N-1 = top
            int targetLayerIndex = numLayers - 1 - drop_target_row_;

            // Clamp target to valid range
            targetLayerIndex = std::max(0, std::min(targetLayerIndex, numLayers - 1));

            if (drag_source_layer_ != targetLayerIndex) {
                auto layer = layers[drag_source_layer_];
                document_->moveLayer(layer, targetLayerIndex);
            }
        }

        // Reset layer drag state
        layer_dragging_ = false;
        drag_source_row_ = -1;
        drag_source_layer_ = -1;
        drop_target_row_ = -1;
    }

    return false;
}

bool LayerPanel::onMouseMove(float mx, float my) {
    if (!document_) return false;

    // Handle panel dragging
    if (panel_dragging_) {
        x_ = mx - panel_drag_offset_x_;
        y_ = my - panel_drag_offset_y_;
        return true;
    }

    float localX = mx - x_;
    float localY = my - y_;

    // Handle layer dragging
    if (layer_dragging_) {
        drag_current_y_ = my;

        // Calculate drop target row based on cursor position
        float relY = localY - TITLE_HEIGHT;
        int targetRow = static_cast<int>((relY + ROW_HEIGHT / 2) / ROW_HEIGHT);

        // Clamp to valid range
        int numLayers = static_cast<int>(document_->layers().size());
        targetRow = std::max(0, std::min(targetRow, numLayers));

        // Don't show indicator at same position
        if (targetRow == drag_source_row_ || targetRow == drag_source_row_ + 1) {
            drop_target_row_ = -1;
        } else {
            drop_target_row_ = targetRow;
        }

        return true;
    }

    // Reset hover state
    hovered_row_ = -1;
    hovered_button_ = -1;

    // Check bounds
    if (localX < 0 || localX > width_ || localY < 0 || localY > height_) {
        return false;
    }

    // Check title bar buttons
    if (localY < TITLE_HEIGHT) {
        float btnX = width_ - BUTTON_SIZE * 2 - PADDING - 4;
        if (localX >= btnX && localX < btnX + BUTTON_SIZE) {
            hovered_button_ = 0;  // Add button
            return true;
        }
        btnX = width_ - BUTTON_SIZE - PADDING;
        if (localX >= btnX && localX < btnX + BUTTON_SIZE) {
            hovered_button_ = 1;  // Delete button
            return true;
        }
        return true;  // In title bar
    }

    // Check layer rows
    int rowIndex = static_cast<int>((localY - TITLE_HEIGHT) / ROW_HEIGHT);
    if (rowIndex >= 0 && rowIndex < static_cast<int>(document_->layers().size())) {
        hovered_row_ = rowIndex;
        return true;
    }

    return false;
}

void LayerPanel::addNewLayer() {
    if (!document_) return;

    // Create a new layer with auto-generated name
    size_t count = document_->layers().size() + 1;
    std::string name = "Layer " + std::to_string(count);
    auto layer = Layer::create(name);
    document_->addLayer(layer);
    document_->setActiveLayer(layer);
}

void LayerPanel::deleteActiveLayer() {
    if (!document_) return;

    auto active = document_->activeLayer();
    if (active && document_->layers().size() > 1) {
        document_->removeLayer(active);
    }
}

void LayerPanel::moveLayerUp(size_t index) {
    if (!document_) return;

    auto& layers = document_->layers();
    if (index < layers.size() - 1) {
        auto layer = layers[index];
        document_->moveLayer(layer, index + 1);
    }
}

void LayerPanel::moveLayerDown(size_t index) {
    if (!document_) return;

    auto& layers = document_->layers();
    if (index > 0) {
        auto layer = layers[index];
        document_->moveLayer(layer, index - 1);
    }
}

} // namespace editor
