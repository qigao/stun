#include <flexUI/shadcn_ir.h>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/dialog_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/listview_widget.h>
#include <flexUI/widgets/menu_widget.h>
#include <flexUI/widgets/notification_widget.h>
#include <flexUI/widgets/popover_widget.h>
#include <flexUI/widgets/radio_widget.h>
#include <flexUI/widgets/spinner_widget.h>
#include <flexUI/widgets/searchbox_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/sidebar_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/toolbar_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/accordion_widget.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace flexUI::shadcn_ir {

namespace {

std::string sanitize_identifier(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  bool last_dash = false;
  for (unsigned char ch : input) {
    if (std::isalnum(ch)) {
      out.push_back(static_cast<char>(std::tolower(ch)));
      last_dash = false;
    } else if (ch == '_' || ch == '-') {
      out.push_back(static_cast<char>(ch));
      last_dash = false;
    } else if (!last_dash) {
      out.push_back('-');
      last_dash = true;
    }
  }
  while (!out.empty() && out.back() == '-') {
    out.pop_back();
  }
  if (out.empty()) {
    return "component";
  }
  return out;
}

std::string replace_all(std::string text, const std::string& needle,
                        const std::string& replacement) {
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    text.replace(pos, needle.size(), replacement);
    pos += replacement.size();
  }
  return text;
}

std::string pseudo_for_state(const std::string& state) {
  static const std::map<std::string, std::string> pseudos = {
      {"active", ":active"},
      {"disabled", ":disabled"},
      {"focus", ":focus"},
      {"focus-visible", ":focus-visible"},
      {"focus-within", ":focus-within"},
      {"hover", ":hover"},
      {"invalid", ":invalid"},
      {"modal", ":modal"},
      {"open", ":open"},
      {"optional", ":optional"},
      {"placeholder-shown", ":placeholder-shown"},
      {"read-only", ":read-only"},
      {"read-write", ":read-write"},
      {"required", ":required"},
      {"selected", ":selected"},
      {"valid", ":valid"}};
  const auto it = pseudos.find(state);
  if (it != pseudos.end()) {
    return it->second;
  }
  return ":" + state;
}

std::string quote_attr_value(const Json& value) {
  if (value.is_boolean()) {
    return value.get<bool>() ? "true" : "false";
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<long long>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<unsigned long long>());
  }
  if (value.is_number_float()) {
    std::ostringstream out;
    out << value.get<double>();
    return out.str();
  }
  return value.get<std::string>();
}

std::string css_escape_class_name(const std::string& value) {
  std::ostringstream out;
  for (size_t i = 0; i < value.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    const bool is_name_char = std::isalnum(ch) || ch == '_' || ch == '-';
    const bool needs_hex_leading_escape =
        (i == 0 && std::isdigit(ch)) ||
        (i == 1 && value[0] == '-' && std::isdigit(ch));

    if (is_name_char && !needs_hex_leading_escape) {
      out << static_cast<char>(ch);
      continue;
    }

    if (needs_hex_leading_escape || ch < 0x20 || ch == 0x7f) {
      out << '\\' << std::uppercase << std::hex << static_cast<int>(ch)
          << std::nouppercase << std::dec << ' ';
      continue;
    }

    out << '\\' << static_cast<char>(ch);
  }
  return out.str();
}

std::string utility_selector(const std::string& class_token, const Json& token_ir) {
  const std::string base_selector = "." + css_escape_class_name(class_token);
  std::string selector = token_ir.value("selector", base_selector);
  if (selector.find('&') != std::string::npos) {
    selector = replace_all(selector, "&", base_selector);
  }

  if (token_ir.contains("when")) {
    for (const auto& condition : token_ir.at("when")) {
      const std::string type = condition.value("type", std::string());
      if (type == "state") {
        selector += pseudo_for_state(condition.value("name", std::string()));
      } else if (type == "attr") {
        const std::string name = condition.value("name", std::string());
        if (name.empty()) {
          continue;
        }
        if (condition.contains("equals") || condition.contains("value")) {
          const Json& expected =
              condition.contains("equals") ? condition.at("equals")
                                           : condition.at("value");
          selector += "[" + name + "=\"" + quote_attr_value(expected) + "\"]";
        } else {
          selector += "[" + name + "]";
        }
      }
    }
  }

  if (token_ir.value("kind", std::string()) == "pseudo") {
    selector += token_ir.value("pseudo", std::string());
  }

  return selector;
}

void emit_utility_rule(std::ostringstream& out, const Json& tokens,
                       const std::string& class_token,
                       const std::string& utility_token,
                       std::unordered_set<std::string>& visiting) {
  const auto token_it = tokens.find(utility_token);
  if (token_it == tokens.end() || !token_it->is_object()) {
    return;
  }

  const Json& token_ir = *token_it;
  const std::string kind = token_ir.value("kind", std::string());
  if (kind == "macro") {
    if (!token_ir.contains("expand") || !token_ir.at("expand").is_array()) {
      return;
    }
    if (!visiting.insert(utility_token).second) {
      return;
    }
    for (const auto& expanded : token_ir.at("expand")) {
      if (expanded.is_string()) {
        emit_utility_rule(out, tokens, class_token, expanded.get<std::string>(),
                          visiting);
      }
    }
    visiting.erase(utility_token);
    return;
  }

  if (!token_ir.contains("decls") || !token_ir.at("decls").is_object()) {
    return;
  }

  const bool has_media =
      token_ir.contains("media") && token_ir.at("media").is_string() &&
      !token_ir.at("media").get<std::string>().empty();
  if (has_media) {
    out << "@media " << token_ir.at("media").get<std::string>() << " {\n";
  }

  const std::string indent = has_media ? "  " : "";
  out << indent << utility_selector(class_token, token_ir) << " {\n";
  for (auto it = token_ir.at("decls").begin(); it != token_ir.at("decls").end();
       ++it) {
    out << indent << "  " << it.key() << ": " << it.value().get<std::string>()
        << ";\n";
  }
  out << indent << "}\n";
  if (has_media) {
    out << "}\n";
  }
}

bool rule_matches_variants(const Json& rule, const Json& resolved_variants) {
  if (!rule.contains("when")) {
    return true;
  }
  for (const auto& condition : rule.at("when")) {
    if (condition.value("type", std::string()) != "variant") {
      continue;
    }
    const std::string name = condition.value("name", std::string());
    if (name.empty()) {
      return false;
    }
    const auto it = resolved_variants.find(name);
    if (it == resolved_variants.end()) {
      return false;
    }
    const std::string expected =
        condition.contains("value") ? quote_attr_value(condition.at("value"))
                                    : quote_attr_value(condition.at("equals"));
    if (quote_attr_value(*it) != expected) {
      return false;
    }
  }
  return true;
}

std::string compose_selector(const Json& component_ir, const Json& rule,
                             const std::string& instance_id) {
  const std::string target = rule.at("target").get<std::string>();
  const std::string base_id = "#" + node_dom_id(component_ir, target, instance_id);

  std::string selector = rule.at("selector").get<std::string>();
  if (selector.find('&') != std::string::npos) {
    selector = replace_all(selector, "&", base_id);
  } else if (selector.empty()) {
    selector = base_id;
  } else if (selector[0] == ':' || selector[0] == '[') {
    selector = base_id + selector;
  }

  if (rule.contains("when")) {
    for (const auto& condition : rule.at("when")) {
      const std::string type = condition.value("type", std::string());
      if (type == "variant") {
        continue;
      }
      if (type == "state") {
        selector += pseudo_for_state(condition.value("name", std::string()));
      } else if (type == "attr") {
        const std::string name = condition.value("name", std::string());
        if (name.empty()) {
          continue;
        }
        if (condition.contains("equals")) {
          selector += "[" + name + "=\"" +
                      quote_attr_value(condition.at("equals")) + "\"]";
        } else {
          selector += "[" + name + "]";
        }
      }
    }
  }

  return selector;
}

void emit_rule_block(std::ostringstream& out, const Json& component_ir,
                     const Json& rule, const Json& resolved_variants,
                     const std::string& instance_id, int indent) {
  if (!rule_matches_variants(rule, resolved_variants)) {
    return;
  }

  const std::string selector = compose_selector(component_ir, rule, instance_id);
  const std::string pad(static_cast<size_t>(indent), ' ');
  out << pad << selector << " {\n";
  for (auto it = rule.at("decls").begin(); it != rule.at("decls").end(); ++it) {
    out << pad << "  " << it.key() << ": " << it.value().get<std::string>()
        << ";\n";
  }
  out << pad << "}\n";
}

Json merge_variants(const Json& component_ir, const Json& resolved_variants) {
  Json merged = Json::object();
  if (component_ir.contains("defaults")) {
    merged = component_ir.at("defaults");
  }
  for (auto it = resolved_variants.begin(); it != resolved_variants.end(); ++it) {
    merged[it.key()] = it.value();
  }
  return merged;
}

void emit_keyframes(std::ostringstream& out, const Json& keyframes) {
  out << "@keyframes " << keyframes.at("name").get<std::string>() << " {\n";
  for (const auto& step : keyframes.at("steps")) {
    const int pct = static_cast<int>(step.at("offset").get<double>() * 100.0 + 0.5);
    out << "  " << pct << "% {\n";
    for (auto it = step.at("decls").begin(); it != step.at("decls").end(); ++it) {
      out << "    " << it.key() << ": " << it.value().get<std::string>() << ";\n";
    }
    out << "  }\n";
  }
  out << "}\n";
}

void emit_rule_list(std::ostringstream& out, const Json& component_ir,
                    const Json& rules, const Json& resolved_variants,
                    const std::string& instance_id, int indent) {
  for (const auto& rule : rules) {
    emit_rule_block(out, component_ir, rule, resolved_variants, instance_id, indent);
  }
}

std::string prop_string(const Json& object, const char* key,
                        const std::string& fallback = "") {
  const auto it = object.find(key);
  if (it == object.end()) {
    return fallback;
  }
  return quote_attr_value(*it);
}

bool prop_bool(const Json& object, const char* key, bool fallback = false) {
  const auto it = object.find(key);
  if (it == object.end()) {
    return fallback;
  }
  if (it->is_boolean()) {
    return it->get<bool>();
  }
  if (it->is_number_integer()) {
    return it->get<int>() != 0;
  }
  if (it->is_string()) {
    const std::string lowered = sanitize_identifier(it->get<std::string>());
    return lowered == "true" || lowered == "1" || lowered == "yes" ||
           lowered == "open";
  }
  return fallback;
}

int prop_int(const Json& object, const char* key, int fallback = 0) {
  const auto it = object.find(key);
  if (it == object.end()) {
    return fallback;
  }
  if (it->is_number_integer()) {
    return it->get<int>();
  }
  if (it->is_boolean()) {
    return it->get<bool>() ? 1 : 0;
  }
  if (it->is_string()) {
    try {
      return std::stoi(it->get<std::string>());
    } catch (const std::exception&) {
      return fallback;
    }
  }
  return fallback;
}

float prop_float(const Json& object, const char* key, float fallback = 0.0f) {
  const auto it = object.find(key);
  if (it == object.end()) {
    return fallback;
  }
  if (it->is_number()) {
    return it->get<float>();
  }
  if (it->is_boolean()) {
    return it->get<bool>() ? 1.0f : 0.0f;
  }
  if (it->is_string()) {
    try {
      return std::stof(it->get<std::string>());
    } catch (const std::exception&) {
      return fallback;
    }
  }
  return fallback;
}

const Json* prop_json(const Json& primary, const Json& secondary, const char* key) {
  const auto primary_it = primary.find(key);
  if (primary_it != primary.end()) {
    return &(*primary_it);
  }
  const auto secondary_it = secondary.find(key);
  if (secondary_it != secondary.end()) {
    return &(*secondary_it);
  }
  return nullptr;
}

Date date_from_json(const Json& value, Date fallback) {
  if (value.is_object()) {
    Date date = fallback;
    if (value.contains("year") && value.at("year").is_number_integer()) {
      date.year = value.at("year").get<int>();
    }
    if (value.contains("month") && value.at("month").is_number_integer()) {
      date.month = value.at("month").get<int>();
    }
    if (value.contains("day") && value.at("day").is_number_integer()) {
      date.day = value.at("day").get<int>();
    }
    return date;
  }
  if (value.is_string()) {
    Date date = fallback;
    const std::string text = value.get<std::string>();
    if (std::sscanf(text.c_str(), "%d-%d-%d", &date.year, &date.month,
                    &date.day) == 3) {
      return date;
    }
  }
  return fallback;
}

Date prop_date(const Json& primary, const Json& secondary, const char* key,
               Date fallback) {
  if (const Json* value = prop_json(primary, secondary, key)) {
    return date_from_json(*value, fallback);
  }
  return fallback;
}

std::vector<std::string> json_string_list(const Json* value) {
  std::vector<std::string> items;
  if (!value || !value->is_array()) {
    return items;
  }

  for (const auto& item : *value) {
    items.push_back(quote_attr_value(item));
  }
  return items;
}

std::vector<DropdownWidget::Option> json_dropdown_options(const Json* value) {
  std::vector<DropdownWidget::Option> options;
  if (!value || !value->is_array()) {
    return options;
  }

  for (const auto& item : *value) {
    if (item.is_string()) {
      const std::string label = item.get<std::string>();
      options.push_back({label, label, false});
      continue;
    }
    if (!item.is_object()) {
      continue;
    }

    const std::string label = prop_string(item, "label", "");
    const std::string fallback_value =
        !label.empty() ? sanitize_identifier(label) : std::string();
    options.push_back(
        {label, prop_string(item, "value", fallback_value),
         prop_bool(item, "disabled", false)});
  }
  return options;
}

std::vector<MenuWidget::MenuItem> json_menu_items(const Json* value) {
  std::vector<MenuWidget::MenuItem> items;
  if (!value || !value->is_array()) {
    return items;
  }

  for (const auto& item : *value) {
    if (!item.is_object()) {
      continue;
    }
    if (prop_bool(item, "separator", false)) {
      items.push_back(MenuWidget::MenuItem::Separator());
      continue;
    }

    MenuWidget::MenuItem menu_item;
    menu_item.id = prop_string(item, "id", "");
    menu_item.label = prop_string(item, "label", "");
    menu_item.shortcut = prop_string(item, "shortcut", "");
    menu_item.enabled = prop_bool(item, "enabled", true);
    menu_item.checked = prop_bool(item, "checked", false);
    menu_item.children = json_menu_items(prop_json(item, Json::object(), "children"));
    items.push_back(std::move(menu_item));
  }
  return items;
}

std::vector<SearchBoxWidget::Suggestion> json_search_suggestions(const Json* value) {
  std::vector<SearchBoxWidget::Suggestion> suggestions;
  if (value == nullptr || !value->is_array()) {
    return suggestions;
  }

  for (const auto& item : *value) {
    if (!item.is_object()) {
      continue;
    }
    SearchBoxWidget::Suggestion suggestion;
    suggestion.id = prop_string(item, "id", "");
    suggestion.text = prop_string(item, "text", "");
    suggestion.description = prop_string(item, "description", "");
    suggestions.push_back(std::move(suggestion));
  }
  return suggestions;
}

std::vector<AccordionWidget::Section> json_accordion_sections(const Json* value) {
  std::vector<AccordionWidget::Section> sections;
  if (value == nullptr || !value->is_array()) {
    return sections;
  }

  for (const auto& section_value : *value) {
    if (!section_value.is_object()) {
      continue;
    }
    AccordionWidget::Section section;
    section.title = prop_string(section_value, "title", "");
    section.id = prop_string(section_value, "id",
                             sanitize_identifier(section.title.empty()
                                                     ? std::string("section")
                                                     : section.title));
    section.expanded = prop_bool(section_value, "expanded", false);
    section.content_height =
        static_cast<float>(prop_int(section_value, "content_height", 100));
    sections.push_back(std::move(section));
  }

  return sections;
}

std::vector<ToggleGroupWidget::Option> json_toggle_options(const Json* value) {
  std::vector<ToggleGroupWidget::Option> options;
  if (value == nullptr || !value->is_array()) {
    return options;
  }

  for (const auto& option_value : *value) {
    if (!option_value.is_object()) {
      continue;
    }
    const std::string label = prop_string(option_value, "label", "");
    options.push_back({prop_string(option_value, "id", sanitize_identifier(label)),
                       label});
  }

  return options;
}

SpinnerWidget::Variant spinner_variant_from_string(const std::string& value) {
  if (value == "dots") {
    return SpinnerWidget::Variant::Dots;
  }
  if (value == "bars") {
    return SpinnerWidget::Variant::Bars;
  }
  return SpinnerWidget::Variant::Ring;
}

std::vector<ListViewWidget::Item> json_listview_items(const Json* value) {
  std::vector<ListViewWidget::Item> items;
  if (value == nullptr || !value->is_array()) {
    return items;
  }

  for (const auto& item_value : *value) {
    if (!item_value.is_object()) {
      continue;
    }
    ListViewWidget::Item item;
    item.id = prop_string(item_value, "id", "");
    item.text = prop_string(item_value, "text", "");
    item.secondary = prop_string(item_value, "secondary", "");
    item.selected = prop_bool(item_value, "selected", false);
    items.push_back(std::move(item));
  }

  return items;
}

std::vector<ToolbarWidget::Item> json_toolbar_items(const Json* value) {
  std::vector<ToolbarWidget::Item> items;
  if (value == nullptr || !value->is_array()) {
    return items;
  }

  for (const auto& item_value : *value) {
    if (!item_value.is_object()) {
      continue;
    }

    ToolbarWidget::Item item;
    const std::string type = prop_string(item_value, "type", "button");
    if (type == "separator") {
      item.type = ToolbarWidget::Item::Separator;
    } else if (type == "toggle") {
      item.type = ToolbarWidget::Item::Toggle;
    } else if (type == "dropdown") {
      item.type = ToolbarWidget::Item::Dropdown;
    } else {
      item.type = ToolbarWidget::Item::Button;
    }

    item.id = prop_string(item_value, "id", "");
    item.icon = prop_string(item_value, "icon", "");
    item.tooltip = prop_string(item_value, "tooltip", "");
    item.enabled = prop_bool(item_value, "enabled", true);
    item.toggled = prop_bool(item_value, "toggled", false);
    if (const Json* dropdown_items =
            prop_json(item_value, Json::object(), "items");
        dropdown_items && dropdown_items->is_array()) {
      for (const auto& dropdown_value : *dropdown_items) {
        if (!dropdown_value.is_object()) {
          continue;
        }
        item.dropdown_items.emplace_back(
            prop_string(dropdown_value, "id", ""),
            prop_string(dropdown_value, "label", ""));
      }
    }
    items.push_back(std::move(item));
  }

  return items;
}

std::vector<SidebarWidget::Section> json_sidebar_sections(const Json* value) {
  std::vector<SidebarWidget::Section> sections;
  if (value == nullptr || !value->is_array()) {
    return sections;
  }

  for (const auto& section_value : *value) {
    if (!section_value.is_object()) {
      continue;
    }

    SidebarWidget::Section section;
    section.title = prop_string(section_value, "title", "");
    section.collapsed = prop_bool(section_value, "collapsed", false);
    if (const Json* items = prop_json(section_value, Json::object(), "items");
        items && items->is_array()) {
      for (const auto& item_value : *items) {
        if (!item_value.is_object()) {
          continue;
        }
        SidebarWidget::Item item;
        item.id = prop_string(item_value, "id", "");
        item.icon = prop_string(item_value, "icon", "");
        item.label = prop_string(item_value, "label", "");
        item.badge = prop_string(item_value, "badge", "");
        item.enabled = prop_bool(item_value, "enabled", true);
        section.items.push_back(std::move(item));
      }
    }
    sections.push_back(std::move(section));
  }

  return sections;
}

NotificationWidget::Position notification_position_from_string(
    const std::string& value) {
  if (value == "top-left") {
    return NotificationWidget::Position::TopLeft;
  }
  if (value == "bottom-right") {
    return NotificationWidget::Position::BottomRight;
  }
  if (value == "bottom-left") {
    return NotificationWidget::Position::BottomLeft;
  }
  return NotificationWidget::Position::TopRight;
}

NotificationWidget::Type notification_type_from_string(
    const std::string& value) {
  if (value == "success") {
    return NotificationWidget::Type::Success;
  }
  if (value == "warning") {
    return NotificationWidget::Type::Warning;
  }
  if (value == "error") {
    return NotificationWidget::Type::Error;
  }
  return NotificationWidget::Type::Info;
}

DialogWidget::Type dialog_type_from_props(const Json& props) {
  const std::string tone = prop_string(props, "tone", "default");
  if (tone == "warning") {
    return DialogWidget::Type::Warning;
  }
  if (tone == "error" || tone == "destructive") {
    return DialogWidget::Type::Error;
  }
  if (tone == "confirm") {
    return DialogWidget::Type::Confirm;
  }
  return DialogWidget::Type::Info;
}

PopoverWidget::Position popover_position_from_string(const std::string& value) {
  if (value == "top") {
    return PopoverWidget::Position::Top;
  }
  if (value == "left") {
    return PopoverWidget::Position::Left;
  }
  if (value == "right") {
    return PopoverWidget::Position::Right;
  }
  return PopoverWidget::Position::Bottom;
}

ToastWidget::Type toast_type_from_string(const std::string& value) {
  if (value == "success") {
    return ToastWidget::Type::Success;
  }
  if (value == "error") {
    return ToastWidget::Type::Error;
  }
  if (value == "warning") {
    return ToastWidget::Type::Warning;
  }
  if (value == "info") {
    return ToastWidget::Type::Info;
  }
  return ToastWidget::Type::Default;
}

TooltipWidget::Position tooltip_position_from_string(const std::string& value) {
  if (value == "bottom") {
    return TooltipWidget::Position::Bottom;
  }
  if (value == "left") {
    return TooltipWidget::Position::Left;
  }
  if (value == "right") {
    return TooltipWidget::Position::Right;
  }
  return TooltipWidget::Position::Top;
}

void apply_node_metadata(Element* elem, const Json& node) {
  if (!elem) {
    return;
  }
  if (node.contains("role")) {
    elem->set_attribute("role", node.at("role").get<std::string>());
  }
  if (node.contains("attrs")) {
    for (auto it = node.at("attrs").begin(); it != node.at("attrs").end(); ++it) {
      elem->set_attribute(it.key(), quote_attr_value(it.value()));
    }
  }
}

Element* instantiate_node(Box& box, const Json& component_ir, const Json& node,
                          const Json& props, const std::string& instance_id) {
  const std::string node_id = node.at("id").get<std::string>();
  const std::string dom_id = node_dom_id(component_ir, node_id, instance_id);
  const std::string tag = node.at("tag").get<std::string>();
  const std::string type = node.at("type").get<std::string>();
  const Json node_props = node.value("props", Json::object());

  if (type == "widget") {
    const std::string widget = node.at("widget").get<std::string>();
    if (widget == "button") {
      const std::string text = props.contains("text")
                                   ? prop_string(props, "text")
                                   : prop_string(node_props, "text");
      auto* elem = box.create_widget<ButtonWidget>(tag, dom_id, text);
      if (auto* button = dynamic_cast<ButtonWidget*>(elem->widget)) {
        button->set_disabled(prop_bool(props, "disabled", false));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "dialog") {
      const std::string title = props.contains("title")
                                    ? prop_string(props, "title")
                                    : prop_string(node_props, "title", "Dialog");
      auto* elem = box.create_widget<DialogWidget>(
          tag, dom_id, title, dialog_type_from_props(props));
      if (auto* dialog = dynamic_cast<DialogWidget*>(elem->widget)) {
        if (props.contains("message")) {
          dialog->set_message(prop_string(props, "message"));
        }
        dialog->set_alert_role(prop_bool(props, "alert_role", false));
        if (prop_bool(props, "open", false)) {
          dialog->show();
        } else {
          dialog->hide();
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "checkbox") {
      const std::string label = props.contains("label")
                                    ? prop_string(props, "label")
                                    : prop_string(node_props, "label");
      const bool checked = props.contains("checked")
                               ? prop_bool(props, "checked", false)
                               : prop_bool(node_props, "checked", false);
      auto* elem = box.create_widget<CheckboxWidget>(tag, dom_id, label, checked);
      if (auto* checkbox = dynamic_cast<CheckboxWidget*>(elem->widget)) {
        checkbox->set_disabled(
            prop_bool(props, "disabled", prop_bool(node_props, "disabled", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "radio") {
      const std::string label = props.contains("label")
                                    ? prop_string(props, "label")
                                    : prop_string(node_props, "label");
      const std::string value = props.contains("value")
                                    ? prop_string(props, "value")
                                    : prop_string(node_props, "value");
      const std::string group = props.contains("group")
                                    ? prop_string(props, "group")
                                    : prop_string(node_props, "group");
      const bool checked = props.contains("checked")
                               ? prop_bool(props, "checked", false)
                               : prop_bool(node_props, "checked", false);
      auto* elem =
          box.create_widget<RadioWidget>(tag, dom_id, label, value, group, checked);
      if (auto* radio = dynamic_cast<RadioWidget*>(elem->widget)) {
        radio->set_disabled(
            prop_bool(props, "disabled", prop_bool(node_props, "disabled", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "switch") {
      const std::string label = props.contains("label")
                                    ? prop_string(props, "label")
                                    : prop_string(node_props, "label");
      const bool checked = props.contains("checked")
                               ? prop_bool(props, "checked", false)
                               : prop_bool(node_props, "checked", false);
      auto* elem = box.create_widget<SwitchWidget>(tag, dom_id, label, checked);
      if (auto* toggle = dynamic_cast<SwitchWidget*>(elem->widget)) {
        toggle->set_disabled(
            prop_bool(props, "disabled", prop_bool(node_props, "disabled", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "slider") {
      const float min = props.contains("min")
                            ? prop_float(props, "min", 0.0f)
                            : prop_float(node_props, "min", 0.0f);
      const float max = props.contains("max")
                            ? prop_float(props, "max", 100.0f)
                            : prop_float(node_props, "max", 100.0f);
      const float value = props.contains("value")
                              ? prop_float(props, "value", min)
                              : prop_float(node_props, "value", min);
      const float step = props.contains("step")
                             ? prop_float(props, "step", 0.0f)
                             : prop_float(node_props, "step", 0.0f);
      auto* elem = box.create_widget<SliderWidget>(tag, dom_id, min, max, value, step);
      if (auto* slider = dynamic_cast<SliderWidget*>(elem->widget)) {
        slider->set_disabled(prop_bool(
            props, "disabled", prop_bool(node_props, "disabled", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "calendar") {
      auto* elem = box.create_widget<CalendarWidget>(tag, dom_id);
      if (auto* calendar = dynamic_cast<CalendarWidget*>(elem->widget)) {
        const Date default_date{2024, 1, 1};
        const Date selected =
            prop_date(props, node_props, "selected_date",
                      prop_date(props, node_props, "selected", default_date));
        const Date view =
            prop_date(props, node_props, "view_date",
                      prop_date(props, node_props, "view", selected));
        calendar->set_selected_date(selected);
        calendar->set_view_date(view);
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "input") {
      const std::string placeholder = props.contains("placeholder")
                                          ? prop_string(props, "placeholder")
                                          : prop_string(node_props, "placeholder");
      const bool password = props.contains("password")
                                ? prop_bool(props, "password", false)
                                : prop_bool(node_props, "password", false);
      auto* elem = box.create_widget<InputWidget>(tag, dom_id, placeholder, password);
      if (auto* input = dynamic_cast<InputWidget*>(elem->widget)) {
        if (props.contains("text")) {
          input->set_text(prop_string(props, "text"));
        } else if (node_props.contains("text")) {
          input->set_text(prop_string(node_props, "text"));
        }
        input->set_readonly(prop_bool(props, "readonly",
                                      prop_bool(node_props, "readonly", false)));
        input->set_disabled(prop_bool(props, "disabled",
                                      prop_bool(node_props, "disabled", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "textarea") {
      const std::string text = props.contains("text")
                                   ? prop_string(props, "text")
                                   : prop_string(node_props, "text");
      const std::string placeholder = props.contains("placeholder")
                                          ? prop_string(props, "placeholder")
                                          : prop_string(node_props, "placeholder");
      auto* elem =
          box.create_widget<TextAreaWidget>(tag, dom_id, text, placeholder);
      if (auto* textarea = dynamic_cast<TextAreaWidget*>(elem->widget)) {
        if (props.contains("cursor_position")) {
          textarea->set_cursor_position(prop_int(props, "cursor_position", 0));
        } else if (node_props.contains("cursor_position")) {
          textarea->set_cursor_position(prop_int(node_props, "cursor_position", 0));
        }
        textarea->set_readonly(prop_bool(props, "readonly",
                                         prop_bool(node_props, "readonly", false)));
        textarea->set_disabled(prop_bool(props, "disabled",
                                         prop_bool(node_props, "disabled", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "searchbox") {
      const std::string placeholder = props.contains("placeholder")
                                          ? prop_string(props, "placeholder")
                                          : prop_string(node_props, "placeholder",
                                                        "Search...");
      auto* elem = box.create_widget<SearchBoxWidget>(tag, dom_id, placeholder);
      if (auto* searchbox = dynamic_cast<SearchBoxWidget*>(elem->widget)) {
        searchbox->set_suggestions(
            json_search_suggestions(prop_json(props, node_props, "suggestions")));
        if (props.contains("text")) {
          searchbox->set_text(prop_string(props, "text"));
        } else if (node_props.contains("text")) {
          searchbox->set_text(prop_string(node_props, "text"));
        }
        searchbox->set_focused(
            prop_bool(props, "focused", prop_bool(node_props, "focused", false)));
        const bool open = props.contains("open")
                              ? prop_bool(props, "open", false)
                              : prop_bool(node_props, "open",
                                          prop_bool(props, "dropdown_open",
                                                    prop_bool(node_props, "dropdown_open",
                                                              false)));
        searchbox->set_dropdown_open(open);
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "accordion") {
      auto* elem = box.create_widget<AccordionWidget>(tag, dom_id);
      if (auto* accordion = dynamic_cast<AccordionWidget*>(elem->widget)) {
        accordion->set_allow_multiple(
            prop_bool(props, "allow_multiple",
                      prop_bool(node_props, "allow_multiple", false)));
        for (const auto& section :
             json_accordion_sections(prop_json(props, node_props, "sections"))) {
          accordion->add_section(section.title, section.id, section.content_height);
          if (section.expanded) {
            accordion->expand(section.id);
          }
        }
        if (const Json* expanded_ids = prop_json(props, node_props, "expanded_ids");
            expanded_ids && expanded_ids->is_array()) {
          for (const auto& expanded_id : *expanded_ids) {
            accordion->expand(quote_attr_value(expanded_id));
          }
        }
        if (props.contains("expanded_id")) {
          accordion->expand(prop_string(props, "expanded_id"));
        } else if (node_props.contains("expanded_id")) {
          accordion->expand(prop_string(node_props, "expanded_id"));
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "toggle-group") {
      auto* elem = box.create_widget<ToggleGroupWidget>(
          tag, dom_id, json_toggle_options(prop_json(props, node_props, "options")));
      if (auto* toggle = dynamic_cast<ToggleGroupWidget*>(elem->widget)) {
        toggle->set_multi_select(prop_bool(props, "multi_select",
                                           prop_bool(node_props, "multi_select", false)));
        if (const Json* selected_indices =
                prop_json(props, node_props, "selected_indices");
            selected_indices && selected_indices->is_array()) {
          toggle->set_selected_indices(
              selected_indices->get<std::vector<int>>());
        } else if (props.contains("selected_index")) {
          toggle->set_selected_index(prop_int(props, "selected_index", -1));
        } else if (node_props.contains("selected_index")) {
          toggle->set_selected_index(prop_int(node_props, "selected_index", -1));
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "sidebar") {
      const bool collapsible = props.contains("collapsible")
                                   ? prop_bool(props, "collapsible", true)
                                   : prop_bool(node_props, "collapsible", true);
      auto* elem = box.create_widget<SidebarWidget>(tag, dom_id, collapsible);
      if (auto* sidebar = dynamic_cast<SidebarWidget*>(elem->widget)) {
        for (const auto& section :
             json_sidebar_sections(prop_json(props, node_props, "sections"))) {
          sidebar->add_section(section.title);
          for (const auto& item : section.items) {
            sidebar->add_item(item.id, item.icon, item.label, item.badge);
          }
        }
        if (props.contains("selected_id")) {
          sidebar->select(prop_string(props, "selected_id"));
        } else if (node_props.contains("selected_id")) {
          sidebar->select(prop_string(node_props, "selected_id"));
        }
        sidebar->set_collapsed(prop_bool(
            props, "collapsed", prop_bool(node_props, "collapsed", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "spinner") {
      const std::string variant_name = props.contains("variant")
                                           ? prop_string(props, "variant", "ring")
                                           : prop_string(node_props, "variant", "ring");
      auto* elem = box.create_widget<SpinnerWidget>(
          tag, dom_id, spinner_variant_from_string(variant_name));
      if (auto* spinner = dynamic_cast<SpinnerWidget*>(elem->widget)) {
        if (props.contains("spinning")) {
          if (prop_bool(props, "spinning", true)) {
            spinner->start();
          } else {
            spinner->stop();
          }
        } else if (node_props.contains("spinning") &&
                   !prop_bool(node_props, "spinning", true)) {
          spinner->stop();
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "toolbar") {
      auto* elem = box.create_widget<ToolbarWidget>(tag, dom_id);
      if (auto* toolbar = dynamic_cast<ToolbarWidget*>(elem->widget)) {
        for (const auto& item :
             json_toolbar_items(prop_json(props, node_props, "items"))) {
          if (item.type == ToolbarWidget::Item::Separator) {
            toolbar->add_separator();
            continue;
          }
          if (item.type == ToolbarWidget::Item::Toggle) {
            toolbar->add_toggle(item.id, item.icon, item.toggled, item.tooltip);
            toolbar->set_enabled(item.id, item.enabled);
            continue;
          }
          if (item.type == ToolbarWidget::Item::Dropdown) {
            toolbar->add_dropdown(item.id, item.icon, item.dropdown_items,
                                  item.tooltip);
            toolbar->set_enabled(item.id, item.enabled);
            continue;
          }
          toolbar->add_button(item.id, item.icon, [] {}, item.tooltip);
          toolbar->set_enabled(item.id, item.enabled);
        }
        if (props.contains("open_dropdown")) {
          toolbar->set_open_dropdown(prop_string(props, "open_dropdown"));
        } else if (node_props.contains("open_dropdown")) {
          toolbar->set_open_dropdown(prop_string(node_props, "open_dropdown"));
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "listview") {
      auto* elem = box.create_widget<ListViewWidget>(tag, dom_id);
      if (auto* list = dynamic_cast<ListViewWidget*>(elem->widget)) {
        list->set_multi_select(prop_bool(props, "multi_select",
                                         prop_bool(node_props, "multi_select", false)));
        list->set_items(json_listview_items(prop_json(props, node_props, "items")));
        if (const Json* selected_indices =
                prop_json(props, node_props, "selected_indices");
            selected_indices && selected_indices->is_array()) {
          for (auto& item : list->items()) {
            item.selected = false;
          }
          for (const auto& index_value : *selected_indices) {
            const int index = index_value.get<int>();
            if (index >= 0 &&
                index < static_cast<int>(list->items().size())) {
              list->items()[static_cast<size_t>(index)].selected = true;
            }
          }
          list->sync_host_semantics_for_layout(*elem);
        } else {
          const int index = props.contains("selected_index")
                                ? prop_int(props, "selected_index", -1)
                                : prop_int(node_props, "selected_index", -1);
          if (index >= 0 &&
              index < static_cast<int>(list->items().size())) {
            for (auto& item : list->items()) {
              item.selected = false;
            }
            list->items()[static_cast<size_t>(index)].selected = true;
            list->sync_host_semantics_for_layout(*elem);
          }
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "menu") {
      auto* elem = box.create_widget<MenuWidget>(tag, dom_id);
      if (auto* menu = dynamic_cast<MenuWidget*>(elem->widget)) {
        menu->items() = json_menu_items(prop_json(props, node_props, "items"));
        const float x = static_cast<float>(
            props.contains("x") ? prop_int(props, "x", 0) : prop_int(node_props, "x", 0));
        const float y = static_cast<float>(
            props.contains("y") ? prop_int(props, "y", 0) : prop_int(node_props, "y", 0));
        if (prop_bool(props, "open", prop_bool(node_props, "open", false))) {
          menu->show(x, y);
        } else {
          menu->hide();
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "tooltip") {
      const std::string text = props.contains("text")
                                   ? prop_string(props, "text")
                                   : prop_string(node_props, "text");
      const std::string position = props.contains("position")
                                       ? prop_string(props, "position")
                                       : prop_string(node_props, "position", "top");
      auto* elem = box.create_widget<TooltipWidget>(tag, dom_id, text);
      if (auto* tooltip = dynamic_cast<TooltipWidget*>(elem->widget)) {
        tooltip->set_position(tooltip_position_from_string(position));
        if (prop_bool(props, "open", prop_bool(node_props, "open", false))) {
          tooltip->show();
        } else {
          tooltip->hide();
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "notification") {
      const std::string position = props.contains("position")
                                       ? prop_string(props, "position")
                                       : prop_string(node_props, "position",
                                                     "top-right");
      auto* elem = box.create_widget<NotificationWidget>(
          tag, dom_id, notification_position_from_string(position));
      if (auto* notification = dynamic_cast<NotificationWidget*>(elem->widget)) {
        if (const Json* entries = prop_json(props, node_props, "notifications");
            entries && entries->is_array()) {
          for (const auto& entry : *entries) {
            if (!entry.is_object()) {
              continue;
            }
            notification->notify(
                prop_string(entry, "title", ""),
                prop_string(entry, "message", ""),
                notification_type_from_string(prop_string(entry, "type", "info")),
                static_cast<float>(prop_int(entry, "duration", 5000)));
          }
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "dropdown") {
      const std::string placeholder = props.contains("placeholder")
                                          ? prop_string(props, "placeholder")
                                          : prop_string(node_props, "placeholder",
                                                        "Select...");
      auto* elem = box.create_widget<DropdownWidget>(tag, dom_id, placeholder);
      if (auto* dropdown = dynamic_cast<DropdownWidget*>(elem->widget)) {
        const std::vector<DropdownWidget::Option> options =
            json_dropdown_options(prop_json(props, node_props, "options"));
        dropdown->clear_options();
        for (const auto& option : options) {
          dropdown->add_option(option.label, option.value, option.disabled);
        }

        if (props.contains("selected_value")) {
          dropdown->set_selected_value(prop_string(props, "selected_value"));
        } else if (node_props.contains("selected_value")) {
          dropdown->set_selected_value(prop_string(node_props, "selected_value"));
        } else if (props.contains("selected_index")) {
          dropdown->set_selected_index(prop_int(props, "selected_index", -1));
        } else if (node_props.contains("selected_index")) {
          dropdown->set_selected_index(prop_int(node_props, "selected_index", -1));
        }

        if (prop_bool(props, "open", prop_bool(node_props, "open", false))) {
          dropdown->open();
        } else {
          dropdown->close();
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "popover") {
      const std::string content = props.contains("content")
                                      ? prop_string(props, "content")
                                      : prop_string(node_props, "content");
      const std::string position = props.contains("position")
                                       ? prop_string(props, "position")
                                       : prop_string(node_props, "position", "bottom");
      auto* elem = box.create_widget<PopoverWidget>(
          tag, dom_id, content, popover_position_from_string(position));
      if (auto* popover = dynamic_cast<PopoverWidget*>(elem->widget)) {
        if (props.contains("title")) {
          popover->set_title(prop_string(props, "title"));
        } else if (node_props.contains("title")) {
          popover->set_title(prop_string(node_props, "title"));
        }
        if (props.contains("content")) {
          popover->set_content(prop_string(props, "content"));
        } else if (node_props.contains("content")) {
          popover->set_content(prop_string(node_props, "content"));
        }
        if (props.contains("width") || node_props.contains("width") ||
            props.contains("height") || node_props.contains("height")) {
          popover->set_size(
              props.contains("width") ? static_cast<float>(prop_int(props, "width", 0))
                                      : static_cast<float>(prop_int(node_props, "width", 0)),
              props.contains("height")
                  ? static_cast<float>(prop_int(props, "height", 0))
                  : static_cast<float>(prop_int(node_props, "height", 0)));
        }
        popover->set_position(popover_position_from_string(position));
        if (prop_bool(props, "open", prop_bool(node_props, "open", false))) {
          popover->show();
        } else {
          popover->hide();
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "select") {
      const std::vector<std::string> options =
          json_string_list(prop_json(props, node_props, "options"));
      const int selected_index = props.contains("selected_index")
                                     ? prop_int(props, "selected_index", -1)
                                     : prop_int(node_props, "selected_index", -1);
      auto* elem = box.create_widget<SelectWidget>(tag, dom_id, options, selected_index);
      if (auto* select = dynamic_cast<SelectWidget*>(elem->widget)) {
        select->set_disabled(prop_bool(props, "disabled",
                                       prop_bool(node_props, "disabled", false)));
        select->set_expanded(prop_bool(props, "open",
                                       prop_bool(node_props, "open", false)));
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "toast") {
      const std::string message = props.contains("message")
                                      ? prop_string(props, "message")
                                      : prop_string(node_props, "message");
      const std::string type_name = props.contains("type")
                                        ? prop_string(props, "type")
                                        : prop_string(node_props, "type", "default");
      auto* elem = box.create_widget<ToastWidget>(
          tag, dom_id, message, toast_type_from_string(type_name));
      if (auto* toast = dynamic_cast<ToastWidget*>(elem->widget)) {
        toast->set_duration(static_cast<float>(
            props.contains("duration") ? prop_int(props, "duration", 3000)
                                       : prop_int(node_props, "duration", 3000)));
        if (prop_bool(props, "open", prop_bool(node_props, "open", false))) {
          toast->show();
        } else {
          toast->hide();
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    if (widget == "tabs") {
      auto* elem = box.create_widget<TabsWidget>(tag, dom_id);
      if (auto* tabs = dynamic_cast<TabsWidget*>(elem->widget)) {
        if (const Json* tab_items = prop_json(props, node_props, "tabs");
            tab_items && tab_items->is_array()) {
          tabs->clear_tabs();
          for (const auto& tab : *tab_items) {
            if (!tab.is_object()) {
              continue;
            }
            tabs->add_tab(prop_string(tab, "label", "Tab"),
                          prop_string(tab, "id", sanitize_identifier(prop_string(tab, "label", "tab"))),
                          nullptr, prop_bool(tab, "disabled", false));
          }
        }
        if (props.contains("active_id")) {
          tabs->set_active_id(prop_string(props, "active_id"));
        } else if (node_props.contains("active_id")) {
          tabs->set_active_id(prop_string(node_props, "active_id"));
        } else if (props.contains("active_index")) {
          tabs->set_active_index(prop_int(props, "active_index", 0));
        } else if (node_props.contains("active_index")) {
          tabs->set_active_index(prop_int(node_props, "active_index", 0));
        }
      }
      apply_node_metadata(elem, node);
      return elem;
    }
    throw std::runtime_error("unsupported shadcn ir widget: " + widget);
  }

  auto* elem = box.create(tag, dom_id);
  apply_node_metadata(elem, node);
  return elem;
}

bool condition_matches(const Json& condition, const Json& active_states) {
  const std::string type = condition.value("type", std::string());
  const std::string name = condition.value("name", std::string());
  if (type == "state" || type == "prop" || type == "variant") {
    const auto it = active_states.find(name);
    if (it == active_states.end()) {
      return false;
    }
    const Json& expected =
        condition.contains("equals") ? condition.at("equals")
                                      : condition.value("value", Json(true));
    return *it == expected;
  }
  return true;
}

bool bridge_is_active(const Json& bridge, const Json& active_states) {
  if (bridge.contains("state")) {
    const auto it = active_states.find(bridge.at("state").get<std::string>());
    if (it == active_states.end() || !it->is_boolean() || !it->get<bool>()) {
      return false;
    }
  }
  if (bridge.contains("when")) {
    for (const auto& condition : bridge.at("when")) {
      if (!condition_matches(condition, active_states)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

std::string component_scope(const Json& component_ir) {
  return sanitize_identifier(component_ir.at("component").get<std::string>());
}

std::string node_dom_id(const Json& component_ir, const std::string& node_id) {
  return component_scope(component_ir) + "__" + sanitize_identifier(node_id);
}

std::string node_dom_id(const Json& component_ir, const std::string& node_id,
                        const std::string& instance_id) {
  if (instance_id.empty()) {
    return node_dom_id(component_ir, node_id);
  }
  return component_scope(component_ir) + "--" + sanitize_identifier(instance_id) +
         "__" + sanitize_identifier(node_id);
}

std::string emit_css(const Json& component_ir, const Json& style_ir,
                     const Json& resolved_variants) {
  return emit_css(component_ir, style_ir, resolved_variants, std::string());
}

std::string emit_css(const Json& component_ir, const Json& style_ir,
                     const Json& resolved_variants,
                     const std::string& instance_id) {
  if (!style_ir.contains("rules")) {
    throw std::runtime_error("style ir must contain rules");
  }

  const Json variants = merge_variants(component_ir, resolved_variants);
  std::ostringstream out;

  emit_rule_list(out, component_ir, style_ir.at("rules"), variants, instance_id, 0);

  if (style_ir.contains("media")) {
    for (const auto& block : style_ir.at("media")) {
      out << "@media " << block.at("query").get<std::string>() << " {\n";
      emit_rule_list(out, component_ir, block.at("rules"), variants, instance_id, 2);
      out << "}\n";
    }
  }

  if (style_ir.contains("container")) {
    for (const auto& block : style_ir.at("container")) {
      out << "@container";
      if (block.contains("name") && !block.at("name").get<std::string>().empty()) {
        out << " " << block.at("name").get<std::string>();
      }
      out << " " << block.at("query").get<std::string>() << " {\n";
      emit_rule_list(out, component_ir, block.at("rules"), variants, instance_id, 2);
      out << "}\n";
    }
  }

  if (style_ir.contains("keyframes")) {
    for (const auto& keyframes : style_ir.at("keyframes")) {
      emit_keyframes(out, keyframes);
    }
  }

  return out.str();
}

std::string emit_utility_css(const Json& utility_whitelist,
                             const std::vector<std::string>& class_tokens) {
  if (!utility_whitelist.contains("tokens") ||
      !utility_whitelist.at("tokens").is_object()) {
    throw std::runtime_error("utility whitelist must contain tokens");
  }

  std::ostringstream out;
  const Json& tokens = utility_whitelist.at("tokens");
  std::unordered_set<std::string> emitted;
  for (const auto& class_token : class_tokens) {
    if (!emitted.insert(class_token).second) {
      continue;
    }
    std::unordered_set<std::string> visiting;
    emit_utility_rule(out, tokens, class_token, class_token, visiting);
  }
  return out.str();
}

std::vector<std::string> missing_utility_tokens(
    const Json& utility_whitelist,
    const std::vector<std::string>& class_tokens) {
  if (!utility_whitelist.contains("tokens") ||
      !utility_whitelist.at("tokens").is_object()) {
    throw std::runtime_error("utility whitelist must contain tokens");
  }

  std::vector<std::string> missing;
  std::unordered_set<std::string> seen;
  const Json& tokens = utility_whitelist.at("tokens");
  for (const auto& class_token : class_tokens) {
    if (!seen.insert(class_token).second) {
      continue;
    }
    if (tokens.find(class_token) == tokens.end()) {
      missing.push_back(class_token);
    }
  }
  return missing;
}

InstantiatedTree instantiate_component(Box& box, const Json& component_ir,
                                       const Json& props) {
  return instantiate_component(box, component_ir, props, std::string());
}

InstantiatedTree instantiate_component(Box& box, const Json& component_ir,
                                       const Json& props,
                                       const std::string& instance_id) {
  InstantiatedTree tree =
      instantiate_component_tree(box, component_ir, props, instance_id);
  box.set_root(tree.root);
  return tree;
}

InstantiatedTree instantiate_component_tree(Box& box, const Json& component_ir,
                                            const Json& props) {
  return instantiate_component_tree(box, component_ir, props, std::string());
}

InstantiatedTree instantiate_component_tree(Box& box, const Json& component_ir,
                                            const Json& props,
                                            const std::string& instance_id) {
  InstantiatedTree tree;
  std::unordered_map<std::string, Json> node_specs;
  for (const auto& node : component_ir.at("nodes")) {
    const std::string id = node.at("id").get<std::string>();
    node_specs.emplace(id, node);
    tree.nodes.emplace(id, instantiate_node(box, component_ir, node, props,
                                            instance_id));
  }

  if (component_ir.contains("edges")) {
    for (const auto& edge : component_ir.at("edges")) {
      auto* parent = tree.nodes.at(edge.at("parent").get<std::string>());
      auto* child = tree.nodes.at(edge.at("child").get<std::string>());
      parent->append(child);
    }
  }

  tree.root = tree.nodes.at(component_ir.at("root").get<std::string>());
  return tree;
}

void apply_bridge_states(const Json& bridge_ir, const Json& active_states,
                         const InstantiatedTree& tree) {
  if (!bridge_ir.contains("bridges")) {
    return;
  }

  struct TargetProjection {
    std::unordered_map<std::string, std::string> attributes;
    std::unordered_set<std::string> managed_attributes;
    std::unordered_set<std::string> removed_attributes;
    std::unordered_map<std::string, Json> properties;
    std::unordered_set<std::string> managed_properties;
    std::unordered_set<std::string> active_pseudo_states;
    std::unordered_set<std::string> managed_pseudo_states;
  };

  auto apply_widget_property = [](Element* elem, const std::string& name,
                                  const Json* value) {
    if (!elem || !elem->widget) {
      return;
    }

    if (name == "disabled") {
      const bool disabled = value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* button = dynamic_cast<ButtonWidget*>(elem->widget)) {
        button->set_disabled(disabled);
      } else if (auto* checkbox = dynamic_cast<CheckboxWidget*>(elem->widget)) {
        checkbox->set_disabled(disabled);
      } else if (auto* input = dynamic_cast<InputWidget*>(elem->widget)) {
        input->set_disabled(disabled);
      } else if (auto* textarea = dynamic_cast<TextAreaWidget*>(elem->widget)) {
        textarea->set_disabled(disabled);
      } else if (auto* radio = dynamic_cast<RadioWidget*>(elem->widget)) {
        radio->set_disabled(disabled);
      } else if (auto* select = dynamic_cast<SelectWidget*>(elem->widget)) {
        select->set_disabled(disabled);
      } else if (auto* toggle = dynamic_cast<SwitchWidget*>(elem->widget)) {
        toggle->set_disabled(disabled);
      } else if (auto* slider = dynamic_cast<SliderWidget*>(elem->widget)) {
        slider->set_disabled(disabled);
      }
      return;
    }

    if (name == "checked") {
      const bool checked = value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* checkbox = dynamic_cast<CheckboxWidget*>(elem->widget)) {
        checkbox->set_checked(checked);
      } else if (auto* radio = dynamic_cast<RadioWidget*>(elem->widget)) {
        radio->set_checked(checked);
      } else if (auto* toggle = dynamic_cast<SwitchWidget*>(elem->widget)) {
        toggle->set_checked(checked);
      }
      return;
    }

    if (name == "readonly") {
      const bool readonly = value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* input = dynamic_cast<InputWidget*>(elem->widget)) {
        input->set_readonly(readonly);
      } else if (auto* textarea = dynamic_cast<TextAreaWidget*>(elem->widget)) {
        textarea->set_readonly(readonly);
      }
      return;
    }

    if (name == "open") {
      const bool open = value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* dialog = dynamic_cast<DialogWidget*>(elem->widget)) {
        if (open) {
          dialog->show();
        } else {
          dialog->hide();
        }
      } else if (auto* dropdown = dynamic_cast<DropdownWidget*>(elem->widget)) {
        if (open) {
          dropdown->open();
        } else {
          dropdown->close();
        }
      } else if (auto* menu = dynamic_cast<MenuWidget*>(elem->widget)) {
        if (open) {
          menu->show(0.0f, 0.0f);
        } else {
          menu->hide();
        }
      } else if (auto* searchbox = dynamic_cast<SearchBoxWidget*>(elem->widget)) {
        searchbox->set_dropdown_open(open);
      } else if (auto* popover = dynamic_cast<PopoverWidget*>(elem->widget)) {
        if (open) {
          popover->show();
        } else {
          popover->hide();
        }
      } else if (auto* select = dynamic_cast<SelectWidget*>(elem->widget)) {
        select->set_expanded(open);
      } else if (auto* toast = dynamic_cast<ToastWidget*>(elem->widget)) {
        if (open) {
          toast->show();
        } else {
          toast->hide();
        }
      } else if (auto* tooltip = dynamic_cast<TooltipWidget*>(elem->widget)) {
        if (open) {
          tooltip->show();
        } else {
          tooltip->hide();
        }
      }
      return;
    }

    if (name == "focused") {
      const bool focused = value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* searchbox = dynamic_cast<SearchBoxWidget*>(elem->widget)) {
        searchbox->set_focused(focused);
      }
      return;
    }

    if (name == "text") {
      if (auto* input = dynamic_cast<InputWidget*>(elem->widget)) {
        input->set_text(value ? quote_attr_value(*value) : std::string());
      } else if (auto* textarea = dynamic_cast<TextAreaWidget*>(elem->widget)) {
        textarea->set_text(value ? quote_attr_value(*value) : std::string());
      } else if (auto* button = dynamic_cast<ButtonWidget*>(elem->widget)) {
        button->set_text(value ? quote_attr_value(*value) : std::string());
      } else if (auto* searchbox = dynamic_cast<SearchBoxWidget*>(elem->widget)) {
        searchbox->set_text(value ? quote_attr_value(*value) : std::string());
      } else if (auto* tooltip = dynamic_cast<TooltipWidget*>(elem->widget)) {
        tooltip->set_text(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "label") {
      const std::string label = value ? quote_attr_value(*value) : std::string();
      if (auto* checkbox = dynamic_cast<CheckboxWidget*>(elem->widget)) {
        checkbox->set_label(label);
      } else if (auto* radio = dynamic_cast<RadioWidget*>(elem->widget)) {
        radio->set_label(label);
      } else if (auto* toggle = dynamic_cast<SwitchWidget*>(elem->widget)) {
        toggle->set_label(label);
      }
      return;
    }

    if (name == "value") {
      if (auto* radio = dynamic_cast<RadioWidget*>(elem->widget)) {
        radio->set_value(value ? quote_attr_value(*value) : std::string());
      } else if (auto* slider = dynamic_cast<SliderWidget*>(elem->widget)) {
        slider->set_value(value != nullptr
                              ? prop_float(Json{{"value", *value}}, "value", 0.0f)
                              : 0.0f);
      }
      return;
    }

    if (name == "min") {
      if (auto* slider = dynamic_cast<SliderWidget*>(elem->widget)) {
        slider->set_min(value != nullptr
                            ? prop_float(Json{{"value", *value}}, "value", 0.0f)
                            : 0.0f);
      }
      return;
    }

    if (name == "max") {
      if (auto* slider = dynamic_cast<SliderWidget*>(elem->widget)) {
        slider->set_max(value != nullptr
                            ? prop_float(Json{{"value", *value}}, "value", 100.0f)
                            : 100.0f);
      }
      return;
    }

    if (name == "step") {
      if (auto* slider = dynamic_cast<SliderWidget*>(elem->widget)) {
        slider->set_step(value != nullptr
                             ? prop_float(Json{{"value", *value}}, "value", 0.0f)
                             : 0.0f);
      }
      return;
    }

    if (name == "selected_date") {
      if (auto* calendar = dynamic_cast<CalendarWidget*>(elem->widget)) {
        calendar->set_selected_date(
            value != nullptr ? date_from_json(*value, calendar->selected_date())
                             : calendar->selected_date());
      }
      return;
    }

    if (name == "view_date") {
      if (auto* calendar = dynamic_cast<CalendarWidget*>(elem->widget)) {
        calendar->set_view_date(value != nullptr
                                    ? date_from_json(*value, calendar->view_date())
                                    : calendar->view_date());
      }
      return;
    }

    if (name == "group") {
      if (auto* radio = dynamic_cast<RadioWidget*>(elem->widget)) {
        radio->set_group(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "placeholder") {
      const std::string placeholder =
          value ? quote_attr_value(*value) : std::string();
      if (auto* input = dynamic_cast<InputWidget*>(elem->widget)) {
        input->set_placeholder(placeholder);
      } else if (auto* textarea = dynamic_cast<TextAreaWidget*>(elem->widget)) {
        textarea->set_placeholder(placeholder);
      } else if (auto* dropdown = dynamic_cast<DropdownWidget*>(elem->widget)) {
        dropdown->set_placeholder(placeholder);
      }
      return;
    }

    if (name == "cursor_position") {
      const int index =
          value != nullptr ? prop_int(Json{{"value", *value}}, "value", 0) : 0;
      if (auto* textarea = dynamic_cast<TextAreaWidget*>(elem->widget)) {
        textarea->set_cursor_position(index);
      }
      return;
    }

    if (name == "content") {
      if (auto* popover = dynamic_cast<PopoverWidget*>(elem->widget)) {
        popover->set_content(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "title") {
      if (auto* popover = dynamic_cast<PopoverWidget*>(elem->widget)) {
        popover->set_title(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "position") {
      if (auto* popover = dynamic_cast<PopoverWidget*>(elem->widget)) {
        popover->set_position(popover_position_from_string(
            value ? quote_attr_value(*value) : std::string("bottom")));
      } else if (auto* tooltip = dynamic_cast<TooltipWidget*>(elem->widget)) {
        tooltip->set_position(tooltip_position_from_string(
            value ? quote_attr_value(*value) : std::string("top")));
      }
      return;
    }

    if (name == "message") {
      if (auto* toast = dynamic_cast<ToastWidget*>(elem->widget)) {
        toast->set_message(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "type") {
      if (auto* toast = dynamic_cast<ToastWidget*>(elem->widget)) {
        const std::string type_name =
            value ? quote_attr_value(*value) : std::string("default");
        const ToastWidget::Type type = toast_type_from_string(type_name);
        const bool urgent =
            type == ToastWidget::Type::Error || type == ToastWidget::Type::Warning;
        toast->set_type(type);
        elem->set_attribute("data-type", type_name);
        elem->set_attribute("role", urgent ? "alert" : "status");
        elem->set_attribute("aria-live", urgent ? "assertive" : "polite");
        elem->set_attribute("aria-atomic", "true");
      }
      return;
    }

    if (name == "duration") {
      if (auto* toast = dynamic_cast<ToastWidget*>(elem->widget)) {
        const float duration = value != nullptr
                                   ? static_cast<float>(
                                         prop_int(Json{{"value", *value}}, "value", 0))
                                   : 0.0f;
        toast->set_duration(duration);
      }
      return;
    }

    if (name == "selected_index") {
      const int index = value != nullptr ? prop_int(Json{{"value", *value}}, "value", -1) : -1;
      if (auto* dropdown = dynamic_cast<DropdownWidget*>(elem->widget)) {
        dropdown->set_selected_index(index);
      } else if (auto* select = dynamic_cast<SelectWidget*>(elem->widget)) {
        select->set_selected_index(index);
      } else if (auto* toggle_group =
                     dynamic_cast<ToggleGroupWidget*>(elem->widget)) {
        toggle_group->set_selected_index(index);
      } else if (auto* list = dynamic_cast<ListViewWidget*>(elem->widget)) {
        for (auto& item : list->items()) {
          item.selected = false;
        }
        if (index >= 0 && index < static_cast<int>(list->items().size())) {
          list->items()[static_cast<size_t>(index)].selected = true;
        }
        list->sync_host_semantics_for_layout(*elem);
      }
      return;
    }

    if (name == "selected_indices") {
      if (value == nullptr || !value->is_array()) {
        return;
      }
      if (auto* toggle_group = dynamic_cast<ToggleGroupWidget*>(elem->widget)) {
        toggle_group->set_selected_indices(value->get<std::vector<int>>());
      } else if (auto* list = dynamic_cast<ListViewWidget*>(elem->widget)) {
        for (auto& item : list->items()) {
          item.selected = false;
        }
        for (const auto& index_value : *value) {
          const int index = index_value.get<int>();
          if (index >= 0 && index < static_cast<int>(list->items().size())) {
            list->items()[static_cast<size_t>(index)].selected = true;
          }
        }
        list->sync_host_semantics_for_layout(*elem);
      }
      return;
    }

    if (name == "selected_value") {
      if (auto* dropdown = dynamic_cast<DropdownWidget*>(elem->widget)) {
        dropdown->set_selected_value(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "options") {
      if (auto* dropdown = dynamic_cast<DropdownWidget*>(elem->widget)) {
        dropdown->clear_options();
        for (const auto& option : json_dropdown_options(value)) {
          dropdown->add_option(option.label, option.value, option.disabled);
        }
      } else if (auto* toggle_group =
                     dynamic_cast<ToggleGroupWidget*>(elem->widget)) {
        toggle_group->set_options(json_toggle_options(value));
      }
      return;
    }

    if (name == "items") {
      if (auto* menu = dynamic_cast<MenuWidget*>(elem->widget)) {
        menu->items() = json_menu_items(value);
      } else if (auto* list = dynamic_cast<ListViewWidget*>(elem->widget)) {
        list->set_items(json_listview_items(value));
      } else if (auto* accordion = dynamic_cast<AccordionWidget*>(elem->widget)) {
        accordion->clear_sections();
        for (const auto& section : json_accordion_sections(value)) {
          accordion->add_section(section.title, section.id, section.content_height);
          if (section.expanded) {
            accordion->expand(section.id);
          }
        }
      }
      return;
    }

    if (name == "suggestions") {
      if (auto* searchbox = dynamic_cast<SearchBoxWidget*>(elem->widget)) {
        searchbox->set_suggestions(json_search_suggestions(value));
      }
      return;
    }

    if (name == "selected_id") {
      if (auto* sidebar = dynamic_cast<SidebarWidget*>(elem->widget)) {
        sidebar->select(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "collapsed") {
      const bool collapsed = value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* sidebar = dynamic_cast<SidebarWidget*>(elem->widget)) {
        sidebar->set_collapsed(collapsed);
      }
      return;
    }

    if (name == "allow_multiple") {
      const bool allow_multiple =
          value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* accordion = dynamic_cast<AccordionWidget*>(elem->widget)) {
        accordion->set_allow_multiple(allow_multiple);
      }
      return;
    }

    if (name == "expanded_id") {
      if (auto* accordion = dynamic_cast<AccordionWidget*>(elem->widget)) {
        if (value != nullptr && !quote_attr_value(*value).empty()) {
          accordion->expand(quote_attr_value(*value));
        }
      }
      return;
    }

    if (name == "expanded_ids") {
      if (auto* accordion = dynamic_cast<AccordionWidget*>(elem->widget)) {
        if (value == nullptr || !value->is_array()) {
          return;
        }
        for (const auto& expanded_id : *value) {
          accordion->expand(quote_attr_value(expanded_id));
        }
      }
      return;
    }

    if (name == "multi_select") {
      const bool multi_select =
          value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* toggle_group = dynamic_cast<ToggleGroupWidget*>(elem->widget)) {
        toggle_group->set_multi_select(multi_select);
      } else if (auto* list = dynamic_cast<ListViewWidget*>(elem->widget)) {
        list->set_multi_select(multi_select);
      }
      return;
    }

    if (name == "spinning") {
      const bool spinning =
          value != nullptr && value->is_boolean() && value->get<bool>();
      if (auto* spinner = dynamic_cast<SpinnerWidget*>(elem->widget)) {
        if (spinning) {
          spinner->start();
        } else {
          spinner->stop();
        }
      }
      return;
    }

    if (name == "variant") {
      if (auto* spinner = dynamic_cast<SpinnerWidget*>(elem->widget)) {
        spinner->set_variant(
            spinner_variant_from_string(value ? quote_attr_value(*value)
                                             : std::string("ring")));
      }
      return;
    }

    if (name == "open_dropdown") {
      if (auto* toolbar = dynamic_cast<ToolbarWidget*>(elem->widget)) {
        toolbar->set_open_dropdown(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "active_id") {
      if (auto* tabs = dynamic_cast<TabsWidget*>(elem->widget)) {
        tabs->set_active_id(value ? quote_attr_value(*value) : std::string());
      }
      return;
    }

    if (name == "active_index") {
      const int index = value != nullptr ? prop_int(Json{{"value", *value}}, "value", 0) : 0;
      if (auto* tabs = dynamic_cast<TabsWidget*>(elem->widget)) {
        tabs->set_active_index(index);
      }
    }
  };

  std::unordered_map<std::string, TargetProjection> projections;
  for (const auto& bridge : bridge_ir.at("bridges")) {
    const std::string target = bridge.at("target").get<std::string>();
    auto& projection = projections[target];

    if (bridge.contains("attributes")) {
      for (auto it = bridge.at("attributes").begin();
           it != bridge.at("attributes").end(); ++it) {
        projection.managed_attributes.insert(it.key());
      }
    }
    for (const auto& attr : bridge.value("missing_attributes", Json::array())) {
      projection.managed_attributes.insert(attr.get<std::string>());
    }
    for (const auto& state : bridge.value("pseudo_states", Json::array())) {
      projection.managed_pseudo_states.insert(state.get<std::string>());
    }
    if (bridge.contains("properties")) {
      for (auto it = bridge.at("properties").begin();
           it != bridge.at("properties").end(); ++it) {
        projection.managed_properties.insert(it.key());
      }
    }

    if (!bridge_is_active(bridge, active_states)) {
      continue;
    }

    if (bridge.contains("attributes")) {
      for (auto it = bridge.at("attributes").begin();
           it != bridge.at("attributes").end(); ++it) {
        projection.attributes[it.key()] = quote_attr_value(it.value());
        projection.removed_attributes.erase(it.key());
      }
    }
    for (const auto& attr : bridge.value("missing_attributes", Json::array())) {
      const std::string name = attr.get<std::string>();
      projection.attributes.erase(name);
      projection.removed_attributes.insert(name);
    }
    for (const auto& state : bridge.value("pseudo_states", Json::array())) {
      projection.active_pseudo_states.insert(state.get<std::string>());
    }
    if (bridge.contains("properties")) {
      for (auto it = bridge.at("properties").begin();
           it != bridge.at("properties").end(); ++it) {
        projection.properties[it.key()] = it.value();
      }
    }
  }

  for (const auto& entry : projections) {
    const auto node_it = tree.nodes.find(entry.first);
    if (node_it == tree.nodes.end() || !node_it->second) {
      continue;
    }
    Element* elem = node_it->second;
    const auto& projection = entry.second;

    for (const auto& attr : projection.managed_attributes) {
      if (projection.removed_attributes.count(attr) > 0) {
        elem->remove_attribute(attr);
        continue;
      }

      const auto value_it = projection.attributes.find(attr);
      if (value_it != projection.attributes.end()) {
        elem->set_attribute(attr, value_it->second);
      } else {
        elem->remove_attribute(attr);
      }
    }

    for (const auto& property : projection.managed_properties) {
      const auto property_it = projection.properties.find(property);
      apply_widget_property(elem, property,
                            property_it != projection.properties.end()
                                ? &property_it->second
                                : nullptr);
    }

    for (const auto& state : projection.managed_pseudo_states) {
      elem->set_state(state.c_str(),
                      projection.active_pseudo_states.count(state) > 0);
    }
  }
}

}  // namespace flexUI::shadcn_ir
