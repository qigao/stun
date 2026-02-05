/*
 * Meta Editor SDK - Core Editor Implementation
 */

#include "meta_editor/core/editor.h"
#include "meta_editor/svg_importer.h"
#include "meta_editor/tools/select_tool.h"
#include "meta_editor/tools/path_edit_tool.h"
#include "meta_editor/tools/text_edit_tool.h"
#include "meta_editor/tools/pen_tool.h"
#include "meta_editor/tools/shape_tool.h"
#include "meta_editor/tools/text_tool.h"
#include "meta_editor/tools/line_tool.h"
#include "meta_editor/tools/connector_tool.h"
#include "meta_editor/tools/freehand_tool.h"
#include "meta_editor/tools/sticky_note_tool.h"
#include "meta_editor/tools/hand_tool.h"
#include "meta_editor/tools/eraser_tool.h"
#include "meta_editor/tools/frame_tool.h"
#include "meta_editor/tools/image_tool.h"
#include "meta_editor/tools/laser_tool.h"

namespace meta_editor {

Editor::Editor(float width, float height)
    : width_(width), height_(height) {}

Editor::~Editor() = default;

void Editor::init() {
    canvas_ = std::make_unique<Canvas>(width_, height_);
    selection_ = std::make_unique<SelectionManager>(canvas_.get());
    command_manager_ = std::make_unique<CommandManager>();
    tool_manager_ = std::make_unique<ToolManager>(
        canvas_.get(), selection_.get(), command_manager_.get());
    connector_manager_ = std::make_unique<ConnectorManager>(canvas_.get());
    exporter_ = std::make_unique<Exporter>(canvas_.get());
    serializer_ = std::make_unique<Serializer>(canvas_.get());
    svg_importer_ = std::make_unique<SvgImporter>(canvas_.get());

    setup_default_tools();
    canvas_->create_layer("Layer 1");
}

void Editor::shutdown() {
    tool_manager_.reset();
    connector_manager_.reset();
    command_manager_.reset();
    selection_.reset();
    canvas_.reset();
}

void Editor::setup_default_tools() {
    tool_manager_->register_tool(std::make_unique<SelectTool>());
    tool_manager_->register_tool(std::make_unique<PathEditTool>());
    tool_manager_->register_tool(std::make_unique<TextEditTool>());
    tool_manager_->register_tool(std::make_unique<PenTool>());
    tool_manager_->register_tool(std::make_unique<TextTool>());
    tool_manager_->register_tool(std::make_unique<LineTool>());
    tool_manager_->register_tool(std::make_unique<FreehandTool>());
    tool_manager_->register_tool(std::make_unique<StickyNoteTool>());
    tool_manager_->register_tool(std::make_unique<HandTool>());
    tool_manager_->register_tool(std::make_unique<EraserTool>());
    tool_manager_->register_tool(std::make_unique<FrameTool>());
    tool_manager_->register_tool(std::make_unique<ImageTool>());
    tool_manager_->register_tool(std::make_unique<LaserTool>());
    
    auto connector_tool = std::make_unique<ConnectorTool>();
    connector_tool->set_connector_manager(connector_manager_.get());
    tool_manager_->register_tool(std::move(connector_tool));
    
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Rectangle));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Circle));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Ellipse));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Star));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Polygon));
    tool_manager_->set_active_tool("Select");
}

void Editor::update(float dt) {
    canvas_->update(dt);
    tool_manager_->update(dt);
    connector_manager_->update_all();
}

void Editor::render(flex::Renderer& renderer) {
    canvas_->render(renderer);
}

void Editor::render_tool_overlay(flex::Renderer& renderer) {
    // Tool overlay draws in WORLD coordinates (shape preview, pen path, etc.)
    // Apply camera transform so world coordinates render correctly
    renderer.save();
    renderer.set_transform(canvas_->camera_transform());
    tool_manager_->render_overlay(renderer);
    renderer.restore();
}

void Editor::set_viewport(float width, float height) {
    width_ = width;
    height_ = height;
}

void Editor::notify_change() {
    if (change_callback_) {
        change_callback_();
    }
}

// ============================================================================
// Event Handling
// ============================================================================

bool Editor::handle_event(const EditorEvent& event) {
    switch (event.type) {
        case EditorEvent::Type::PointerDown:
            return handle_pointer_down(event);
        case EditorEvent::Type::PointerMove:
            return handle_pointer_move(event);
        case EditorEvent::Type::PointerUp:
            return handle_pointer_up(event);
        case EditorEvent::Type::KeyDown:
            return handle_key_down(event);
        case EditorEvent::Type::KeyUp:
            return handle_key_up(event);
        case EditorEvent::Type::TextInput:
            return tool_manager_->on_text_input(event.text);
        default:
            return false;
    }
}

bool Editor::handle_pointer_down(const EditorEvent& event) {
    flex::Vec2 screen_pos(event.x, event.y);
    flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

    if (event.button == MouseButton::Right) {
        if (context_menu_callback_) {
            context_menu_callback_(event.x, event.y);
        }
        return true;
    }

    if (event.button == MouseButton::Left) {
        tool_manager_->on_key_down(0, event.mods);
        bool handled = tool_manager_->on_pointer_down(screen_pos, world_pos);
        if (handled) notify_change();
        return handled;
    }
    return false;
}

bool Editor::handle_pointer_move(const EditorEvent& event) {
    flex::Vec2 screen_pos(event.x, event.y);
    flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

    bool handled = tool_manager_->on_pointer_move(screen_pos, world_pos);
    if (handled) notify_change();
    return handled;
}

bool Editor::handle_pointer_up(const EditorEvent& event) {
    flex::Vec2 screen_pos(event.x, event.y);
    flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

    if (event.button == MouseButton::Left) {
        bool handled = tool_manager_->on_pointer_up(screen_pos, world_pos);
        if (handled) notify_change();
        return handled;
    }
    return false;
}

bool Editor::handle_key_down(const EditorEvent& event) {
    int key = event.key;
    uint16_t mods = event.mods;

    // Ctrl shortcuts
    if (event.has_ctrl()) {
        switch (key) {
            case 'z': case 'Z':
                command_manager_->undo();
                notify_change();
                return true;
            case 'y': case 'Y':
                command_manager_->redo();
                notify_change();
                return true;
            case 'a': case 'A':
                select_all();
                return true;
            case 'c': case 'C':
                if (!event.has_shift()) {
                    copy_selection();
                    return true;
                }
                break;
            case 'v': case 'V':
                paste();
                notify_change();
                return true;
            case 'd': case 'D':
                duplicate_selection();
                notify_change();
                return true;
            case 'g': case 'G':
                if (event.has_shift()) {
                    ungroup_selection();
                } else {
                    group_selection();
                }
                notify_change();
                return true;
            case '0':
                canvas_->reset_camera();
                notify_change();
                return true;
            case '=': case '+':
                canvas_->zoom(0.1f);
                notify_change();
                return true;
            case '-':
                canvas_->zoom(-0.1f);
                notify_change();
                return true;
        }
    }

    // Zoom without Ctrl (+ / -)
    if (!event.has_ctrl() && !event.has_alt()) {
        if (key == '=' || key == '+') {
            canvas_->zoom(0.1f);
            notify_change();
            return true;
        }
        if (key == '-') {
            canvas_->zoom(-0.1f);
            notify_change();
            return true;
        }
    }

    // Delete
    if (key == 127 || key == 8) {  // Delete or Backspace
        delete_selection();
        notify_change();
        return true;
    }

    // Escape
    if (key == 27) {
        selection_->clear_selection();
        notify_change();
        return true;
    }

    // Space - temporary switch to Hand tool
    if (key == 32 && !space_held_) {
        space_held_ = true;
        previous_tool_ = tool_manager_->active_tool() ? tool_manager_->active_tool()->name() : "Select";
        tool_manager_->set_active_tool("Hand");
        return true;
    }

    // Tool shortcuts (without modifiers)
    if (!event.has_ctrl() && !event.has_alt()) {
        switch (key) {
            case 'v': case 'V':
                tool_manager_->set_active_tool("Select");
                return true;
            case 'p': case 'P':
                tool_manager_->set_active_tool("Pen");
                return true;
            case 'l': case 'L':
                tool_manager_->set_active_tool("Line");
                return true;
            case 'c': case 'C':
                tool_manager_->set_active_tool("Connector");
                return true;
            case 'd': case 'D':
                tool_manager_->set_active_tool("Freehand");
                return true;
            case 'r': case 'R':
                tool_manager_->set_active_tool("Rectangle");
                return true;
            case 'o': case 'O':
                tool_manager_->set_active_tool("Circle");
                return true;
            case 'e': case 'E':
                tool_manager_->set_active_tool("Ellipse");
                return true;
            case 't': case 'T':
                tool_manager_->set_active_tool("Text");
                return true;
            case 's': case 'S':
                tool_manager_->set_active_tool("Star");
                return true;
            case 'n': case 'N':
                tool_manager_->set_active_tool("StickyNote");
                return true;
            case 'h': case 'H':
                tool_manager_->set_active_tool("Hand");
                return true;
            case 'x': case 'X':
                tool_manager_->set_active_tool("Eraser");
                return true;
            case 'f': case 'F':
                tool_manager_->set_active_tool("Frame");
                return true;
            case 'i': case 'I':
                tool_manager_->set_active_tool("Image");
                return true;
            case 'z': case 'Z':
                tool_manager_->set_active_tool("Laser");
                return true;
            case 'g': case 'G':
                canvas_->set_snap_to_grid(!canvas_->is_snap_to_grid());
                return true;
            case '/': case '?':
                if (shortcut_callback_) shortcut_callback_();
                return true;
        }
    }

    return tool_manager_->on_key_down(key, mods);
}

bool Editor::handle_key_up(const EditorEvent& event) {
    // Space released - restore previous tool
    if (event.key == 32 && space_held_) {
        space_held_ = false;
        if (!previous_tool_.empty()) {
            tool_manager_->set_active_tool(previous_tool_);
        }
        return true;
    }
    return tool_manager_->on_key_up(event.key, event.mods);
}

// ============================================================================
// Serialization
// ============================================================================

bool Editor::save_project(const std::string& path) {
    return serializer_ && serializer_->save(path);
}

bool Editor::load_project(const std::string& path) {
    bool ok = serializer_ && serializer_->load(path);
    if (ok) notify_change();
    return ok;
}

std::string Editor::to_json() const {
    return serializer_ ? serializer_->to_json() : "";
}

bool Editor::from_json(const std::string& json) {
    bool ok = serializer_ && serializer_->from_json(json);
    if (ok && change_callback_) change_callback_();
    return ok;
}

// ============================================================================
// Export
// ============================================================================

bool Editor::save_svg(const std::string& path) {
    return exporter_ && exporter_->save_svg(path);
}

bool Editor::save_png(const std::string& path, const uint32_t* buffer, int w, int h) {
    return exporter_ && exporter_->save_png(path, buffer, w, h);
}

std::string Editor::selection_to_svg() {
    if (!exporter_ || !selection_->has_selection()) return "";
    return exporter_->selection_to_svg(selection_->selection());
}

// ============================================================================
// Import
// ============================================================================

bool Editor::import_svg(const std::string& path) {
    if (!svg_importer_) return false;
    bool ok = svg_importer_->import_file(path);
    if (ok) notify_change();
    return ok;
}

bool Editor::import_svg_string(const std::string& svg_content) {
    if (!svg_importer_) return false;
    bool ok = svg_importer_->import_string(svg_content);
    if (ok) notify_change();
    return ok;
}

// ============================================================================
// Selection Operations
// ============================================================================

void Editor::select_all() {
    selection_->select_all();
    notify_change();
}

void Editor::delete_selection() {
    if (!selection_->has_selection()) return;

    for (auto* node : selection_->selection()) {
        if (node->parent()) {
            static_cast<flex::Group*>(node->parent())->remove_child(node);
        }
    }
    selection_->clear_selection();
}

void Editor::copy_selection() {
    if (!selection_->has_selection()) return;

    clipboard_.clear();
    for (auto* node : selection_->selection()) {
        if (node->type() != flex::NodeType::Shape) continue;

        auto* shape = static_cast<flex::Shape*>(node);
        ClipboardShape cs;
        cs.geometry_type = shape->geometry_type();
        cs.x = shape->x();
        cs.y = shape->y();
        auto bounds = shape->bounds();
        cs.width = bounds.width;
        cs.height = bounds.height;
        cs.fill_color = shape->has_fill() ? shape->fill().color : flex::Color{0,0,0,0};
        cs.stroke_color = shape->has_stroke() ? shape->stroke().color : flex::Color{0,0,0,1};
        cs.stroke_width = shape->has_stroke() ? shape->stroke().width : 2.0f;

        switch (cs.geometry_type) {
            case flex::GeometryType::Polygon:
                cs.sides = shape->polygon().sides;
                cs.radius = shape->polygon().radius;
                break;
            case flex::GeometryType::Star:
                cs.points = shape->star().points;
                cs.radius = shape->star().outer_radius;
                cs.inner_radius = shape->star().inner_radius;
                break;
            case flex::GeometryType::Circle:
                cs.radius = shape->circle().radius;
                break;
            case flex::GeometryType::Path:
                cs.path_data = shape->path().d;
                break;
            default:
                break;
        }
        clipboard_.push_back(cs);
    }
}

void Editor::paste() {
    if (clipboard_.empty()) return;

    auto layers = canvas_->get_all_layers();
    if (layers.empty()) return;

    auto* layer = layers[0];
    auto* allocator = canvas_->instance()->object_allocator();

    selection_->clear_selection();
    for (const auto& cs : clipboard_) {
        auto* shape = flex::Shape::create(*allocator);
        shape->set_position(cs.x + 20, cs.y + 20);

        switch (cs.geometry_type) {
            case flex::GeometryType::Rect:
                shape->set_rect(cs.width, cs.height, 0);
                break;
            case flex::GeometryType::Circle:
                shape->set_circle(cs.radius);
                break;
            case flex::GeometryType::Ellipse:
                shape->set_ellipse(cs.width / 2, cs.height / 2);
                break;
            case flex::GeometryType::Polygon:
                shape->set_polygon(cs.sides, cs.radius);
                break;
            case flex::GeometryType::Star:
                shape->set_star(cs.points, cs.radius, cs.inner_radius);
                break;
            case flex::GeometryType::Path:
                shape->set_path(cs.path_data);
                break;
            default:
                shape->set_rect(cs.width, cs.height, 0);
                break;
        }

        if (cs.fill_color.a > 0) shape->set_fill(cs.fill_color);
        shape->set_stroke(cs.stroke_color, cs.stroke_width);
        layer->add_child(shape);
        selection_->add_to_selection(shape);
    }
}

void Editor::duplicate_selection() {
    copy_selection();
    paste();
}

void Editor::group_selection() {
    if (!selection_->has_selection() || selection_->selection().size() < 2) return;

    auto layers = canvas_->get_all_layers();
    if (layers.empty()) return;

    auto* layer = layers[0];
    auto* allocator = canvas_->instance()->object_allocator();
    auto* group = flex::Group::create(*allocator);

    auto bounds = selection_->selection_bounds();
    std::vector<flex::Node*> nodes_to_group(
        selection_->selection().begin(), selection_->selection().end());

    for (auto* node : nodes_to_group) {
        float old_x = node->x();
        float old_y = node->y();
        if (node->parent()) {
            static_cast<flex::Group*>(node->parent())->remove_child(node);
        }
        node->set_position(old_x - bounds.x, old_y - bounds.y);
        group->add_child(node);
    }

    group->set_position(bounds.x, bounds.y);
    layer->add_child(group);

    selection_->clear_selection();
    selection_->select(group);
}

void Editor::ungroup_selection() {
    if (!selection_->has_selection() || selection_->selection().size() != 1) return;

    auto* node = selection_->selection()[0];
    if (node->type() != flex::NodeType::Group) return;

    auto* group = static_cast<flex::Group*>(node);
    auto* parent = group->parent();
    if (!parent) return;

    auto* parent_group = static_cast<flex::Group*>(parent);
    float gx = group->x();
    float gy = group->y();

    std::vector<flex::Node*> children_copy;
    for (auto* child : group->children()) {
        children_copy.push_back(child);
    }

    selection_->clear_selection();
    for (auto* child : children_copy) {
        group->remove_child(child);
        child->set_position(child->x() + gx, child->y() + gy);
        parent_group->add_child(child);
        selection_->add_to_selection(child);
    }

    parent_group->remove_child(group);
}

void Editor::lock_selection() {
    selection_->lock_selection();
    notify_change();
}

void Editor::unlock_selection() {
    selection_->unlock_selection();
    notify_change();
}

} // namespace meta_editor
