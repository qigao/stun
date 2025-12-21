/*
 * Flex Engine - Main Implementation
 *
 * Definition loading and Instance management.
 *
 * SIMPLIFIED: Parser directly builds Runtime objects (no AST/Builder)
 */

#include "flex/flex.h"
#include "flex/script.h"
#include "flex/component.h"
#include <thorvg.h>
#include <fstream>
#include <sstream>
#include <set>
#include <vector>
#include <algorithm>
#include <cstring>

// Note: Lemon parser removed - using hand-written recursive descent parser now

namespace flex {

// ============================================================================
// Definition Implementation (stores Runtime objects)
// ============================================================================

struct Definition::Impl {
    Artboard::Ptr artboard;
    std::vector<Timeline::Ptr> timelines;
    Machine::Ptr machine;

    std::string error_message;
    int error_line = 0;
    int error_column = 0;
    bool has_error = false;
};

Definition::Definition() : impl_(std::make_unique<Impl>()) {}
Definition::~Definition() = default;

Definition::Ptr Definition::load(const char* source) {
    auto def = std::shared_ptr<Definition>(new Definition());

    // Parse source - Parser directly builds Runtime objects
    try {
        std::vector<Timeline::Ptr> timelines;
        Machine::Ptr machine;

        auto artboard = parser::parse(source, &timelines, &machine);

        if (!artboard) {
            def->impl_->has_error = true;
            def->impl_->error_message = parser::get_error();
            def->impl_->error_line = parser::get_error_line();
            def->impl_->error_column = parser::get_error_column();
            return def;
        }

        def->impl_->artboard = artboard;
        def->impl_->timelines = std::move(timelines);
        def->impl_->machine = machine;

    } catch (const std::exception& e) {
        def->impl_->has_error = true;
        def->impl_->error_message = std::string("Parse exception: ") + e.what();
        return def;
    } catch (...) {
        def->impl_->has_error = true;
        def->impl_->error_message = "Unknown parse exception";
        return def;
    }
    return def;
}

Definition::Ptr Definition::load_file(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        auto def = std::shared_ptr<Definition>(new Definition());
        def->impl_->has_error = true;
        def->impl_->error_message = "Failed to open file: " + std::string(path);
        return def;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    return load(content.c_str());
}

bool Definition::has_error() const { return impl_->has_error; }
const char* Definition::error_message() const { return impl_->error_message.c_str(); }
int Definition::error_line() const { return impl_->error_line; }
int Definition::error_column() const { return impl_->error_column; }

Artboard::Ptr Definition::artboard() const { return impl_->artboard; }
const std::vector<Timeline::Ptr>& Definition::timelines() const { return impl_->timelines; }
Machine::Ptr Definition::machine() const { return impl_->machine; }

// ============================================================================
// Instance Implementation
// ============================================================================

struct Instance::Impl {
    Definition::Ptr definition;
    Artboard::Ptr artboard;
    float time = 0;

    // Input values
    std::unordered_map<std::string, float> float_inputs;
    std::unordered_map<std::string, std::string> string_inputs;
    std::unordered_map<std::string, std::string> assets;

    // Data bindings
    std::unique_ptr<BindingContext> bindings;

    // Script context for expression evaluation
    std::unique_ptr<ScriptContext> script_ctx;

    // Phase 2: Animation and State Machine
    AnimationController animation_controller;
    Machine::Ptr machine;
    std::set<std::string> fired_events;

    // Pointer state for event handling
    std::weak_ptr<Node> hover_node;
    std::weak_ptr<Node> pointer_down_node;
    bool is_pointer_down = false;
};

Instance::Instance() : impl_(std::make_unique<Impl>()) {}
Instance::~Instance() = default;

Instance::Ptr Instance::create(Definition::Ptr definition) {
    auto instance = std::shared_ptr<Instance>(new Instance());
    instance->impl_->definition = definition;

    if (definition && definition->artboard()) {
        // No Builder - Definition already has Runtime objects
        instance->impl_->artboard = definition->artboard();
        instance->impl_->machine = definition->machine();

        // Initialize bindings context
        instance->impl_->bindings = std::make_unique<BindingContext>();

        // TODO: Extract inputs and assets from parsed Definition
        // (These will come from DSL later)

        if (instance->impl_->machine) {
            instance->impl_->machine->init();
            for (const auto& layer : instance->impl_->machine->layers()) {
                layer->on_state_change([inst = instance.get(), lyr = layer.get()](const std::string& from, const std::string& to) {
                    (void)from;
                    auto* state = lyr->get_state(to);
                    if (state && !state->animation().empty()) {
                        inst->play(state->animation());
                    }
                });
            }
        }

        // Initialize Animations - Timelines are already built by Parser
        for (const auto& tl : definition->timelines()) {
            instance->impl_->animation_controller.add_timeline(tl);
        }

        // Initialize script context for expression bindings
        instance->impl_->script_ctx = std::make_unique<ScriptContext>();
        script::bind_math(instance->impl_->script_ctx.get());

        // Connect script context to bindings
        if (instance->impl_->bindings) {
            instance->impl_->bindings->set_script_context(instance->impl_->script_ctx.get());
            instance->impl_->bindings->evaluate();
        }

        // Auto-play timelines set to Loop
        // (Optional: logic to auto-play)
    }

    return instance;
}

Instance::Ptr Instance::create(float width, float height) {
    auto instance = std::shared_ptr<Instance>(new Instance());
    instance->impl_->artboard = Artboard::create(width, height);

    // Initialize script context for expression bindings
    instance->impl_->script_ctx = std::make_unique<ScriptContext>();
    script::bind_math(instance->impl_->script_ctx.get());

    return instance;
}

Artboard* Instance::artboard() const {
    return impl_->artboard.get();
}

void Instance::set_input(const std::string& name, float value) {
    impl_->float_inputs[name] = value;
    if (impl_->bindings) {
        impl_->bindings->set_input(name, value);
    }
}

void Instance::set_input(const std::string& name, const std::string& value) {
    impl_->string_inputs[name] = value;
    if (impl_->bindings) {
        impl_->bindings->set_input(name, value);
    }
}

void Instance::advance(float dt) {
    impl_->time += dt;

    // Update bindings time and evaluate
    if (impl_->bindings) {
        impl_->bindings->advance_time(dt);
        impl_->bindings->evaluate();
    }

    // Update state machine
    if (impl_->machine) {
        impl_->machine->update(dt,
            [this](const std::string& name) { return get_input(name); },
            [this](const std::string& name) {
                return impl_->fired_events.count(name) > 0;
            },
            [this](const std::string& name) {
                return !impl_->animation_controller.is_playing(name);
            });
    }

    // Update animations
    impl_->animation_controller.advance(dt);

    // Clear fired events
    impl_->fired_events.clear();
}

void Instance::render(Renderer& renderer) {
    if (impl_->artboard) {
        impl_->artboard->render(renderer);
    }
}

// Helper: recursive hit test on scene graph (back-to-front, returns topmost hit)
// x, y are in the coordinate space of node's parent
static Node* hit_test_recursive(Node* node, float x, float y) {
    if (!node || !node->visible()) return nullptr;

    // First check if point is within this node's bounds (in parent's coordinate space)
    Bounds b = node->bounds();
    bool inside = b.contains(x, y);

    // For groups, check children first (they may be on top of this node's bounds)
    if (node->is_group()) {
        auto* group = static_cast<Group*>(node);
        // Transform to this node's local coordinate space for children
        float local_x = x - node->x();
        float local_y = y - node->y();

        const auto& children = group->children();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            Node* hit = hit_test_recursive(it->get(), local_x, local_y);
            if (hit) return hit;
        }
    }

    // If point is inside this node, return it
    if (inside) {
        return node;
    }

    return nullptr;
}

// Helper: build path from root to target node
static void build_propagation_path(Node* target, std::vector<Node*>& path) {
    path.clear();
    for (Node* n = target; n != nullptr; n = n->parent()) {
        path.push_back(n);
    }
    // path is now [target, parent, grandparent, ..., root]
    // Reverse to get [root, ..., grandparent, parent, target]
    std::reverse(path.begin(), path.end());
}

// Helper: dispatch event through propagation path with bubbling
static void dispatch_with_bubbling(
    PointerEvent& event,
    const std::vector<Node*>& path,
    void (Node::*fire_method)(PointerEvent&)
) {
    if (path.empty()) return;

    // Target phase: last node in path
    Node* target = path.back();
    event.phase = EventPhase::Target;
    event.current_target = target;
    event.local_x = event.x - target->x();
    event.local_y = event.y - target->y();
    (target->*fire_method)(event);
    if (event.propagation_stopped()) return;

    // Bubble phase: from parent to root (reverse order, skip target)
    event.phase = EventPhase::Bubble;
    for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
        Node* node = path[i];
        event.current_target = node;
        event.local_x = event.x - node->x();
        event.local_y = event.y - node->y();
        (node->*fire_method)(event);
        if (event.propagation_stopped()) return;
    }
}

void Instance::send_pointer_event(float x, float y, bool is_down) {
    if (!impl_->artboard) return;

    // Find node at pointer position
    Node* hit_node = hit_test_recursive(impl_->artboard->root(), x, y);

    // Build propagation path for hit node
    std::vector<Node*> path;
    if (hit_node) {
        build_propagation_path(hit_node, path);
    }

    // Handle hover enter/leave (no bubbling for enter/leave)
    auto prev_hover = impl_->hover_node.lock();
    if (hit_node != prev_hover.get()) {
        // Leave old node
        if (prev_hover) {
            PointerEvent leave_event;
            leave_event.type = PointerEventType::Leave;
            leave_event.phase = EventPhase::Target;
            leave_event.x = x;
            leave_event.y = y;
            leave_event.target = prev_hover.get();
            leave_event.current_target = prev_hover.get();
            leave_event.local_x = x - prev_hover->x();
            leave_event.local_y = y - prev_hover->y();
            prev_hover->fire_hover_leave(leave_event);
        }

        // Enter new node
        if (hit_node) {
            PointerEvent enter_event;
            enter_event.type = PointerEventType::Enter;
            enter_event.phase = EventPhase::Target;
            enter_event.x = x;
            enter_event.y = y;
            enter_event.target = hit_node;
            enter_event.current_target = hit_node;
            enter_event.local_x = x - hit_node->x();
            enter_event.local_y = y - hit_node->y();
            hit_node->fire_hover_enter(enter_event);
            impl_->hover_node = hit_node->shared_from_this();
        } else {
            impl_->hover_node.reset();
        }
    }

    // Handle pointer down (with bubbling)
    if (is_down && !impl_->is_pointer_down) {
        impl_->is_pointer_down = true;
        if (hit_node) {
            impl_->pointer_down_node = hit_node->shared_from_this();
            PointerEvent event;
            event.type = PointerEventType::Down;
            event.x = x;
            event.y = y;
            event.target = hit_node;
            dispatch_with_bubbling(event, path, &Node::fire_pointer_down);
        } else {
            impl_->pointer_down_node.reset();
        }
    }
    // Handle pointer up (with bubbling)
    else if (!is_down && impl_->is_pointer_down) {
        impl_->is_pointer_down = false;

        auto down_node = impl_->pointer_down_node.lock();
        if (down_node) {
            // Build path for the original down node (not current hit)
            std::vector<Node*> down_path;
            build_propagation_path(down_node.get(), down_path);

            PointerEvent up_event;
            up_event.type = PointerEventType::Up;
            up_event.x = x;
            up_event.y = y;
            up_event.target = down_node.get();
            dispatch_with_bubbling(up_event, down_path, &Node::fire_pointer_up);

            // Fire click if up on same node as down (with bubbling)
            if (down_node.get() == hit_node) {
                // Click events bubble from target to root
                for (auto it = down_path.rbegin(); it != down_path.rend(); ++it) {
                    (*it)->fire_click();
                }
            }
        }

        impl_->pointer_down_node.reset();
    }
    // Handle pointer move (with bubbling)
    else if (hit_node) {
        PointerEvent event;
        event.type = PointerEventType::Move;
        event.x = x;
        event.y = y;
        event.target = hit_node;
        dispatch_with_bubbling(event, path, &Node::fire_pointer_move);
    }
}

void Instance::send_event(const std::string& name) {
    impl_->fired_events.insert(name);
}

// ============================================================================
// Animation Control (Phase 2)
// ============================================================================

AnimationController* Instance::animation_controller() const {
    return &impl_->animation_controller;
}

void Instance::add_timeline(Timeline::Ptr timeline) {
    impl_->animation_controller.add_timeline(timeline);
}

TimelinePlayer* Instance::play(const std::string& timeline_name, Node* target) {
    return impl_->animation_controller.play(timeline_name, target);
}

TimelinePlayer* Instance::play(const std::string& timeline_name) {
    return impl_->animation_controller.play(timeline_name, impl_->artboard->root());
}

void Instance::stop(const std::string& timeline_name) {
    impl_->animation_controller.stop(timeline_name);
}

void Instance::stop_all() {
    impl_->animation_controller.stop_all();
}

// ============================================================================
// State Machine Control (Phase 2)
// ============================================================================

void Instance::set_machine(Machine::Ptr machine) {
    impl_->machine = machine;
    if (impl_->machine) {
        impl_->machine->init();
    }
}

Machine* Instance::machine() const {
    return impl_->machine.get();
}

const std::string& Instance::current_state(const std::string& layer) const {
    static const std::string empty;
    if (impl_->machine) {
        return impl_->machine->current_state(layer);
    }
    return empty;
}

float Instance::get_input(const std::string& name) const {
    auto it = impl_->float_inputs.find(name);
    return (it != impl_->float_inputs.end()) ? it->second : 0.0f;
}

void Instance::register_asset(const std::string& name, const std::string& path) {
    impl_->assets[name] = path;
}

const std::string& Instance::resolve_asset(const std::string& name) const {
    static const std::string empty;
    auto it = impl_->assets.find(name);
    return (it != impl_->assets.end()) ? it->second : empty;
}

// ============================================================================
// Simple Recursive Descent Parser (builds Runtime objects directly)
// ============================================================================

namespace parser {

static std::string last_error;
static int last_error_line = 0;
static int last_error_column = 0;

// Simple tokenizer for basic parsing
class SimpleTokenizer {
public:
    SimpleTokenizer(const char* source) : source_(source), pos_(0), line_(1), col_(1) {}

    void skip_whitespace() {
        while (source_[pos_]) {
            // Skip whitespace
            if (source_[pos_] == ' ' || source_[pos_] == '\t' ||
                source_[pos_] == '\r' || source_[pos_] == '\n') {
                if (source_[pos_] == '\n') {
                    line_++;
                    col_ = 1;
                } else {
                    col_++;
                }
                pos_++;
            }
            // Skip // comments
            else if (source_[pos_] == '/' && source_[pos_ + 1] == '/') {
                // Skip to end of line
                while (source_[pos_] && source_[pos_] != '\n') {
                    pos_++;
                    col_++;
                }
            }
            else {
                break;
            }
        }
    }

    bool match(const char* keyword) {
        skip_whitespace();
        size_t len = strlen(keyword);
        if (strncmp(source_ + pos_, keyword, len) == 0) {
            // Check that it's not part of a longer identifier
            char next = source_[pos_ + len];
            if (next && (isalnum(next) || next == '_')) {
                return false;
            }
            pos_ += len;
            col_ += len;
            return true;
        }
        return false;
    }

    bool match_char(char c) {
        skip_whitespace();
        if (source_[pos_] == c) {
            pos_++;
            col_++;
            return true;
        }
        return false;
    }

    std::string read_identifier() {
        skip_whitespace();
        std::string result;
        while (source_[pos_] && (isalnum(source_[pos_]) || source_[pos_] == '_' || source_[pos_] == '.')) {
            result += source_[pos_];
            pos_++;
            col_++;
        }
        return result;
    }

    std::string read_string() {
        skip_whitespace();
        if (source_[pos_] != '"') return "";
        pos_++; col_++; // skip opening "

        std::string result;
        while (source_[pos_] && source_[pos_] != '"') {
            result += source_[pos_];
            pos_++;
            col_++;
        }

        if (source_[pos_] == '"') {
            pos_++; col_++; // skip closing "
        }
        return result;
    }

    // Parse hex color: #RGB, #RRGGBB, #RRGGBBAA
    Color read_color() {
        skip_whitespace();
        if (source_[pos_] != '#') return Color::Black;

        pos_++; col_++; // skip #
        std::string hex;
        while (source_[pos_] && isxdigit(source_[pos_]) && hex.length() < 8) {
            hex += source_[pos_];
            pos_++;
            col_++;
        }

        if (hex.length() == 3) {
            // #RGB -> #RRGGBB
            int r = std::stoi(std::string(1, hex[0]) + hex[0], nullptr, 16);
            int g = std::stoi(std::string(1, hex[1]) + hex[1], nullptr, 16);
            int b = std::stoi(std::string(1, hex[2]) + hex[2], nullptr, 16);
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        } else if (hex.length() == 6) {
            // #RRGGBB
            int r = std::stoi(hex.substr(0, 2), nullptr, 16);
            int g = std::stoi(hex.substr(2, 2), nullptr, 16);
            int b = std::stoi(hex.substr(4, 2), nullptr, 16);
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        } else if (hex.length() == 8) {
            // #RRGGBBAA
            int r = std::stoi(hex.substr(0, 2), nullptr, 16);
            int g = std::stoi(hex.substr(2, 2), nullptr, 16);
            int b = std::stoi(hex.substr(4, 2), nullptr, 16);
            int a = std::stoi(hex.substr(6, 2), nullptr, 16);
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        }

        return Color::Black;
    }

    // Rewind by one character (used for color parsing)
    void rewind() {
        if (pos_ > 0) {
            pos_--;
            col_--;
        }
    }

    float read_number() {
        skip_whitespace();
        char* end;
        float value = strtof(source_ + pos_, &end);
        if (end > source_ + pos_) {
            size_t len = end - (source_ + pos_);
            col_ += len;
            pos_ += len;

            // Check for time unit (s, ms)
            if (source_[pos_] == 's') {
                pos_++; col_++;
                // Check for 'ms'
                if (pos_ > 0 && source_[pos_-2] == 'm') {
                    value /= 1000.0f; // Convert ms to s
                }
            }
        }
        return value;
    }

    bool is_eof() {
        skip_whitespace();
        return source_[pos_] == '\0';
    }

    char peek() const {
        return source_[pos_];
    }

    int line() const { return line_; }
    int col() const { return col_; }

private:
    const char* source_;
    size_t pos_;
    int line_;
    int col_;
};

Artboard::Ptr parse(const char* source,
                    std::vector<Timeline::Ptr>* out_timelines,
                    Machine::Ptr* out_machine) {
    if (!source || source[0] == '\0') {
        last_error = "Empty source";
        last_error_line = 0;
        last_error_column = 0;
        return nullptr;
    }

    SimpleTokenizer tok(source);
    Artboard::Ptr artboard = nullptr;

    // Helper: Parse a single node recursively
    std::function<Node::Ptr(SimpleTokenizer&, const std::string&)> parse_node;
    parse_node = [&](SimpleTokenizer& t, const std::string& node_type) -> Node::Ptr {
        std::string node_name = t.read_identifier();

        if (!t.match_char('{')) {
            last_error = "Expected '{' after node name";
            last_error_line = t.line();
            return nullptr;
        }

        Node::Ptr node;
        float default_width = 100, default_height = 100;
        float node_width = default_width, node_height = default_height;
        float scale_x_val = 1.0f, scale_y_val = 1.0f;
        Color stroke_color = Color::Black;
        float stroke_width_val = 1.0f;
        bool has_stroke = false;

        // Create node based on type
        if (node_type == "group") {
            auto group = Group::create();
            node = group;
        } else if (node_type == "rect") {
            auto shape = Shape::create();
            shape->set_rect(default_width, default_height);
            node = shape;
        } else if (node_type == "circle") {
            auto shape = Shape::create();
            shape->set_circle(50);
            node = shape;
        } else if (node_type == "ellipse") {
            auto shape = Shape::create();
            shape->set_ellipse(50, 25);
            node = shape;
        } else if (node_type == "polygon") {
            auto shape = Shape::create();
            shape->set_polygon(5, 50); // 5 sides, radius 50
            node = shape;
        } else if (node_type == "star") {
            auto shape = Shape::create();
            shape->set_star(5, 50, 25); // 5 points, outer 50, inner 25
            node = shape;
        } else if (node_type == "text") {
            auto text_node = Text::create();
            text_node->set_content("Text");
            node = text_node;
        } else if (node_type == "image" || node_type == "img") {
            auto img = Image::create();
            node = img;
        } else if (node_type == "svg") {
            auto svg = Svg::create();
            node = svg;
        }

        if (!node) return nullptr;
        node->set_id(node_name);

        // Parse node properties
        while (!t.match_char('}')) {
            if (t.is_eof()) {
                last_error = "Unexpected EOF in node block";
                return nullptr;
            }

            std::string prop = t.read_identifier();

            // If empty identifier, skip unknown character and continue
            if (prop.empty()) {
                last_error = std::string("Unexpected character '") + t.peek() + "' in node block";
                last_error_line = t.line();
                return nullptr;
            }

            // Check for child nodes (recursive)
            if (prop == "rect" || prop == "circle" || prop == "ellipse" ||
                prop == "polygon" || prop == "star" || prop == "text" ||
                prop == "group" || prop == "image" || prop == "img" || prop == "svg") {

                auto child = parse_node(t, prop);
                if (child) {
                    // Add to group if this is a container
                    if (auto group = std::dynamic_pointer_cast<Group>(node)) {
                        group->add_child(child);
                    }
                }
                continue;
            }
            // Check for component instances as children
                if (ComponentRegistry::instance().has(prop)) {
                    std::string component_name = prop;
                    std::string component_id;

                    // Check for optional instance name
                    t.skip_whitespace();
                    if (t.peek() != '{') {
                        component_id = t.read_identifier();
                    }

                    if (!t.match_char('{')) {
                        last_error = "Expected '{' after component name";
                        last_error_line = t.line();
                        return nullptr;
                    }

                // Collect component props AND node properties
                Props component_props;
                float child_x = 0, child_y = 0;
                float child_opacity = 1.0f;
                float child_rotation = 0.0f;
                bool has_position = false;

                while (!t.match_char('}')) {
                    if (t.is_eof()) {
                        last_error = "Unexpected EOF in component block";
                        return nullptr;
                    }

                    std::string prop_name = t.read_identifier();

                    // Empty identifier means unexpected character or closing brace
                    if (prop_name.empty()) {
                        break;
                    }

                    t.match_char(':');

                    // Check if it's a node property (x, y, opacity, rotation)
                    if (prop_name == "x") {
                        child_x = t.read_number();
                        has_position = true;
                    } else if (prop_name == "y") {
                        child_y = t.read_number();
                        has_position = true;
                    } else if (prop_name == "opacity") {
                        child_opacity = t.read_number();
                    } else if (prop_name == "rotation") {
                        child_rotation = t.read_number();
                    } else {
                        // Component prop - read value
                        t.skip_whitespace();
                        char next = t.peek();

                        if (next == '"') {
                            std::string str_val = t.read_string();
                            component_props[prop_name] = str_val;
                        } else if (next == '#') {
                            Color color = t.read_color();
                            uint32_t argb = ((uint32_t)(color.a * 255) << 24) |
                                           ((uint32_t)(color.r * 255) << 16) |
                                           ((uint32_t)(color.g * 255) << 8) |
                                           ((uint32_t)(color.b * 255));
                            component_props[prop_name] = argb;
                        } else if (next == 't' || next == 'f') {
                            std::string bool_val = t.read_identifier();
                            component_props[prop_name] = (bool_val == "true");
                        } else {
                            float num_val = t.read_number();
                            component_props[prop_name] = num_val;
                        }
                    }

                    t.match_char(',');
                }

                // Create component instance
                auto child = create_component_instance(component_name, component_props);
                if (child) {
                    if (!component_id.empty()) {
                        child->set_id(component_id);
                    }

                    // Apply node properties
                    if (has_position) {
                        child->set_position(child_x, child_y);
                    }
                    if (child_opacity != 1.0f) {
                        child->set_opacity(child_opacity);
                    }
                    if (child_rotation != 0.0f) {
                        child->set_rotation(child_rotation);
                    }

                    // Add to group if this is a container
                    if (auto group = std::dynamic_pointer_cast<Group>(node)) {
                        group->add_child(child);
                    }
                }
                continue;
            }

            // Parse property value
            if (!t.match_char(':')) {
                last_error = std::string("Expected ':' after property '") + prop + "'";
                last_error_line = t.line();
                return nullptr;
            }

            if (prop == "x") {
                node->set_x(t.read_number());
            } else if (prop == "y") {
                node->set_y(t.read_number());
            } else if (prop == "opacity") {
                node->set_opacity(t.read_number());
            } else if (prop == "rotation") {
                node->set_rotation(t.read_number());
            } else if (prop == "scaleX") {
                scale_x_val = t.read_number();
                node->set_scale(scale_x_val, scale_y_val);
            } else if (prop == "scaleY") {
                scale_y_val = t.read_number();
                node->set_scale(scale_x_val, scale_y_val);
            } else if (prop == "width") {
                node_width = t.read_number();
                if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                    shape->set_rect(node_width, node_height);
                }
            } else if (prop == "height") {
                node_height = t.read_number();
                if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                    shape->set_rect(node_width, node_height);
                }
            } else if (prop == "radius") {
                float r = t.read_number();
                if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                    shape->set_circle(r);
                }
            } else if (prop == "rx") {
                float rx = t.read_number();
                if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                    shape->set_ellipse(rx, node_height / 2);
                }
            } else if (prop == "ry") {
                float ry = t.read_number();
                if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                    shape->set_ellipse(node_width / 2, ry);
                }
            } else if (prop == "sides") {
                int sides = (int)t.read_number();
                if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                    shape->set_polygon(sides, 50);
                }
            } else if (prop == "points") {
                int points = (int)t.read_number();
                if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                    shape->set_star(points, 50, 25);
                }
            } else if (prop == "color" || prop == "fill") {
                // Check if hex color or named color
                Color parsed_color = Color::Black;
                bool has_color = false;

                if (t.match_char('#')) {
                    // Rewind to parse color
                    t.rewind();
                    parsed_color = t.read_color();
                    has_color = true;
                } else {
                    std::string color_str = t.read_identifier();
                    if (color_str == "red") { parsed_color = Color::Red; has_color = true; }
                    else if (color_str == "green") { parsed_color = Color::Green; has_color = true; }
                    else if (color_str == "blue") { parsed_color = Color::Blue; has_color = true; }
                    else if (color_str == "white") { parsed_color = Color::White; has_color = true; }
                    else if (color_str == "black") { parsed_color = Color::Black; has_color = true; }
                    else if (color_str == "yellow") { parsed_color = Color::Yellow; has_color = true; }
                }

                if (has_color) {
                    // Apply to Shape (fill) or Text (color)
                    if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                        shape->set_fill(parsed_color);
                    } else if (auto text_node = std::dynamic_pointer_cast<Text>(node)) {
                        text_node->set_color(parsed_color);
                    }
                }
            } else if (prop == "stroke") {
                if (t.match_char('#')) {
                    t.rewind();
                    stroke_color = t.read_color();
                    has_stroke = true;
                } else {
                    std::string color_str = t.read_identifier();
                    if (color_str == "red") stroke_color = Color::Red;
                    else if (color_str == "green") stroke_color = Color::Green;
                    else if (color_str == "blue") stroke_color = Color::Blue;
                    else if (color_str == "white") stroke_color = Color::White;
                    has_stroke = true;
                }
            } else if (prop == "strokeWidth") {
                stroke_width_val = t.read_number();
                has_stroke = true;
            } else if (prop == "content") {
                if (auto text_node = std::dynamic_pointer_cast<Text>(node)) {
                    text_node->set_content(t.read_string());
                }
            } else if (prop == "fontSize") {
                if (auto text_node = std::dynamic_pointer_cast<Text>(node)) {
                    text_node->set_font_size(t.read_number());
                }
            } else if (prop == "src" || prop == "path") {
                std::string path = t.read_string();
                if (auto img = std::dynamic_pointer_cast<Image>(node)) {
                    img->set_src(path);
                } else if (auto svg = std::dynamic_pointer_cast<Svg>(node)) {
                    svg->set_src(path);
                }
            } else if (prop == "clip") {
                bool clip = t.read_identifier() == "true";
                if (auto group = std::dynamic_pointer_cast<Group>(node)) {
                    group->set_clip(clip);
                }
            } else {
                // Unknown property, skip value
                t.read_identifier();
            }

            // Optional: skip comma separator (allows both styles)
            t.match_char(',');
        }

        // Apply stroke if it was set
        if (has_stroke) {
            if (auto shape = std::dynamic_pointer_cast<Shape>(node)) {
                shape->set_stroke(stroke_color, stroke_width_val);
            }
        }

        return node;
    };

    try {
        // Parse top-level blocks
        while (!tok.is_eof()) {
            if (tok.match("scene") || tok.match("artboard")) {
                // scene name { width: 800, height: 600, ... children }
                auto name = tok.read_identifier();
                if (!tok.match_char('{')) {
                    last_error = "Expected '{' after scene name";
                    last_error_line = tok.line();
                    return nullptr;
                }

                float width = 800, height = 600;

                // Create artboard
                artboard = Artboard::create(width, height);

                // Parse scene properties and children
                while (!tok.match_char('}')) {
                    if (tok.is_eof()) {
                        last_error = "Unexpected EOF in scene block";
                        return nullptr;
                    }

                    std::string prop = tok.read_identifier();

                    if (prop == "width") {
                        tok.match_char(':');
                        width = tok.read_number();
                        artboard = Artboard::create(width, height);
                    } else if (prop == "height") {
                        tok.match_char(':');
                        height = tok.read_number();
                        artboard = Artboard::create(width, height);
                    } else if (prop == "rect" || prop == "circle" || prop == "ellipse" ||
                               prop == "polygon" || prop == "star" || prop == "text" ||
                               prop == "group" || prop == "image" || prop == "img" || prop == "svg") {
                        // Parse built-in node types
                        auto child = parse_node(tok, prop);
                        if (child && artboard) {
                            artboard->add_child(child);
                        }
                    } else if (ComponentRegistry::instance().has(prop)) {
                        // Parse component instance
                        // Component { prop1: value1, prop2: value2 }
                        std::string component_name = prop;

                        if (!tok.match_char('{')) {
                            last_error = "Expected '{' after component name";
                            last_error_line = tok.line();
                            return nullptr;
                        }

                        // Collect props
                        Props component_props;

                        while (!tok.match_char('}')) {
                            if (tok.is_eof()) {
                                last_error = "Unexpected EOF in component block";
                                return nullptr;
                            }

                            std::string prop_name = tok.read_identifier();

                            // Empty identifier means closing brace
                            if (prop_name.empty()) {
                                break;
                            }

                            tok.match_char(':');

                            // Read prop value
                            tok.skip_whitespace();
                            char next = tok.peek();

                            if (next == '"') {
                                // String value
                                std::string str_val = tok.read_string();
                                component_props[prop_name] = str_val;
                            } else if (next == '#') {
                                // Color value
                                Color color = tok.read_color();
                                uint32_t argb = ((uint32_t)(color.a * 255) << 24) |
                                               ((uint32_t)(color.r * 255) << 16) |
                                               ((uint32_t)(color.g * 255) << 8) |
                                               ((uint32_t)(color.b * 255));
                                component_props[prop_name] = argb;
                            } else if (next == 't' || next == 'f') {
                                // Boolean value
                                std::string bool_val = tok.read_identifier();
                                component_props[prop_name] = (bool_val == "true");
                            } else {
                                // Number value
                                float num_val = tok.read_number();
                                component_props[prop_name] = num_val;
                            }

                            // Optional: skip comma separator
                            tok.match_char(',');
                        }

                        // Create component instance
                        auto child = create_component_instance(component_name, component_props);
                        if (child && artboard) {
                            artboard->add_child(child);
                        }
                    } else if (!prop.empty()) {
                        // Unknown property - skip the entire block
                        // Format could be:
                        //   unknownProp: value
                        //   UnknownNode name { ... }

                        // Try to read optional name
                        std::string name = tok.read_identifier();

                        if (tok.match_char('{')) {
                            // It's a block - skip until matching '}'
                            int depth = 1;
                            while (depth > 0 && !tok.is_eof()) {
                                char c = tok.peek();
                                if (c == '{') {
                                    tok.match_char('{');
                                    depth++;
                                } else if (c == '}') {
                                    tok.match_char('}');
                                    depth--;
                                } else {
                                    // Skip this character
                                    tok.read_identifier();
                                    tok.skip_whitespace();
                                    if (tok.peek() == ':') tok.match_char(':');
                                    if (tok.peek() == ',') tok.match_char(',');
                                    if (tok.peek() == '#') tok.read_color();
                                    if (tok.peek() == '"') tok.read_string();
                                    else if (isdigit(tok.peek()) || tok.peek() == '-') tok.read_number();
                                }
                            }
                        } else if (tok.match_char(':')) {
                            // It's a property - skip value
                            tok.read_number();
                        }
                    }

                    // Optional: skip comma separator
                    tok.match_char(',');
                }

            } else if (tok.match("anim")) {
                // anim "name" { duration: 2s, loop: loop, track "x" { keyframe 0s -> 100 } }
                std::string anim_name = tok.read_string();

                if (!tok.match_char('{')) {
                    last_error = "Expected '{' after animation name";
                    last_error_line = tok.line();
                    return nullptr;
                }

                auto timeline = Timeline::create(anim_name);
                float duration = 0;

                while (!tok.match_char('}')) {
                    if (tok.is_eof()) break;

                    std::string prop = tok.read_identifier();

                    if (prop == "duration") {
                        tok.match_char(':');
                        duration = tok.read_number();
                        timeline->set_duration(duration);
                    } else if (prop == "loop") {
                        tok.match_char(':');
                        std::string loop_mode = tok.read_identifier();
                        if (loop_mode == "loop") {
                            timeline->set_loop_mode(LoopMode::Loop);
                        } else if (loop_mode == "pingpong") {
                            timeline->set_loop_mode(LoopMode::PingPong);
                        } else {
                            timeline->set_loop_mode(LoopMode::Once);
                        }
                    } else if (prop == "track") {
                        std::string track_property = tok.read_string();

                        if (!tok.match_char('{')) {
                            last_error = "Expected '{' after track property";
                            last_error_line = tok.line();
                            return nullptr;
                        }

                        auto track = timeline->add_track(track_property);

                        // Parse keyframes
                        while (!tok.match_char('}')) {
                            if (tok.is_eof()) break;

                            if (tok.match("keyframe")) {
                                float time = tok.read_number();
                                tok.match("->");  // arrow
                                float value = tok.read_number();

                                track->add_keyframe(time, value);

                                // Optional: skip comma separator
                                tok.match_char(',');
                            }
                        }
                    }

                    // Optional: skip comma separator
                    tok.match_char(',');
                }

                if (out_timelines) {
                    out_timelines->push_back(timeline);
                }

            } else {
                // Unknown block, skip it
                tok.read_identifier();
            }
        }

    } catch (const std::exception& e) {
        last_error = std::string("Parse error: ") + e.what();
        last_error_line = tok.line();
        return nullptr;
    }

    // Create default artboard if none was parsed
    if (!artboard) {
        artboard = Artboard::create(800, 600);
    }

    if (out_machine) {
        *out_machine = nullptr; // State machines not yet implemented
    }

    return artboard;
}

const char* get_error() {
    return last_error.c_str();
}

int get_error_line() {
    return last_error_line;
}

int get_error_column() {
    return last_error_column;
}

} // namespace parser

// ============================================================================
// Engine Initialization
// ============================================================================

void init() {
    // Initialize ThorVG
    tvg::Initializer::init(0);
}

void shutdown() {
    // Terminate ThorVG
    tvg::Initializer::term();
}

// ============================================================================
// Font Management
// ============================================================================

bool load_font(const char* path) {
    if (!path) return false;
    return tvg::Text::load(path) == tvg::Result::Success;
}

bool load_font(const char* name, const char* path) {
    // ThorVG's Text::load(filename) uses filename as the font name
    // For custom name, we need to load from memory
    if (!name || !path) return false;

    // Read file into memory
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) return false;

    return tvg::Text::load(name, buffer.data(), static_cast<uint32_t>(size), "ttf", true) == tvg::Result::Success;
}

bool load_font_data(const char* name, const char* data, uint32_t size) {
    if (!name || !data || size == 0) return false;
    return tvg::Text::load(name, data, size, "ttf", true) == tvg::Result::Success;
}

void unload_font(const char* name) {
    if (name) {
        tvg::Text::unload(name);
    }
}

} // namespace flex
