/*
 * Selection Manager
 *
 * Manages the current selection of nodes in the editor.
 * Supports single and multi-selection.
 */

#pragma once

#include "../core/types.h"
#include "../core/observable.h"
#include "../model/node.h"
#include <vector>

namespace editor {

class SelectionManager : public Observable {
public:
    // Selection modes
    enum class Mode {
        Replace,   // Clear existing selection
        Add,       // Add to selection
        Remove,    // Remove from selection
        Toggle,    // Toggle selection state
    };

    // Get current selection
    const std::vector<EditorNode::Ptr>& selection() const { return selection_; }

    bool isEmpty() const { return selection_.empty(); }
    size_t count() const { return selection_.size(); }

    bool isSelected(EditorNode::Ptr node) const;

    // Primary selection (first selected, or last if multi-select)
    EditorNode::Ptr primary() const;

    // Selection operations
    void select(EditorNode::Ptr node, Mode mode = Mode::Replace);
    void selectAll(const std::vector<EditorNode::Ptr>& nodes);
    void selectRect(const Rect& rect, const std::vector<EditorNode::Ptr>& candidates, Mode mode = Mode::Replace);
    void clear();

    // Get combined bounds of selection
    Rect bounds() const;

    // Get center of selection
    Point center() const;

private:
    void addToSelection(EditorNode::Ptr node);
    void removeFromSelection(EditorNode::Ptr node);
    void clearSelection();

    std::vector<EditorNode::Ptr> selection_;
};

} // namespace editor
