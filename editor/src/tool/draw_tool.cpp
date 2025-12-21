/*
 * Draw Tool Implementation
 */

#include <editor/tool/draw_tool.h>
#include <cmath>

namespace editor {

// CreateNodeCommand implementation
void CreateNodeCommand::execute() {
    vm_->addNode(node_);
    vm_->selection().select(node_);
}

void CreateNodeCommand::undo() {
    vm_->selection().clear();
    if (auto* layer = node_->layer()) {
        layer->removeNode(node_);
    }
}

std::string CreateNodeCommand::description() const {
    return "Create " + node_->name();
}

// DrawTool implementation
const char* DrawTool::name() const {
    switch (shape_type_) {
        case ShapeType::Rectangle: return "Rectangle";
        case ShapeType::Ellipse: return "Ellipse";
        case ShapeType::Line: return "Line";
        case ShapeType::Polygon: return "Polygon";
        case ShapeType::Path: return "Path";
    }
    return "Draw";
}

const char* DrawTool::icon() const {
    switch (shape_type_) {
        case ShapeType::Rectangle: return "square";
        case ShapeType::Ellipse: return "circle";
        case ShapeType::Line: return "line";
        case ShapeType::Polygon: return "polygon";
        case ShapeType::Path: return "pen";
    }
    return "shapes";
}

bool DrawTool::onMouseDown(const MouseEvent& e) {
    if (e.button != 0) return false;

    is_drawing_ = true;
    start_point_ = e.position;
    current_point_ = e.position;
    return true;
}

bool DrawTool::onMouseDrag(const MouseEvent& e) {
    if (!is_drawing_) return false;

    current_point_ = e.position;

    // Constrain to square/circle if shift is held
    if (e.shift) {
        float dx = current_point_.x - start_point_.x;
        float dy = current_point_.y - start_point_.y;
        float size = std::max(std::abs(dx), std::abs(dy));
        current_point_.x = start_point_.x + (dx >= 0 ? size : -size);
        current_point_.y = start_point_.y + (dy >= 0 ? size : -size);
    }

    return true;
}

bool DrawTool::onMouseUp(const MouseEvent& e) {
    if (!is_drawing_) return false;
    (void)e;

    is_drawing_ = false;

    // Create the shape
    auto node = createShape();
    if (node) {
        node->setName(name());
        vm_->executeCommand(std::make_unique<CreateNodeCommand>(vm_, node));
    }

    return true;
}

void DrawTool::render(flex::Renderer& renderer) {
    if (!is_drawing_) return;

    // Draw preview
    drawPreview(renderer);
}

ShapeNode::Ptr DrawTool::createShape() {
    float x = std::min(start_point_.x, current_point_.x);
    float y = std::min(start_point_.y, current_point_.y);
    float w = std::abs(current_point_.x - start_point_.x);
    float h = std::abs(current_point_.y - start_point_.y);

    if (w < 1 || h < 1) return nullptr;

    auto shape = flex::Shape::create();

    switch (shape_type_) {
        case ShapeType::Rectangle:
            shape->set_position(x, y);
            shape->set_rect(w, h);
            break;

        case ShapeType::Ellipse:
            shape->set_position(x + w/2, y + h/2);
            shape->set_ellipse(w/2, h/2);
            break;

        case ShapeType::Line:
            shape->set_position(start_point_.x, start_point_.y);
            shape->set_line(current_point_.x - start_point_.x,
                           current_point_.y - start_point_.y);
            break;

        default:
            return nullptr;
    }

    if (has_fill_) {
        shape->set_fill(flex::Color{fill_color_.r, fill_color_.g, fill_color_.b, fill_color_.a});
    }
    if (has_stroke_) {
        shape->set_stroke(flex::Color{stroke_color_.r, stroke_color_.g, stroke_color_.b, stroke_color_.a}, stroke_width_);
    }

    return ShapeNode::create(shape);
}

void DrawTool::drawPreview(flex::Renderer& renderer) {
    float x = std::min(start_point_.x, current_point_.x);
    float y = std::min(start_point_.y, current_point_.y);
    float w = std::abs(current_point_.x - start_point_.x);
    float h = std::abs(current_point_.y - start_point_.y);

    std::string path;

    switch (shape_type_) {
        case ShapeType::Rectangle:
            path = "M " + std::to_string(x) + " " + std::to_string(y) +
                " h " + std::to_string(w) +
                " v " + std::to_string(h) +
                " h " + std::to_string(-w) + " Z";
            break;

        case ShapeType::Ellipse: {
            float cx = x + w/2;
            float cy = y + h/2;
            float rx = w/2;
            float ry = h/2;
            // Approximate ellipse with bezier curves
            float k = 0.5522848f;
            path = "M " + std::to_string(cx) + " " + std::to_string(cy - ry) +
                " C " + std::to_string(cx + rx*k) + " " + std::to_string(cy - ry) +
                " " + std::to_string(cx + rx) + " " + std::to_string(cy - ry*k) +
                " " + std::to_string(cx + rx) + " " + std::to_string(cy) +
                " C " + std::to_string(cx + rx) + " " + std::to_string(cy + ry*k) +
                " " + std::to_string(cx + rx*k) + " " + std::to_string(cy + ry) +
                " " + std::to_string(cx) + " " + std::to_string(cy + ry) +
                " C " + std::to_string(cx - rx*k) + " " + std::to_string(cy + ry) +
                " " + std::to_string(cx - rx) + " " + std::to_string(cy + ry*k) +
                " " + std::to_string(cx - rx) + " " + std::to_string(cy) +
                " C " + std::to_string(cx - rx) + " " + std::to_string(cy - ry*k) +
                " " + std::to_string(cx - rx*k) + " " + std::to_string(cy - ry) +
                " " + std::to_string(cx) + " " + std::to_string(cy - ry) + " Z";
            break;
        }

        case ShapeType::Line:
            path = "M " + std::to_string(start_point_.x) + " " + std::to_string(start_point_.y) +
                " L " + std::to_string(current_point_.x) + " " + std::to_string(current_point_.y);
            break;

        default:
            return;
    }

    if (has_fill_ && shape_type_ != ShapeType::Line) {
        renderer.fill_path(path, flex::Paint::solid({fill_color_.r, fill_color_.g, fill_color_.b, 0.5f}));
    }
    renderer.stroke_path(path, flex::Paint::solid({0.0f, 0.6f, 1.0f, 1.0f}), vm_->camera().screenToWorldScale());
}

// LineTool constructor
LineTool::LineTool() : DrawTool(ShapeType::Line) {
    clearFill();
}

} // namespace editor
