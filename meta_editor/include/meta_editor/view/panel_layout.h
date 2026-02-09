/*
 * Meta Editor - Panel Layout
 *
 * Automatic dock-based layout for panels.
 * Replaces manual set_position() calls in FullEditorApp.
 */

#pragma once

#include "panel.h"
#include <vector>

namespace meta_editor {

enum class DockSide { Left, Right, Bottom };

class PanelLayout {
public:
    void add_panel(Panel* panel, DockSide side);
    void set_viewport(float width, float height);
    void layout();

private:
    struct Entry {
        Panel* panel;
        DockSide side;
    };

    std::vector<Entry> entries_;
    float viewport_w_ = 0;
    float viewport_h_ = 0;
    float margin_ = 16.0f;
    float gap_ = 8.0f;
};

} // namespace meta_editor
