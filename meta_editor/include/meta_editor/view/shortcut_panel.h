/*
 * Meta Editor - Shortcut Panel
 *
 * Displays all keyboard shortcuts. Press '?' to toggle.
 */

#pragma once

#include "panel.h"
#include <vector>
#include <string>

namespace meta_editor {

struct ShortcutEntry {
    std::string key;
    std::string description;
    std::string category;
};

class ShortcutPanel : public Panel {
public:
    ShortcutPanel();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

protected:
    float content_height() const override { return 500; }

private:
    std::vector<ShortcutEntry> shortcuts_;
};

} // namespace meta_editor
