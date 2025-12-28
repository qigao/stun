/*
 * Meta Editor - Command System Implementation
 */

#include "meta_editor/command.h"

namespace meta_editor {

// MoveCommand
MoveCommand::MoveCommand(std::vector<flex::Node*> nodes,
                         std::vector<flex::Vec2> old_positions,
                         std::vector<flex::Vec2> new_positions)
    : nodes_(std::move(nodes))
    , old_positions_(std::move(old_positions))
    , new_positions_(std::move(new_positions)) {}

void MoveCommand::execute() {
    for (size_t i = 0; i < nodes_.size(); ++i) {
        nodes_[i]->set_position(new_positions_[i].x(), new_positions_[i].y());
    }
}

void MoveCommand::undo() {
    for (size_t i = 0; i < nodes_.size(); ++i) {
        nodes_[i]->set_position(old_positions_[i].x(), old_positions_[i].y());
    }
}

// Helper to apply Paint to Shape
static void apply_fill(flex::Node* node, const flex::Paint& paint) {
    if (node->type() != flex::NodeType::Shape) return;
    auto* shape = static_cast<flex::Shape*>(node);

    switch (paint.type) {
        case flex::Paint::Type::None:
            shape->clear_fill();
            break;
        case flex::Paint::Type::Solid:
            shape->set_fill(paint.color);
            break;
        case flex::Paint::Type::Linear:
            shape->set_fill(paint.linear);
            break;
        case flex::Paint::Type::Radial:
            shape->set_fill(paint.radial);
            break;
    }
}

static void apply_stroke(flex::Node* node, const flex::Paint& paint, float width) {
    if (node->type() != flex::NodeType::Shape) return;
    auto* shape = static_cast<flex::Shape*>(node);

    switch (paint.type) {
        case flex::Paint::Type::None:
            shape->clear_stroke();
            break;
        case flex::Paint::Type::Solid:
            shape->set_stroke(paint.color, width);
            break;
        case flex::Paint::Type::Linear:
            shape->set_stroke(paint.linear, width);
            break;
        case flex::Paint::Type::Radial:
            shape->set_stroke(paint.radial, width);
            break;
    }
}

// SetFillCommand
SetFillCommand::SetFillCommand(std::vector<flex::Node*> nodes,
                               std::vector<flex::Paint> old_fills,
                               flex::Paint new_fill)
    : nodes_(std::move(nodes))
    , old_fills_(std::move(old_fills))
    , new_fill_(std::move(new_fill)) {}

void SetFillCommand::execute() {
    for (auto* node : nodes_) {
        apply_fill(node, new_fill_);
    }
}

void SetFillCommand::undo() {
    for (size_t i = 0; i < nodes_.size(); ++i) {
        apply_fill(nodes_[i], old_fills_[i]);
    }
}

// SetStrokeCommand
SetStrokeCommand::SetStrokeCommand(std::vector<flex::Node*> nodes,
                                   std::vector<flex::Paint> old_strokes,
                                   std::vector<float> old_widths,
                                   flex::Paint new_stroke,
                                   float new_width)
    : nodes_(std::move(nodes))
    , old_strokes_(std::move(old_strokes))
    , old_widths_(std::move(old_widths))
    , new_stroke_(std::move(new_stroke))
    , new_width_(new_width) {}

void SetStrokeCommand::execute() {
    for (auto* node : nodes_) {
        apply_stroke(node, new_stroke_, new_width_);
    }
}

void SetStrokeCommand::undo() {
    for (size_t i = 0; i < nodes_.size(); ++i) {
        apply_stroke(nodes_[i], old_strokes_[i], old_widths_[i]);
    }
}

// EditPathCommand
EditPathCommand::EditPathCommand(flex::Node* node, std::string old_path, std::string new_path)
    : node_(node)
    , old_path_(std::move(old_path))
    , new_path_(std::move(new_path)) {}

void EditPathCommand::execute() {
    if (node_->type() != flex::NodeType::Shape) return;
    auto* shape = static_cast<flex::Shape*>(node_);
    if (shape->geometry_type() != flex::GeometryType::Path) return;
    shape->set_path(new_path_);
}

void EditPathCommand::undo() {
    if (node_->type() != flex::NodeType::Shape) return;
    auto* shape = static_cast<flex::Shape*>(node_);
    if (shape->geometry_type() != flex::GeometryType::Path) return;
    shape->set_path(old_path_);
}

// CommandManager
CommandManager::CommandManager() {}

void CommandManager::execute(std::unique_ptr<Command> command) {
    command->execute();
    
    // Clear redo stack when new command is executed
    redo_stack_.clear();
    
    // Add to undo stack
    undo_stack_.push_back(std::move(command));
    
    // Limit history size
    if (undo_stack_.size() > max_history_) {
        undo_stack_.erase(undo_stack_.begin());
    }
}

void CommandManager::undo() {
    if (undo_stack_.empty()) return;
    
    auto command = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    
    command->undo();
    
    redo_stack_.push_back(std::move(command));
}

void CommandManager::redo() {
    if (redo_stack_.empty()) return;
    
    auto command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    
    command->execute();
    
    undo_stack_.push_back(std::move(command));
}

bool CommandManager::can_undo() const {
    return !undo_stack_.empty();
}

bool CommandManager::can_redo() const {
    return !redo_stack_.empty();
}

void CommandManager::clear_history() {
    undo_stack_.clear();
    redo_stack_.clear();
}

const char* CommandManager::undo_name() const {
    return undo_stack_.empty() ? "" : undo_stack_.back()->name();
}

const char* CommandManager::redo_name() const {
    return redo_stack_.empty() ? "" : redo_stack_.back()->name();
}

} // namespace meta_editor
