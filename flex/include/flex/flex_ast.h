/*
 * Flex DSL Abstract Syntax Tree
 * Intermediate representation before building Runtime objects
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>

namespace flex {
namespace parser {

// Forward declarations
struct AstNode;
struct AstScene;
struct AstComponent;
struct AstAnim;
struct AstMachine;

// Property value types
using AstValue = std::variant<
    float,
    std::string,
    bool
>;

// Property map
using AstProps = std::unordered_map<std::string, AstValue>;

// ============================================================================
// AST Node Types
// ============================================================================

struct AstNode {
    std::string type;        // "rect", "circle", "group", etc.
    std::string id;          // Node ID
    AstProps properties;     // x, y, width, color, etc.
    std::vector<std::shared_ptr<AstNode>> children;

    // Pseudo-class styles
    std::unordered_map<std::string, AstProps> pseudo_classes;  // ":hover" -> props

    // Repeat block info (only for type == "repeat")
    int repeat_count = 0;
    std::vector<std::shared_ptr<AstNode>> repeat_template;

    AstNode(const std::string& t, const std::string& i)
        : type(t), id(i) {}

    // Deep clone for repeat expansion
    std::shared_ptr<AstNode> clone() const {
        auto copy = std::make_shared<AstNode>(type, id);
        copy->properties = properties;
        copy->pseudo_classes = pseudo_classes;
        copy->repeat_count = repeat_count;
        for (const auto& child : children) {
            copy->children.push_back(child->clone());
        }
        for (const auto& tmpl : repeat_template) {
            copy->repeat_template.push_back(tmpl->clone());
        }
        return copy;
    }
};

struct AstScene {
    std::string name;
    float width = 800;
    float height = 600;
    std::vector<std::shared_ptr<AstNode>> children;
    
    AstScene(const std::string& n) : name(n) {}
};

struct AstComponent {
    std::string name;
    std::string body_source;  // Raw DSL source for the component body
    
    AstComponent(const std::string& n) : name(n) {}
};

struct AstKeyframe {
    float time;
    AstValue value;
    
    AstKeyframe(float t, const AstValue& v) : time(t), value(v) {}
};

struct AstTrack {
    std::string property;  // "x", "#node/opacity", etc.
    std::vector<AstKeyframe> keyframes;

    AstTrack() = default;
    AstTrack(const std::string& p) : property(p) {}
};

struct AstAnim {
    std::string name;
    float duration = 0;
    std::string loop_mode = "once";  // "once", "loop", "pingpong"
    std::vector<AstTrack> tracks;

    AstAnim() = default;
    AstAnim(const std::string& n) : name(n) {}
};

struct AstTransition {
    std::string from_state;
    std::string to_state;
    std::string condition_var;
    std::string condition_op;  // ">", "<", "==", "!="
    float condition_val = 0;

    AstTransition() = default;
};

// ============================================================================
// Data and For Loop
// ============================================================================

// Single data item: { name: "Laptop", price: 999 }
struct AstDataItem {
    std::string key;  // Item key in data block
    AstProps properties;

    AstDataItem() = default;
    AstDataItem(const std::string& k) : key(k) {}
};

// Data block: data products { item1: {...}, item2: {...} }
struct AstData {
    std::string name;  // "products"
    std::vector<AstDataItem> items;

    AstData() = default;
    AstData(const std::string& n) : name(n) {}
};

// For loop: for item in products { ... }
struct AstForLoop {
    std::string iterator_name;  // "item"
    std::string data_source;    // "products"
    std::vector<std::shared_ptr<AstNode>> template_children;

    AstForLoop() = default;
};

struct AstState {
    std::string name;
    bool initial = false;
    std::string animation;

    AstState() = default;
    AstState(const std::string& n) : name(n) {}
};

struct AstLayer {
    std::string name;
    std::vector<AstState> states;
    std::vector<AstTransition> transitions;
    std::string initial_state;

    AstLayer() = default;
    AstLayer(const std::string& n) : name(n) {}
};

struct AstMachine {
    std::string name;
    std::vector<AstLayer> layers;
    
    AstMachine(const std::string& n) : name(n) {}
};

// ============================================================================
// Top-level Program
// ============================================================================

struct AstProgram {
    std::shared_ptr<AstScene> scene;
    std::vector<std::shared_ptr<AstComponent>> components;
    std::vector<std::shared_ptr<AstAnim>> animations;
    std::vector<std::shared_ptr<AstMachine>> machines;
    std::vector<std::shared_ptr<AstData>> data_blocks;  // data { ... }

    AstProgram() = default;
};

} // namespace parser
} // namespace flex
