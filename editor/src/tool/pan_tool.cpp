/*
 * Pan Tool Implementation
 */

#include <editor/tool/pan_tool.h>
#include <editor/viewmodel/editor_vm.h>

namespace editor {

void PanTool::activate(EditorViewModel* vm) {
    Tool::activate(vm);
    panning_ = false;
}

bool PanTool::onMouseDown(const MouseEvent& e) {
    if (e.button == 0) {
        panning_ = true;
        last_pos_ = e.screenPosition;
        return true;
    }
    return false;
}

bool PanTool::onMouseDrag(const MouseEvent& e) {
    if (panning_ && vm_) {
        float dx = e.screenPosition.x - last_pos_.x;
        float dy = e.screenPosition.y - last_pos_.y;
        vm_->camera().pan(dx, dy);
        last_pos_ = e.screenPosition;
        return true;
    }
    return false;
}

bool PanTool::onMouseUp(const MouseEvent& e) {
    (void)e;
    panning_ = false;
    return false;
}

bool PanTool::onMouseMove(const MouseEvent& e) {
    (void)e;
    return false;
}

} // namespace editor
