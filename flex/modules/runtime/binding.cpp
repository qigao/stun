/*
 * Flex Engine - Data Binding Implementation
 */

#include "flex/core/binding.h"
#include "flex/core/expr.h"
#include "flex/core/expr_compiled.h"
#include "flex/core/expr_compiled_impl.h"
#include "flex/core/node.h"
#include "flex/core/shape.h"
#include "flex/core/text.h"
#include "flex/core/group.h"
#include "flex/core/component.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <deque>
#include <set>

namespace flex {

// ============================================================================
// BindingContext Implementation
// ============================================================================

// ============================================================================
// BindingContext Implementation
// ============================================================================

struct BindingContext::Impl {
    // Input storage
    std::unordered_map<Symbol, float, SymbolHash> float_inputs;
    std::unordered_map<Symbol, std::string, SymbolHash> string_inputs;
    std::unordered_map<Symbol, bool, SymbolHash> bool_inputs;

    // All bindings
    std::vector<Binding> bindings;
    struct ComponentBindingRuntime {
        ComponentPropBinding def;
        Props last_props;
    };
    std::vector<ComponentBindingRuntime> component_bindings;

    // Empty string for default return
    std::string empty_string;
};

BindingContext::BindingContext() : impl_(std::make_unique<Impl>()) {}

BindingContext::~BindingContext() = default;

// -------------------------------------------
// Input Management
// -------------------------------------------

void BindingContext::set_input(Symbol name, float value) {
    impl_->string_inputs.erase(name);
    impl_->bool_inputs.erase(name);
    impl_->float_inputs[name] = value;
}

void BindingContext::set_input(Symbol name, const std::string& value) {
    impl_->float_inputs.erase(name);
    impl_->bool_inputs.erase(name);
    impl_->string_inputs[name] = value;
}

void BindingContext::set_input(Symbol name, const char* value) {
    set_input(name, std::string(value ? value : ""));
}

void BindingContext::set_input(Symbol name, bool value) {
    impl_->float_inputs.erase(name);
    impl_->string_inputs.erase(name);
    impl_->bool_inputs[name] = value;
}

float BindingContext::get_float_input(Symbol name) const {
    auto it = impl_->float_inputs.find(name);
    if (it != impl_->float_inputs.end()) {
        return it->second;
    }
    return 0.0f;
}

const std::string& BindingContext::get_string_input(Symbol name) const {
    auto it = impl_->string_inputs.find(name);
    if (it != impl_->string_inputs.end()) {
        return it->second;
    }
    return impl_->empty_string;
}

bool BindingContext::get_bool_input(Symbol name) const {
    auto it = impl_->bool_inputs.find(name);
    if (it != impl_->bool_inputs.end()) {
        return it->second;
    }
    return false;
}

bool BindingContext::has_input(Symbol name) const {
    return impl_->float_inputs.count(name) > 0 ||
           impl_->string_inputs.count(name) > 0 ||
           impl_->bool_inputs.count(name) > 0;
}

// -------------------------------------------
// Binding Registration
// -------------------------------------------

void BindingContext::add_binding(Node* target, Symbol property, const Binding& binding) {
    Binding b = binding;
    b.target = target;
    b.property = property;
    impl_->bindings.push_back(b);
}

void BindingContext::add_binding(Node* target, const char* property_name, const Binding& binding) {
    if (!property_name || property_name[0] == '\0') {
        add_binding(target, Symbol(), binding);
        return;
    }
    Binding b = binding;
    b.target = target;
    b.property_name = property_name;
    b.property = Symbol(property_name);
    b.property_id = get_property_id(property_name);
    impl_->bindings.push_back(b);
}

void BindingContext::add_component_binding(const ComponentPropBinding& binding) {
    Impl::ComponentBindingRuntime runtime;
    runtime.def = binding;
    runtime.last_props = binding.base_props;
    impl_->component_bindings.push_back(std::move(runtime));
}

void BindingContext::remove_bindings(Node* target) {
    impl_->bindings.erase(
        std::remove_if(impl_->bindings.begin(), impl_->bindings.end(),
            [target](const Binding& b) { return b.target == target; }),
        impl_->bindings.end()
    );
}

void BindingContext::remove_binding(Node* target, Symbol property) {
    impl_->bindings.erase(
        std::remove_if(impl_->bindings.begin(), impl_->bindings.end(),
            [target, property](const Binding& b) {
                return b.target == target && b.property == property;
            }),
        impl_->bindings.end()
    );
}

// -------------------------------------------
// Evaluation
// -------------------------------------------

// ============================================================================
// Helper Functions
// ============================================================================

// Strip $-prefixed input references for expression compatibility:
// "$posX" → "posX", "$Speed * 2" → "Speed * 2"
static std::string strip_dollar_refs(const std::string& expr) {
    std::string out;
    out.reserve(expr.size());
    for (size_t i = 0; i < expr.size(); ++i) {
        if (expr[i] == '$') {
            size_t j = i + 1;
            if (j < expr.size() && (std::isalpha(static_cast<unsigned char>(expr[j])) || expr[j] == '_')) {
                // Skip the '$', the identifier will be kept as-is
                continue;
            }
        }
        out.push_back(expr[i]);
    }
    return out;
}

static std::string trim_copy(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

static bool is_simple_identifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value[0]);
    if (!(std::isalpha(first) || value[0] == '_')) {
        return false;
    }
    for (size_t i = 1; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (!(std::isalnum(ch) || value[i] == '_')) {
            return false;
        }
    }
    return true;
}

bool is_binding_string(const std::string& str) {
    if (str.empty()) return false;

    // Check for ${ expression }
    if (str.size() >= 3 && str[0] == '$' && str[1] == '{') {
        return true;
    }

    // Check for $( expression )
    if (str.size() >= 3 && str[0] == '$' && str[1] == '(') {
        return true;
    }

    // Check for $InputName (starts with $ but not ${)
    if (str[0] == '$' && (str.size() < 2 || str[1] != '{')) {
        return true;
    }

    return false;
}

Binding parse_binding(const std::string& str) {
    if (str.empty() || str[0] != '$') {
        return Binding::input("");
    }

    // Expression/input binding: $( ... )
    if (str.size() >= 3 && str[1] == '(') {
        size_t end = str.rfind(')');
        std::string expr = (end != std::string::npos && end > 2)
            ? str.substr(2, end - 2)
            : str.substr(2);
        expr = trim_copy(expr);

        if (is_simple_identifier(expr)) {
            return Binding::input(expr);
        }

        return Binding::exprtk(strip_dollar_refs(expr));
    }

    // Expression binding: ${ ... }
    if (str.size() >= 3 && str[1] == '{') {
        size_t end = str.rfind('}');
        std::string expr;
        if (end != std::string::npos && end > 2) {
            expr = str.substr(2, end - 2);
        } else {
            expr = str.substr(2);
        }

        // Trim whitespace
        size_t start_pos = expr.find_first_not_of(" \t");
        size_t end_pos = expr.find_last_not_of(" \t");
        if (start_pos != std::string::npos) {
            expr = expr.substr(start_pos, end_pos - start_pos + 1);
        }

        // Keep simple identifiers on the direct input path so string/bool bindings
        // do not get forced through numeric expression evaluation.
        if (is_simple_identifier(expr)) {
            return Binding::input(expr);
        }

        // Check if it's a simple input reference: ${$varName}
        // If the expression is just "$identifier" with no operators, treat as Input binding
        if (!expr.empty() && expr[0] == '$') {
            std::string name = expr.substr(1);
            bool is_simple_ref = !name.empty();
            for (char c : name) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                    is_simple_ref = false;
                    break;
                }
            }
            if (is_simple_ref) {
                return Binding::input(name);
            }
        }

        return Binding::exprtk(strip_dollar_refs(expr));
    }

    // Simple input binding: $InputName
    return Binding::input(str.substr(1));
}

void BindingContext::evaluate() {
    evaluate(nullptr);
}

namespace {

bool apply_binding_value(Node* target, PropertyID pid, const AnimValue& value) {
    if (!target || pid == PropertyID::Unknown) {
        return false;
    }

    if (pid == PropertyID::Width) {
        if (auto* f = std::get_if<float>(&value)) {
            if (auto* shape = dynamic_cast<Shape*>(target)) {
                shape->set_animated_property(pid, value);
            }
            target->set_layout_width(*f);
            return true;
        }
        return false;
    }

    if (pid == PropertyID::Height) {
        if (auto* f = std::get_if<float>(&value)) {
            if (auto* shape = dynamic_cast<Shape*>(target)) {
                shape->set_animated_property(pid, value);
            }
            target->set_layout_height(*f);
            return true;
        }
        return false;
    }

    if (pid == PropertyID::Content || pid == PropertyID::Text) {
        if (auto* f = std::get_if<float>(&value)) {
            std::ostringstream oss;
            oss << *f;
            return target->set_animated_property(pid, std::string(oss.str()));
        }
    }

    return target->set_animated_property(pid, value);
}

} // namespace


bool evaluate_exprtk(Binding& binding,
                     const std::unordered_map<Symbol, float, SymbolHash>& float_inputs,
                     const std::unordered_map<Symbol, std::string, SymbolHash>& string_inputs,
                     const std::unordered_map<Symbol, bool, SymbolHash>& bool_inputs,
                     float time,
                     float& out_result) {
    (void)string_inputs;

    if (!binding.compiled_expr) {
        if (!compile_exprtk(binding.expression, binding.compiled_expr)) {
            return false;
        }
    }
    auto* data = static_cast<ExprTkCompiled*>(binding.compiled_expr.get());
    if (!data) return false;

    if (!data->mir_program) return false;

    std::unordered_map<Symbol, float, SymbolHash> numeric_inputs = float_inputs;
    for (size_t i = 0; i < data->names.size(); ++i) {
        const auto& name = data->names[i];
        Symbol sym = data->symbol_ids[i];
        if (name == "Time" || name == "time") {
            numeric_inputs[sym] = time;
            continue;
        }
        auto it_b = bool_inputs.find(sym);
        if (it_b != bool_inputs.end()) {
            numeric_inputs[sym] = it_b->second ? 1.0f : 0.0f;
        }
    }
    out_result = data->mir_program->evaluate(numeric_inputs);
    return true;
}

namespace {

bool prop_value_equals(const PropValue& a, const PropValue& b) {
    if (a.index() != b.index()) {
        return false;
    }
    if (auto* fa = std::get_if<float>(&a)) {
        return *fa == std::get<float>(b);
    }
    if (auto* sa = std::get_if<std::string>(&a)) {
        return *sa == std::get<std::string>(b);
    }
    if (auto* ba = std::get_if<bool>(&a)) {
        return *ba == std::get<bool>(b);
    }
    if (auto* ca = std::get_if<uint32_t>(&a)) {
        return *ca == std::get<uint32_t>(b);
    }
    return false;
}

bool resolve_binding_to_prop(Binding& binding,
                             const std::unordered_map<Symbol, float, SymbolHash>& float_inputs,
                             const std::unordered_map<Symbol, std::string, SymbolHash>& string_inputs,
                             const std::unordered_map<Symbol, bool, SymbolHash>& bool_inputs,
                             float time, PropValue& out) {
    if (binding.type == BindingType::Input) {
        auto it_num = float_inputs.find(binding.input_name);
        if (it_num != float_inputs.end()) {
            out = static_cast<float>(it_num->second);
            return true;
        }
        auto it_str = string_inputs.find(binding.input_name);
        if (it_str != string_inputs.end()) {
            const std::string& val = it_str->second;
            if (!val.empty() && val[0] == '#') {
                out = Color::from_hex(val.c_str()).to_rgba32();
            } else {
                out = val;
            }
            return true;
        }
        auto it_bool = bool_inputs.find(binding.input_name);
        if (it_bool != bool_inputs.end()) {
            out = it_bool->second;
            return true;
        }
        return false;
    }

    if (binding.type == BindingType::ExprTk) {
        float result = 0.0f;
        if (evaluate_exprtk(binding, float_inputs, string_inputs, bool_inputs, time, result)) {
            out = result;
            return true;
        }
        return false;
    }

    return false;
}

void copy_node_layout_and_visual(Node* src, Node* dst) {
    if (!src || !dst) {
        return;
    }

    dst->set_id(src->id());
    dst->set_position(src->x(), src->y());
    dst->set_rotation(src->rotation());
    dst->set_scale(src->scale_x(), src->scale_y());
    dst->set_opacity(src->opacity());
    dst->set_visible(src->visible());

    dst->set_anchor(src->anchor());
    dst->set_align_self(src->align_self());
    dst->set_position_mode(src->position_mode());
    dst->set_position_offsets(src->position_top(), src->position_right(),
                              src->position_bottom(), src->position_left());
    dst->set_flex(src->flex_grow(), src->flex_shrink(), src->flex_basis());
    dst->set_flex_basis_auto(src->flex_basis_auto());
    dst->set_z_index(src->z_index());
    dst->set_box_sizing(src->box_sizing());

    if (src->width_is_percent()) {
        dst->set_width_percent(src->layout_width());
    } else if (src->layout_width() > 0) {
        dst->set_layout_width(src->layout_width());
    }
    if (src->height_is_percent()) {
        dst->set_height_percent(src->layout_height());
    } else if (src->layout_height() > 0) {
        dst->set_layout_height(src->layout_height());
    }

    dst->set_margin(src->margin_top(), src->margin_right(),
                    src->margin_bottom(), src->margin_left());
    dst->set_border_width(src->border_width_top(), src->border_width_right(),
                          src->border_width_bottom(), src->border_width_left());

    if (auto* src_group = dynamic_cast<Group*>(src)) {
        if (auto* dst_group = dynamic_cast<Group*>(dst)) {
            dst_group->set_layout(src_group->layout());
            dst_group->set_flex_direction(src_group->flex_direction());
            dst_group->set_justify_content(src_group->justify_content());
            dst_group->set_align_items(src_group->align_items());
            dst_group->set_flex_wrap(src_group->flex_wrap());
            dst_group->set_gap(src_group->gap());
            dst_group->set_padding(src_group->padding_top(), src_group->padding_right(),
                                   src_group->padding_bottom(), src_group->padding_left());
        }
    }
}

class ComponentInstanceManager {
public:
    void rebuild(std::vector<Binding>& bindings,
                 ComponentNodePtr& node,
                 const Component::SharedPtr& component_override,
                 const std::string& component_name,
                 Props& last_props,
                 const Props& next_props) const {
        auto component = component_override;
        if (!component) {
            component = ComponentRegistry::instance().get(component_name);
        }
        if (!component) {
            return;
        }

        auto new_node = component->instantiate(next_props);
        if (!new_node) {
            return;
        }

        auto old_node = node;
        if (old_node) {
            copy_node_layout_and_visual(old_node.get(), new_node.get());
        }

        // Preserve component-provided sizing when props explicitly drive width/height.
        if (auto it = next_props.find("width"); it != next_props.end()) {
            if (auto* fval = std::get_if<float>(&it->second)) {
                new_node->set_layout_width(*fval);
            }
        }
        if (auto it = next_props.find("height"); it != next_props.end()) {
            if (auto* fval = std::get_if<float>(&it->second)) {
                new_node->set_layout_height(*fval);
            }
        }

        auto* parent = old_node ? dynamic_cast<Group*>(old_node->parent()) : nullptr;
        if (parent) {
            size_t index = 0;
            const auto& children = parent->children();
            for (; index < children.size(); ++index) {
                if (children[index] == old_node.get()) {
                    break;
                }
            }
            parent->remove_child(old_node.get());
            parent->insert_child(new_node, index);
        }

        retarget_bindings(bindings, old_node.get(), new_node.get());

        node = new_node;
        last_props = next_props;
    }

private:
    void retarget_bindings(std::vector<Binding>& bindings, Node* old_node, Node* new_node) const {
        if (!old_node || !new_node) {
            return;
        }
        for (auto& binding : bindings) {
            if (binding.target == old_node) {
                binding.target = new_node;
            }
        }
    }
};

} // namespace

void BindingContext::evaluate(Node* target) {
    for (auto& binding : impl_->bindings) {
        if (target && binding.target != target) {
            continue;
        }

        PropertyID pid = binding.property_id;
        if (pid == PropertyID::Unknown && !binding.property_name.empty()) {
            pid = get_property_id(binding.property_name.c_str());
        }

        if (binding.type == BindingType::Input) {
            if (!has_input(binding.input_name)) {
                continue;
            }
            auto it_num = impl_->float_inputs.find(binding.input_name);
            if (it_num != impl_->float_inputs.end()) {
                apply_binding_value(binding.target, pid, AnimValue(it_num->second));
                continue;
            }
            auto it_str = impl_->string_inputs.find(binding.input_name);
            if (it_str != impl_->string_inputs.end()) {
                apply_binding_value(binding.target, pid, AnimValue(it_str->second));
                continue;
            }
            auto it_bool = impl_->bool_inputs.find(binding.input_name);
            if (it_bool != impl_->bool_inputs.end()) {
                apply_binding_value(binding.target, pid, AnimValue(it_bool->second ? 1.0f : 0.0f));
                continue;
            }
            continue;
        }

        if (binding.type == BindingType::ExprTk) {
            float val = 0.0f;
            if (evaluate_exprtk(binding, impl_->float_inputs, impl_->string_inputs, impl_->bool_inputs, time_, val)) {
                apply_binding_value(binding.target, pid, AnimValue(val));
            }
        }
    }

    for (auto& component_binding : impl_->component_bindings) {
        if (target && component_binding.def.node.get() != target) {
            continue;
        }

        Props next_props = component_binding.def.base_props;
        bool changed = false;

        for (auto& [prop_name, bind] : component_binding.def.prop_bindings) {
            auto last_it = component_binding.last_props.find(prop_name);
            if (last_it != component_binding.last_props.end()) {
                next_props[prop_name] = last_it->second;
            }

            PropValue resolved;
            if (!resolve_binding_to_prop(bind,
                                         impl_->float_inputs,
                                         impl_->string_inputs,
                                         impl_->bool_inputs,
                                         time_,
                                         resolved)) {
                continue;
            }

            auto existing = next_props.find(prop_name);
            if (existing == next_props.end() || !prop_value_equals(existing->second, resolved)) {
                next_props[prop_name] = resolved;
                changed = true;
            }
        }

        if (changed) {
            ComponentInstanceManager manager;
            manager.rebuild(impl_->bindings,
                            component_binding.def.node,
                            component_binding.def.component,
                            component_binding.def.component_name,
                            component_binding.last_props,
                            next_props);
        }
    }
}

} // namespace flex
