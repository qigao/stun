/*
 * flexUI Designer - History Panel Implementation
 */

#include "flexui_designer/history_panel.h"
#include "flexui_designer/designer.h"
#include <algorithm>

namespace flexui_designer {

HistoryPanel::HistoryPanel() {
    set_size(200, 300);
    set_draggable(false);
}

void HistoryPanel::set_stacks(
    const std::vector<HistoryEntry>* undo,
    const std::vector<HistoryEntry>* redo
) {
    undo_stack_ = undo;
    redo_stack_ = redo;
}

void HistoryPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    render_background(renderer);

    // Header
    flex::Color header_text{0.9f, 0.9f, 0.9f, 1.0f};
    renderer.draw_text("History", x_ + 10, y_ + 20, "sans", 13, true, header_text);

    if (!undo_stack_ && !redo_stack_) return;

    // Calculate total entries (undo + current + redo)
    size_t undo_count = undo_stack_ ? undo_stack_->size() : 0;
    size_t redo_count = redo_stack_ ? redo_stack_->size() : 0;
    size_t total = undo_count + redo_count;

    if (total == 0) {
        flex::Color dim{0.5f, 0.5f, 0.5f, 1.0f};
        renderer.draw_text("No history", x_ + 10, y_ + HEADER_HEIGHT + 20, "sans", 11, false, dim);
        return;
    }

    // Limit display to MAX_DISPLAY entries
    size_t display_count = std::min(total, (size_t)MAX_DISPLAY);

    // Clip region for scrolling
    float list_y = y_ + HEADER_HEIGHT;
    float list_h = height_ - HEADER_HEIGHT;

    // Render entries
    float entry_y = list_y - scroll_offset_;

    // Undo entries (past states) - oldest first
    for (size_t i = 0; i < undo_count && i < MAX_DISPLAY; ++i) {
        if (entry_y + ENTRY_HEIGHT > list_y && entry_y < list_y + list_h) {
            bool is_current = (i == undo_count - 1) && redo_count == 0;
            render_entry(renderer, entry_y, (*undo_stack_)[i].description, is_current, false);
        }
        entry_y += ENTRY_HEIGHT;
    }

    // Current state marker (between undo and redo)
    if (redo_count > 0 && entry_y + ENTRY_HEIGHT > list_y && entry_y < list_y + list_h) {
        render_entry(renderer, entry_y, "(Current)", true, false);
        entry_y += ENTRY_HEIGHT;
    }

    // Redo entries (future states) - shown grayed out
    for (size_t i = 0; i < redo_count && (undo_count + i) < MAX_DISPLAY; ++i) {
        size_t redo_idx = redo_count - 1 - i;  // Reverse order
        if (entry_y + ENTRY_HEIGHT > list_y && entry_y < list_y + list_h) {
            render_entry(renderer, entry_y, (*redo_stack_)[redo_idx].description, false, true);
        }
        entry_y += ENTRY_HEIGHT;
    }
}

void HistoryPanel::render_entry(flex::Renderer& renderer, float y, 
                                 const std::string& text, bool is_current, bool is_redo) {
    float entry_x = x_ + 5;
    float entry_w = width_ - 10;

    // Background for current entry
    if (is_current) {
        flex::Paint highlight = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 0.3f});
        renderer.draw_rect(entry_x, y, entry_w, ENTRY_HEIGHT - 2, 3, highlight, flex::Paint::none(), 0);
    }

    // Text color
    flex::Color text_color;
    if (is_current) {
        text_color = {0.4f, 0.7f, 1.0f, 1.0f};  // Bright blue for current
    } else if (is_redo) {
        text_color = {0.5f, 0.5f, 0.5f, 0.7f};  // Dimmed for redo (future)
    } else {
        text_color = {0.8f, 0.8f, 0.8f, 1.0f};  // Normal for undo (past)
    }

    // Current position indicator
    if (is_current) {
        renderer.draw_text(">", entry_x + 4, y + 16, "sans", 11, true, text_color);
    }

    // Entry text
    float text_x = entry_x + (is_current ? 16 : 8);
    std::string display_text = text.empty() ? "State" : text;
    if (display_text.length() > 25) {
        display_text = display_text.substr(0, 22) + "...";
    }
    renderer.draw_text(display_text, text_x, y + 16, "sans", 11, false, text_color);
}

bool HistoryPanel::handle_click(float screen_x, float screen_y) {
    if (!visible_ || !contains(screen_x, screen_y)) return false;

    float local_y = screen_y - y_ - HEADER_HEIGHT + scroll_offset_;
    if (local_y < 0) return false;

    int clicked_index = static_cast<int>(local_y / ENTRY_HEIGHT);

    size_t undo_count = undo_stack_ ? undo_stack_->size() : 0;
    size_t redo_count = redo_stack_ ? redo_stack_->size() : 0;

    if (clicked_index < 0) return false;

    // Determine which entry was clicked
    if (clicked_index < (int)undo_count) {
        // Clicked on undo entry - jump to that state
        if (on_jump_) {
            on_jump_(clicked_index);
        }
        return true;
    }

    // Clicked on current marker or redo entry
    int redo_offset = clicked_index - (int)undo_count;
    if (redo_count > 0 && redo_offset == 0) {
        // Clicked on "(Current)" - no action
        return true;
    }

    if (redo_offset > 0 && redo_offset <= (int)redo_count) {
        // Clicked on redo entry - jump forward
        int target = (int)undo_count + redo_offset;
        if (on_jump_) {
            on_jump_(target);
        }
        return true;
    }

    return false;
}

} // namespace flexui_designer
