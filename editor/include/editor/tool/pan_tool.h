/*
 * Pan Tool
 *
 * Tool for panning/scrolling the canvas view.
 */

#pragma once

#include "../viewmodel/tool.h"

namespace editor {

class PanTool : public Tool {
public:
    const char* name() const override { return "Pan"; }
    const char* icon() const override { return "hand"; }
    const char* tooltip() const override { return "Pan the canvas (H)"; }

    void activate(EditorViewModel* vm) override;

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;

private:
    bool panning_ = false;
    Point last_pos_;
};

} // namespace editor
