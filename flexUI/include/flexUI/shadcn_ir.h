#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace flexUI {
class Box;
class Element;
}

namespace flexUI::shadcn_ir {

using Json = nlohmann::json;

struct InstantiatedTree {
  Element* root = nullptr;
  std::unordered_map<std::string, Element*> nodes;
};

std::string component_scope(const Json& component_ir);
std::string node_dom_id(const Json& component_ir, const std::string& node_id);
std::string node_dom_id(const Json& component_ir, const std::string& node_id,
                        const std::string& instance_id);
std::string emit_css(const Json& component_ir, const Json& style_ir,
                     const Json& resolved_variants = Json::object());
std::string emit_css(const Json& component_ir, const Json& style_ir,
                     const Json& resolved_variants,
                     const std::string& instance_id);
std::string emit_utility_css(const Json& utility_whitelist,
                             const std::vector<std::string>& class_tokens);
std::vector<std::string> missing_utility_tokens(
    const Json& utility_whitelist,
    const std::vector<std::string>& class_tokens);
InstantiatedTree instantiate_component(Box& box, const Json& component_ir,
                                       const Json& props = Json::object());
InstantiatedTree instantiate_component(Box& box, const Json& component_ir,
                                       const Json& props,
                                       const std::string& instance_id);
InstantiatedTree instantiate_component_tree(Box& box, const Json& component_ir,
                                            const Json& props = Json::object());
InstantiatedTree instantiate_component_tree(Box& box, const Json& component_ir,
                                            const Json& props,
                                            const std::string& instance_id);
void apply_bridge_states(const Json& bridge_ir, const Json& active_states,
                         const InstantiatedTree& tree);

}  // namespace flexUI::shadcn_ir
