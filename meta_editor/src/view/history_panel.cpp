/*
 * Meta Editor - History Panel Implementation
 */

#include "meta_editor/view/history_panel.h"
#include "meta_editor/command.h"

namespace meta_editor {

HistoryPanel::HistoryPanel(CommandManager* commands) : commands_(commands) {
    set_size(200, 300);
}

float HistoryPanel::content_height() const {
    if (!commands_) return HEADER_HEIGHT;
    int total = commands_->undo_count() + commands_->redo_count() + 1;  // +1 for current state
    return HEADER_HEIGHT + total * ITEM_HEIGHT + 8;
}

void HistoryPanel::render(flex::Renderer& renderer) {
    if (!visible_ || !commands_) return;

    render_background(renderer);

    // Title
    renderer.draw_text("History", x_ + 12, y_ + 24, "Arial", 14.0f, true,
                       flex::Color{1.0f, 1.0f, 1.0f, 1.0f});

    float item_y = y_ + HEADER_HEIGHT;
    
    const auto& undo_stack = commands_->undo_stack();
    const auto& redo_stack = commands_->redo_stack();

    // Redo items (future - grayed out, in reverse order)
    for (int i = (int)redo_stack.size() - 1; i >= 0; --i) {
        flex::Color text_color{0.5f, 0.5f, 0.5f, 0.7f};
        renderer.draw_text(redo_stack[i]->name(), x_ + 12, item_y + 16,
                          "Arial", 11.0f, false, text_color);
        item_y += ITEM_HEIGHT;
    }

    // Current state marker
    flex::Paint current_bg = flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.8f, 0.6f});
    renderer.draw_rect(x_ + 4, item_y, width_ - 8, ITEM_HEIGHT, 4.0f,
                      current_bg, flex::Paint::none(), 0);
    renderer.draw_text("Current", x_ + 12, item_y + 16, "Arial", 11.0f, true,
                      flex::Color{1.0f, 1.0f, 1.0f, 1.0f});
    item_y += ITEM_HEIGHT;

    // Undo items (past - in reverse order, most recent first)
    for (int i = (int)undo_stack.size() - 1; i >= 0; --i) {
        bool hovered = (hovered_index_ == i);
        
        if (hovered) {
            flex::Paint hover_bg = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 0.8f});
            renderer.draw_rect(x_ + 4, item_y, width_ - 8, ITEM_HEIGHT, 4.0f,
                              hover_bg, flex::Paint::none(), 0);
        }

        flex::Color text_color{0.8f, 0.8f, 0.8f, 1.0f};
        renderer.draw_text(undo_stack[i]->name(), x_ + 12, item_y + 16,
                          "Arial", 11.0f, false, text_color);
        item_y += ITEM_HEIGHT;
    }
}

bool HistoryPanel::handle_click(float px, float py) {
    if (!commands_) return false;

    float item_y = y_ + HEADER_HEIGHT;
    const auto& redo_stack = commands_->redo_stack();
    const auto& undo_stack = commands_->undo_stack();

    // Click on redo items = redo to that point
    for (int i = (int)redo_stack.size() - 1; i >= 0; --i) {
        if (py >= item_y && py < item_y + ITEM_HEIGHT) {
            int redo_count = (int)redo_stack.size() - i;
            for (int j = 0; j < redo_count; ++j) {
                commands_->redo();
            }
            return true;
        }
        item_y += ITEM_HEIGHT;
    }

    item_y += ITEM_HEIGHT;  // Skip current state

    // Click on undo items = undo to that point
    for (int i = (int)undo_stack.size() - 1; i >= 0; --i) {
        if (py >= item_y && py < item_y + ITEM_HEIGHT) {
            int undo_count = (int)undo_stack.size() - i;
            for (int j = 0; j < undo_count; ++j) {
                commands_->undo();
            }
            return true;
        }
        item_y += ITEM_HEIGHT;
    }

    return false;
}

} // namespace meta_editor
