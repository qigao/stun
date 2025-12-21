/*
 * History Panel Implementation
 */

#include <editor/view/history_panel.h>
#include <editor/viewmodel/editor_vm.h>
#include <algorithm>
#include <cmath>

namespace editor {

HistoryPanel::HistoryPanel() {}

bool HistoryPanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + style_.width &&
           my >= y_ && my < y_ + style_.title_height;
}

void HistoryPanel::render(flex::Renderer& renderer) {
    if (!vm_) return;

    auto& history = vm_->history();
    auto& undoStack = history.undoStack();
    auto& redoStack = history.redoStack();

    size_t totalItems = undoStack.size() + redoStack.size() + 1;  // +1 for "Initial State"
    float contentHeight = style_.title_height + totalItems * style_.row_height + style_.padding;
    float maxHeight = 400;  // Maximum panel height
    calculated_height_ = std::min(contentHeight, maxHeight);
    max_scroll_ = std::max(0.0f, contentHeight - calculated_height_);

    // Background with rounded corners
    std::string bg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(calculated_height_ - 8) +
        " a 4 4 0 0 1 -4 4" +
        " h " + std::to_string(-(style_.width - 8)) +
        " a 4 4 0 0 1 -4 -4" +
        " v " + std::to_string(-(calculated_height_ - 8)) +
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

    // Drag grip (three horizontal lines)
    if (draggable_) {
        float gripX = x_ + 8;
        float gripY = y_ + style_.title_height / 2 - 4;
        for (int i = 0; i < 3; ++i) {
            std::string line = "M " + std::to_string(gripX) + " " + std::to_string(gripY + i * 3) +
                " h 8";
            renderer.stroke_path(line, flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.5f}), 1.0f);
        }
    }

    renderer.draw_text("History", x_ + 22, y_ + 18, "Arial", 11, true,
        {style_.title_text.r, style_.title_text.g, style_.title_text.b, 1});

    // Undo/Redo buttons in title
    float btnX = x_ + style_.width - 50;
    bool canUndo = history.canUndo();
    bool canRedo = history.canRedo();

    // Undo button
    renderer.draw_text("<", btnX, y_ + 18, "Arial", 14, true,
        {canUndo ? 0.9f : 0.4f, canUndo ? 0.9f : 0.4f, canUndo ? 0.9f : 0.4f, 1});

    // Redo button
    renderer.draw_text(">", btnX + 20, y_ + 18, "Arial", 14, true,
        {canRedo ? 0.9f : 0.4f, canRedo ? 0.9f : 0.4f, canRedo ? 0.9f : 0.4f, 1});

    // Content area with clipping (simulated by drawing within bounds)
    float contentY = y_ + style_.title_height - scroll_offset_;
    int itemIndex = 0;

    // Redo stack (future states - shown at top, dimmed)
    for (int i = static_cast<int>(redoStack.size()) - 1; i >= 0; --i) {
        float rowY = contentY + itemIndex * style_.row_height;
        if (rowY >= y_ + style_.title_height - style_.row_height &&
            rowY < y_ + calculated_height_) {
            bool isHovered = (hovered_item_ == -(i + 1));
            renderHistoryItem(renderer, redoStack[i]->description(), rowY,
                             false, false, isHovered);
        }
        itemIndex++;
    }

    // Current state marker
    float currentY = contentY + itemIndex * style_.row_height;
    if (currentY >= y_ + style_.title_height - style_.row_height &&
        currentY < y_ + calculated_height_) {
        renderHistoryItem(renderer, undoStack.empty() ? "Initial State" : undoStack.back()->description(),
                         currentY, true, true, hovered_item_ == 0);
    }
    itemIndex++;

    // Undo stack (past states)
    for (int i = static_cast<int>(undoStack.size()) - 2; i >= 0; --i) {
        float rowY = contentY + itemIndex * style_.row_height;
        if (rowY >= y_ + style_.title_height - style_.row_height &&
            rowY < y_ + calculated_height_) {
            int undoIndex = static_cast<int>(undoStack.size()) - 1 - i;
            bool isHovered = (hovered_item_ == undoIndex);
            renderHistoryItem(renderer, undoStack[i]->description(), rowY,
                             false, true, isHovered);
        }
        itemIndex++;
    }

    // Initial state at bottom
    if (undoStack.size() > 0) {
        float rowY = contentY + itemIndex * style_.row_height;
        if (rowY >= y_ + style_.title_height - style_.row_height &&
            rowY < y_ + calculated_height_) {
            int undoIndex = static_cast<int>(undoStack.size());
            bool isHovered = (hovered_item_ == undoIndex);
            renderHistoryItem(renderer, "Initial State", rowY, false, true, isHovered);
        }
    }

    // Scrollbar if needed
    if (max_scroll_ > 0) {
        float scrollbarHeight = calculated_height_ - style_.title_height;
        float thumbHeight = scrollbarHeight * (calculated_height_ / contentHeight);
        float thumbY = y_ + style_.title_height +
            (scroll_offset_ / max_scroll_) * (scrollbarHeight - thumbHeight);

        std::string scrollTrack = "M " + std::to_string(x_ + style_.width - 6) +
            " " + std::to_string(y_ + style_.title_height) +
            " h 4 v " + std::to_string(scrollbarHeight) + " h -4 Z";
        renderer.fill_path(scrollTrack, flex::Paint::solid({0.15f, 0.15f, 0.15f, 1}));

        std::string scrollThumb = "M " + std::to_string(x_ + style_.width - 6) +
            " " + std::to_string(thumbY) +
            " h 4 v " + std::to_string(thumbHeight) + " h -4 Z";
        renderer.fill_path(scrollThumb, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}));
    }
}

void HistoryPanel::renderHistoryItem(flex::Renderer& renderer, const std::string& desc,
                                     float y, bool isCurrent, bool isUndo, bool isHovered) {
    float rowX = x_;
    float rowW = style_.width;
    float rowH = style_.row_height;

    // Background
    Color bgColor;
    if (isCurrent) {
        bgColor = style_.row_current;
    } else if (isHovered) {
        bgColor = style_.row_hover;
    } else if (isUndo) {
        bgColor = style_.row_undo;
    } else {
        bgColor = style_.row_redo;
    }

    std::string rowBg = "M " + std::to_string(rowX) + " " + std::to_string(y) +
        " h " + std::to_string(rowW) + " v " + std::to_string(rowH) +
        " h " + std::to_string(-rowW) + " Z";
    renderer.fill_path(rowBg, flex::Paint::solid({bgColor.r, bgColor.g, bgColor.b, bgColor.a}));

    // Icon (circle for current, dot for others)
    float iconX = rowX + style_.padding + 6;
    float iconY = y + rowH / 2;

    if (isCurrent) {
        std::string circle = "M " + std::to_string(iconX - 4) + " " + std::to_string(iconY) +
            " a 4 4 0 1 1 8 0 a 4 4 0 1 1 -8 0";
        renderer.fill_path(circle, flex::Paint::solid({1.0f, 1.0f, 1.0f, 1}));
    } else {
        std::string dot = "M " + std::to_string(iconX - 2) + " " + std::to_string(iconY) +
            " a 2 2 0 1 1 4 0 a 2 2 0 1 1 -4 0";
        Color dotColor = isUndo ? style_.text_normal : style_.text_dimmed;
        renderer.fill_path(dot, flex::Paint::solid({dotColor.r, dotColor.g, dotColor.b, 1}));
    }

    // Text
    Color textColor = isCurrent ? Color{1, 1, 1, 1} :
                      (isUndo ? style_.text_normal : style_.text_dimmed);
    renderer.draw_text(desc, rowX + style_.padding + 18, y + rowH - 7, "Arial", 11,
        isCurrent, {textColor.r, textColor.g, textColor.b, 1});
}

int HistoryPanel::itemAt(float mx, float my) const {
    if (!vm_) return -999;
    if (mx < x_ || mx > x_ + style_.width) return -999;
    if (my < y_ + style_.title_height || my > y_ + calculated_height_) return -999;

    auto& history = vm_->history();
    auto& undoStack = history.undoStack();
    auto& redoStack = history.redoStack();

    float contentY = y_ + style_.title_height - scroll_offset_;
    float relY = my - contentY;
    int row = static_cast<int>(relY / style_.row_height);

    int redoCount = static_cast<int>(redoStack.size());

    if (row < redoCount) {
        return -(redoCount - row);
    } else if (row == redoCount) {
        return 0;
    } else {
        return row - redoCount;
    }
}

bool HistoryPanel::onMouseDown(float mx, float my, int button) {
    if (button != 0) return false;

    // Check for drag start on title bar
    if (draggable_ && isInTitleBar(mx, my)) {
        // Check if not clicking on undo/redo buttons
        float btnX = x_ + style_.width - 50;
        if (mx < btnX) {
            dragging_ = true;
            drag_offset_x_ = mx - x_;
            drag_offset_y_ = my - y_;
            return true;
        }
    }

    if (!vm_) return false;

    // Check title bar buttons
    if (my >= y_ && my < y_ + style_.title_height) {
        float btnX = x_ + style_.width - 50;
        if (mx >= btnX && mx < btnX + 18) {
            vm_->undo();
            return true;
        }
        if (mx >= btnX + 20 && mx < btnX + 38) {
            vm_->redo();
            return true;
        }
        return mx >= x_ && mx < x_ + style_.width;
    }

    int item = itemAt(mx, my);
    if (item == -999) return false;

    auto& history = vm_->history();

    if (item < 0) {
        int redoIndex = -item - 1;
        history.redoTo(redoIndex);
    } else if (item > 0) {
        size_t targetSize = history.undoStack().size() - item;
        history.undoTo(targetSize);
    }

    return true;
}

bool HistoryPanel::onMouseMove(float mx, float my) {
    // Handle dragging
    if (dragging_) {
        x_ = mx - drag_offset_x_;
        y_ = my - drag_offset_y_;
        return true;
    }

    hovered_item_ = itemAt(mx, my);
    return hovered_item_ != -999 || isInTitleBar(mx, my);
}

bool HistoryPanel::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    if (dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

bool HistoryPanel::onScroll(float mx, float my, float delta) {
    if (mx < x_ || mx > x_ + style_.width ||
        my < y_ || my > y_ + calculated_height_) {
        return false;
    }

    scroll_offset_ = std::max(0.0f, std::min(max_scroll_, scroll_offset_ - delta * 20));
    return true;
}

} // namespace editor
