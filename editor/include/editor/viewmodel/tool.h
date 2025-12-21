/*
 * Tool System
 *
 * Base class for editor tools and the tool manager.
 * Tools handle user input and perform operations on the document.
 */

#pragma once

#include "../core/types.h"
#include "../core/observable.h"
#include "../model/document.h"
#include "../command/command.h"
#include "selection.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace editor {

// Forward declaration
class EditorViewModel;

// Input event structures
struct MouseEvent {
    Point position;       // World coordinates
    Point screenPosition; // Screen coordinates
    int button = 0;       // 0=left, 1=middle, 2=right
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

struct KeyEvent {
    int key = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

// Tool base class
class Tool {
public:
    virtual ~Tool() = default;

    // Tool identity
    virtual const char* name() const = 0;
    virtual const char* icon() const { return nullptr; }
    virtual const char* tooltip() const { return name(); }
    virtual const char* cursor() const { return "default"; }

    // Lifecycle
    virtual void activate(EditorViewModel* vm) { vm_ = vm; }
    virtual void deactivate() { vm_ = nullptr; }

    // Input handling - return true if handled
    virtual bool onMouseDown(const MouseEvent& e) { (void)e; return false; }
    virtual bool onMouseUp(const MouseEvent& e) { (void)e; return false; }
    virtual bool onMouseMove(const MouseEvent& e) { (void)e; return false; }
    virtual bool onMouseDrag(const MouseEvent& e) { (void)e; return false; }
    virtual bool onDoubleClick(const MouseEvent& e) { (void)e; return false; }
    virtual bool onKeyDown(const KeyEvent& e) { (void)e; return false; }
    virtual bool onKeyUp(const KeyEvent& e) { (void)e; return false; }

    // Rendering overlay (gizmos, guides, etc.)
    virtual void render(flex::Renderer& renderer) { (void)renderer; }

protected:
    EditorViewModel* vm_ = nullptr;
};

// Tool Manager - manages available tools and current tool
class ToolManager : public Observable {
public:
    using ToolPtr = std::unique_ptr<Tool>;

    // Register a tool
    void registerTool(const std::string& id, ToolPtr tool) {
        tools_[id] = std::move(tool);
    }

    // Get tool by id
    Tool* getTool(const std::string& id) const {
        auto it = tools_.find(id);
        return it != tools_.end() ? it->second.get() : nullptr;
    }

    // Current tool
    Tool* currentTool() const { return current_tool_; }
    const std::string& currentToolId() const { return current_tool_id_; }

    void setCurrentTool(const std::string& id, EditorViewModel* vm) {
        auto* tool = getTool(id);
        if (!tool) return;

        if (current_tool_) {
            current_tool_->deactivate();
        }

        current_tool_ = tool;
        current_tool_id_ = id;
        current_tool_->activate(vm);

        notify(EventType::ToolChanged, tool);
    }

    // Get all registered tool IDs
    std::vector<std::string> toolIds() const {
        std::vector<std::string> ids;
        for (const auto& pair : tools_) {
            ids.push_back(pair.first);
        }
        return ids;
    }

private:
    std::unordered_map<std::string, ToolPtr> tools_;
    Tool* current_tool_ = nullptr;
    std::string current_tool_id_;
};

} // namespace editor
