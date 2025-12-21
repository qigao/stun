/*
 * Transform Commands
 *
 * Commands for scaling, rotating, and other transform operations.
 * All commands use Transform2D for unified matrix-based transforms.
 * All commands support undo/redo.
 */

#pragma once

#include "../command/command.h"
#include "../model/node.h"
#include "../core/types.h"
#include "../core/transform.h"
#include <vector>
#include <cmath>

namespace editor {

// Generic transform command - applies a Transform2D to nodes
class TransformCommand : public Command {
public:
    TransformCommand(std::vector<EditorNode::Ptr> nodes, const Transform2D& transform,
                     const std::string& desc = "Transform")
        : nodes_(std::move(nodes)), transform_(transform), description_(desc) {
        // Store original transforms
        for (auto& node : nodes_) {
            originals_.push_back(node->transform());
        }
    }

    void execute() override {
        for (size_t i = 0; i < nodes_.size(); ++i) {
            nodes_[i]->setTransform(transform_ * originals_[i]);
        }
    }

    void undo() override {
        for (size_t i = 0; i < nodes_.size(); ++i) {
            nodes_[i]->setTransform(originals_[i]);
        }
    }

    std::string description() const override {
        return description_ + " " + std::to_string(nodes_.size()) + " object(s)";
    }

    bool canMerge(const Command* other) const override {
        auto* t = dynamic_cast<const TransformCommand*>(other);
        if (!t) return false;
        if (t->nodes_.size() != nodes_.size()) return false;
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (t->nodes_[i] != nodes_[i]) return false;
        }
        return true;
    }

    void merge(Command* other) override {
        auto* t = dynamic_cast<TransformCommand*>(other);
        if (t) {
            transform_ = t->transform_ * transform_;
        }
    }

protected:
    std::vector<EditorNode::Ptr> nodes_;
    std::vector<Transform2D> originals_;
    Transform2D transform_;
    std::string description_;
};

// Move command - translates nodes by delta
class MoveCommand : public TransformCommand {
public:
    MoveCommand(std::vector<EditorNode::Ptr> nodes, float dx, float dy)
        : TransformCommand(std::move(nodes), Transform2D::translation(dx, dy), "Move") {}

    // Convenience: create from two points
    static std::unique_ptr<MoveCommand> fromPoints(std::vector<EditorNode::Ptr> nodes,
                                                    const Point& from, const Point& to) {
        return std::make_unique<MoveCommand>(std::move(nodes), to.x - from.x, to.y - from.y);
    }
};

// Scale command - scales nodes around a pivot
class ScaleCommand : public TransformCommand {
public:
    ScaleCommand(std::vector<EditorNode::Ptr> nodes, Point pivot, float scaleX, float scaleY)
        : TransformCommand(std::move(nodes),
                          Transform2D::scaleAround(scaleX, scaleY, pivot), "Scale") {}

    // Uniform scale
    ScaleCommand(std::vector<EditorNode::Ptr> nodes, Point pivot, float scale)
        : ScaleCommand(std::move(nodes), pivot, scale, scale) {}
};

// Rotate command - rotates nodes around a pivot
class RotateCommand : public TransformCommand {
public:
    RotateCommand(std::vector<EditorNode::Ptr> nodes, Point pivot, float angleDegrees)
        : TransformCommand(std::move(nodes),
                          Transform2D::rotationAround(angleDegrees * 3.14159265f / 180.0f, pivot),
                          "Rotate") {}
};

// Set transform command - sets absolute transform
class SetTransformCommand : public Command {
public:
    SetTransformCommand(EditorNode::Ptr node, const Transform2D& newTransform)
        : node_(node), newTransform_(newTransform), oldTransform_(node->transform()) {}

    void execute() override {
        node_->setTransform(newTransform_);
    }

    void undo() override {
        node_->setTransform(oldTransform_);
    }

    std::string description() const override {
        return "Set transform";
    }

private:
    EditorNode::Ptr node_;
    Transform2D oldTransform_;
    Transform2D newTransform_;
};

// Set property command (generic)
template<typename T>
class SetPropertyCommand : public Command {
public:
    using Getter = std::function<T(EditorNode*)>;
    using Setter = std::function<void(EditorNode*, const T&)>;

    SetPropertyCommand(EditorNode::Ptr node, const std::string& propName,
                       const T& newValue, Getter getter, Setter setter)
        : node_(node), propName_(propName), newValue_(newValue),
          getter_(getter), setter_(setter) {
        oldValue_ = getter_(node_.get());
    }

    void execute() override {
        setter_(node_.get(), newValue_);
    }

    void undo() override {
        setter_(node_.get(), oldValue_);
    }

    std::string description() const override {
        return "Change " + propName_;
    }

private:
    EditorNode::Ptr node_;
    std::string propName_;
    T oldValue_;
    T newValue_;
    Getter getter_;
    Setter setter_;
};

// Delete nodes command
class DeleteNodesCommand : public Command {
public:
    DeleteNodesCommand(std::vector<EditorNode::Ptr> nodes)
        : nodes_(std::move(nodes)) {
        // Store layer references
        for (auto& node : nodes_) {
            layers_.push_back(node->layer());
        }
    }

    void execute() override {
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (layers_[i]) {
                layers_[i]->removeNode(nodes_[i]);
            }
        }
    }

    void undo() override {
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (layers_[i]) {
                layers_[i]->addNode(nodes_[i]);
            }
        }
    }

    std::string description() const override {
        return "Delete " + std::to_string(nodes_.size()) + " object(s)";
    }

private:
    std::vector<EditorNode::Ptr> nodes_;
    std::vector<Layer*> layers_;
};

} // namespace editor
