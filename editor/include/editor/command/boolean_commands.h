/*
 * Boolean Operations Command
 *
 * Commands for combining shapes using boolean operations:
 * Union, Intersect, Subtract, Exclude (XOR)
 */

#pragma once

#include "command.h"
#include "../model/node.h"
#include "../model/layer.h"
#include "../model/document.h"
#include "../core/transform.h"
#include <vector>

namespace editor {

enum class BooleanOp {
    Union,      // Combine all shapes
    Intersect,  // Keep only overlapping area
    Subtract,   // First shape minus others
    Exclude     // XOR - keep non-overlapping areas
};

// Get display name for operation
inline const char* boolean_op_name(BooleanOp op) {
    switch (op) {
        case BooleanOp::Union: return "Union";
        case BooleanOp::Intersect: return "Intersect";
        case BooleanOp::Subtract: return "Subtract";
        case BooleanOp::Exclude: return "Exclude";
        default: return "Boolean";
    }
}

// Perform boolean operation on shapes, returns new shape node
// Returns nullptr if operation fails (e.g., no overlap for intersect)
ShapeNode::Ptr perform_boolean_operation(
    const std::vector<ShapeNode::Ptr>& shapes,
    BooleanOp op);

// Boolean Operation Command - supports undo/redo
class BooleanCommand : public Command {
public:
    BooleanCommand(Document* doc, Layer* layer,
                   std::vector<ShapeNode::Ptr> shapes, BooleanOp op);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    Document* document_;
    Layer* layer_;
    std::vector<ShapeNode::Ptr> original_shapes_;
    ShapeNode::Ptr result_shape_;
    BooleanOp op_;
    bool executed_ = false;
};

} // namespace editor
