/*
 * Shortcut Manager Implementation
 */

#include <editor/input/shortcut.h>
#include <editor/viewmodel/editor_vm.h>
#include <algorithm>

namespace editor {

std::string Shortcut::toString() const {
    std::string result;

    if (modifiers & Modifier::Ctrl) result += "Ctrl+";
    if (modifiers & Modifier::Shift) result += "Shift+";
    if (modifiers & Modifier::Alt) result += "Alt+";

    // Key name
    if (key >= 'A' && key <= 'Z') {
        result += static_cast<char>(key);
    } else if (key >= '0' && key <= '9') {
        result += static_cast<char>(key);
    } else {
        switch (key) {
            case Key::Backspace: result += "Backspace"; break;
            case Key::Tab: result += "Tab"; break;
            case Key::Return: result += "Enter"; break;
            case Key::Escape: result += "Esc"; break;
            case Key::Space: result += "Space"; break;
            case Key::Delete: result += "Delete"; break;
            case Key::Left: result += "Left"; break;
            case Key::Right: result += "Right"; break;
            case Key::Up: result += "Up"; break;
            case Key::Down: result += "Down"; break;
            case Key::Plus: case Key::Equals: result += "+"; break;
            case Key::Minus: result += "-"; break;
            case Key::BracketLeft: result += "["; break;
            case Key::BracketRight: result += "]"; break;
            default: result += "?"; break;
        }
    }

    return result;
}

void ShortcutManager::registerAction(const Action& action) {
    action_index_[action.id] = actions_.size();
    actions_.push_back(action);
}

bool ShortcutManager::handleKey(int key, bool ctrl, bool shift, bool alt) {
    // Normalize key to uppercase for letters
    if (key >= 'a' && key <= 'z') {
        key = key - 'a' + 'A';
    }

    for (const auto& action : actions_) {
        if (action.shortcut.matches(key, ctrl, shift, alt)) {
            if (action.canExecute && !action.canExecute()) {
                return false;
            }
            if (action.execute) {
                action.execute();
                return true;
            }
        }
    }
    return false;
}

bool ShortcutManager::executeAction(const std::string& id) {
    auto it = action_index_.find(id);
    if (it == action_index_.end()) return false;

    const auto& action = actions_[it->second];
    if (action.canExecute && !action.canExecute()) {
        return false;
    }
    if (action.execute) {
        action.execute();
        return true;
    }
    return false;
}

std::vector<const Action*> ShortcutManager::actionsByCategory(ActionCategory cat) const {
    std::vector<const Action*> result;
    for (const auto& action : actions_) {
        if (action.category == cat) {
            result.push_back(&action);
        }
    }
    return result;
}

void ShortcutManager::setShortcut(const std::string& actionId, const Shortcut& sc) {
    auto it = action_index_.find(actionId);
    if (it != action_index_.end()) {
        actions_[it->second].shortcut = sc;
    }
}

void ShortcutManager::setupDefaults() {
    if (!vm_) return;

    // ========================================
    // File operations
    // ========================================
    registerAction({
        "file.new", "New Document", ActionCategory::File,
        {Key::N, Modifier::Ctrl},
        [this]() { /* TODO: vm_->newDocument(); */ }
    });

    registerAction({
        "file.open", "Open...", ActionCategory::File,
        {Key::O, Modifier::Ctrl},
        [this]() { /* TODO: vm_->openDocument(); */ }
    });

    registerAction({
        "file.save", "Save", ActionCategory::File,
        {Key::S, Modifier::Ctrl},
        [this]() { /* TODO: vm_->saveDocument(); */ }
    });

    registerAction({
        "file.save_as", "Save As...", ActionCategory::File,
        {Key::S, Modifier::Ctrl | Modifier::Shift},
        [this]() { /* TODO: vm_->saveDocumentAs(); */ }
    });

    // ========================================
    // Edit operations
    // ========================================
    registerAction({
        "edit.undo", "Undo", ActionCategory::Edit,
        {Key::Z, Modifier::Ctrl},
        [this]() { vm_->undo(); },
        [this]() { return vm_->history().canUndo(); }
    });

    registerAction({
        "edit.redo", "Redo", ActionCategory::Edit,
        {Key::Y, Modifier::Ctrl},
        [this]() { vm_->redo(); },
        [this]() { return vm_->history().canRedo(); }
    });

    registerAction({
        "edit.redo_alt", "Redo", ActionCategory::Edit,
        {Key::Z, Modifier::Ctrl | Modifier::Shift},
        [this]() { vm_->redo(); },
        [this]() { return vm_->history().canRedo(); }
    });

    registerAction({
        "edit.cut", "Cut", ActionCategory::Edit,
        {Key::X, Modifier::Ctrl},
        [this]() { /* TODO: vm_->cut(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "edit.copy", "Copy", ActionCategory::Edit,
        {Key::C, Modifier::Ctrl},
        [this]() { /* TODO: vm_->copy(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "edit.paste", "Paste", ActionCategory::Edit,
        {Key::V, Modifier::Ctrl},
        [this]() { /* TODO: vm_->paste(); */ }
    });

    registerAction({
        "edit.duplicate", "Duplicate", ActionCategory::Edit,
        {Key::D, Modifier::Ctrl},
        [this]() { /* TODO: vm_->duplicate(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "edit.delete", "Delete", ActionCategory::Edit,
        {Key::Delete, Modifier::None},
        [this]() { vm_->removeSelectedNodes(); },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "edit.delete_backspace", "Delete", ActionCategory::Edit,
        {Key::Backspace, Modifier::None},
        [this]() { vm_->removeSelectedNodes(); },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "edit.select_all", "Select All", ActionCategory::Edit,
        {Key::A, Modifier::Ctrl},
        [this]() { vm_->selection().selectAll(vm_->document()->allNodes()); }
    });

    registerAction({
        "edit.deselect", "Deselect All", ActionCategory::Edit,
        {Key::D, Modifier::Ctrl | Modifier::Shift},
        [this]() { vm_->selection().clear(); }
    });

    // ========================================
    // View operations
    // ========================================
    registerAction({
        "view.zoom_in", "Zoom In", ActionCategory::View,
        {Key::Plus, Modifier::Ctrl},
        [this]() { vm_->camera().setZoom(vm_->camera().zoom() * 1.25f); }
    });

    registerAction({
        "view.zoom_in_alt", "Zoom In", ActionCategory::View,
        {Key::Equals, Modifier::Ctrl},
        [this]() { vm_->camera().setZoom(vm_->camera().zoom() * 1.25f); }
    });

    registerAction({
        "view.zoom_out", "Zoom Out", ActionCategory::View,
        {Key::Minus, Modifier::Ctrl},
        [this]() { vm_->camera().setZoom(vm_->camera().zoom() * 0.8f); }
    });

    registerAction({
        "view.zoom_reset", "Zoom 100%", ActionCategory::View,
        {Key::Num0, Modifier::Ctrl},
        [this]() { vm_->camera().setZoom(1.0f); }
    });

    registerAction({
        "view.zoom_fit", "Zoom to Fit", ActionCategory::View,
        {Key::Num1, Modifier::Ctrl},
        [this]() { /* TODO: vm_->zoomToFit(); */ }
    });

    // ========================================
    // Tool switching
    // ========================================
    registerAction({
        "tool.select", "Select Tool", ActionCategory::Tool,
        {Key::V, Modifier::None},
        [this]() { vm_->setTool("select"); }
    });

    registerAction({
        "tool.rectangle", "Rectangle Tool", ActionCategory::Tool,
        {Key::R, Modifier::None},
        [this]() { vm_->setTool("rectangle"); }
    });

    registerAction({
        "tool.ellipse", "Ellipse Tool", ActionCategory::Tool,
        {Key::E, Modifier::None},
        [this]() { vm_->setTool("ellipse"); }
    });

    registerAction({
        "tool.line", "Line Tool", ActionCategory::Tool,
        {Key::L, Modifier::None},
        [this]() { vm_->setTool("line"); }
    });

    registerAction({
        "tool.pen", "Pen Tool", ActionCategory::Tool,
        {Key::P, Modifier::None},
        [this]() { vm_->setTool("pen"); }
    });

    registerAction({
        "tool.text", "Text Tool", ActionCategory::Tool,
        {Key::T, Modifier::None},
        [this]() { vm_->setTool("text"); }
    });

    // ========================================
    // Object operations
    // ========================================
    registerAction({
        "object.group", "Group", ActionCategory::Object,
        {Key::G, Modifier::Ctrl},
        [this]() { /* TODO: vm_->groupSelection(); */ },
        [this]() { return vm_->selection().count() > 1; }
    });

    registerAction({
        "object.ungroup", "Ungroup", ActionCategory::Object,
        {Key::G, Modifier::Ctrl | Modifier::Shift},
        [this]() { /* TODO: vm_->ungroupSelection(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    // Boolean operations
    registerAction({
        "object.bool_union", "Union", ActionCategory::Object,
        {Key::U, Modifier::Ctrl},
        [this]() { vm_->booleanUnion(); },
        [this]() { return vm_->selection().count() >= 2; }
    });

    registerAction({
        "object.bool_subtract", "Subtract", ActionCategory::Object,
        {Key::BracketLeft, Modifier::Ctrl},
        [this]() { vm_->booleanSubtract(); },
        [this]() { return vm_->selection().count() >= 2; }
    });

    registerAction({
        "object.bool_intersect", "Intersect", ActionCategory::Object,
        {Key::I, Modifier::Ctrl | Modifier::Shift},
        [this]() { vm_->booleanIntersect(); },
        [this]() { return vm_->selection().count() >= 2; }
    });

    // ========================================
    // Arrange operations
    // ========================================
    registerAction({
        "arrange.bring_front", "Bring to Front", ActionCategory::Arrange,
        {Key::BracketRight, Modifier::Ctrl | Modifier::Shift},
        [this]() { /* TODO: vm_->bringToFront(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "arrange.send_back", "Send to Back", ActionCategory::Arrange,
        {Key::BracketLeft, Modifier::Ctrl | Modifier::Shift},
        [this]() { /* TODO: vm_->sendToBack(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "arrange.bring_forward", "Bring Forward", ActionCategory::Arrange,
        {Key::BracketRight, Modifier::Ctrl},
        [this]() { /* TODO: vm_->bringForward(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "arrange.send_backward", "Send Backward", ActionCategory::Arrange,
        {Key::BracketLeft, Modifier::Ctrl},
        [this]() { /* TODO: vm_->sendBackward(); */ },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    // ========================================
    // Transform with arrow keys
    // ========================================
    registerAction({
        "transform.nudge_left", "Move Left", ActionCategory::Edit,
        {Key::Left, Modifier::None},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x() - 1, node->y());
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "transform.nudge_right", "Move Right", ActionCategory::Edit,
        {Key::Right, Modifier::None},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x() + 1, node->y());
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "transform.nudge_up", "Move Up", ActionCategory::Edit,
        {Key::Up, Modifier::None},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x(), node->y() - 1);
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "transform.nudge_down", "Move Down", ActionCategory::Edit,
        {Key::Down, Modifier::None},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x(), node->y() + 1);
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    // Big nudge with Shift
    registerAction({
        "transform.nudge_left_big", "Move Left (10px)", ActionCategory::Edit,
        {Key::Left, Modifier::Shift},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x() - 10, node->y());
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "transform.nudge_right_big", "Move Right (10px)", ActionCategory::Edit,
        {Key::Right, Modifier::Shift},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x() + 10, node->y());
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "transform.nudge_up_big", "Move Up (10px)", ActionCategory::Edit,
        {Key::Up, Modifier::Shift},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x(), node->y() - 10);
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    registerAction({
        "transform.nudge_down_big", "Move Down (10px)", ActionCategory::Edit,
        {Key::Down, Modifier::Shift},
        [this]() {
            for (auto& node : vm_->selection().selection()) {
                node->setPosition(node->x(), node->y() + 10);
            }
        },
        [this]() { return !vm_->selection().isEmpty(); }
    });

    // Escape to deselect/cancel
    registerAction({
        "edit.escape", "Cancel/Deselect", ActionCategory::Edit,
        {Key::Escape, Modifier::None},
        [this]() { vm_->selection().clear(); }
    });
}

} // namespace editor
