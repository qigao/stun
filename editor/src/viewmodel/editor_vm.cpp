/*
 * Editor ViewModel Implementation
 */

#include <editor/viewmodel/editor_vm.h>
#include <editor/command/boolean_commands.h>
#include <editor/tool/select_tool.h>
#include <editor/tool/pan_tool.h>
#include <editor/tool/draw_tool.h>
#include <editor/tool/pen_tool.h>
#include <editor/tool/text_tool.h>

namespace editor {

EditorViewModel::EditorViewModel() {
    document_ = Document::create();
    setupDefaultTools();
    shortcuts_.setViewModel(this);
    shortcuts_.setupDefaults();
}

void EditorViewModel::setDocument(Document::Ptr doc) {
    document_ = doc;
    selection_.clear();
    notify(EventType::DocumentChanged, document_.get());
}

void EditorViewModel::executeCommand(CommandPtr cmd) {
    history_.execute(std::move(cmd));
    document_->setModified(true);
}

void EditorViewModel::setTool(const std::string& toolId) {
    tools_.setCurrentTool(toolId, this);
}

bool EditorViewModel::onMouseDown(const MouseEvent& e) {
    if (auto* tool = currentTool()) {
        return tool->onMouseDown(e);
    }
    return false;
}

bool EditorViewModel::onMouseUp(const MouseEvent& e) {
    if (auto* tool = currentTool()) {
        return tool->onMouseUp(e);
    }
    return false;
}

bool EditorViewModel::onMouseMove(const MouseEvent& e) {
    if (auto* tool = currentTool()) {
        return tool->onMouseMove(e);
    }
    return false;
}

bool EditorViewModel::onMouseDrag(const MouseEvent& e) {
    if (auto* tool = currentTool()) {
        return tool->onMouseDrag(e);
    }
    return false;
}

bool EditorViewModel::onKeyDown(const KeyEvent& e) {
    // Handle shortcuts via ShortcutManager
    if (shortcuts_.handleKey(e.key, e.ctrl, e.shift, e.alt)) {
        return true;
    }

    // Forward to current tool
    if (auto* tool = currentTool()) {
        return tool->onKeyDown(e);
    }
    return false;
}

void EditorViewModel::render(flex::Renderer& renderer) {
    // Render document
    document_->render(renderer);

    // Render tool overlay (gizmos, etc.)
    if (auto* tool = currentTool()) {
        tool->render(renderer);
    }
}

void EditorViewModel::addNode(EditorNode::Ptr node) {
    if (auto layer = document_->activeLayer()) {
        layer->addNode(node);
    }
}

void EditorViewModel::removeSelectedNodes() {
    for (auto& node : selection_.selection()) {
        if (auto* layer = node->layer()) {
            layer->removeNode(node);
        }
    }
    selection_.clear();
}

ShapeNode::Ptr EditorViewModel::createRect(float x, float y, float w, float h, const Color& fill) {
    auto shape = flex::Shape::create();
    shape->set_position(x, y);
    shape->set_rect(w, h);
    shape->set_fill(flex::Color{fill.r, fill.g, fill.b, fill.a});
    return ShapeNode::create(shape);
}

ShapeNode::Ptr EditorViewModel::createCircle(float x, float y, float radius, const Color& fill) {
    auto shape = flex::Shape::create();
    shape->set_position(x, y);
    shape->set_circle(radius);
    shape->set_fill(flex::Color{fill.r, fill.g, fill.b, fill.a});
    return ShapeNode::create(shape);
}

// Helper to get selected shapes
static std::vector<ShapeNode::Ptr> getSelectedShapes(SelectionManager& selection) {
    std::vector<ShapeNode::Ptr> shapes;
    for (auto& node : selection.selection()) {
        if (auto shapeNode = std::dynamic_pointer_cast<ShapeNode>(node)) {
            shapes.push_back(shapeNode);
        }
    }
    return shapes;
}

void EditorViewModel::booleanUnion() {
    auto shapes = getSelectedShapes(selection_);
    if (shapes.size() < 2) return;

    auto layer = document_->activeLayer();
    if (!layer) return;

    auto cmd = std::make_unique<BooleanCommand>(
        document_.get(), layer.get(), shapes, BooleanOp::Union);
    executeCommand(std::move(cmd));
    selection_.clear();
}

void EditorViewModel::booleanIntersect() {
    auto shapes = getSelectedShapes(selection_);
    if (shapes.size() < 2) return;

    auto layer = document_->activeLayer();
    if (!layer) return;

    auto cmd = std::make_unique<BooleanCommand>(
        document_.get(), layer.get(), shapes, BooleanOp::Intersect);
    executeCommand(std::move(cmd));
    selection_.clear();
}

void EditorViewModel::booleanSubtract() {
    auto shapes = getSelectedShapes(selection_);
    if (shapes.size() < 2) return;

    auto layer = document_->activeLayer();
    if (!layer) return;

    auto cmd = std::make_unique<BooleanCommand>(
        document_.get(), layer.get(), shapes, BooleanOp::Subtract);
    executeCommand(std::move(cmd));
    selection_.clear();
}

void EditorViewModel::booleanExclude() {
    auto shapes = getSelectedShapes(selection_);
    if (shapes.size() < 2) return;

    auto layer = document_->activeLayer();
    if (!layer) return;

    auto cmd = std::make_unique<BooleanCommand>(
        document_.get(), layer.get(), shapes, BooleanOp::Exclude);
    executeCommand(std::move(cmd));
    selection_.clear();
}

void EditorViewModel::setupDefaultTools() {
    tools_.registerTool("select", std::make_unique<SelectTool>());
    tools_.registerTool("pan", std::make_unique<PanTool>());
    tools_.registerTool("rectangle", std::make_unique<RectangleTool>());
    tools_.registerTool("ellipse", std::make_unique<EllipseTool>());
    tools_.registerTool("line", std::make_unique<LineTool>());
    tools_.registerTool("pen", std::make_unique<PenTool>());
    tools_.registerTool("text", std::make_unique<TextTool>());
    setTool("select");
}

} // namespace editor
