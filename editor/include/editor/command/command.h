/*
 * Command Pattern
 *
 * Base class for all editor commands. Supports undo/redo.
 * All modifications to the document should go through commands.
 */

#pragma once

#include "../core/types.h"
#include <string>
#include <vector>
#include <memory>
#include "../core/observable.h"
namespace editor {

// Base Command class
class Command {
public:
    virtual ~Command() = default;

    // Execute the command
    virtual void execute() = 0;

    // Undo the command
    virtual void undo() = 0;

    // Command description for UI
    virtual std::string description() const = 0;

    // Can this command be merged with another? (for continuous operations like dragging)
    virtual bool canMerge(const Command* other) const { (void)other; return false; }
    virtual void merge(Command* other) { (void)other; }
};

// Group multiple commands as one undoable operation
class CommandGroup : public Command {
public:
    explicit CommandGroup(std::string desc) : description_(std::move(desc)) {}

    void add(CommandPtr cmd) {
        commands_.push_back(std::move(cmd));
    }

    void execute() override {
        for (auto& cmd : commands_) {
            cmd->execute();
        }
    }

    void undo() override {
        // Undo in reverse order
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            (*it)->undo();
        }
    }

    std::string description() const override { return description_; }

private:
    std::string description_;
    std::vector<CommandPtr> commands_;
};

// History Manager - manages undo/redo stack
class HistoryManager : public Observable {
public:
    void execute(CommandPtr cmd) {
        cmd->execute();

        // Try to merge with previous command
        if (!undo_stack_.empty() && undo_stack_.back()->canMerge(cmd.get())) {
            undo_stack_.back()->merge(cmd.get());
        } else {
            undo_stack_.push_back(std::move(cmd));
        }

        // Clear redo stack on new command
        redo_stack_.clear();

        notify(EventType::HistoryChanged);
    }

    void undo() {
        if (undo_stack_.empty()) return;

        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();

        cmd->undo();
        redo_stack_.push_back(std::move(cmd));

        notify(EventType::HistoryChanged);
    }

    void redo() {
        if (redo_stack_.empty()) return;

        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();

        cmd->execute();
        undo_stack_.push_back(std::move(cmd));

        notify(EventType::HistoryChanged);
    }

    bool canUndo() const { return !undo_stack_.empty(); }
    bool canRedo() const { return !redo_stack_.empty(); }

    size_t undoCount() const { return undo_stack_.size(); }
    size_t redoCount() const { return redo_stack_.size(); }

    // Access stack for UI display
    const std::vector<CommandPtr>& undoStack() const { return undo_stack_; }
    const std::vector<CommandPtr>& redoStack() const { return redo_stack_; }

    // Jump to specific state in history
    void undoTo(size_t index) {
        while (undo_stack_.size() > index + 1 && !undo_stack_.empty()) {
            undo();
        }
    }

    void redoTo(size_t index) {
        size_t target = redo_stack_.size() - index - 1;
        while (redo_stack_.size() > target && !redo_stack_.empty()) {
            redo();
        }
    }

    std::string undoDescription() const {
        return undo_stack_.empty() ? "" : undo_stack_.back()->description();
    }

    std::string redoDescription() const {
        return redo_stack_.empty() ? "" : redo_stack_.back()->description();
    }

    void clear() {
        undo_stack_.clear();
        redo_stack_.clear();
        notify(EventType::HistoryChanged);
    }

private:
    std::vector<CommandPtr> undo_stack_;
    std::vector<CommandPtr> redo_stack_;
};

} // namespace editor
