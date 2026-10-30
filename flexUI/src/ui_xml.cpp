#include <flexUI/ui_xml.h>
#include <flexUI/widget_registry.h>

#include <pugixml.hpp>
#include "ui_xml_admission.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace flexUI {

namespace {

struct XmlSource {
  UiDocumentDefinition definition;
  std::vector<UiResourceDefinition> resources;
  std::vector<UiScopedComponent> scoped_components;
  UiDocumentError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

UiDocumentError make_error(UiDocumentErrorCode code, std::string message, int line = 0,
                           int column = 0) {
  return {code, std::move(message), line, column};
}


UiDocumentError node_error(std::string_view source, const pugi::xml_node &node,
                           UiDocumentErrorCode code, std::string message) {
  const auto location = detail::xml_source_span(source, node.offset_debug());
  return make_error(code, std::move(message), location.line, location.column);
}

bool is_namespace_declaration(std::string_view name) {
  return name == "xmlns" || name.rfind("xmlns:", 0) == 0;
}

constexpr const char *kXmlWhitespace = " \t\r\n";

bool has_non_whitespace(std::string_view text) {
  return text.find_first_not_of(kXmlWhitespace) != std::string_view::npos;
}

bool is_string_property(std::string_view name) {
  return name == "class" || name == "classes" || name == "utility" || name == "utilities" ||
         name == "text" || name == "content" || name == "slot" ||
         name.rfind("var.", 0) == 0 || name.rfind("on.", 0) == 0 ||
         name.rfind("bind.", 0) == 0 || name.rfind("attr.", 0) == 0;
}

bool canonical_property_name(std::string_view xml_name, std::string &result) {
  static constexpr std::pair<std::string_view, std::string_view> prefixes[] = {
      {"on:", "on."}, {"bind:", "bind."}, {"attr:", "attr."}, {"var:", "var."}};
  for (const auto &[xml_prefix, canonical_prefix] : prefixes) {
    if (xml_name.rfind(xml_prefix, 0) == 0) {
      if (xml_name.size() == xml_prefix.size()) {
        return false;
      }
      result.assign(canonical_prefix);
      result.append(xml_name.substr(xml_prefix.size()));
      return true;
    }
  }
  if (xml_name.find(':') != std::string_view::npos || xml_name.rfind("on.", 0) == 0 ||
      xml_name.rfind("bind.", 0) == 0 || xml_name.rfind("attr.", 0) == 0 ||
      xml_name.rfind("var.", 0) == 0) {
    return false;
  }
  result.assign(xml_name);
  return !result.empty();
}

UiDocumentValue parse_scalar(std::string_view property, std::string_view source) {
  if (is_string_property(property)) {
    return std::string(source);
  }
  if (source == "true") {
    return true;
  }
  if (source == "false") {
    return false;
  }

  float number = 0.0F;
  const auto parsed = std::from_chars(source.data(), source.data() + source.size(), number,
                                      std::chars_format::general);
  if (parsed.ec == std::errc{} && parsed.ptr == source.data() + source.size() &&
      std::isfinite(number)) {
    return number;
  }
  return std::string(source);
}

// Declared typed Props use the registry as the ONE source of truth. The
// untyped legacy frontend keeps its original scalar inference behavior.
bool parse_typed_property(UiPropertyKind kind, std::string_view value,
                          UiDocumentValue &result) {
  if (kind == UiPropertyKind::String) {
    result = std::string(value);
    return true;
  }
  if (kind == UiPropertyKind::Boolean) {
    if (value == "true") { result = true; return true; }
    if (value == "false") { result = false; return true; }
    return false;
  }
  if (kind == UiPropertyKind::Number) {
    float number = 0.0F;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                        number, std::chars_format::general);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        !std::isfinite(number)) return false;
    result = number;
    return true;
  }
  if (kind == UiPropertyKind::Integer) {
    std::int64_t integer = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), integer);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        integer < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
        integer > static_cast<std::int64_t>(std::numeric_limits<int>::max()))
      return false;
    const auto number = static_cast<float>(integer);
    if (static_cast<std::int64_t>(number) != integer) return false;
    result = number;
    return true;
  }
  return false;
}

UiDocumentError parse_widget_node(std::string_view source, const pugi::xml_node &xml_node,
                                  const UiDocumentLimits &limits, const WidgetRegistry *registry,
                                  std::size_t depth, std::size_t &node_count,
                                  std::unordered_set<std::string> &ids,
                                  UiNodeDefinition &definition) {
  const std::string_view tag(xml_node.name());
  if (tag == "ui" || tag == "resources" || tag == "resource") {
    return node_error(source, xml_node, UiDocumentErrorCode::InvalidNode,
                      "reserved XML structural element cannot be used as a widget: " +
                          std::string(tag));
  }
  if (depth > limits.max_depth) {
    return node_error(source, xml_node, UiDocumentErrorCode::DepthLimitExceeded,
                      "UI document exceeds maximum tree depth");
  }
  if (++node_count > limits.max_nodes) {
    return node_error(source, xml_node, UiDocumentErrorCode::NodeLimitExceeded,
                      "UI document exceeds maximum node count");
  }
  if (tag.empty() || tag.size() > limits.max_string_bytes) {
    return node_error(source, xml_node,
                      tag.empty() ? UiDocumentErrorCode::InvalidNode
                                  : UiDocumentErrorCode::StringLimitExceeded,
                      "XML widget tag must be non-empty and within the string limit");
  }

  const UiNodeDescriptor *schema = registry ? registry->descriptor(tag) : nullptr;
  if (registry && !schema) {
    return node_error(source, xml_node, UiDocumentErrorCode::UnknownElementTag,
                      "unknown registered UI tag: " + std::string(tag));
  }

  const auto id_attribute = xml_node.attribute("id");
  const std::string_view id(id_attribute.value());
  if (id_attribute && id.empty()) {
    return node_error(source, xml_node, UiDocumentErrorCode::InvalidNode,
                      "XML widget id must be non-empty when the attribute is present");
  }
  if (id.size() > limits.max_string_bytes) {
    return node_error(source, xml_node, UiDocumentErrorCode::StringLimitExceeded,
                      "XML widget id exceeds the string limit");
  }
  if (!id.empty() && !ids.emplace(id).second) {
    return node_error(source, xml_node, UiDocumentErrorCode::DuplicateElementId,
                      "duplicate UI element id: " + std::string(id));
  }

  definition.tag.assign(tag);
  definition.id.assign(id);
  std::unordered_set<std::string> property_names;
  std::size_t id_attribute_count = 0;
  const auto location = detail::xml_source_span(source, xml_node.offset_debug());
  definition.source = SourceSpan{location.line, location.column, tag.size()};
  for (const auto &attribute : xml_node.attributes()) {
    const std::string_view xml_name(attribute.name());
    if (xml_name == "id") {
      if (++id_attribute_count > 1) {
        return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                          "XML widget contains duplicate id attributes");
      }
      continue;
    }
    if (is_namespace_declaration(xml_name)) {
      continue;
    }
    std::string property_name;
    if (!canonical_property_name(xml_name, property_name)) {
      return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                        "invalid XML UI attribute name: " + std::string(xml_name));
    }
    if (!property_names.insert(property_name).second) {
      return node_error(
          source, xml_node,
          property_name.rfind("on.", 0) == 0     ? UiDocumentErrorCode::DuplicateEventBinding
          : property_name.rfind("bind.", 0) == 0 ? UiDocumentErrorCode::DuplicateBindingTarget
                                                 : UiDocumentErrorCode::InvalidProperty,
          "duplicate XML UI attribute: " + property_name);
    }
    if (definition.properties.size() >= limits.max_properties_per_node) {
      return node_error(source, xml_node, UiDocumentErrorCode::PropertyLimitExceeded,
                        "UI node exceeds maximum property count: " + definition.id);
    }
    const std::string_view value(attribute.value());
    if (property_name.size() > limits.max_string_bytes || value.size() > limits.max_string_bytes) {
      return node_error(source, xml_node, UiDocumentErrorCode::StringLimitExceeded,
                        "XML UI attribute exceeds the string limit: " + property_name);
    }
    UiDocumentValue parsed_value;
    if (schema) {
      if (const auto *spec = schema->property(property_name)) {
        if (!parse_typed_property(spec->kind, value, parsed_value)) {
          return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                            "UI tag '" + std::string(tag) + "' property '" +
                            property_name + "' violates its declared type");
        }
      } else {
        parsed_value = parse_scalar(property_name, value);
      }
    } else {
      parsed_value = parse_scalar(property_name, value);
    }
    definition.properties.emplace(property_name, std::move(parsed_value));
    definition.property_spans.emplace(std::move(property_name),
                                      SourceSpan{location.line, location.column, xml_name.size()});
  }

  std::size_t text_bytes = 0;
  std::size_t content_begin = 0;
  std::size_t content_end = 0;
  SourceSpan text_source;
  bool has_text_content = false;
  bool has_element_children = false;
  for (const auto &child : xml_node.children()) {
    if (child.type() == pugi::node_element) {
      has_element_children = true;
      definition.children.emplace_back();
      auto error = parse_widget_node(source, child, limits, registry, depth + 1,
                                     node_count, ids,
                                     definition.children.back());
      if (error) {
        return error;
      }
    } else if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
      const std::string_view text(child.value());
      const auto first = text.find_first_not_of(kXmlWhitespace);
      if (first != std::string_view::npos) {
        if (!has_text_content) {
          content_begin = text_bytes + first;
          // Anchor the original XML text node. Entity/CDATA decoding and outer
          // whitespace trimming need not map to one contiguous source span.
          text_source = detail::xml_source_span(source, child.offset_debug());
          has_text_content = true;
        }
        content_end = text_bytes + text.find_last_not_of(kXmlWhitespace) + 1;
      }
      text_bytes += text.size();
    }
  }

  if (has_text_content) {
    if (has_element_children) {
      return make_error(UiDocumentErrorCode::InvalidNode,
                        "mixed XML text and child elements are not supported",
                        text_source.line, text_source.column);
    }
    if (definition.properties.count("text") != 0 ||
        definition.properties.count("content") != 0) {
      return make_error(UiDocumentErrorCode::InvalidProperty,
                        "XML text content conflicts with text/content attribute",
                        text_source.line, text_source.column);
    }
    if (definition.properties.size() >= limits.max_properties_per_node) {
      return make_error(UiDocumentErrorCode::PropertyLimitExceeded,
                        "UI node exceeds maximum property count: " + definition.id,
                        text_source.line, text_source.column);
    }
    const auto normalized_size = content_end - content_begin;
    if (normalized_size > limits.max_string_bytes) {
      return make_error(UiDocumentErrorCode::StringLimitExceeded,
                        "XML text content exceeds the string limit",
                        text_source.line, text_source.column);
    }
    // Determine the admitted size before allocating; formatting whitespace
    // alone must not consume the text value budget or require a large copy.
    std::string text_content;
    text_content.reserve(normalized_size);
    std::size_t offset = 0;
    for (const auto &child : xml_node.children()) {
      if (child.type() != pugi::node_pcdata && child.type() != pugi::node_cdata) {
        continue;
      }
      const std::string_view text(child.value());
      const auto begin = content_begin > offset ? std::min(content_begin - offset, text.size()) : 0;
      const auto end = content_end > offset ? std::min(content_end - offset, text.size()) : 0;
      if (end > begin) {
        text_content.append(text.data() + begin, end - begin);
      }
      offset += text.size();
    }
    definition.properties.emplace("text", std::move(text_content));
    definition.property_spans.emplace("text", text_source);
  }
  return {};
}

UiDocumentError parse_resources(std::string_view source, const pugi::xml_node &resources_node,
                                const UiDocumentLimits &limits,
                                std::vector<UiResourceDefinition> &resources) {
  for (const auto &attribute : resources_node.attributes()) {
    if (!is_namespace_declaration(attribute.name())) {
      return node_error(source, resources_node, UiDocumentErrorCode::InvalidProperty,
                        "resources element does not accept attributes");
    }
  }

  std::unordered_set<std::string> resource_ids;
  for (const auto &child : resources_node.children()) {
    if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
      if (has_non_whitespace(child.value())) {
        return node_error(source, child, UiDocumentErrorCode::InvalidNode,
                          "resources must not contain text content");
      }
      continue;
    }
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (std::string_view(child.name()) != "resource") {
      return node_error(source, child, UiDocumentErrorCode::InvalidNode,
                        "resources may only contain resource elements");
    }
    if (resources.size() >= limits.max_resources) {
      return node_error(source, child, UiDocumentErrorCode::ResourceLimitExceeded,
                        "UI document exceeds maximum declared resource count");
    }
    UiResourceDefinition resource;
    std::unordered_set<std::string> attribute_names;
    for (const auto &attribute : child.attributes()) {
      const std::string_view name(attribute.name());
      const std::string_view value(attribute.value());
      if (!attribute_names.emplace(name).second) {
        return node_error(source, child, UiDocumentErrorCode::InvalidProperty,
                          "XML resource contains a duplicate attribute: " + std::string(name));
      }
      if (name.size() > limits.max_string_bytes || value.size() > limits.max_string_bytes) {
        return node_error(source, child, UiDocumentErrorCode::StringLimitExceeded,
                          "XML resource attribute exceeds the string limit");
      }
      if (name == "type") {
        resource.type.assign(value);
      } else if (name == "id") {
        resource.id.assign(value);
      } else if (name == "path") {
        resource.path.assign(value);
      } else if (!is_namespace_declaration(name)) {
        if (resource.options.size() >= limits.max_properties_per_node) {
          return node_error(source, child, UiDocumentErrorCode::PropertyLimitExceeded,
                            "XML resource exceeds maximum option count: " + resource.id);
        }
        resource.options.emplace(std::string(name), parse_scalar(name, value));
      }
    }
    if (resource.type.empty() || resource.id.empty() || resource.path.empty()) {
      return node_error(source, child, UiDocumentErrorCode::InvalidProperty,
                        "XML resource requires non-empty type, id and path attributes");
    }
    if (!resource_ids.insert(resource.id).second) {
      return node_error(source, child, UiDocumentErrorCode::InvalidProperty,
                        "duplicate XML resource id: " + resource.id);
    }
    for (const auto &nested : child.children()) {
      if (nested.type() == pugi::node_element) {
        return node_error(source, nested, UiDocumentErrorCode::InvalidNode,
                          "XML resource elements must not contain child elements");
      }
      if ((nested.type() == pugi::node_pcdata || nested.type() == pugi::node_cdata) &&
          has_non_whitespace(nested.value())) {
        return node_error(source, nested, UiDocumentErrorCode::InvalidNode,
                          "XML resource elements must not contain text content");
      }
    }
    resources.push_back(std::move(resource));
  }
  return {};
}


// Compiler-only XML component definitions; these descriptors are never
// registered as runtime Widget factories or used as a second XML AST.
struct XmlComponentDefinition {
  UiDocumentDefinition templ;
  UiNodeDescriptor props;
  std::unordered_set<std::string> slots;
};
using XmlComponentCatalog = std::unordered_map<std::string, XmlComponentDefinition>;

bool xml_component_identifier(std::string_view name, bool type_name = false) {
  if (name.empty()) return false;
  const auto alpha = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  };
  if ((!alpha(name.front()) && name.front() != '_') ||
      (type_name && !(name.front() >= 'A' && name.front() <= 'Z')))
    return false;
  for (std::size_t i = 1; i < name.size(); ++i) {
    const char c = name[i];
    if (!alpha(c) && (c < '0' || c > '9') && c != '_' && c != '-')
      return false;
  }
  return true;
}

bool xml_attributes_only(const pugi::xml_node &node,
                         std::initializer_list<std::string_view> allowed) {
  std::unordered_set<std::string> used;
  for (const auto &attribute : node.attributes()) {
    const std::string_view name(attribute.name());
    if (is_namespace_declaration(name)) continue;
    if (std::find(allowed.begin(), allowed.end(), name) == allowed.end() ||
        !used.emplace(name).second)
      return false;
  }
  return true;
}

bool xml_structural_children_only(const pugi::xml_node &node) {
  for (const auto &child : node.children()) {
    if ((child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) &&
        has_non_whitespace(child.value()))
      return false;
  }
  return true;
}

UiDocumentError read_xml_components(std::string_view source,
                                    const pugi::xml_node &section,
                                    const UiDocumentLimits &limits,
                                    const WidgetRegistry &registry,
                                    XmlComponentCatalog &catalog) {
  if (!xml_attributes_only(section, {}) ||
      !xml_structural_children_only(section))
    return node_error(source, section, UiDocumentErrorCode::InvalidNode,
                      "components section accepts declarations only");
  for (const auto &decl : section.children()) {
    if (decl.type() != pugi::node_element) continue;
    if (std::string_view(decl.name()) != "component" ||
        !xml_attributes_only(decl, {"name"}) ||
        !xml_structural_children_only(decl))
      return node_error(source, decl, UiDocumentErrorCode::InvalidNode,
                        "component requires name, interface and template");
    const std::string name = decl.attribute("name").value();
    if (!xml_component_identifier(name, true) ||
        name.size() > limits.max_string_bytes ||
        registry.contains(name) || catalog.count(name) != 0 ||
        catalog.size() >= limits.max_nodes)
      return node_error(source, decl, UiDocumentErrorCode::InvalidNode,
                        "component name is invalid, duplicated or registered as a native tag");

    pugi::xml_node interface;
    pugi::xml_node templ;
    for (const auto &part : decl.children()) {
      if (part.type() != pugi::node_element) continue;
      const std::string_view tag(part.name());
      if (tag == "interface" && !interface) interface = part;
      else if (tag == "template" && !templ) templ = part;
      else
        return node_error(source, part, UiDocumentErrorCode::InvalidNode,
                          "component accepts exactly one interface and one template");
    }
    if (!interface || !templ ||
        !xml_attributes_only(interface, {}) ||
        !xml_attributes_only(templ, {}) ||
        !xml_structural_children_only(interface) ||
        !xml_structural_children_only(templ))
      return node_error(source, decl, UiDocumentErrorCode::InvalidNode,
                        "component interface and template must be structural");

    XmlComponentDefinition definition;
    definition.props.kind = UiNodeKind::Container;
    definition.props.reject_unknown_properties = true;
    for (const auto &item : interface.children()) {
      if (item.type() != pugi::node_element) continue;
      const std::string_view tag(item.name());
      if (!xml_structural_children_only(item))
        return node_error(source, item, UiDocumentErrorCode::InvalidNode,
                          "component interface declarations cannot contain text");
      for (const auto &nested : item.children()) {
        if (nested.type() == pugi::node_element)
          return node_error(source, nested, UiDocumentErrorCode::InvalidNode,
                            "component interface declarations must be empty");
      }
      if (tag == "prop") {
        if (!xml_attributes_only(item, {"name", "type", "required"}) ||
            definition.props.properties.size() >= limits.max_properties_per_node)
          return node_error(source, item, UiDocumentErrorCode::PropertyLimitExceeded,
                            "invalid or excessive component property declaration");
        const std::string_view prop_name(item.attribute("name").value());
        const std::string_view type(item.attribute("type").value());
        const std::string_view required(item.attribute("required").value());
        if (!xml_component_identifier(prop_name) ||
            prop_name.size() > limits.max_string_bytes ||
            prop_name == "id" || prop_name == "slot" ||
            (!required.empty() && required != "true" && required != "false"))
          return node_error(source, item, UiDocumentErrorCode::InvalidProperty,
                            "component property name or required flag is invalid");
        UiPropertySpec spec;
        spec.name.assign(prop_name);
        if (type == "string") spec.kind = UiPropertyKind::String;
        else if (type == "boolean") spec.kind = UiPropertyKind::Boolean;
        else if (type == "number") spec.kind = UiPropertyKind::Number;
        else if (type == "integer") spec.kind = UiPropertyKind::Integer;
        else
          return node_error(source, item, UiDocumentErrorCode::InvalidProperty,
                            "component property requires a supported exact type");
        spec.required = required == "true";
        definition.props.properties.push_back(std::move(spec));
      } else if (tag == "slot") {
        if (!xml_attributes_only(item, {"name"}) ||
            definition.slots.size() >= limits.max_properties_per_node)
          return node_error(source, item, UiDocumentErrorCode::InvalidNode,
                            "invalid or excessive named slot declaration");
        const std::string_view slot_name(item.attribute("name").value());
        if (!xml_component_identifier(slot_name) ||
            slot_name.size() > limits.max_string_bytes ||
            !definition.slots.emplace(slot_name).second)
          return node_error(source, item, UiDocumentErrorCode::DuplicateSlot,
                            "component interface slot requires a unique valid name");
      } else {
        return node_error(source, item, UiDocumentErrorCode::InvalidNode,
                          "unsupported component interface declaration");
      }
    }
    if (!definition.props.valid())
      return node_error(source, interface, UiDocumentErrorCode::InvalidProperty,
                        "invalid or duplicate typed component Props");
    pugi::xml_node templ_root;
    for (const auto &node : templ.children()) {
      if (node.type() != pugi::node_element) continue;
      if (templ_root)
        return node_error(source, node, UiDocumentErrorCode::InvalidRootCount,
                          "component template must have exactly one root");
      templ_root = node;
    }
    if (!templ_root)
      return node_error(source, templ, UiDocumentErrorCode::InvalidRootCount,
                        "component template requires one native Element root");
    definition.templ.name = name;
    std::size_t parsed_nodes = 0;
    std::unordered_set<std::string> local_ids;
    if (auto error = parse_widget_node(source, templ_root, limits, nullptr,
                                        1, parsed_nodes, local_ids,
                                        definition.templ.root))
      return error;
    std::unordered_set<std::string> declared_outlets;
    struct Pending {
      const UiNodeDefinition *node;
      bool root;
    };
    std::vector<Pending> pending{{&definition.templ.root, true}};
    while (!pending.empty()) {
      const auto current = pending.back();
      pending.pop_back();
      const auto &node = *current.node;
      if (node.tag == "slot") {
        const auto prop = node.properties.find("name");
        const auto *slot = prop != node.properties.end()
                               ? std::get_if<std::string>(&prop->second)
                               : nullptr;
        if (current.root || !node.id.empty() || !node.children.empty() ||
            node.properties.size() != 1 || !slot ||
            !xml_component_identifier(*slot) ||
            definition.slots.count(*slot) == 0)
          return make_error(UiDocumentErrorCode::InvalidNode,
                            "template outlet must match one declared named slot",
                            node.source.line, node.source.column);
        if (!declared_outlets.insert(*slot).second)
          return make_error(UiDocumentErrorCode::DuplicateSlot,
                            "template contains duplicate slot outlet",
                            node.source.line, node.source.column);
      } else if (!registry.contains(node.tag)) {
        return make_error(UiDocumentErrorCode::UnknownElementTag,
                          "component template contains an unknown native tag: " + node.tag,
                          node.source.line, node.source.column);
      }
      for (const auto &child : node.children) pending.push_back({&child, false});
    }
    if (declared_outlets != definition.slots)
      return node_error(source, templ, UiDocumentErrorCode::MissingSlot,
                        "component template must expose each interface slot exactly once");
    catalog.emplace(name, std::move(definition));
  }
  return {};
}


// A component call is an ordinary structural host in the one application
// program. Its template is a static scoped fragment; no ComponentWidget,
// private Box, runtime template executor or synthetic parser is created.
UiDocumentError lower_xml_component_calls(
    std::string_view source, const pugi::xml_node &xml_node,
    UiNodeDefinition &node, const UiDocumentLimits &limits,
    const WidgetRegistry &registry, const XmlComponentCatalog &catalog,
    std::vector<UiScopedComponent> &instances, bool allow_calls) {
  // Namespace prefixes are fixed by the application's explicit root
  // import. No descendant may shadow an imported binding: pugixml retains
  // qualified tag names, not a second namespace-aware type registry.
  for (const auto &attribute : xml_node.attributes()) {
    const std::string_view name(attribute.name());
    if (name.rfind("xmlns:", 0) != 0) continue;
    const std::string prefix(name.substr(6));
    for (const auto &entry : catalog) {
      if (entry.first.rfind(prefix + ":", 0) == 0)
        return node_error(source, xml_node, UiDocumentErrorCode::InvalidComponentScope,
                          "imported component namespace cannot be rebound in a subtree");
    }
  }
  const auto component = catalog.find(std::string(xml_node.name()));
  if (component != catalog.end()) {
    if (!allow_calls)
      return node_error(source, xml_node, UiDocumentErrorCode::InvalidComponentScope,
                        "nested component calls inside a caller slot are not admitted");
    if (!xml_component_identifier(node.id) ||
        node.id.size() > limits.max_string_bytes / 2 ||
        instances.size() >= limits.max_nodes)
      return node_error(source, xml_node, UiDocumentErrorCode::InvalidComponentScope,
                        "component call requires a bounded unique ASCII id/scope");

    UiNodeDefinition call;
    call.tag = node.tag;
    call.id = node.id;
    call.source = node.source;
    for (const auto &attribute : xml_node.attributes()) {
      const std::string_view raw_name(attribute.name());
      if (raw_name == "id" || is_namespace_declaration(raw_name)) continue;
      std::string name;
      if (!canonical_property_name(raw_name, name))
        return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                          "invalid component call property");
      const auto *spec = component->second.props.property(name);
      if (!spec)
        return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                          "component call contains undeclared property: " + name);
      const std::string_view raw_value(attribute.value());
      if (raw_value.size() > limits.max_string_bytes)
        return node_error(source, xml_node, UiDocumentErrorCode::StringLimitExceeded,
                          "component property exceeds the string budget");
      UiDocumentValue value;
      if (!parse_typed_property(spec->kind, raw_value, value))
        return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                          "component call property violates declared type: " + name);
      if (!call.properties.emplace(name, std::move(value)).second)
        return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                          "component call contains duplicate property: " + name);
      call.property_spans.emplace(name, node.source);
    }
    if (auto error = component->second.props.validate_properties(call)) return error;

    UiDocumentDefinition templ = component->second.templ;
    std::vector<UiNodeDefinition *> stack{&templ.root};
    while (!stack.empty()) {
      auto *element = stack.back();
      stack.pop_back();
      for (auto &[key, value] : element->properties) {
        const auto *string = std::get_if<std::string>(&value);
        if (!string || string->rfind("$props.", 0) != 0) continue;
        const std::string prop = string->substr(7);
        const auto supplied = call.properties.find(prop);
        if (!component->second.props.property(prop) ||
            supplied == call.properties.end() ||
            key.rfind("on.", 0) == 0 || key.rfind("bind.", 0) == 0)
          return make_error(UiDocumentErrorCode::InvalidProperty,
                            "invalid or unbound static component Prop: " + prop,
                            element->source.line, element->source.column);
        // The replacement is a typed AST value, never rendered as XML text.
        // It cannot introduce tags, handler names or executable expressions.
        value = supplied->second;
      }
      for (auto &child : element->children) stack.push_back(&child);
    }
    auto template_program = compile_ui_definition(std::move(templ), {}, limits);
    if (!template_program) return template_program.error;
    UiScopedComponent instance;
    instance.scope = node.id;
    instance.host_id = node.id;
    instance.program = std::move(template_program.program);

    std::unordered_set<std::string> provided_slots;
    std::size_t ordinal = 0;
    for (const auto &child : xml_node.children()) {
      if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
        if (has_non_whitespace(child.value()))
          return node_error(source, child, UiDocumentErrorCode::InvalidNode,
                            "component call accepts only named fill elements");
        continue;
      }
      if (child.type() != pugi::node_element) continue;
      if (ordinal >= node.children.size() ||
          std::string_view(child.name()) != "fill" ||
          !xml_attributes_only(child, {"name"}) ||
          !xml_structural_children_only(child))
        return node_error(source, child, UiDocumentErrorCode::InvalidNode,
                          "component caller children must be named fill elements");
      const std::string slot(child.attribute("name").value());
      if (component->second.slots.count(slot) == 0 ||
          !provided_slots.emplace(slot).second)
        return node_error(source, child, UiDocumentErrorCode::DuplicateSlot,
                          "unknown or repeated caller slot fill: " + slot);

      const auto &parsed_fill = node.children[ordinal++];
      pugi::xml_node fill_root;
      for (const auto &candidate : child.children()) {
        if (candidate.type() != pugi::node_element) continue;
        if (fill_root)
          return node_error(source, candidate, UiDocumentErrorCode::InvalidRootCount,
                            "one named slot fill requires exactly one root element");
        fill_root = candidate;
      }
      if (!fill_root || parsed_fill.children.size() != 1 ||
          parsed_fill.properties.size() != 1 ||
          parsed_fill.properties.count("name") != 1 ||
          parsed_fill.properties.count("text") != 0)
        return node_error(source, child, UiDocumentErrorCode::InvalidNode,
                          "named slot fill must contain exactly one native XML root");

      UiDocumentDefinition filled;
      filled.name = "SlotFill";
      filled.root = parsed_fill.children.front();
      if (auto error = lower_xml_component_calls(
              source, fill_root, filled.root, limits, registry,
              catalog, instances, false))
        return error;
      auto compiled_fill = compile_ui_definition(std::move(filled), {}, limits);
      if (!compiled_fill) return compiled_fill.error;
      instance.slot_fills.push_back({slot, std::move(compiled_fill.program)});
    }
    if (provided_slots.size() != component->second.slots.size())
      return node_error(source, xml_node, UiDocumentErrorCode::MissingSlot,
                        "component call must fill each required named slot");

    // Erase compile-only invocation/fill/Props nodes from the real app tree.
    // The existing scoped assembler later appends this instance's native
    // template, substitutes its caller roots, and revalidates one program.
    UiNodeDefinition host;
    host.tag = "div";
    host.id = node.id;
    host.source = node.source;
    node = std::move(host);
    instances.push_back(std::move(instance));
    return {};
  }

  const auto *schema = registry.descriptor(node.tag);
  if (!schema)
    return node_error(source, xml_node, UiDocumentErrorCode::UnknownElementTag,
                      "unknown native or declared component tag: " + node.tag);
  // The ordinary native XML body uses the SAME typed parsing rule as #357.
  // This normalization is only needed for documents containing components,
  // whose compile-only invocation/fill tags were first parsed structurally.
  for (const auto &attribute : xml_node.attributes()) {
    const std::string_view raw_name(attribute.name());
    if (raw_name == "id" || is_namespace_declaration(raw_name)) continue;
    std::string canonical;
    if (!canonical_property_name(raw_name, canonical)) continue;
    const auto *spec = schema->property(canonical);
    if (!spec) continue;
    UiDocumentValue value;
    if (!parse_typed_property(spec->kind, attribute.value(), value))
      return node_error(source, xml_node, UiDocumentErrorCode::InvalidProperty,
                        "native XML property violates the declared type: " + canonical);
    node.properties[canonical] = std::move(value);
  }
  std::size_t ordinal = 0;
  for (const auto &child : xml_node.children()) {
    if (child.type() != pugi::node_element) continue;
    if (ordinal >= node.children.size())
      return node_error(source, child, UiDocumentErrorCode::InvalidNode,
                        "XML node/definition child identity mismatch");
    if (auto error = lower_xml_component_calls(
            source, child, node.children[ordinal++], limits, registry,
            catalog, instances, allow_calls))
      return error;
  }
  if (ordinal != node.children.size())
    return node_error(source, xml_node, UiDocumentErrorCode::InvalidNode,
                      "XML node/definition children cannot be reconciled");
  return {};
}


// A module is a source-identity-bound, immutable input supplied by the real
// compiler host. No source lookup, path normalization, file access, native
// loader callback, network fetch or second template engine exists here.
bool xml_module_uri(std::string_view uri) {
  if (uri.size() < 5 || uri.rfind("urn:", 0) != 0) return false;
  bool segment = false;
  for (std::size_t i = 4; i < uri.size(); ++i) {
    const char c = uri[i];
    if (c == ':') {
      if (!segment) return false;
      segment = false;
    } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_') {
      segment = true;
    } else {
      return false;
    }
  }
  return segment;
}

bool xml_module_source_key(std::string_view name) {
  if (name.empty() || name.front() == '/' || name.front() == '.' ||
      name.find('\\') != std::string_view::npos) return false;
  std::size_t begin = 0;
  while (begin < name.size()) {
    const auto end = name.find('/', begin);
    const auto length = (end == std::string_view::npos ? name.size() : end) - begin;
    if (length == 0) return false;
    const auto segment = name.substr(begin, length);
    if (segment == "." || segment == ".." || segment.back() == '.') return false;
    for (const char c : segment) {
      if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'))
        return false;
    }
    if (end == std::string_view::npos) return true;
    begin = end + 1;
  }
  return false;
}

bool xml_module_version(std::string_view text) {
  if (text.empty() || text.front() == '0') return false;
  unsigned version = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), version);
  return parsed.ec == std::errc{} &&
         parsed.ptr == text.data() + text.size() && version != 0;
}

bool xml_module_sha256(
    std::string_view source, std::string_view hexdigest,
    const UiXmlSha256Provider &provider) {
  constexpr char hex[] = "0123456789abcdef";
  if (hexdigest.size() != 64 || provider.compute == nullptr) return false;
  std::array<std::uint8_t, 32> digest{};
  // One required externally-owned, synchronous canonical provider. No
  // fallback or implicit dependency on stale Crypto exports from old SDKs.
  if (!provider.compute(source.data(), source.size(), digest.data(),
                        provider.userdata))
    return false;
  for (std::size_t i = 0; i < digest.size(); ++i) {
    if (hexdigest[i * 2] != hex[digest[i] >> 4] ||
        hexdigest[i * 2 + 1] != hex[digest[i] & 15u]) return false;
  }
  return true;
}

struct XmlImportDeclaration {
  UiXmlImportSpec spec;
  pugi::xml_node node; // Borrowed only while the one XML document is alive.
};

// One source of policy for both host import discovery and admission.
// Discovering keys does not authorize reading their bytes or skip hash checks.
UiDocumentError parse_xml_import_declarations(
    std::string_view source, const pugi::xml_node &ui,
    const pugi::xml_node &imports, const UiDocumentLimits &limits,
    std::vector<XmlImportDeclaration> &out) {
  if (!xml_attributes_only(imports, {}) ||
      !xml_structural_children_only(imports))
    return node_error(source, imports, UiDocumentErrorCode::InvalidNode,
                      "imports may contain only declared module references");
  std::unordered_set<std::string> used_sources, used_uris, used_prefixes;
  for (const auto &entry : imports.children()) {
    if (entry.type() != pugi::node_element) continue;
    if (out.size() >= limits.max_resources || out.size() >= limits.max_nodes ||
        std::string_view(entry.name()) != "import" ||
        !xml_attributes_only(entry,
                             {"namespace", "prefix", "source", "version", "sha256"}) ||
        !xml_structural_children_only(entry))
      return node_error(source, entry, UiDocumentErrorCode::InvalidNode,
                        "imports require bounded standalone import declarations");
    for (const auto &nested : entry.children()) {
      if (nested.type() == pugi::node_element)
        return node_error(source, nested, UiDocumentErrorCode::InvalidNode,
                          "import declarations must not contain child elements");
    }
    UiXmlImportSpec spec;
    spec.namespace_uri = entry.attribute("namespace").value();
    spec.prefix = entry.attribute("prefix").value();
    spec.source = entry.attribute("source").value();
    spec.version = entry.attribute("version").value();
    spec.sha256 = entry.attribute("sha256").value();
    if (spec.namespace_uri.size() > limits.max_string_bytes ||
        spec.prefix.size() > limits.max_string_bytes ||
        spec.source.size() > limits.max_string_bytes ||
        spec.version.size() > limits.max_string_bytes ||
        spec.sha256.size() != 64 ||
        !std::all_of(spec.sha256.begin(), spec.sha256.end(), [](char ch) {
          return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        }) ||
        !xml_module_uri(spec.namespace_uri) ||
        !xml_component_identifier(spec.prefix) ||
        spec.prefix == "xml" || spec.prefix == "xmlns" ||
        spec.prefix == "on" || spec.prefix == "bind" ||
        spec.prefix == "attr" || spec.prefix == "var" ||
        !xml_module_source_key(spec.source) ||
        !xml_module_version(spec.version) ||
        !used_sources.emplace(spec.source).second ||
        !used_uris.emplace(spec.namespace_uri).second ||
        !used_prefixes.emplace(spec.prefix).second)
      return node_error(source, entry, UiDocumentErrorCode::InvalidComponentScope,
                        "import requires unique namespace, prefix, safe source, "
                        "positive version and lowercase SHA-256");
    const std::string xmlns_key = "xmlns:" + spec.prefix;
    std::size_t bound = 0;
    for (const auto &attr : ui.attributes()) {
      if (xmlns_key != attr.name()) continue;
      if (std::string_view(attr.value()) != spec.namespace_uri)
        return node_error(source, entry, UiDocumentErrorCode::InvalidComponentScope,
                          "import prefix must match its root namespace URI");
      ++bound;
    }
    if (bound != 1)
      return node_error(source, entry, UiDocumentErrorCode::InvalidComponentScope,
                        "import requires one matching root xmlns declaration");
    out.push_back({std::move(spec), entry});
  }
  return {};
}

UiDocumentError read_xml_imports(
    std::string_view application_source, const pugi::xml_node &ui,
    const pugi::xml_node &imports, const UiDocumentLimits &limits,
    const WidgetRegistry &registry, const std::vector<UiXmlModuleSource> &modules,
    const UiXmlSha256Provider &provider, XmlComponentCatalog &catalog) {
  std::vector<XmlImportDeclaration> declarations;
  if (auto error = parse_xml_import_declarations(
          application_source, ui, imports, limits, declarations))
    return error;
  if (modules.size() > limits.max_resources || modules.size() > limits.max_nodes)
    return node_error(application_source, imports, UiDocumentErrorCode::ResourceLimitExceeded,
                      "source catalog exceeds the declared module count budget");
  std::unordered_set<std::string> catalog_names;
  for (const auto &module : modules) {
    if (module.source.size() > limits.max_string_bytes ||
        !xml_module_source_key(module.source) ||
        !catalog_names.emplace(module.source).second)
      return node_error(application_source, imports, UiDocumentErrorCode::InvalidProperty,
                        "module source catalog must contain unique safe relative keys");
  }
  std::size_t admitted_bytes = application_source.size();
  for (const auto &decl : declarations) {
    const pugi::xml_node &entry = decl.node;
    const std::string &uri = decl.spec.namespace_uri;
    const std::string &version = decl.spec.version;
    const std::string &prefix = decl.spec.prefix;
    const std::string &source_key = decl.spec.source;
    const std::string &expected_hash = decl.spec.sha256;
    const UiXmlModuleSource *selected = nullptr;
    for (const auto &candidate : modules) {
      if (candidate.source == source_key) {
        selected = &candidate;
        break;
      }
    }
    if (!selected)
      return node_error(application_source, entry, UiDocumentErrorCode::InvalidComponentScope,
                        "import source is absent from the trusted immutable source catalog");
    if (admitted_bytes > limits.max_source_bytes ||
        selected->xml.size() > limits.max_source_bytes - admitted_bytes)
      return node_error(application_source, entry, UiDocumentErrorCode::SourceTooLarge,
                        "application and imported module sources exceed the cumulative byte budget");
    admitted_bytes += selected->xml.size();
    if (!xml_module_sha256(selected->xml, expected_hash, provider))
      return node_error(application_source, entry, UiDocumentErrorCode::InvalidComponentScope,
                        "imported module content does not match the declared SHA-256");

    const auto &text = selected->xml;
    if (text.find('\0') != std::string::npos)
      return node_error(application_source, entry, UiDocumentErrorCode::ParseError,
                        "module source contains an embedded NUL");
    if (auto error = detail::validate_ui_xml_utf8(text)) {
      error.message = "module " + source_key + ": " + error.message;
      return error;
    }
    pugi::xml_document document;
    const auto parsed = document.load_buffer(
        text.data(), text.size(), pugi::parse_default | pugi::parse_ws_pcdata,
        pugi::encoding_utf8);
    if (!parsed) {
      const auto place = detail::xml_source_span(text, parsed.offset);
      return make_error(UiDocumentErrorCode::ParseError,
                        "module " + source_key + ": invalid XML: " + parsed.description(),
                        place.line, place.column);
    }
    const auto root = document.document_element();
    if (!root || std::string_view(root.name()) != "module" ||
        !xml_attributes_only(root, {"namespace", "version"}) ||
        !xml_structural_children_only(root) ||
        root.attribute("namespace").value() != uri ||
        root.attribute("version").value() != version)
      return node_error(application_source, entry, UiDocumentErrorCode::InvalidComponentScope,
                        "module identity must match its declared namespace and exact version");
    pugi::xml_node definitions;
    for (const auto &part : root.children()) {
      if (part.type() != pugi::node_element) continue;
      if (std::string_view(part.name()) != "components" || definitions)
        return node_error(text, part, UiDocumentErrorCode::InvalidNode,
                          "modules accept only one components section, no recursive imports");
      definitions = part;
    }
    if (!definitions)
      return node_error(application_source, entry, UiDocumentErrorCode::InvalidNode,
                        "imported module needs one components declaration section");
    XmlComponentCatalog namespace_definitions;
    if (auto error = read_xml_components(
            text, definitions, limits, registry, namespace_definitions)) {
      error.message = "module " + source_key + ": " + error.message;
      return error;
    }
    for (auto &definition : namespace_definitions) {
      const std::string qualified = prefix + ":" + definition.first;
      if (qualified.size() > limits.max_string_bytes ||
          catalog.size() >= limits.max_nodes ||
          !catalog.emplace(qualified, std::move(definition.second)).second)
        return node_error(application_source, entry, UiDocumentErrorCode::InvalidNode,
                          "import collides with an existing qualified component");
    }
  }
  if (declarations.size() != modules.size())
    return node_error(application_source, imports, UiDocumentErrorCode::InvalidComponentScope,
                      "source catalog contains an unreferenced or unadmitted module");
  return {};
}

XmlSource parse_xml_source(
    std::string_view source, const UiDocumentLimits &limits,
    const WidgetRegistry *registry = nullptr,
    const std::vector<UiXmlModuleSource> *modules = nullptr,
    const UiXmlSha256Provider *provider = nullptr) {
  XmlSource result;
  if (source.size() > limits.max_source_bytes) {
    result.error =
        make_error(UiDocumentErrorCode::SourceTooLarge, "UI XML source exceeds configured limit");
    return result;
  }
  if (source.find('\0') != std::string_view::npos) {
    result.error = make_error(UiDocumentErrorCode::ParseError,
                              "UI XML source must not contain embedded NUL bytes");
    return result;
  }

  result.error = detail::validate_ui_xml_utf8(source);
  if (result.error) {
    return result;
  }

  pugi::xml_document document;
  const auto parsed =
      document.load_buffer(source.data(), source.size(), pugi::parse_default | pugi::parse_ws_pcdata, pugi::encoding_utf8);
  if (!parsed) {
    const auto location = detail::xml_source_span(source, parsed.offset);
    result.error = make_error(UiDocumentErrorCode::ParseError,
                              std::string("invalid UI XML: ") + parsed.description(), location.line,
                              location.column);
    return result;
  }

  const auto ui = document.document_element();
  if (!ui || std::string_view(ui.name()) != "ui") {
    result.error =
        make_error(UiDocumentErrorCode::MissingDocument, "UI XML requires one ui document element");
    return result;
  }
  for (const auto &attribute : ui.attributes()) {
    const std::string_view name(attribute.name());
    if (name != "name" && !is_namespace_declaration(name)) {
      result.error = node_error(source, ui, UiDocumentErrorCode::InvalidProperty,
                                "ui element accepts only name and namespace attributes");
      return result;
    }
  }
  result.definition.name = ui.attribute("name").value();
  if (result.definition.name.empty()) {
    result.error = node_error(source, ui, UiDocumentErrorCode::InvalidNode,
                              "UI XML requires a non-empty document name");
    return result;
  }
  if (result.definition.name.size() > limits.max_string_bytes) {
    result.error = node_error(source, ui, UiDocumentErrorCode::StringLimitExceeded,
                              "UI XML document name exceeds the string limit");
    return result;
  }

  pugi::xml_node widget_root;
  pugi::xml_node components_root;
  pugi::xml_node imports_root;
  bool saw_resources = false;
  for (const auto &child : ui.children()) {
    if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
      if (has_non_whitespace(child.value())) {
        result.error = node_error(source, child, UiDocumentErrorCode::InvalidNode,
                                  "ui element must not contain text content");
        return result;
      }
      continue;
    }
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (std::string_view(child.name()) == "imports" && modules != nullptr) {
      if (imports_root) {
        result.error = node_error(source, child, UiDocumentErrorCode::InvalidRootCount,
                                  "UI XML may declare one imports section");
        return result;
      }
      imports_root = child;
    } else if (std::string_view(child.name()) == "components" && registry != nullptr) {
      if (components_root) {
        result.error = node_error(source, child, UiDocumentErrorCode::InvalidRootCount,
                                  "UI XML may declare one components section");
        return result;
      }
      components_root = child;
    } else if (std::string_view(child.name()) == "resources") {
      if (saw_resources) {
        result.error = node_error(source, child, UiDocumentErrorCode::InvalidRootCount,
                                  "UI XML may contain at most one resources section");
        return result;
      }
      saw_resources = true;
      result.error = parse_resources(source, child, limits, result.resources);
      if (result.error) {
        return result;
      }
    } else if (widget_root) {
      result.error = node_error(source, child, UiDocumentErrorCode::InvalidRootCount,
                                "UI XML must contain exactly one widget root");
      return result;
    } else {
      widget_root = child;
    }
  }
  if (!widget_root) {
    result.error = node_error(source, ui, UiDocumentErrorCode::InvalidRootCount,
                              "UI XML must contain exactly one widget root");
    return result;
  }

  std::size_t node_count = 0;
  std::unordered_set<std::string> ids;
  // Components are compile-only declarations. Only the registry-aware
  // frontend admits them; the legacy XML frontend remains unchanged.
  XmlComponentCatalog components;
  if (imports_root) {
    if (provider == nullptr || provider->compute == nullptr) {
      result.error = node_error(
          source, imports_root, UiDocumentErrorCode::InvalidComponentScope,
          "imported modules require an explicit trusted SHA-256 host provider");
      return result;
    }
    if (auto error = read_xml_imports(
            source, ui, imports_root, limits, *registry, *modules,
            *provider, components)) {
      result.error = std::move(error);
      return result;
    }
  } else if (modules != nullptr && !modules->empty()) {
    result.error = node_error(source, ui, UiDocumentErrorCode::InvalidComponentScope,
                              "supplied immutable module catalog needs explicit XML imports");
    return result;
  }
  if (components_root) {
    result.error = read_xml_components(source, components_root, limits, *registry, components);
    if (result.error) return result;
  }
  result.error = parse_widget_node(source, widget_root, limits,
                                   (components_root || imports_root) ? nullptr : registry,
                                   1, node_count, ids, result.definition.root);
  if (result.error) return result;
  if (components_root || imports_root) {
    result.error = lower_xml_component_calls(
        source, widget_root, result.definition.root, limits, *registry,
        components, result.scoped_components, true);
  }
  return result;
}

} // namespace

UiDocumentParseResult parse_ui_xml(std::string_view source, const UiDocumentLimits &limits) {
  auto parsed = parse_xml_source(source, limits);
  if (!parsed) {
    return {{}, std::move(parsed.error)};
  }
  auto compiled =
      compile_ui_definition(std::move(parsed.definition), std::move(parsed.resources), limits);
  if (!compiled) {
    return {{}, std::move(compiled.error)};
  }
  return {std::make_shared<const UiDocumentDefinition>(compiled.program->definition()), {}};
}

UiDocumentCompileResult compile_ui_xml(std::string_view source, const UiDocumentLimits &limits) {
  auto parsed = parse_xml_source(source, limits);
  if (!parsed) {
    return {{}, std::move(parsed.error)};
  }
  return compile_ui_definition(std::move(parsed.definition), std::move(parsed.resources), limits);
}

UiDocumentCompileResult compile_ui_xml(std::string_view source,
                                       const WidgetRegistry &registry,
                                       const UiDocumentLimits &limits) {
  auto parsed = parse_xml_source(source, limits, &registry);
  if (!parsed) return {{}, std::move(parsed.error)};
  auto compiled = compile_ui_definition(std::move(parsed.definition),
                                        std::move(parsed.resources), limits);
  if (!compiled) return compiled;
  if (!parsed.scoped_components.empty())
    return compile_scoped_ui_components(*compiled.program,
                                        parsed.scoped_components, registry, limits);
  if (auto error = registry.validate(*compiled.program, limits))
    return {{}, std::move(error)};
  return compiled;
}

UiDocumentParseResult parse_ui_xml(std::string_view source,
                                   const WidgetRegistry &registry,
                                   const UiDocumentLimits &limits) {
  auto compiled = compile_ui_xml(source, registry, limits);
  if (!compiled) return {{}, std::move(compiled.error)};
  return {std::make_shared<const UiDocumentDefinition>(compiled.program->definition()), {}};
}

UiDocumentCompileResult compile_ui_xml(
    std::string_view source, const WidgetRegistry &registry,
    const std::vector<UiXmlModuleSource> &modules,
    const UiXmlSha256Provider &provider,
    const UiDocumentLimits &limits) {
  auto parsed = parse_xml_source(source, limits, &registry, &modules, &provider);
  if (!parsed) return {{}, std::move(parsed.error)};
  auto compiled = compile_ui_definition(
      std::move(parsed.definition), std::move(parsed.resources), limits);
  if (!compiled) return compiled;
  if (!parsed.scoped_components.empty())
    return compile_scoped_ui_components(
        *compiled.program, parsed.scoped_components, registry, limits);
  if (auto error = registry.validate(*compiled.program, limits))
    return {{}, std::move(error)};
  return compiled;
}


UiXmlImportInspectionResult inspect_ui_xml_imports(
    std::string_view source, const UiDocumentLimits &limits) {
  UiXmlImportInspectionResult result;
  if (source.size() > limits.max_source_bytes) {
    result.error = make_error(UiDocumentErrorCode::SourceTooLarge,
                              "UI XML import manifest exceeds the source byte limit");
    return result;
  }
  if (source.find('\0') != std::string_view::npos) {
    result.error = make_error(UiDocumentErrorCode::ParseError,
                              "UI XML import manifest contains NUL bytes");
    return result;
  }
  result.error = detail::validate_ui_xml_utf8(source);
  if (result.error) return result;
  pugi::xml_document document;
  const auto parsed = document.load_buffer(
      source.data(), source.size(), pugi::parse_default | pugi::parse_ws_pcdata,
      pugi::encoding_utf8);
  if (!parsed) {
    const auto at = detail::xml_source_span(source, parsed.offset);
    result.error = make_error(
        UiDocumentErrorCode::ParseError,
        std::string("invalid UI XML import manifest: ") + parsed.description(),
        at.line, at.column);
    return result;
  }
  const auto ui = document.document_element();
  if (!ui || std::string_view(ui.name()) != "ui" ||
      !xml_attributes_only(ui, {"name"}) ||
      !xml_structural_children_only(ui) ||
      std::string_view(ui.attribute("name").value()).empty() ||
      std::string_view(ui.attribute("name").value()).size() > limits.max_string_bytes) {
    result.error = make_error(UiDocumentErrorCode::MissingDocument,
                              "import inspection requires one named UI root");
    return result;
  }
  pugi::xml_node imports;
  bool saw_components = false, saw_resources = false, saw_widget = false;
  for (const auto &child : ui.children()) {
    if (child.type() != pugi::node_element) continue;
    const std::string_view tag(child.name());
    if (tag == "imports") {
      if (imports) {
        result.error = node_error(source, child, UiDocumentErrorCode::InvalidRootCount,
                                  "UI XML has more than one imports section");
        return result;
      }
      imports = child;
    } else if (tag == "components" && !saw_components) {
      saw_components = true;
    } else if (tag == "resources" && !saw_resources) {
      saw_resources = true;
    } else if ((tag == "components" || tag == "resources") || saw_widget) {
      result.error = node_error(source, child, UiDocumentErrorCode::InvalidRootCount,
                                "UI import manifest has an invalid root structure");
      return result;
    } else {
      saw_widget = true;
    }
  }
  if (!saw_widget) {
    result.error = node_error(source, ui, UiDocumentErrorCode::InvalidRootCount,
                              "UI import inspection requires one native application root");
    return result;
  }
  if (!imports) return result;
  std::vector<XmlImportDeclaration> declarations;
  result.error = parse_xml_import_declarations(
      source, ui, imports, limits, declarations);
  if (result.error) return result;
  result.imports.reserve(declarations.size());
  for (auto &entry : declarations)
    result.imports.push_back(std::move(entry.spec));
  return result;
}

} // namespace flexUI
