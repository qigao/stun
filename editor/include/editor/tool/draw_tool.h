/*
 * Draw Tools
 *
 * Tools for creating various shapes.
 */

#pragma once

#include "../viewmodel/tool.h"
#include "../viewmodel/editor_vm.h"
#include "../command/command.h"

namespace editor {

// Command for creating a node
class CreateNodeCommand : public Command {
public:
    CreateNodeCommand(EditorViewModel* vm, EditorNode::Ptr node)
        : vm_(vm), node_(node) {}

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    EditorViewModel* vm_;
    EditorNode::Ptr node_;
};

// Shape type for draw tools
enum class ShapeType {
    Rectangle,
    Ellipse,
    Line,
    Polygon,
    Path
};

// Base Draw Tool
class DrawTool : public Tool {
public:
    explicit DrawTool(ShapeType type = ShapeType::Rectangle) : shape_type_(type) {}

    const char* name() const override;
    const char* icon() const override;

    // Fill and stroke settings
    void setFillColor(const Color& c) { fill_color_ = c; has_fill_ = true; }
    void setStrokeColor(const Color& c, float width = 1.0f) {
        stroke_color_ = c;
        stroke_width_ = width;
        has_stroke_ = true;
    }
    void clearFill() { has_fill_ = false; }
    void clearStroke() { has_stroke_ = false; }

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;

    void render(flex::Renderer& renderer) override;

protected:
    virtual ShapeNode::Ptr createShape();
    virtual void drawPreview(flex::Renderer& renderer);

    ShapeType shape_type_;
    bool is_drawing_ = false;
    Point start_point_;
    Point current_point_;

    bool has_fill_ = true;
    bool has_stroke_ = true;
    Color fill_color_ = Color::rgb(200, 200, 200);
    Color stroke_color_ = Color::rgb(0, 0, 0);
    float stroke_width_ = 1.0f;
};

// Convenience tool classes
class RectangleTool : public DrawTool {
public:
    RectangleTool() : DrawTool(ShapeType::Rectangle) {}
};

class EllipseTool : public DrawTool {
public:
    EllipseTool() : DrawTool(ShapeType::Ellipse) {}
};

class LineTool : public DrawTool {
public:
    LineTool();
};

} // namespace editor
