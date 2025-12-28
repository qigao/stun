/*
 * Meta Editor - Command System
 *
 * Command pattern for undo/redo support.
 */

#pragma once

#include <flex.h>
#include <memory>
#include <vector>
#include <string>

namespace meta_editor {

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual const char* name() const = 0;
};

class MoveCommand : public Command {
public:
    MoveCommand(std::vector<flex::Node*> nodes,
                std::vector<flex::Vec2> old_positions,
                std::vector<flex::Vec2> new_positions);

    void execute() override;
    void undo() override;
    const char* name() const override { return "Move"; }

private:
    std::vector<flex::Node*> nodes_;
    std::vector<flex::Vec2> old_positions_;
    std::vector<flex::Vec2> new_positions_;
};

class SetFillCommand : public Command {
public:
    SetFillCommand(std::vector<flex::Node*> nodes,
                   std::vector<flex::Paint> old_fills,
                   flex::Paint new_fill);

    void execute() override;
    void undo() override;
    const char* name() const override { return "Set Fill"; }

private:
    std::vector<flex::Node*> nodes_;
    std::vector<flex::Paint> old_fills_;
    flex::Paint new_fill_;
};

class SetStrokeCommand : public Command {
public:
    SetStrokeCommand(std::vector<flex::Node*> nodes,
                     std::vector<flex::Paint> old_strokes,
                     std::vector<float> old_widths,
                     flex::Paint new_stroke,
                     float new_width);

    void execute() override;
    void undo() override;
    const char* name() const override { return "Set Stroke"; }

private:
    std::vector<flex::Node*> nodes_;
    std::vector<flex::Paint> old_strokes_;
    std::vector<float> old_widths_;
    flex::Paint new_stroke_;
    float new_width_;
};

class EditPathCommand : public Command {
public:
    EditPathCommand(flex::Node* node, std::string old_path, std::string new_path);

    void execute() override;
    void undo() override;
    const char* name() const override { return "Edit Path"; }

private:
    flex::Node* node_;
    std::string old_path_;
    std::string new_path_;
};

class CommandManager {
public:
    CommandManager();

    void execute(std::unique_ptr<Command> command);

    void undo();
    void redo();
    bool can_undo() const;
    bool can_redo() const;

    void clear_history();
    int undo_count() const { return (int)undo_stack_.size(); }
    int redo_count() const { return (int)redo_stack_.size(); }

    const char* undo_name() const;
    const char* redo_name() const;

private:
    std::vector<std::unique_ptr<Command>> undo_stack_;
    std::vector<std::unique_ptr<Command>> redo_stack_;
    size_t max_history_ = 100;
};

} // namespace meta_editor
