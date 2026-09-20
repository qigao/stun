#include <flexUI/ui_xml.h>
#include <flexUI/text_util.h>

#include <pugixml.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace flexUI {

namespace {

struct XmlSource {
  UiDocumentDefinition definition;
  std::vector<UiResourceDefinition> resources;
  UiDocumentError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

UiDocumentError make_error(UiDocumentErrorCode code, std::string message, int line = 0,
                           int column = 0) {
  return {code, std::move(message), line, column};
}

SourceSpan source_span(std::string_view source, std::ptrdiff_t offset) {
  SourceSpan span{1, 1, 0};
  if (offset <= 0) {
    return span;
  }
  const auto limit = static_cast<std::size_t>(offset) < source.size()
                         ? static_cast<std::size_t>(offset)
                         : source.size();
  for (std::size_t index = 0; index < limit; ++index) {
    if (source[index] == '\n') {
      ++span.line;
      span.column = 1;
    } else {
      ++span.column;
    }
  }
  return span;
}

UiDocumentError node_error(std::string_view source, const pugi::xml_node &node,
                           UiDocumentErrorCode code, std::string message) {
  const auto location = source_span(source, node.offset_debug());
  return make_error(code, std::move(message), location.line, location.column);
}

bool is_namespace_declaration(std::string_view name) {
  return name == "xmlns" || name.rfind("xmlns:", 0) == 0;
}

bool has_non_whitespace(std::string_view text) {
  return text.find_first_not_of(" \t\r\n") != std::string_view::npos;
}

bool is_string_property(std::string_view name) {
  return name == "class" || name == "classes" || name == "utility" || name == "utilities" ||
         name == "text" || name == "content" || name.rfind("on.", 0) == 0 ||
         name.rfind("bind.", 0) == 0 || name.rfind("attr.", 0) == 0;
}

bool canonical_property_name(std::string_view xml_name, std::string &result) {
  static constexpr std::pair<std::string_view, std::string_view> prefixes[] = {
      {"on:", "on."}, {"bind:", "bind."}, {"attr:", "attr."}};
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
      xml_name.rfind("bind.", 0) == 0 || xml_name.rfind("attr.", 0) == 0) {
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

UiDocumentError parse_widget_node(std::string_view source, const pugi::xml_node &xml_node,
                                  const UiDocumentLimits &limits, std::size_t depth,
                                  std::size_t &node_count, std::unordered_set<std::string> &ids,
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

  const auto id_attribute = xml_node.attribute("id");
  const std::string_view id(id_attribute.value());
  if (!id_attribute || id.empty()) {
    return node_error(source, xml_node, UiDocumentErrorCode::InvalidNode,
                      "XML widget requires a non-empty id attribute");
  }
  if (id.size() > limits.max_string_bytes) {
    return node_error(source, xml_node, UiDocumentErrorCode::StringLimitExceeded,
                      "XML widget id exceeds the string limit");
  }
  if (!ids.emplace(id).second) {
    return node_error(source, xml_node, UiDocumentErrorCode::DuplicateElementId,
                      "duplicate UI element id: " + std::string(id));
  }

  definition.tag.assign(tag);
  definition.id.assign(id);
  std::unordered_set<std::string> property_names;
  std::size_t id_attribute_count = 0;
  const auto location = source_span(source, xml_node.offset_debug());
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
    definition.properties.emplace(property_name, parse_scalar(property_name, value));
    definition.property_spans.emplace(std::move(property_name),
                                      SourceSpan{location.line, location.column, xml_name.size()});
  }

  for (const auto &child : xml_node.children()) {
    if (child.type() == pugi::node_element) {
      definition.children.emplace_back();
      auto error = parse_widget_node(source, child, limits, depth + 1, node_count, ids,
                                     definition.children.back());
      if (error) {
        return error;
      }
    } else if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
      const std::string_view text(child.value());
      if (has_non_whitespace(text)) {
        return node_error(source, child, UiDocumentErrorCode::InvalidNode,
                          "XML widget text nodes are not supported; use the text attribute");
      }
    }
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

XmlSource parse_xml_source(std::string_view source, const UiDocumentLimits &limits) {
  XmlSource result;
  if (source.size() > limits.max_source_bytes) {
    result.error =
        make_error(UiDocumentErrorCode::SourceTooLarge, "UI XML source exceeds configured limit");
    return result;
  }
  const auto utf8 = validate_utf8(source);
  if (!utf8.valid) {
    const auto location = source_span(source, static_cast<std::ptrdiff_t>(utf8.invalid_offset));
    result.error = make_error(UiDocumentErrorCode::InvalidUtf8,
                              "UI XML source contains invalid UTF-8",
                              location.line, location.column);
    return result;
  }
  if (source.find('\0') != std::string_view::npos) {
    result.error = make_error(UiDocumentErrorCode::ParseError,
                              "UI XML source must not contain embedded NUL bytes");
    return result;
  }

  pugi::xml_document document;
  const auto parsed =
      document.load_buffer(source.data(), source.size(), pugi::parse_default, pugi::encoding_utf8);
  if (!parsed) {
    const auto location = source_span(source, parsed.offset);
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
    if (std::string_view(child.name()) == "resources") {
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
  result.error =
      parse_widget_node(source, widget_root, limits, 1, node_count, ids, result.definition.root);
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

} // namespace flexUI
