/*
 * Meta Editor - History Panel
 *
 * Visual undo/redo history list.
 */

#pragma once

#include "panel.h"
#include <functional>

namespace meta_editor {

class CommandManager;

class HistoryPanel : public Panel {
public:
    explicit HistoryPanel(CommandManager* commands);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float px, float py) override;

protected:
    float content_height() const override;

private:
    CommandManager* commands_;
    int hovered_index_ = -1;
    float scroll_offset_ = 0;
    
    static constexpr float ITEM_HEIGHT = 24.0f;
    static constexpr float HEADER_HEIGHT = 40.0f;
};

} // namespace meta_editor
