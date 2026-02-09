/*
 * Meta Editor SDK - Core Editor Implementation
 */

#include "meta_editor/core/editor.h"
#include "meta_editor/page.h"
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
#include <cstdio>
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
    page_manager_ = std::make_unique<PageManager>(width_, height_);
    
    // Create first page
    Page* first_page = page_manager_->create_page("Page 1");

    // Initialize tool manager with the first page
    tool_manager_ = std::make_unique<ToolManager>(
        first_page->canvas(), first_page->selection(), first_page->command_manager());

    setup_default_tools();
}

void Editor::shutdown() {
    tool_manager_.reset();
    page_manager_.reset();
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
    connector_tool->set_connector_manager(connectors());
    tool_manager_->register_tool(std::move(connector_tool));
    
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Rectangle));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Circle));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Ellipse));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Star));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Polygon));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Triangle));
    tool_manager_->set_active_tool("Select");
}

Canvas* Editor::canvas() {
    return active_page() ? active_page()->canvas() : nullptr;
}

SelectionManager* Editor::selection() {
    return active_page() ? active_page()->selection() : nullptr;
}

CommandManager* Editor::command_manager() {
    return active_page() ? active_page()->command_manager() : nullptr;
}

ConnectorManager* Editor::connectors() {
    return active_page() ? active_page()->connector_manager() : nullptr;
}

void Editor::set_active_page(int index) {
    if (!page_manager_) return;
    
    page_manager_->set_active_page(index);
    Page* page = active_page();
    if (page) {
        tool_manager_->set_target(page->canvas(), page->selection(), page->command_manager());
        
        // Update connector tool if it's active or registered
        auto* conn_tool = dynamic_cast<ConnectorTool*>(tool_manager_->get_tool("Connector"));
        if (conn_tool) {
            conn_tool->set_connector_manager(page->connector_manager());
        }
    }
    notify_change();
}

void Editor::update(float dt) {
    if (page_manager_) page_manager_->update(dt);
    if (tool_manager_) tool_manager_->update(dt);
}

void Editor::render(flex::Renderer& renderer) {
    if (active_page()) active_page()->render(renderer);
}

void Editor::render_tool_overlay(flex::Renderer& renderer) {
    Page* page = active_page();
    if (!page) return;

    renderer.save();
    renderer.translate(page->canvas()->camera_pan_x(), page->canvas()->camera_pan_y());
    renderer.scale(page->canvas()->camera_zoom(), page->canvas()->camera_zoom());
    tool_manager_->render_overlay(renderer);
    renderer.restore();

    tool_manager_->render_screen_overlay(renderer);
}

void Editor::set_viewport(float width, float height) {
    width_ = width;
    height_ = height;
    if (active_page()) active_page()->set_size(width, height);
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
    if (!active_page()) return false;

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
    Page* page = active_page();
    flex::Vec2 screen_pos(event.x, event.y);
    flex::Vec2 world_pos = page->canvas()->screen_to_world(screen_pos);

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
    Page* page = active_page();
    flex::Vec2 screen_pos(event.x, event.y);
    flex::Vec2 world_pos = page->canvas()->screen_to_world(screen_pos);

    bool handled = tool_manager_->on_pointer_move(screen_pos, world_pos);
    if (handled) notify_change();
    return handled;
}

bool Editor::handle_pointer_up(const EditorEvent& event) {
    Page* page = active_page();
    flex::Vec2 screen_pos(event.x, event.y);
    flex::Vec2 world_pos = page->canvas()->screen_to_world(screen_pos);

    if (event.button == MouseButton::Left) {
        bool handled = tool_manager_->on_pointer_up(screen_pos, world_pos);
        if (handled) notify_change();
        return handled;
    }
    return false;
}

bool Editor::handle_key_down(const EditorEvent& event) {
    Page* page = active_page();
    int key = event.key;
    uint16_t mods = event.mods;

    // Ctrl shortcuts
    if (event.has_ctrl()) {
        switch (key) {
            case 'z': case 'Z':
                page->command_manager()->undo();
                notify_change();
                return true;
            case 'y': case 'Y':
                page->command_manager()->redo();
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
                page->canvas()->reset_camera();
                notify_change();
                return true;
            case '=': case '+':
                page->canvas()->zoom(0.1f);
                notify_change();
                return true;
            case '-':
                page->canvas()->zoom(-0.1f);
                notify_change();
                return true;
        }
    }

    // Zoom without Ctrl (+ / -)
    if (!event.has_ctrl() && !event.has_alt()) {
        if (key == '=' || key == '+') {
            page->canvas()->zoom(0.1f);
            notify_change();
            return true;
        }
        if (key == '-') {
            page->canvas()->zoom(-0.1f);
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
        page->selection()->clear_selection();
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
            case 'g': case 'G':
                tool_manager_->set_active_tool("Polygon");
                return true;
            case 'w': case 'W':
                tool_manager_->set_active_tool("Triangle");
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
            case 'k': case 'K':
                page->canvas()->set_snap_to_grid(!page->canvas()->is_snap_to_grid());
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
    Page* page = active_page();
    return page && page->serializer()->save(path);
}

bool Editor::load_project(const std::string& path) {
    Page* page = active_page();
    bool ok = page && page->serializer()->load(path);
    if (ok) notify_change();
    return ok;
}

std::string Editor::to_json() const {
    Page* page = const_cast<Editor*>(this)->active_page();
    return page ? page->serializer()->to_json() : "";
}

bool Editor::from_json(const std::string& json) {
    Page* page = active_page();
    bool ok = page && page->serializer()->from_json(json);
    if (ok) notify_change();
    return ok;
}

// ============================================================================
// Export
// ============================================================================

bool Editor::save_svg(const std::string& path) {
    Page* page = active_page();
    return page && page->exporter()->save_svg(path);
}

bool Editor::save_png(const std::string& path, const uint32_t* buffer, int w, int h) {
    Page* page = active_page();
    return page && page->exporter()->save_png(path, buffer, w, h);
}

std::string Editor::selection_to_svg() {
    Page* page = active_page();
    if (!page || !page->selection()->has_selection()) return "";
    return page->exporter()->selection_to_svg(page->selection()->selection());
}

// ============================================================================
// Import
// ============================================================================

bool Editor::import_svg(const std::string& path) {
    Page* page = active_page();
    if (!page) return false;
    bool ok = page->svg_importer()->import_file(path);
    if (ok) notify_change();
    return ok;
}

bool Editor::import_svg_string(const std::string& svg_content) {
    Page* page = active_page();
    if (!page) return false;
    bool ok = page->svg_importer()->import_string(svg_content);
    if (ok) notify_change();
    return ok;
}

// ============================================================================
// Selection Operations
// ============================================================================

void Editor::select_all() {
    Page* page = active_page();
    if (page) {
        page->selection()->select_all();
        notify_change();
    }
}

void Editor::delete_selection() {
    Page* page = active_page();
    if (!page || !page->selection()->has_selection()) return;

    for (auto* node : page->selection()->selection()) {
        if (node->parent()) {
            static_cast<flex::Group*>(node->parent())->remove_child(node);
        }
    }
    page->selection()->clear_selection();
}

void Editor::copy_selection() {
    Page* page = active_page();
    if (!page || !page->selection()->has_selection()) return;

    clipboard_.clear();
    for (auto* node : page->selection()->selection()) {
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
    Page* page = active_page();
    if (!page || clipboard_.empty()) return;

    auto layers = page->canvas()->get_all_layers();
    if (layers.empty()) return;

    auto* layer = layers[0];
    auto* allocator = page->canvas()->instance()->object_allocator();

    page->selection()->clear_selection();
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
        page->selection()->add_to_selection(shape);
    }
}

void Editor::duplicate_selection() {
    copy_selection();
    paste();
}

void Editor::group_selection() {
    Page* page = active_page();
    if (!page || !page->selection()->has_selection() || page->selection()->selection().size() < 2) return;

    auto layers = page->canvas()->get_all_layers();
    if (layers.empty()) return;

    auto* layer = layers[0];
    auto* allocator = page->canvas()->instance()->object_allocator();
    auto* group = flex::Group::create(*allocator);

    auto bounds = page->selection()->selection_bounds();
    std::vector<flex::Node*> nodes_to_group(
        page->selection()->selection().begin(), page->selection()->selection().end());

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

    page->selection()->clear_selection();
    page->selection()->select(group);
}

void Editor::ungroup_selection() {
    Page* page = active_page();
    if (!page || !page->selection()->has_selection() || page->selection()->selection().size() != 1) return;

    auto* node = page->selection()->selection()[0];
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

    page->selection()->clear_selection();
    for (auto* child : children_copy) {
        group->remove_child(child);
        child->set_position(child->x() + gx, child->y() + gy);
        parent_group->add_child(child);
        page->selection()->add_to_selection(child);
    }

    parent_group->remove_child(group);
}

void Editor::lock_selection() {
    Page* page = active_page();
    if (page) {
        page->selection()->lock_selection();
        notify_change();
    }
}

void Editor::unlock_selection() {
    Page* page = active_page();
    if (page) {
        page->selection()->unlock_selection();
        notify_change();
    }
}

} // namespace meta_editor
