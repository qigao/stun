/*
 * flexUI Designer - History Panel
 *
 * Visual undo/redo history with click-to-jump navigation.
 */

#pragma once

#include <meta_editor/view/panel.h>
#include <string>
#include <vector>
#include <functional>

namespace flexui_designer {

struct DesignWidget;
struct HistoryEntry;

class HistoryPanel : public meta_editor::Panel {
public:
    HistoryPanel();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;

    // Set data source - direct reference to Designer's stacks
    void set_stacks(
        const std::vector<HistoryEntry>* undo,
        const std::vector<HistoryEntry>* redo
    );

    // Jump callback - called when user clicks a history entry
    using JumpCallback = std::function<void(int index)>;
    void set_jump_callback(JumpCallback cb) { on_jump_ = std::move(cb); }

    // Current position in history (undo_stack_.size())
    void set_current_position(int pos) { current_position_ = pos; }

private:
    static constexpr int MAX_DISPLAY = 50;
    static constexpr float ENTRY_HEIGHT = 24.0f;
    static constexpr float HEADER_HEIGHT = 30.0f;

    void render_entry(flex::Renderer& renderer, float y, 
                      const std::string& text, bool is_current, bool is_redo);

    const std::vector<HistoryEntry>* undo_stack_ = nullptr;
    const std::vector<HistoryEntry>* redo_stack_ = nullptr;
    JumpCallback on_jump_;
    float scroll_offset_ = 0;
    int current_position_ = 0;
};

} // namespace flexui_designer
