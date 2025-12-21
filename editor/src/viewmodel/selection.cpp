/*
 * Selection Manager Implementation
 */

#include <editor/viewmodel/selection.h>
#include <cfloat>
#include <algorithm>

namespace editor {

bool SelectionManager::isSelected(EditorNode::Ptr node) const {
    return std::find(selection_.begin(), selection_.end(), node) != selection_.end();
}

EditorNode::Ptr SelectionManager::primary() const {
    return selection_.empty() ? nullptr : selection_.back();
}

void SelectionManager::select(EditorNode::Ptr node, Mode mode) {
    if (!node || node->locked()) return;

    switch (mode) {
        case Mode::Replace:
            clearSelection();
            addToSelection(node);
            break;
        case Mode::Add:
            addToSelection(node);
            break;
        case Mode::Remove:
            removeFromSelection(node);
            break;
        case Mode::Toggle:
            if (isSelected(node)) {
                removeFromSelection(node);
            } else {
                addToSelection(node);
            }
            break;
    }

    notify(EventType::SelectionChanged, this);
}

void SelectionManager::selectAll(const std::vector<EditorNode::Ptr>& nodes) {
    clearSelection();
    for (auto& node : nodes) {
        if (!node->locked()) {
            addToSelection(node);
        }
    }
    notify(EventType::SelectionChanged, this);
}

void SelectionManager::selectRect(const Rect& rect, const std::vector<EditorNode::Ptr>& candidates, Mode mode) {
    if (mode == Mode::Replace) {
        clearSelection();
    }

    for (auto& node : candidates) {
        if (node->locked()) continue;
        auto bounds = node->bounds();
        // Check if bounds intersects selection rect
        bool intersects = !(bounds.x + bounds.width < rect.x ||
                           bounds.x > rect.x + rect.width ||
                           bounds.y + bounds.height < rect.y ||
                           bounds.y > rect.y + rect.height);
        if (intersects) {
            if (mode == Mode::Toggle) {
                if (isSelected(node)) {
                    removeFromSelection(node);
                } else {
                    addToSelection(node);
                }
            } else {
                addToSelection(node);
            }
        }
    }

    notify(EventType::SelectionChanged, this);
}

void SelectionManager::clear() {
    clearSelection();
    notify(EventType::SelectionChanged, this);
}

Rect SelectionManager::bounds() const {
    if (selection_.empty()) return {};

    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;

    for (auto& node : selection_) {
        auto b = node->bounds();
        minX = std::min(minX, b.x);
        minY = std::min(minY, b.y);
        maxX = std::max(maxX, b.x + b.width);
        maxY = std::max(maxY, b.y + b.height);
    }

    return {minX, minY, maxX - minX, maxY - minY};
}

Point SelectionManager::center() const {
    auto b = bounds();
    return b.center();
}

void SelectionManager::addToSelection(EditorNode::Ptr node) {
    if (!isSelected(node)) {
        node->setSelected(true);
        selection_.push_back(node);
    }
}

void SelectionManager::removeFromSelection(EditorNode::Ptr node) {
    auto it = std::find(selection_.begin(), selection_.end(), node);
    if (it != selection_.end()) {
        (*it)->setSelected(false);
        selection_.erase(it);
    }
}

void SelectionManager::clearSelection() {
    for (auto& node : selection_) {
        node->setSelected(false);
    }
    selection_.clear();
}

} // namespace editor
