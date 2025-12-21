/*
 * Editor ViewModel
 *
 * Main ViewModel that coordinates all editor functionality.
 * Acts as the central hub connecting Model, Tools, Selection, and History.
 */

#pragma once

#include "../core/types.h"
#include "../core/observable.h"
#include "../core/transform.h"
#include "../model/document.h"
#include "../command/command.h"
#include "selection.h"
#include "tool.h"
#include "../input/shortcut.h"

namespace editor {

// Backwards-compatible Camera alias for ViewTransform
// Provides .x, .y, .zoom accessors for existing code
struct Camera : public ViewTransform {
    // Legacy accessors for compatibility
    float x() const { return panX(); }
    float y() const { return panY(); }
};

// Main Editor ViewModel
class EditorViewModel : public Observable {
public:
    EditorViewModel();

    // Document access
    Document* document() const { return document_.get(); }
    void setDocument(Document::Ptr doc);

    // Selection
    SelectionManager& selection() { return selection_; }
    const SelectionManager& selection() const { return selection_; }

    // History (undo/redo)
    HistoryManager& history() { return history_; }
    const HistoryManager& history() const { return history_; }

    void executeCommand(CommandPtr cmd);

    void undo() { history_.undo(); }
    void redo() { history_.redo(); }

    // Tools
    ToolManager& tools() { return tools_; }
    const ToolManager& tools() const { return tools_; }
    Tool* currentTool() const { return tools_.currentTool(); }

    void setTool(const std::string& toolId);

    // Camera/Viewport
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }

    // Shortcuts
    ShortcutManager& shortcuts() { return shortcuts_; }
    const ShortcutManager& shortcuts() const { return shortcuts_; }

    // Input handling - delegates to current tool
    bool onMouseDown(const MouseEvent& e);
    bool onMouseUp(const MouseEvent& e);
    bool onMouseMove(const MouseEvent& e);
    bool onMouseDrag(const MouseEvent& e);
    bool onKeyDown(const KeyEvent& e);

    // Rendering
    void render(flex::Renderer& renderer);

    // Node operations (convenience methods)
    void addNode(EditorNode::Ptr node);
    void removeSelectedNodes();

    // Boolean operations
    void booleanUnion();
    void booleanIntersect();
    void booleanSubtract();
    void booleanExclude();

    // Create shape helpers
    ShapeNode::Ptr createRect(float x, float y, float w, float h, const Color& fill);
    ShapeNode::Ptr createCircle(float x, float y, float radius, const Color& fill);

private:
    void setupDefaultTools();

    Document::Ptr document_;
    SelectionManager selection_;
    HistoryManager history_;
    ToolManager tools_;
    Camera camera_;
    ShortcutManager shortcuts_;
};

} // namespace editor
