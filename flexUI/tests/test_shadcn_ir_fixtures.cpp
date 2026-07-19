#include <tinytest.h>
#ifdef group
#undef group
#endif
#include "test_support.h"
#include <nlohmann/json.hpp>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/shadcn_ir.h>
#include <flexUI/widgets/accordion_widget.h>
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
#include <flexUI/widgets/searchbox_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/sidebar_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/spinner_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/toolbar_widget.h>
#include <flexUI/widgets/tooltip_widget.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

json load_json_file(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to open json file: " + path.string());
  }
  json value;
  input >> value;
  return value;
}

std::string load_text_file(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to open text file: " + path.string());
  }
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

fs::path ir_root() {
  return fs::path(FLEXUI_PROJECT_SOURCE_DIR) / "tools" / "shadcn-ir";
}

void require_node_targets_exist(const json& component_ir, const json& values,
                                const char* array_key) {
  std::set<std::string> ids;
  for (const auto& node : component_ir.at("nodes")) {
    ids.insert(node.at("id").get<std::string>());
  }

  for (const auto& entry : values.at(array_key)) {
    check(entry.contains("target"));
    const auto target = entry.at("target").get<std::string>();
    check(ids.find(target) != ids.end());
  }
}

void validate_component_ir(const json& value) {
  check(value.contains("component"));
  check(value.contains("kind"));
  check(value.contains("root"));
  check(value.contains("nodes"));
  check(value.at("nodes").is_array());
  check(value.at("nodes").size() >= 1);

  std::set<std::string> ids;
  for (const auto& node : value.at("nodes")) {
    check(node.contains("id"));
    check(node.contains("type"));
    check(node.contains("tag"));
    const auto id = node.at("id").get<std::string>();
    check(!id.empty());
    check(ids.insert(id).second);
    if (node.at("type").get<std::string>() == "widget") {
      check(node.contains("widget"));
      check(!node.at("widget").get<std::string>().empty());
    }
  }

  check(ids.find(value.at("root").get<std::string>()) != ids.end());

  if (value.contains("edges")) {
    for (const auto& edge : value.at("edges")) {
      check(edge.contains("parent"));
      check(edge.contains("child"));
      check(ids.find(edge.at("parent").get<std::string>()) != ids.end());
      check(ids.find(edge.at("child").get<std::string>()) != ids.end());
    }
  }

  if (value.contains("defaults") && value.contains("variants")) {
    for (auto it = value.at("defaults").begin(); it != value.at("defaults").end(); ++it) {
      check(value.at("variants").contains(it.key()));
      const auto& choices = value.at("variants").at(it.key());
      bool found = false;
      for (const auto& choice : choices) {
        if (choice == it.value()) {
          found = true;
          break;
        }
      }
      check(found);
    }
  }
}

void validate_style_ir(const json& component_ir, const json& value) {
  check(value.contains("rules"));
  check(value.at("rules").is_array());
  check(value.at("rules").size() >= 1);
  require_node_targets_exist(component_ir, value, "rules");

  for (const auto& rule : value.at("rules")) {
    check(rule.contains("selector"));
    check(rule.contains("decls"));
    check(rule.at("decls").is_object());
    check(!rule.at("selector").get<std::string>().empty());
    check(rule.at("decls").size() >= 1);
  }

  if (value.contains("keyframes")) {
    for (const auto& keyframes : value.at("keyframes")) {
      check(keyframes.contains("name"));
      check(keyframes.contains("steps"));
      check(keyframes.at("steps").is_array());
      check(keyframes.at("steps").size() >= 1);
      for (const auto& step : keyframes.at("steps")) {
        check(step.contains("offset"));
        check(step.contains("decls"));
        const double offset = step.at("offset").get<double>();
        check(offset >= 0.0);
        check(offset <= 1.0);
        check(step.at("decls").is_object());
        check(step.at("decls").size() >= 1);
      }
    }
  }
}

void validate_bridge_ir(const json& component_ir, const json& value) {
  check(value.contains("bridges"));
  check(value.at("bridges").is_array());
  check(value.at("bridges").size() >= 1);
  require_node_targets_exist(component_ir, value, "bridges");
}

void validate_utility_whitelist(const json& value) {
  check(value.contains("version"));
  check(value.at("version").get<int>() == 1);
  check(value.contains("tokens"));
  check(value.at("tokens").is_object());
  check(value.at("tokens").size() >= 1);

  for (auto it = value.at("tokens").begin(); it != value.at("tokens").end(); ++it) {
    const auto& token = it.value();
    check(token.contains("kind"));
    const auto kind = token.at("kind").get<std::string>();
    check(kind == "decl" || kind == "conditional" || kind == "pseudo" ||
          kind == "macro");

    if (kind == "decl" || kind == "conditional" || kind == "pseudo") {
      check(token.contains("decls"));
      check(token.at("decls").is_object());
      check(token.at("decls").size() >= 1);
    }
    if (kind == "pseudo") {
      check(token.contains("pseudo"));
      check(!token.at("pseudo").get<std::string>().empty());
    }
    if (kind == "macro") {
      check(token.contains("expand"));
      check(token.at("expand").is_array());
      check(token.at("expand").size() >= 1);
    }
  }
}

void require_contains(const std::string& text, const std::string& needle) {
  check_str_contains(text.c_str(), needle.c_str());
}

bool ends_with_text(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_source_identifier_token(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  if (std::isupper(static_cast<unsigned char>(token.front()))) {
    for (unsigned char ch : token) {
      if (!std::isalnum(ch) && ch != '[') {
        return false;
      }
    }
    if (ends_with_text(token, "[")) {
      return true;
    }
    for (unsigned char ch : token) {
      if (!std::isalnum(ch)) {
        return false;
      }
    }
    return true;
  }
  return ends_with_text(token, "Variants");
}

std::vector<std::string> class_tokens_from_summary(const json& summary) {
  std::vector<std::string> tokens;
  if (!summary.contains("extracted_class_tokens") ||
      !summary.at("extracted_class_tokens").is_array()) {
    return tokens;
  }
  for (const auto& token : summary.at("extracted_class_tokens")) {
    if (token.is_string()) {
      const std::string value = token.get<std::string>();
      if (!is_source_identifier_token(value)) {
        tokens.push_back(value);
      }
    }
  }
  return tokens;
}

std::set<std::string> quoted_strings_in(const std::string& text) {
  std::set<std::string> values;
  const std::regex quoted_string("\"([^\"]+)\"");
  for (auto it = std::sregex_iterator(text.begin(), text.end(), quoted_string);
       it != std::sregex_iterator(); ++it) {
    values.insert((*it)[1].str());
  }
  return values;
}

std::set<std::string> visual_demo_used_utility_tokens(
    const std::string& source) {
  std::set<std::string> tokens;
  size_t pos = 0;
  while ((pos = source.find("add_utilities(", pos)) != std::string::npos) {
    const size_t end_pos = source.find(");", pos);
    if (end_pos == std::string::npos) {
      throw std::runtime_error("unterminated add_utilities() call");
    }
    const auto call_tokens =
        quoted_strings_in(source.substr(pos, end_pos - pos));
    tokens.insert(call_tokens.begin(), call_tokens.end());
    pos = end_pos + 2;
  }
  return tokens;
}

std::vector<std::string> vector_from_set(const std::set<std::string>& values) {
  return std::vector<std::string>(values.begin(), values.end());
}

std::set<std::string> current_ir_sample_names() {
  return {
      "accordion", "alert", "avatar", "badge", "button", "calendar", "card", "chart", "checkbox",
      "dialog", "dropdown", "input", "label", "listview", "menu", "menubar",
      "drawer", "form", "hover-card", "input-otp", "navigation-menu",
      "notification", "pagination", "popover", "progress", "radio",
      "scroll-area", "searchbox", "select", "resizable", "separator",
      "sheet", "sidebar", "skeleton", "slider", "spinner", "switch",
      "table", "tabs", "textarea", "toast", "toggle", "toggle-group",
      "toolbar", "tooltip"};
}

std::set<std::string> component_sample_names_in(const fs::path& sample_dir) {
  std::set<std::string> names;
  constexpr const char* suffix = ".component.json";
  for (const auto& entry : fs::directory_iterator(sample_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (!ends_with_text(filename, suffix)) {
      continue;
    }
    names.insert(filename.substr(0, filename.size() - std::strlen(suffix)));
  }
  return names;
}

std::vector<std::pair<std::string, std::string>> registry_backed_ir_samples() {
  return {
      {"accordion", "shadcn/accordion"},
      {"alert", "shadcn/alert"},
      {"avatar", "shadcn/avatar"},
      {"badge", "shadcn/badge"},
      {"button", "shadcn/button"},
      {"calendar", "shadcn/calendar"},
      {"card", "shadcn/card"},
      {"chart", "shadcn/chart"},
      {"checkbox", "shadcn/checkbox"},
      {"dialog", "shadcn/dialog"},
      {"drawer", "shadcn/drawer"},
      {"dropdown", "shadcn/dropdown-menu"},
      {"form", "shadcn/form"},
      {"hover-card", "shadcn/hover-card"},
      {"input", "shadcn/input"},
      {"input-otp", "shadcn/input-otp"},
      {"label", "shadcn/label"},
      {"menubar", "shadcn/menubar"},
      {"navigation-menu", "shadcn/navigation-menu"},
      {"notification", "shadcn/sonner"},
      {"pagination", "shadcn/pagination"},
      {"popover", "shadcn/popover"},
      {"progress", "shadcn/progress"},
      {"radio", "shadcn/radio-group"},
      {"resizable", "shadcn/resizable"},
      {"scroll-area", "shadcn/scroll-area"},
      {"searchbox", "shadcn/command"},
      {"select", "shadcn/select"},
      {"separator", "shadcn/separator"},
      {"sheet", "shadcn/sheet"},
      {"skeleton", "shadcn/skeleton"},
      {"slider", "shadcn/slider"},
      {"switch", "shadcn/switch"},
      {"table", "shadcn/table"},
      {"tabs", "shadcn/tabs"},
      {"textarea", "shadcn/textarea"},
      {"toast", "shadcn/toast"},
      {"toggle", "shadcn/toggle"},
      {"toggle-group", "shadcn/toggle-group"},
      {"tooltip", "shadcn/tooltip"}};
}

void require_sets_equal(const std::set<std::string>& expected,
                        const std::set<std::string>& actual,
                        const std::string& context) {
  std::vector<std::string> missing;
  std::vector<std::string> extra;
  for (const auto& value : expected) {
    if (actual.find(value) == actual.end()) {
      missing.push_back(value);
    }
  }
  for (const auto& value : actual) {
    if (expected.find(value) == expected.end()) {
      extra.push_back(value);
    }
  }
  if (missing.empty() && extra.empty()) {
    return;
  }

  std::ostringstream message;
  message << context;
  if (!missing.empty()) {
    message << " missing:";
    for (const auto& value : missing) {
      message << " " << value;
    }
  }
  if (!extra.empty()) {
    message << " extra:";
    for (const auto& value : extra) {
      message << " " << value;
    }
  }
  throw std::runtime_error(message.str());
}

void require_no_missing_utility_tokens(const json& whitelist,
                                       const std::vector<std::string>& tokens,
                                       const std::string& component) {
  const auto missing =
      flexUI::shadcn_ir::missing_utility_tokens(whitelist, tokens);
  if (missing.empty()) {
    return;
  }

  std::ostringstream message;
  message << "missing shadcn utility whitelist entries for " << component
          << ": ";
  for (size_t i = 0; i < missing.size(); ++i) {
    if (i != 0) {
      message << ", ";
    }
    message << missing[i];
  }
  throw std::runtime_error(message.str());
}

}  // namespace

spec("shadcn ir schemas and samples are well formed") {
  it("loads schema and sample assets") {
    const fs::path schema_dir = ir_root() / "schema";
    const fs::path sample_dir = ir_root() / "samples";

    const auto component_schema =
        load_json_file(schema_dir / "component_ir.schema.json");
    const auto style_schema = load_json_file(schema_dir / "style_ir.schema.json");
    const auto bridge_schema =
        load_json_file(schema_dir / "bridge_ir.schema.json");
    const auto whitelist = load_json_file(schema_dir / "utility_whitelist.json");

    check(component_schema.contains("$schema"));
    check(style_schema.contains("$schema"));
    check(bridge_schema.contains("$schema"));
    validate_utility_whitelist(whitelist);

    const auto button_component =
        load_json_file(sample_dir / "button.component.json");
    const auto button_style = load_json_file(sample_dir / "button.style.json");
    const auto button_bridge = load_json_file(sample_dir / "button.bridge.json");

    const auto dialog_component =
        load_json_file(sample_dir / "dialog.component.json");
    const auto dialog_style = load_json_file(sample_dir / "dialog.style.json");
    const auto dialog_bridge = load_json_file(sample_dir / "dialog.bridge.json");
    const auto input_component =
        load_json_file(sample_dir / "input.component.json");
    const auto input_style = load_json_file(sample_dir / "input.style.json");
    const auto input_bridge = load_json_file(sample_dir / "input.bridge.json");
    const auto textarea_component =
        load_json_file(sample_dir / "textarea.component.json");
    const auto textarea_style =
        load_json_file(sample_dir / "textarea.style.json");
    const auto textarea_bridge =
        load_json_file(sample_dir / "textarea.bridge.json");
    const auto select_component =
        load_json_file(sample_dir / "select.component.json");
    const auto select_style = load_json_file(sample_dir / "select.style.json");
    const auto select_bridge =
        load_json_file(sample_dir / "select.bridge.json");
    const auto tabs_component =
        load_json_file(sample_dir / "tabs.component.json");
    const auto tabs_style = load_json_file(sample_dir / "tabs.style.json");
    const auto tabs_bridge = load_json_file(sample_dir / "tabs.bridge.json");
    const auto checkbox_component =
        load_json_file(sample_dir / "checkbox.component.json");
    const auto checkbox_style =
        load_json_file(sample_dir / "checkbox.style.json");
    const auto checkbox_bridge =
        load_json_file(sample_dir / "checkbox.bridge.json");
    const auto radio_component =
        load_json_file(sample_dir / "radio.component.json");
    const auto radio_style = load_json_file(sample_dir / "radio.style.json");
    const auto radio_bridge = load_json_file(sample_dir / "radio.bridge.json");
    const auto switch_component =
        load_json_file(sample_dir / "switch.component.json");
    const auto switch_style = load_json_file(sample_dir / "switch.style.json");
    const auto switch_bridge =
        load_json_file(sample_dir / "switch.bridge.json");
    const auto slider_component =
        load_json_file(sample_dir / "slider.component.json");
    const auto slider_style = load_json_file(sample_dir / "slider.style.json");
    const auto slider_bridge =
        load_json_file(sample_dir / "slider.bridge.json");
    const auto dropdown_component =
        load_json_file(sample_dir / "dropdown.component.json");
    const auto dropdown_style =
        load_json_file(sample_dir / "dropdown.style.json");
    const auto dropdown_bridge =
        load_json_file(sample_dir / "dropdown.bridge.json");
    const auto popover_component =
        load_json_file(sample_dir / "popover.component.json");
    const auto popover_style =
        load_json_file(sample_dir / "popover.style.json");
    const auto popover_bridge =
        load_json_file(sample_dir / "popover.bridge.json");
    const auto toast_component =
        load_json_file(sample_dir / "toast.component.json");
    const auto toast_style = load_json_file(sample_dir / "toast.style.json");
    const auto toast_bridge = load_json_file(sample_dir / "toast.bridge.json");
    const auto menu_component =
        load_json_file(sample_dir / "menu.component.json");
    const auto menu_style = load_json_file(sample_dir / "menu.style.json");
    const auto menu_bridge = load_json_file(sample_dir / "menu.bridge.json");
    const auto notification_component =
        load_json_file(sample_dir / "notification.component.json");
    const auto notification_style =
        load_json_file(sample_dir / "notification.style.json");
    const auto notification_bridge =
        load_json_file(sample_dir / "notification.bridge.json");
    const auto searchbox_component =
        load_json_file(sample_dir / "searchbox.component.json");
    const auto searchbox_style =
        load_json_file(sample_dir / "searchbox.style.json");
    const auto searchbox_bridge =
        load_json_file(sample_dir / "searchbox.bridge.json");
    const auto sidebar_component =
        load_json_file(sample_dir / "sidebar.component.json");
    const auto sidebar_style =
        load_json_file(sample_dir / "sidebar.style.json");
    const auto sidebar_bridge =
        load_json_file(sample_dir / "sidebar.bridge.json");
    const auto tooltip_component =
        load_json_file(sample_dir / "tooltip.component.json");
    const auto tooltip_style =
        load_json_file(sample_dir / "tooltip.style.json");
    const auto tooltip_bridge =
        load_json_file(sample_dir / "tooltip.bridge.json");
    const auto accordion_component =
        load_json_file(sample_dir / "accordion.component.json");
    const auto accordion_style =
        load_json_file(sample_dir / "accordion.style.json");
    const auto accordion_bridge =
        load_json_file(sample_dir / "accordion.bridge.json");
    const auto toggle_group_component =
        load_json_file(sample_dir / "toggle-group.component.json");
    const auto toggle_group_style =
        load_json_file(sample_dir / "toggle-group.style.json");
    const auto toggle_group_bridge =
        load_json_file(sample_dir / "toggle-group.bridge.json");
    const auto spinner_component =
        load_json_file(sample_dir / "spinner.component.json");
    const auto spinner_style =
        load_json_file(sample_dir / "spinner.style.json");
    const auto spinner_bridge =
        load_json_file(sample_dir / "spinner.bridge.json");
    const auto toolbar_component =
        load_json_file(sample_dir / "toolbar.component.json");
    const auto toolbar_style =
        load_json_file(sample_dir / "toolbar.style.json");
    const auto toolbar_bridge =
        load_json_file(sample_dir / "toolbar.bridge.json");
    const auto listview_component =
        load_json_file(sample_dir / "listview.component.json");
    const auto listview_style =
        load_json_file(sample_dir / "listview.style.json");
    const auto listview_bridge =
        load_json_file(sample_dir / "listview.bridge.json");

    validate_component_ir(button_component);
    validate_style_ir(button_component, button_style);
    validate_bridge_ir(button_component, button_bridge);

    validate_component_ir(dialog_component);
    validate_style_ir(dialog_component, dialog_style);
    validate_bridge_ir(dialog_component, dialog_bridge);

    validate_component_ir(input_component);
    validate_style_ir(input_component, input_style);
    validate_bridge_ir(input_component, input_bridge);
    validate_component_ir(textarea_component);
    validate_style_ir(textarea_component, textarea_style);
    validate_bridge_ir(textarea_component, textarea_bridge);

    validate_component_ir(select_component);
    validate_style_ir(select_component, select_style);
    validate_bridge_ir(select_component, select_bridge);

    validate_component_ir(tabs_component);
    validate_style_ir(tabs_component, tabs_style);
    validate_bridge_ir(tabs_component, tabs_bridge);

    validate_component_ir(checkbox_component);
    validate_style_ir(checkbox_component, checkbox_style);
    validate_bridge_ir(checkbox_component, checkbox_bridge);

    validate_component_ir(radio_component);
    validate_style_ir(radio_component, radio_style);
    validate_bridge_ir(radio_component, radio_bridge);

    validate_component_ir(switch_component);
    validate_style_ir(switch_component, switch_style);
    validate_bridge_ir(switch_component, switch_bridge);

    validate_component_ir(slider_component);
    validate_style_ir(slider_component, slider_style);
    validate_bridge_ir(slider_component, slider_bridge);

    validate_component_ir(dropdown_component);
    validate_style_ir(dropdown_component, dropdown_style);
    validate_bridge_ir(dropdown_component, dropdown_bridge);

    validate_component_ir(popover_component);
    validate_style_ir(popover_component, popover_style);
    validate_bridge_ir(popover_component, popover_bridge);

    validate_component_ir(toast_component);
    validate_style_ir(toast_component, toast_style);
    validate_bridge_ir(toast_component, toast_bridge);

    validate_component_ir(menu_component);
    validate_style_ir(menu_component, menu_style);
    validate_bridge_ir(menu_component, menu_bridge);

    validate_component_ir(notification_component);
    validate_style_ir(notification_component, notification_style);
    validate_bridge_ir(notification_component, notification_bridge);

    validate_component_ir(searchbox_component);
    validate_style_ir(searchbox_component, searchbox_style);
    validate_bridge_ir(searchbox_component, searchbox_bridge);

    validate_component_ir(sidebar_component);
    validate_style_ir(sidebar_component, sidebar_style);
    validate_bridge_ir(sidebar_component, sidebar_bridge);

    validate_component_ir(tooltip_component);
    validate_style_ir(tooltip_component, tooltip_style);
    validate_bridge_ir(tooltip_component, tooltip_bridge);
    validate_component_ir(accordion_component);
    validate_style_ir(accordion_component, accordion_style);
    validate_bridge_ir(accordion_component, accordion_bridge);
    validate_component_ir(toggle_group_component);
    validate_style_ir(toggle_group_component, toggle_group_style);
    validate_bridge_ir(toggle_group_component, toggle_group_bridge);
    validate_component_ir(spinner_component);
    validate_style_ir(spinner_component, spinner_style);
    validate_bridge_ir(spinner_component, spinner_bridge);
    validate_component_ir(toolbar_component);
    validate_style_ir(toolbar_component, toolbar_style);
    validate_bridge_ir(toolbar_component, toolbar_bridge);
    validate_component_ir(listview_component);
    validate_style_ir(listview_component, listview_style);
    validate_bridge_ir(listview_component, listview_bridge);
  }

  it("keeps current ir sample coverage explicit") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto expected_names = current_ir_sample_names();
    const auto actual_names = component_sample_names_in(sample_dir);
    require_sets_equal(expected_names, actual_names,
                       "shadcn IR sample coverage changed");

    for (const auto& name : expected_names) {
      const auto component_path = sample_dir / (name + ".component.json");
      const auto style_path = sample_dir / (name + ".style.json");
      const auto bridge_path = sample_dir / (name + ".bridge.json");
      check(fs::exists(component_path));
      check(fs::exists(style_path));
      check(fs::exists(bridge_path));

      const auto component = load_json_file(component_path);
      validate_component_ir(component);
      validate_style_ir(component, load_json_file(style_path));
      validate_bridge_ir(component, load_json_file(bridge_path));
    }
  }

  it("keeps registry backed ir mappings explicit") {
    const fs::path sample_dir = ir_root() / "samples";

    for (const auto& mapping : registry_backed_ir_samples()) {
      const auto component =
          load_json_file(sample_dir / (mapping.first + ".component.json"));
      check(component.contains("meta"));
      check(component.at("meta").contains("source"));
      check(component.at("meta").at("source").get<std::string>() ==
            mapping.second);
    }
  }
}

spec("shadcn utility whitelist can emit tailwind-like css") {
  it("drives the live rectangle tree through Box utility JIT") {
    const fs::path schema_dir = ir_root() / "schema";
    const auto whitelist = load_json_file(schema_dir / "utility_whitelist.json");

    flexUI::Box box(nullptr, flexUI::BoxOptions::legacy_without_jit());
    box.enable_utility_jit(whitelist);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "jit-button");
    button->add_utilities(
        "inline-flex rounded-md focus-visible:ring-1");
    button->add_class("unknown-live-token");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 120.0f);
    box.update();
    check(button->computed_style->display == flexUI::Display::Flex);
    check(approx_eq(button->computed_style->border_radius[0], 6.0f, 0.001f));
    check(approx_eq(button->computed_style->ring_width, 0.0f, 0.001f));
    check(box.missing_utility_tokens().empty());

    button->set_focus_visible(true);
    box.update();
    check(approx_eq(button->computed_style->ring_width, 1.0f, 0.001f));

    button->remove_class("rounded-md");
    button->remove_class("unknown-live-token");
    box.update();
    check(approx_eq(button->computed_style->border_radius[0], 0.0f, 0.001f));
    check(box.missing_utility_tokens().empty());
  }

  it("emits escaped class selectors for direct conditional pseudo and macro tokens") {
    const fs::path schema_dir = ir_root() / "schema";
    const auto whitelist = load_json_file(schema_dir / "utility_whitelist.json");

    const std::string css = flexUI::shadcn_ir::emit_utility_css(
        whitelist,
        std::vector<std::string>{
            "bg-primary",
            "focus-visible:ring-1",
            "data-[state=open]:bg-accent",
            "placeholder:text-muted-foreground",
            "button-base",
            "unknown-token"});

    require_contains(css, ".bg-primary {");
    require_contains(css, "background-color: var(--primary);");
    require_contains(css, ".focus-visible\\:ring-1:focus-visible {");
    require_contains(css, "ring-width: 1px;");
    require_contains(css,
                     ".data-\\[state\\=open\\]\\:bg-accent[data-state=\"open\"] {");
    require_contains(css, "background-color: var(--accent);");
    require_contains(css,
                     ".placeholder\\:text-muted-foreground::placeholder {");
    require_contains(css, "color: var(--muted-foreground);");
    require_contains(css, ".button-base {");
    require_contains(css, "border-radius: 0.375rem;");
    require_contains(css, "font-size: 0.875rem;");
    check(css.find("unknown-token") == std::string::npos);
  }

  it("emits shadcn arbitrary value utility tokens from registry sources") {
    const fs::path schema_dir = ir_root() / "schema";
    const auto whitelist = load_json_file(schema_dir / "utility_whitelist.json");

    const std::vector<std::string> tokens{
        "left-[50%]",
        "top-[50%]",
        "translate-x-[-50%]",
        "translate-y-[-50%]",
        "data-[state=closed]:slide-out-to-top-[48%]",
        "data-[state=open]:slide-in-from-top-[48%]",
        "max-h-[var(--radix-dropdown-menu-content-available-height)]",
        "h-[var(--radix-select-trigger-height)]",
        "min-w-[var(--radix-select-trigger-width)]",
        "data-[swipe=end]:translate-x-[var(--radix-toast-swipe-end-x)]",
        "data-[swipe=move]:translate-x-[var(--radix-toast-swipe-move-x)]",
        "data-[side=bottom]:translate-y-1",
        "data-[side=left]:-translate-x-1",
        "[&+div]:text-xs",
        "border-b",
        "sr-only"};

    const auto missing =
        flexUI::shadcn_ir::missing_utility_tokens(whitelist, tokens);
    check(missing.empty());

    const std::string css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, tokens);
    require_contains(css, ".left-\\[50\\%\\] {");
    require_contains(css, "left: 50%;");
    require_contains(
        css,
        ".data-\\[state\\=closed\\]\\:slide-out-to-top-\\[48\\%\\][data-state=\"closed\"] {");
    require_contains(css, "transform: translateY(-48%);");
    require_contains(css,
                     ".max-h-\\[var\\(--radix-dropdown-menu-content-available-height\\)\\] {");
    require_contains(css,
                     "max-height: var(--radix-dropdown-menu-content-available-height);");
    require_contains(
        css,
        ".data-\\[swipe\\=end\\]\\:translate-x-\\[var\\(--radix-toast-swipe-end-x\\)\\][data-swipe=\"end\"] {");
    require_contains(css, "transform: translateX(var(--radix-toast-swipe-end-x));");
    require_contains(
        css,
        ".data-\\[side\\=bottom\\]\\:translate-y-1[data-side=\"bottom\"] {");
    require_contains(css, "transform: translateY(0.25rem);");
    require_contains(css, ".\\[\\&\\+div\\]\\:text-xs+div {");
    require_contains(css, "clip: rect(0, 0, 0, 0);");
  }

  it("covers visual demo utility tokens") {
    const fs::path schema_dir = ir_root() / "schema";
    const auto whitelist = load_json_file(schema_dir / "utility_whitelist.json");
    const std::string visual_demo_source = load_text_file(
        fs::path(FLEXUI_PROJECT_SOURCE_DIR) / "flexUI" / "examples" /
        "visual_demo.cpp");

    const auto used_tokens = visual_demo_used_utility_tokens(visual_demo_source);
    const std::vector<std::string> tokens = vector_from_set(used_tokens);

    require_no_missing_utility_tokens(whitelist, tokens, "visual_demo");

    const std::string css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, tokens);
    require_contains(css, ".w-\\[1200px\\] {");
    require_contains(css, "width: 1200px;");
    require_contains(css, ".gap-2\\.5 {");
    require_contains(css, "gap: 0.625rem;");
    require_contains(css, ".h-\\[900px\\] {");
    require_contains(css, "height: 900px;");
    require_contains(css, ".bg-card {");
    require_contains(css, "background-color: var(--card);");
  }

  it("covers official stable registry summaries without missing tokens") {
    const fs::path schema_dir = ir_root() / "schema";
    const fs::path registry_dir =
        fs::path(FLEXUI_PROJECT_SOURCE_DIR) / "flexUI" / "tests" /
        "shadcn" / "registry-cache";
    const auto whitelist = load_json_file(schema_dir / "utility_whitelist.json");

    const std::vector<std::string> covered_components = {
        "button", "input", "select", "textarea", "toggle-group",
        "radio-group", "checkbox", "tooltip", "tabs", "switch",
        "accordion", "dropdown-menu", "dialog", "toast", "badge",
        "card", "alert", "avatar", "popover", "progress", "separator",
        "skeleton", "slider", "table", "label", "hover-card", "scroll-area",
        "input-otp", "drawer", "resizable", "toggle", "menubar", "command",
        "sheet", "form", "pagination", "sonner", "navigation-menu",
        "calendar", "chart"};

    for (const auto& component : covered_components) {
      const auto tokens = class_tokens_from_summary(
          load_json_file(registry_dir / (component + ".summary.json")));
      require_no_missing_utility_tokens(whitelist, tokens, component);
    }

    const auto button_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "button.summary.json"));
    const std::string button_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, button_tokens);
    require_contains(button_css, ".focus-visible\\:ring-1:focus-visible {");
    require_contains(button_css, ".\\[\\&_svg\\]\\:size-4 svg {");
    require_contains(button_css, "transition-property: color, background-color");

    const auto input_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "input.summary.json"));
    const std::string input_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, input_tokens);
    require_contains(input_css, ".file\\:text-sm::file-selector-button {");
    require_contains(input_css, "@media (min-width: 768px) {");

    const auto select_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "select.summary.json"));
    const std::string select_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, select_tokens);
    require_contains(
        select_css,
        ".data-\\[state\\=open\\]\\:animate-in[data-state=\"open\"] {");
    require_contains(select_css,
                     ".\\[\\&\\>span\\]\\:line-clamp-1>span {");
    require_contains(select_css,
                     "max-height: var(--radix-select-content-available-height);");

    const auto checkbox_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "checkbox.summary.json"));
    const std::string checkbox_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, checkbox_tokens);
    require_contains(
        checkbox_css,
        ".data-\\[state\\=checked\\]\\:bg-primary[data-state=\"checked\"] {");
    require_contains(checkbox_css, ".place-content-center {");

    const auto tooltip_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "tooltip.summary.json"));
    const std::string tooltip_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, tooltip_tokens);
    require_contains(tooltip_css, ".origin-\\[--radix-tooltip-content-transform-origin\\] {");
    require_contains(tooltip_css, ".overflow-hidden {");

    const auto tabs_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "tabs.summary.json"));
    const std::string tabs_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, tabs_tokens);
    require_contains(
        tabs_css,
        ".data-\\[state\\=active\\]\\:bg-background[data-state=\"active\"] {");
    require_contains(tabs_css, ".focus-visible\\:ring-2:focus-visible {");
    require_contains(tabs_css, ".transition-all {");

    const auto switch_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "switch.summary.json"));
    const std::string switch_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, switch_tokens);
    require_contains(
        switch_css,
        ".data-\\[state\\=checked\\]\\:translate-x-4[data-state=\"checked\"] {");
    require_contains(switch_css, "transform: translateX(1rem);");
    require_contains(switch_css,
                     ".focus-visible\\:ring-offset-background:focus-visible {");
    require_contains(switch_css, ".transition-transform {");

    const auto accordion_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "accordion.summary.json"));
    const std::string accordion_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, accordion_tokens);
    require_contains(
        accordion_css,
        ".\\[\\&\\[data-state\\=open\\]\\>svg\\]\\:rotate-180[data-state=open]>svg {");
    require_contains(
        accordion_css,
        ".data-\\[state\\=closed\\]\\:animate-accordion-up[data-state=\"closed\"] {");
    require_contains(accordion_css, "animation-name: accordion-down;");
    require_contains(accordion_css, ".duration-200 {");

    const auto dropdown_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "dropdown-menu.summary.json"));
    const std::string dropdown_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, dropdown_tokens);
    require_contains(dropdown_css, ".\\[\\&\\>svg\\]\\:size-4>svg {");
    require_contains(
        dropdown_css,
        ".origin-\\[--radix-dropdown-menu-content-transform-origin\\] {");
    require_contains(dropdown_css, ".tracking-widest {");
    require_contains(dropdown_css, ".opacity-60 {");

    const auto dialog_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "dialog.summary.json"));
    const std::string dialog_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, dialog_tokens);
    require_contains(dialog_css, ".bg-black\\/80 {");
    require_contains(
        dialog_css,
        ".data-\\[state\\=open\\]\\:slide-in-from-left-1\\/2[data-state=\"open\"] {");
    require_contains(dialog_css, "@media (min-width: 640px) {");
    require_contains(
        dialog_css, ".sm\\:space-x-2 > :not([hidden]) ~ :not([hidden]) {");

    const auto toast_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "toast.summary.json"));
    const std::string toast_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, toast_tokens);
    require_contains(
        toast_css,
        ".data-\\[swipe\\=move\\]\\:transition-none[data-swipe=\"move\"] {");
    require_contains(
        toast_css,
        ".data-\\[state\\=open\\]\\:sm\\:slide-in-from-bottom-full[data-state=\"open\"] {");
    require_contains(
        toast_css,
        ".destructive .group-\\[\\.destructive\\]\\:hover\\:bg-destructive:hover {");
    require_contains(toast_css, ".group:hover .group-hover\\:opacity-100 {");
    require_contains(toast_css, ".text-foreground\\/50 {");
    require_contains(toast_css, ".z-\\[100\\] {");

    const auto badge_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "badge.summary.json"));
    const std::string badge_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, badge_tokens);
    require_contains(badge_css, ".hover\\:bg-primary\\/80:hover {");
    require_contains(badge_css, "padding-left: 0.625rem;");

    const auto alert_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "alert.summary.json"));
    const std::string alert_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, alert_tokens);
    require_contains(alert_css, "transform: translateY(-3px);");
    require_contains(alert_css, "@media (prefers-color-scheme: dark) {");

    const auto popover_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "popover.summary.json"));
    const std::string popover_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, popover_tokens);
    require_contains(popover_css,
                     ".origin-\\[--radix-popover-content-transform-origin\\] {");
    require_contains(popover_css, "width: 18rem;");

    const auto table_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "table.summary.json"));
    const std::string table_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, table_tokens);
    require_contains(table_css, "caption-side: bottom;");
    require_contains(table_css, "vertical-align: middle;");
    require_contains(table_css, "transform: translateY(2px);");

    const auto label_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "label.summary.json"));
    const std::string label_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, label_tokens);
    require_contains(label_css, ".peer:disabled ~ .peer-disabled\\:opacity-70 {");

    const auto command_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "command.summary.json"));
    const std::string command_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, command_tokens);
    require_contains(command_css,
                     ".\\[\\&_\\[cmdk-item\\]\\]\\:py-3 [cmdk-item] {");
    require_contains(command_css,
                     ".data-\\[selected\\=true\\]\\:bg-accent[data-selected=\"true\"] {");

    const auto resizable_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "resizable.summary.json"));
    const std::string resizable_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, resizable_tokens);
    require_contains(resizable_css, ".after\\:absolute::after {");
    require_contains(
        resizable_css,
        ".data-\\[panel-group-direction\\=vertical\\]\\:after\\:h-1[data-panel-group-direction=\"vertical\"]::after {");

    const auto sheet_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "sheet.summary.json"));
    const std::string sheet_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, sheet_tokens);
    require_contains(sheet_css,
                     ".data-\\[state\\=closed\\]\\:duration-300[data-state=\"closed\"] {");
    require_contains(sheet_css, ".sm\\:max-w-sm {");

    const auto navigation_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "navigation-menu.summary.json"));
    const std::string navigation_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, navigation_tokens);
    require_contains(
        navigation_css,
        ".data-\\[motion\\=from-end\\]\\:slide-in-from-right-52[data-motion=\"from-end\"] {");
    require_contains(
        navigation_css,
        ".group[data-state=\"open\"] .group-data-\\[state\\=open\\]\\:rotate-180 {");
    require_contains(navigation_css, "height: var(--radix-navigation-menu-viewport-height);");

    const auto calendar_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "calendar.summary.json"));
    const std::string calendar_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, calendar_tokens);
    require_contains(calendar_css, "--cell-size: 2rem;");
    require_contains(
        calendar_css,
        ".data-\\[range-start\\=true\\]\\:bg-primary[data-range-start=\"true\"] {");
    require_contains(calendar_css,
                     ".group\\/day[data-focused=\"true\"] "
                     ".group-data-\\[focused\\=true\\]\\/day\\:ring-\\[3px\\] {");

    const auto chart_tokens = class_tokens_from_summary(
        load_json_file(registry_dir / "chart.summary.json"));
    const std::string chart_css =
        flexUI::shadcn_ir::emit_utility_css(whitelist, chart_tokens);
    require_contains(chart_css, "aspect-ratio: 16 / 9;");
    require_contains(chart_css, "background-color: var(--color-bg);");
    require_contains(chart_css, "fill: var(--muted-foreground);");
  }
}

spec("shadcn ir assets can emit stable css") {
  it("renders button and dialog css from style ir") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto button_component =
        load_json_file(sample_dir / "button.component.json");
    const auto button_style = load_json_file(sample_dir / "button.style.json");
    const auto dialog_component =
        load_json_file(sample_dir / "dialog.component.json");
    const auto dialog_style = load_json_file(sample_dir / "dialog.style.json");
    const auto input_component =
        load_json_file(sample_dir / "input.component.json");
    const auto input_style = load_json_file(sample_dir / "input.style.json");
    const auto select_component =
        load_json_file(sample_dir / "select.component.json");
    const auto select_style = load_json_file(sample_dir / "select.style.json");
    const auto tabs_component =
        load_json_file(sample_dir / "tabs.component.json");
    const auto tabs_style = load_json_file(sample_dir / "tabs.style.json");
    const auto checkbox_component =
        load_json_file(sample_dir / "checkbox.component.json");
    const auto checkbox_style =
        load_json_file(sample_dir / "checkbox.style.json");
    const auto radio_component =
        load_json_file(sample_dir / "radio.component.json");
    const auto radio_style = load_json_file(sample_dir / "radio.style.json");
    const auto switch_component =
        load_json_file(sample_dir / "switch.component.json");
    const auto switch_style = load_json_file(sample_dir / "switch.style.json");
    const auto slider_component =
        load_json_file(sample_dir / "slider.component.json");
    const auto slider_style = load_json_file(sample_dir / "slider.style.json");
    const auto calendar_component =
        load_json_file(sample_dir / "calendar.component.json");
    const auto calendar_style =
        load_json_file(sample_dir / "calendar.style.json");
    const auto resizable_component =
        load_json_file(sample_dir / "resizable.component.json");
    const auto resizable_style =
        load_json_file(sample_dir / "resizable.style.json");
    const auto chart_component =
        load_json_file(sample_dir / "chart.component.json");
    const auto chart_style = load_json_file(sample_dir / "chart.style.json");
    const auto menubar_component =
        load_json_file(sample_dir / "menubar.component.json");
    const auto menubar_style =
        load_json_file(sample_dir / "menubar.style.json");
    const auto navigation_menu_component =
        load_json_file(sample_dir / "navigation-menu.component.json");
    const auto navigation_menu_style =
        load_json_file(sample_dir / "navigation-menu.style.json");
    const auto dropdown_component =
        load_json_file(sample_dir / "dropdown.component.json");
    const auto dropdown_style =
        load_json_file(sample_dir / "dropdown.style.json");
    const auto popover_component =
        load_json_file(sample_dir / "popover.component.json");
    const auto popover_style =
        load_json_file(sample_dir / "popover.style.json");
    const auto toast_component =
        load_json_file(sample_dir / "toast.component.json");
    const auto toast_style = load_json_file(sample_dir / "toast.style.json");
    const auto menu_component =
        load_json_file(sample_dir / "menu.component.json");
    const auto menu_style = load_json_file(sample_dir / "menu.style.json");
    const auto notification_component =
        load_json_file(sample_dir / "notification.component.json");
    const auto notification_style =
        load_json_file(sample_dir / "notification.style.json");
    const auto searchbox_component =
        load_json_file(sample_dir / "searchbox.component.json");
    const auto searchbox_style =
        load_json_file(sample_dir / "searchbox.style.json");
    const auto sidebar_component =
        load_json_file(sample_dir / "sidebar.component.json");
    const auto sidebar_style =
        load_json_file(sample_dir / "sidebar.style.json");
    const auto tooltip_component =
        load_json_file(sample_dir / "tooltip.component.json");
    const auto tooltip_style =
        load_json_file(sample_dir / "tooltip.style.json");

    const std::string button_css =
        flexUI::shadcn_ir::emit_css(button_component, button_style);
    require_contains(button_css, "#button__button_root {");
    require_contains(button_css, "background-color: var(--primary);");
    require_contains(button_css, "#button__button_root:focus-visible {");
    require_contains(button_css, "ring-width: 1px;");

    const std::string dialog_css =
        flexUI::shadcn_ir::emit_css(dialog_component, dialog_style);
    require_contains(dialog_css, "#dialog__overlay[data-state=\"open\"] {");
    require_contains(dialog_css, "#dialog__content {");
    require_contains(dialog_css, "@keyframes dialog-content-in {");
    require_contains(dialog_css, "transform: translate(-50%, -50%);");

    const std::string input_css =
        flexUI::shadcn_ir::emit_css(input_component, input_style,
                                    json{{"size", "sm"}});
    require_contains(input_css, "#input__input_root {");
    require_contains(input_css, "#input__input_root::placeholder {");
    require_contains(input_css, "height: 2rem;");

    const std::string select_css =
        flexUI::shadcn_ir::emit_css(select_component, select_style);
    require_contains(select_css, "#select__select_root[data-state=\"open\"] {");
    require_contains(select_css, "#select__select_root:focus-visible {");

    const std::string tabs_css =
        flexUI::shadcn_ir::emit_css(tabs_component, tabs_style);
    require_contains(tabs_css, "#tabs__tabs_root {");
    require_contains(tabs_css, "#tabs__tabs_root[data-state=\"active\"] {");

    const std::string checkbox_css =
        flexUI::shadcn_ir::emit_css(checkbox_component, checkbox_style);
    require_contains(checkbox_css, "#checkbox__checkbox_root {");
    require_contains(checkbox_css, "#checkbox__checkbox_root:checked {");

    const std::string radio_css =
        flexUI::shadcn_ir::emit_css(radio_component, radio_style);
    require_contains(radio_css, "#radio__radio_root {");
    require_contains(radio_css, "#radio__radio_root:checked {");

    const std::string switch_css =
        flexUI::shadcn_ir::emit_css(switch_component, switch_style);
    require_contains(switch_css, "#switch__switch_root {");
    require_contains(switch_css, "#switch__switch_root:focus-visible {");

    const std::string slider_css =
        flexUI::shadcn_ir::emit_css(slider_component, slider_style);
    require_contains(slider_css, "#slider__slider_root {");
    require_contains(slider_css, "#slider__slider_root:focus-visible {");
    require_contains(slider_css, "#slider__slider_root:disabled {");

    const std::string calendar_css =
        flexUI::shadcn_ir::emit_css(calendar_component, calendar_style);
    require_contains(calendar_css, "#calendar__calendar_root {");
    require_contains(calendar_css, "--calendar-selected: 59, 130, 246, 255;");
    require_contains(calendar_css, "#calendar__calendar_root:focus-visible {");

    const std::string resizable_css =
        flexUI::shadcn_ir::emit_css(resizable_component, resizable_style);
    require_contains(resizable_css, "#resizable__resizable_group {");
    require_contains(resizable_css,
                     "#resizable__resizable_group[data-panel-group-direction=\"vertical\"] {");
    require_contains(resizable_css,
                     "#resizable__resizable_handle:focus-visible {");

    const std::string chart_css =
        flexUI::shadcn_ir::emit_css(chart_component, chart_style);
    require_contains(chart_css, "#chart__chart_root {");
    require_contains(chart_css, "aspect-ratio: 16 / 9;");
    require_contains(chart_css, "#chart__chart_tooltip[data-state=\"active\"] {");

    const std::string menubar_css =
        flexUI::shadcn_ir::emit_css(menubar_component, menubar_style);
    require_contains(menubar_css, "#menubar__menubar_root {");
    require_contains(menubar_css,
                     "#menubar__menubar_trigger[data-state=\"open\"] {");
    require_contains(menubar_css,
                     "#menubar__menubar_content[data-state=\"open\"] {");

    const std::string navigation_menu_css = flexUI::shadcn_ir::emit_css(
        navigation_menu_component, navigation_menu_style);
    require_contains(navigation_menu_css,
                     "#navigationmenu__navigation_menu_root {");
    require_contains(navigation_menu_css,
                     "#navigationmenu__navigation_menu_trigger[data-state=\"open\"] {");
    require_contains(navigation_menu_css,
                     "height: var(--radix-navigation-menu-viewport-height);");

    const std::string dropdown_css =
        flexUI::shadcn_ir::emit_css(dropdown_component, dropdown_style);
    require_contains(dropdown_css, "#dropdown__dropdown_root {");
    require_contains(dropdown_css,
                     "#dropdown__dropdown_root[data-state=\"open\"] {");

    const std::string popover_css =
        flexUI::shadcn_ir::emit_css(popover_component, popover_style);
    require_contains(popover_css, "#popover__popover_root {");
    require_contains(popover_css,
                     "#popover__popover_root[data-state=\"open\"] {");

    const std::string toast_css =
        flexUI::shadcn_ir::emit_css(toast_component, toast_style);
    require_contains(toast_css, "#toast__toast_root {");
    require_contains(toast_css, "#toast__toast_root[data-state=\"closed\"] {");

    const std::string menu_css =
        flexUI::shadcn_ir::emit_css(menu_component, menu_style);
    require_contains(menu_css, "#menu__menu_root {");
    require_contains(menu_css, "#menu__menu_root[data-state=\"open\"] {");

    const std::string notification_css =
        flexUI::shadcn_ir::emit_css(notification_component, notification_style);
    require_contains(notification_css, "#notification__notification_root {");
    require_contains(notification_css,
                     "#notification__notification_root[data-position=\"top-right\"][data-type=\"success\"] {");

    const std::string searchbox_css =
        flexUI::shadcn_ir::emit_css(searchbox_component, searchbox_style);
    require_contains(searchbox_css, "#searchbox__searchbox_root {");
    require_contains(searchbox_css,
                     "#searchbox__searchbox_root[data-state=\"open\"] {");

    const std::string sidebar_css =
        flexUI::shadcn_ir::emit_css(sidebar_component, sidebar_style);
    require_contains(sidebar_css, "#sidebar__sidebar_root {");
    require_contains(sidebar_css,
                     "#sidebar__sidebar_root[data-state=\"collapsed\"] {");

    const std::string tooltip_css =
        flexUI::shadcn_ir::emit_css(tooltip_component, tooltip_style);
    require_contains(tooltip_css, "#tooltip__tooltip_root {");
    require_contains(tooltip_css,
                     "#tooltip__tooltip_root[data-side=\"right\"] {");
  }
}

spec("shadcn ir emitted css feeds the existing style engine") {
  it("applies default button styles through emitted selectors") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto button_component =
        load_json_file(sample_dir / "button.component.json");
    const auto button_style = load_json_file(sample_dir / "button.style.json");
    const std::string button_css =
        flexUI::shadcn_ir::emit_css(button_component, button_style);

    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create_widget<flexUI::ButtonWidget>(
        "button", flexUI::shadcn_ir::node_dom_id(button_component, "button_root"),
        "Save");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        --primary: #112233;
        --primary-foreground: #f8fafc;
        --ring: #2563eb;
        --input: #cbd5e1;
        --background: #ffffff;
        --foreground: #020617;
        --accent: #f1f5f9;
        --accent-foreground: #0f172a;
      }
    )");
    box.load_css(button_css);
    box.update();

    check(approx_eq(button->style_.font_size, 14.0f, 0.001f));
    check(approx_eq(button->style_.border_radius[0], 6.0f, 0.001f));
    check(approx_eq(button->style_.height, 36.0f, 0.001f));

    button->set_state("focus-visible", true);
    box.update();
    check(approx_eq(button->style_.ring_width, 1.0f, 0.001f));
  }

  it("scopes emitted css and instantiated node ids per component instance") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto button_component =
        load_json_file(sample_dir / "button.component.json");
    const auto button_style = load_json_file(sample_dir / "button.style.json");

    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    auto primary_tree = flexUI::shadcn_ir::instantiate_component_tree(
        box, button_component, json{{"text", "Save"}}, "primary-action");
    auto secondary_tree = flexUI::shadcn_ir::instantiate_component_tree(
        box, button_component, json{{"text", "Cancel"}}, "secondary-action");
    check(box.root() == root);
    root->append(primary_tree.root);
    root->append(secondary_tree.root);
    box.set_viewport(420.0f, 160.0f);

    box.load_css(R"(
      #root {
        --primary: #112233;
        --primary-foreground: #f8fafc;
        --secondary: #e2e8f0;
        --secondary-foreground: #0f172a;
        --ring: #2563eb;
        --input: #cbd5e1;
        --background: #ffffff;
        --foreground: #020617;
        --accent: #f1f5f9;
        --accent-foreground: #0f172a;
      }
    )");
    box.load_css(flexUI::shadcn_ir::emit_css(
        button_component, button_style,
        json{{"variant", "default"}, {"size", "default"}},
        "primary-action"));
    box.load_css(flexUI::shadcn_ir::emit_css(
        button_component, button_style,
        json{{"variant", "secondary"}, {"size", "sm"}},
        "secondary-action"));
    box.update();

    check(primary_tree.root->id() == "button--primary-action__button_root");
    check(secondary_tree.root->id() == "button--secondary-action__button_root");
    check(primary_tree.root != secondary_tree.root);
    check(box.get_by_id(primary_tree.root->id()) == primary_tree.root);
    check(box.get_by_id(secondary_tree.root->id()) == secondary_tree.root);
    const auto primary_bg = primary_tree.root->style_.background_color;
    check(approx_eq(primary_bg.r, 0x11 / 255.0f, 0.001f));
    check(approx_eq(primary_bg.g, 0x22 / 255.0f, 0.001f));
    check(approx_eq(primary_bg.b, 0x33 / 255.0f, 0.001f));
    check(approx_eq(primary_bg.a, 1.0f, 0.001f));

    const auto secondary_bg = secondary_tree.root->style_.background_color;
    check(approx_eq(secondary_bg.r, 0xe2 / 255.0f, 0.001f));
    check(approx_eq(secondary_bg.g, 0xe8 / 255.0f, 0.001f));
    check(approx_eq(secondary_bg.b, 0xf0 / 255.0f, 0.001f));
    check(approx_eq(secondary_bg.a, 1.0f, 0.001f));
    check(approx_eq(primary_tree.root->style_.height, 36.0f, 0.001f));
    check(approx_eq(secondary_tree.root->style_.height, 32.0f, 0.001f));
  }

  it("preserves existing instantiate_component root-setting behavior") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto button_component =
        load_json_file(sample_dir / "button.component.json");

    flexUI::Box box(nullptr);
    auto* app_root = box.create("div", "app-root");
    box.set_root(app_root);

    auto tree = flexUI::shadcn_ir::instantiate_component(
        box, button_component, json{{"text", "Legacy root"}});

    check(tree.root != nullptr);
    check(box.root() == tree.root);
  }
}

spec("shadcn ir can instantiate flexUI trees") {
  it("builds registry backed generic element samples from component ir") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto badge_component =
        load_json_file(sample_dir / "badge.component.json");
    const auto badge_bridge = load_json_file(sample_dir / "badge.bridge.json");
    const auto card_component =
        load_json_file(sample_dir / "card.component.json");
    const auto alert_component =
        load_json_file(sample_dir / "alert.component.json");
    const auto alert_bridge = load_json_file(sample_dir / "alert.bridge.json");
    const auto label_component =
        load_json_file(sample_dir / "label.component.json");
    const auto label_bridge = load_json_file(sample_dir / "label.bridge.json");
    const auto separator_component =
        load_json_file(sample_dir / "separator.component.json");
    const auto separator_bridge =
        load_json_file(sample_dir / "separator.bridge.json");
    const auto skeleton_component =
        load_json_file(sample_dir / "skeleton.component.json");
    const auto skeleton_bridge =
        load_json_file(sample_dir / "skeleton.bridge.json");
    const auto avatar_component =
        load_json_file(sample_dir / "avatar.component.json");
    const auto avatar_bridge = load_json_file(sample_dir / "avatar.bridge.json");
    const auto progress_component =
        load_json_file(sample_dir / "progress.component.json");
    const auto progress_bridge =
        load_json_file(sample_dir / "progress.bridge.json");
    const auto table_component =
        load_json_file(sample_dir / "table.component.json");
    const auto table_bridge = load_json_file(sample_dir / "table.bridge.json");
    const auto pagination_component =
        load_json_file(sample_dir / "pagination.component.json");
    const auto pagination_bridge =
        load_json_file(sample_dir / "pagination.bridge.json");
    const auto toggle_component =
        load_json_file(sample_dir / "toggle.component.json");
    const auto toggle_bridge = load_json_file(sample_dir / "toggle.bridge.json");
    const auto hover_card_component =
        load_json_file(sample_dir / "hover-card.component.json");
    const auto hover_card_bridge =
        load_json_file(sample_dir / "hover-card.bridge.json");
    const auto scroll_area_component =
        load_json_file(sample_dir / "scroll-area.component.json");
    const auto scroll_area_bridge =
        load_json_file(sample_dir / "scroll-area.bridge.json");
    const auto resizable_component =
        load_json_file(sample_dir / "resizable.component.json");
    const auto resizable_bridge =
        load_json_file(sample_dir / "resizable.bridge.json");
    const auto chart_component =
        load_json_file(sample_dir / "chart.component.json");
    const auto chart_bridge = load_json_file(sample_dir / "chart.bridge.json");
    const auto menubar_component =
        load_json_file(sample_dir / "menubar.component.json");
    const auto menubar_bridge =
        load_json_file(sample_dir / "menubar.bridge.json");
    const auto navigation_menu_component =
        load_json_file(sample_dir / "navigation-menu.component.json");
    const auto navigation_menu_bridge =
        load_json_file(sample_dir / "navigation-menu.bridge.json");
    const auto input_otp_component =
        load_json_file(sample_dir / "input-otp.component.json");
    const auto input_otp_bridge =
        load_json_file(sample_dir / "input-otp.bridge.json");
    const auto sheet_component =
        load_json_file(sample_dir / "sheet.component.json");
    const auto sheet_bridge = load_json_file(sample_dir / "sheet.bridge.json");
    const auto drawer_component =
        load_json_file(sample_dir / "drawer.component.json");
    const auto drawer_bridge = load_json_file(sample_dir / "drawer.bridge.json");
    const auto form_component =
        load_json_file(sample_dir / "form.component.json");
    const auto form_bridge = load_json_file(sample_dir / "form.bridge.json");

    flexUI::Box box(nullptr);

    auto badge_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, badge_component);
    check(badge_tree.root->widget == nullptr);
    check(badge_tree.root->tag() == "span");
    if (const auto* role = badge_tree.root->attribute("role")) {
      check(*role == "status");
    } else {
      check(false);
    }
    flexUI::shadcn_ir::apply_bridge_states(
        badge_bridge, json{{"hover", true}, {"disabled", true}}, badge_tree);
    check(badge_tree.root->has_state("hover"));
    check(badge_tree.root->has_state("disabled"));
    if (const auto* disabled = badge_tree.root->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }

    auto card_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, card_component);
    check(card_tree.root->widget == nullptr);
    check(card_tree.nodes.size() == 6);
    check(card_tree.nodes.at("card_header")->parent_elem() == card_tree.root);
    check(card_tree.nodes.at("card_title")->parent_elem() ==
          card_tree.nodes.at("card_header"));
    check(card_tree.nodes.at("card_footer")->parent_elem() == card_tree.root);

    auto alert_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, alert_component);
    check(alert_tree.root->widget == nullptr);
    if (const auto* role = alert_tree.root->attribute("role")) {
      check(*role == "alert");
    } else {
      check(false);
    }
    flexUI::shadcn_ir::apply_bridge_states(
        alert_bridge, json{{"variant", "destructive"}}, alert_tree);
    if (const auto* variant = alert_tree.root->attribute("data-variant")) {
      check(*variant == "destructive");
    } else {
      check(false);
    }

    auto label_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, label_component);
    check(label_tree.root->tag() == "label");
    flexUI::shadcn_ir::apply_bridge_states(
        label_bridge, json{{"disabled", true}}, label_tree);
    check(label_tree.root->has_state("disabled"));
    if (const auto* disabled = label_tree.root->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }

    auto separator_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, separator_component);
    if (const auto* role = separator_tree.root->attribute("role")) {
      check(*role == "separator");
    } else {
      check(false);
    }
    flexUI::shadcn_ir::apply_bridge_states(
        separator_bridge, json{{"vertical", true}}, separator_tree);
    if (const auto* orientation =
            separator_tree.root->attribute("data-orientation")) {
      check(*orientation == "vertical");
    } else {
      check(false);
    }

    auto skeleton_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, skeleton_component);
    flexUI::shadcn_ir::apply_bridge_states(
        skeleton_bridge, json{{"loading", true}}, skeleton_tree);
    check(skeleton_tree.root->has_state("loading"));
    if (const auto* state = skeleton_tree.root->attribute("data-state")) {
      check(*state == "loading");
    } else {
      check(false);
    }

    auto avatar_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, avatar_component);
    check(avatar_tree.nodes.size() == 3);
    check(avatar_tree.nodes.at("avatar_image")->parent_elem() == avatar_tree.root);
    check(avatar_tree.nodes.at("avatar_fallback")->parent_elem() ==
          avatar_tree.root);
    flexUI::shadcn_ir::apply_bridge_states(
        avatar_bridge, json{{"error", true}, {"visible", true}}, avatar_tree);
    if (const auto* state =
            avatar_tree.nodes.at("avatar_image")->attribute("data-state")) {
      check(*state == "error");
    } else {
      check(false);
    }
    if (const auto* state =
            avatar_tree.nodes.at("avatar_fallback")->attribute("data-state")) {
      check(*state == "visible");
    } else {
      check(false);
    }

    auto progress_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, progress_component);
    check(progress_tree.nodes.at("progress_indicator")->parent_elem() ==
          progress_tree.root);
    flexUI::shadcn_ir::apply_bridge_states(
        progress_bridge, json{{"value", true}}, progress_tree);
    if (const auto* value = progress_tree.root->attribute("aria-valuenow")) {
      check(*value == "40");
    } else {
      check(false);
    }
    if (const auto* value =
            progress_tree.nodes.at("progress_indicator")->attribute("data-value")) {
      check(*value == "40");
    } else {
      check(false);
    }

    auto table_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, table_component);
    check(table_tree.root->tag() == "div");
    check(table_tree.nodes.at("table_root")->parent_elem() == table_tree.root);
    check(table_tree.nodes.at("table_row_selected")->parent_elem() ==
          table_tree.nodes.at("table_body"));
    flexUI::shadcn_ir::apply_bridge_states(
        table_bridge, json{{"selected", true}}, table_tree);
    check(table_tree.nodes.at("table_row_selected")->has_state("selected"));
    if (const auto* state =
            table_tree.nodes.at("table_row_selected")->attribute("data-state")) {
      check(*state == "selected");
    } else {
      check(false);
    }

    auto pagination_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, pagination_component);
    if (const auto* role = pagination_tree.root->attribute("role")) {
      check(*role == "navigation");
    } else {
      check(false);
    }
    check(pagination_tree.nodes.at("pagination_content")->parent_elem() ==
          pagination_tree.root);
    flexUI::shadcn_ir::apply_bridge_states(
        pagination_bridge,
        json{{"current", true}, {"disabled", true}, {"focus-visible", true}},
        pagination_tree);
    if (const auto* current =
            pagination_tree.nodes.at("pagination_page_current")
                ->attribute("aria-current")) {
      check(*current == "page");
    } else {
      check(false);
    }
    if (const auto* disabled =
            pagination_tree.nodes.at("pagination_previous")
                ->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }
    check(pagination_tree.nodes.at("pagination_next")
              ->has_state("focus-visible"));

    auto toggle_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, toggle_component);
    check(toggle_tree.root->tag() == "button");
    flexUI::shadcn_ir::apply_bridge_states(
        toggle_bridge, json{{"on", true}, {"disabled", true}}, toggle_tree);
    if (const auto* pressed = toggle_tree.root->attribute("aria-pressed")) {
      check(*pressed == "true");
    } else {
      check(false);
    }
    if (const auto* state = toggle_tree.root->attribute("data-state")) {
      check(*state == "on");
    } else {
      check(false);
    }
    check(toggle_tree.root->has_state("disabled"));

    auto hover_card_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, hover_card_component);
    check(hover_card_tree.nodes.at("hover_card_trigger")->parent_elem() ==
          hover_card_tree.root);
    check(hover_card_tree.nodes.at("hover_card_content")->parent_elem() ==
          hover_card_tree.root);
    flexUI::shadcn_ir::apply_bridge_states(
        hover_card_bridge, json{{"open", true}, {"side-bottom", true}},
        hover_card_tree);
    if (const auto* expanded =
            hover_card_tree.nodes.at("hover_card_trigger")
                ->attribute("aria-expanded")) {
      check(*expanded == "true");
    } else {
      check(false);
    }
    if (const auto* side =
            hover_card_tree.nodes.at("hover_card_content")
                ->attribute("data-side")) {
      check(*side == "bottom");
    } else {
      check(false);
    }

    auto scroll_area_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, scroll_area_component);
    check(scroll_area_tree.nodes.at("scroll_area_viewport")->parent_elem() ==
          scroll_area_tree.root);
    check(scroll_area_tree.nodes.at("scroll_area_thumb")->parent_elem() ==
          scroll_area_tree.nodes.at("scroll_area_scrollbar"));
    flexUI::shadcn_ir::apply_bridge_states(
        scroll_area_bridge,
        json{{"horizontal", true}, {"scrolling", true}}, scroll_area_tree);
    if (const auto* orientation =
            scroll_area_tree.nodes.at("scroll_area_scrollbar")
                ->attribute("data-orientation")) {
      check(*orientation == "horizontal");
    } else {
      check(false);
    }
    if (const auto* state =
            scroll_area_tree.nodes.at("scroll_area_thumb")
                ->attribute("data-state")) {
      check(*state == "scrolling");
    } else {
      check(false);
    }

    auto resizable_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, resizable_component);
    check(resizable_tree.root->widget == nullptr);
    check(resizable_tree.root->tag() == "div");
    check(resizable_tree.nodes.at("resizable_panel_start")->parent_elem() ==
          resizable_tree.root);
    check(resizable_tree.nodes.at("resizable_handle")->parent_elem() ==
          resizable_tree.root);
    check(resizable_tree.nodes.at("resizable_handle_grip")->parent_elem() ==
          resizable_tree.nodes.at("resizable_handle"));
    flexUI::shadcn_ir::apply_bridge_states(
        resizable_bridge,
        json{{"vertical", true},
             {"resizing", true},
             {"dragging", true},
             {"focus-visible", true},
             {"collapsed", true}},
        resizable_tree);
    if (const auto* direction =
            resizable_tree.root->attribute("data-panel-group-direction")) {
      check(*direction == "vertical");
    } else {
      check(false);
    }
    if (const auto* resizing = resizable_tree.root->attribute("data-resizing")) {
      check(*resizing == "true");
    } else {
      check(false);
    }
    if (const auto* orientation =
            resizable_tree.nodes.at("resizable_handle")
                ->attribute("aria-orientation")) {
      check(*orientation == "horizontal");
    } else {
      check(false);
    }
    if (const auto* state =
            resizable_tree.nodes.at("resizable_handle")
                ->attribute("data-resize-handle-state")) {
      check(*state == "drag");
    } else {
      check(false);
    }
    if (const auto* panel_state =
            resizable_tree.nodes.at("resizable_panel_start")
                ->attribute("data-panel-state")) {
      check(*panel_state == "collapsed");
    } else {
      check(false);
    }

    auto chart_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, chart_component);
    check(chart_tree.root->widget == nullptr);
    check(chart_tree.root->tag() == "div");
    check(chart_tree.nodes.at("chart_surface")->parent_elem() == chart_tree.root);
    check(chart_tree.nodes.at("chart_tooltip")->parent_elem() == chart_tree.root);
    check(chart_tree.nodes.at("chart_legend_item")->parent_elem() ==
          chart_tree.nodes.at("chart_legend"));
    flexUI::shadcn_ir::apply_bridge_states(
        chart_bridge,
        json{{"active", true}, {"highlighted", true}, {"theme-dark", true}},
        chart_tree);
    if (const auto* state = chart_tree.nodes.at("chart_tooltip")
                                ->attribute("data-state")) {
      check(*state == "active");
    } else {
      check(false);
    }
    if (const auto* hidden = chart_tree.nodes.at("chart_tooltip")
                                 ->attribute("aria-hidden")) {
      check(*hidden == "false");
    } else {
      check(false);
    }
    if (const auto* highlighted = chart_tree.nodes.at("chart_legend_item")
                                      ->attribute("data-highlighted")) {
      check(*highlighted == "true");
    } else {
      check(false);
    }
    if (const auto* theme = chart_tree.root->attribute("data-theme")) {
      check(*theme == "dark");
    } else {
      check(false);
    }

    auto menubar_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, menubar_component);
    check(menubar_tree.root->widget == nullptr);
    check(menubar_tree.root->tag() == "div");
    check(menubar_tree.nodes.at("menubar_trigger")->parent_elem() ==
          menubar_tree.root);
    check(menubar_tree.nodes.at("menubar_content")->parent_elem() ==
          menubar_tree.root);
    check(menubar_tree.nodes.at("menubar_shortcut")->parent_elem() ==
          menubar_tree.nodes.at("menubar_item"));
    flexUI::shadcn_ir::apply_bridge_states(
        menubar_bridge,
        json{{"open", true},
             {"side-top", true},
             {"focus", true},
             {"disabled", true},
             {"checked", true}},
        menubar_tree);
    if (const auto* expanded =
            menubar_tree.nodes.at("menubar_trigger")->attribute("aria-expanded")) {
      check(*expanded == "true");
    } else {
      check(false);
    }
    if (const auto* hidden =
            menubar_tree.nodes.at("menubar_content")->attribute("aria-hidden")) {
      check(*hidden == "false");
    } else {
      check(false);
    }
    if (const auto* side =
            menubar_tree.nodes.at("menubar_content")->attribute("data-side")) {
      check(*side == "top");
    } else {
      check(false);
    }
    if (const auto* disabled =
            menubar_tree.nodes.at("menubar_item")->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }
    if (const auto* checked = menubar_tree.nodes.at("menubar_checkbox_item")
                                  ->attribute("aria-checked")) {
      check(*checked == "true");
    } else {
      check(false);
    }

    auto navigation_tree = flexUI::shadcn_ir::instantiate_component_tree(
        box, navigation_menu_component);
    check(navigation_tree.root->widget == nullptr);
    check(navigation_tree.root->tag() == "nav");
    check(navigation_tree.nodes.at("navigation_menu_list")->parent_elem() ==
          navigation_tree.root);
    check(navigation_tree.nodes.at("navigation_menu_trigger")->parent_elem() ==
          navigation_tree.nodes.at("navigation_menu_item"));
    check(navigation_tree.nodes.at("navigation_menu_viewport")->parent_elem() ==
          navigation_tree.root);
    flexUI::shadcn_ir::apply_bridge_states(
        navigation_menu_bridge,
        json{{"open", true},
             {"motion-from-start", true},
             {"visible", true},
             {"disabled", true}},
        navigation_tree);
    if (const auto* expanded = navigation_tree.nodes.at("navigation_menu_trigger")
                                   ->attribute("aria-expanded")) {
      check(*expanded == "true");
    } else {
      check(false);
    }
    if (const auto* motion = navigation_tree.nodes.at("navigation_menu_content")
                                 ->attribute("data-motion")) {
      check(*motion == "from-start");
    } else {
      check(false);
    }
    if (const auto* state = navigation_tree.nodes.at("navigation_menu_viewport")
                                ->attribute("data-state")) {
      check(*state == "visible");
    } else {
      check(false);
    }
    if (const auto* disabled =
            navigation_tree.nodes.at("navigation_menu_trigger")
                ->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }

    auto input_otp_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, input_otp_component);
    check(input_otp_tree.nodes.at("input_otp_group")->parent_elem() ==
          input_otp_tree.root);
    check(input_otp_tree.nodes.at("input_otp_slot_0")->parent_elem() ==
          input_otp_tree.nodes.at("input_otp_group"));
    flexUI::shadcn_ir::apply_bridge_states(
        input_otp_bridge,
        json{{"active", true}, {"filled", true}, {"disabled", true}},
        input_otp_tree);
    if (const auto* active =
            input_otp_tree.nodes.at("input_otp_slot_0")
                ->attribute("data-active")) {
      check(*active == "true");
    } else {
      check(false);
    }
    if (const auto* filled =
            input_otp_tree.nodes.at("input_otp_slot_1")
                ->attribute("data-filled")) {
      check(*filled == "true");
    } else {
      check(false);
    }
    if (const auto* disabled =
            input_otp_tree.root->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }

    auto sheet_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, sheet_component);
    check(sheet_tree.root->widget == nullptr);
    check(sheet_tree.nodes.size() == 8);
    check(sheet_tree.nodes.at("sheet_content")->parent_elem() == sheet_tree.root);
    check(sheet_tree.nodes.at("sheet_close")->parent_elem() ==
          sheet_tree.nodes.at("sheet_content"));
    check(sheet_tree.nodes.at("sheet_title")->parent_elem() ==
          sheet_tree.nodes.at("sheet_header"));
    flexUI::shadcn_ir::apply_bridge_states(
        sheet_bridge,
        json{{"open", true}, {"side", "left"}, {"hover", true}},
        sheet_tree);
    check(sheet_tree.root->has_state("open"));
    check(sheet_tree.root->has_state("modal"));
    if (const auto* hidden = sheet_tree.root->attribute("aria-hidden")) {
      check(*hidden == "false");
    } else {
      check(false);
    }
    if (const auto* state =
            sheet_tree.nodes.at("sheet_overlay")->attribute("data-state")) {
      check(*state == "open");
    } else {
      check(false);
    }
    if (const auto* side =
            sheet_tree.nodes.at("sheet_content")->attribute("data-side")) {
      check(*side == "left");
    } else {
      check(false);
    }

    auto drawer_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, drawer_component);
    check(drawer_tree.root->widget == nullptr);
    check(drawer_tree.nodes.size() == 8);
    check(drawer_tree.nodes.at("drawer_content")->parent_elem() == drawer_tree.root);
    check(drawer_tree.nodes.at("drawer_handle")->parent_elem() ==
          drawer_tree.nodes.at("drawer_content"));
    check(drawer_tree.nodes.at("drawer_title")->parent_elem() ==
          drawer_tree.nodes.at("drawer_header"));
    flexUI::shadcn_ir::apply_bridge_states(
        drawer_bridge, json{{"open", true}}, drawer_tree);
    check(drawer_tree.root->has_state("open"));
    check(drawer_tree.root->has_state("modal"));
    if (const auto* hidden = drawer_tree.root->attribute("aria-hidden")) {
      check(*hidden == "false");
    } else {
      check(false);
    }
    if (const auto* state =
            drawer_tree.nodes.at("drawer_content")->attribute("data-state")) {
      check(*state == "open");
    } else {
      check(false);
    }

    auto form_tree =
        flexUI::shadcn_ir::instantiate_component_tree(box, form_component);
    check(form_tree.root->widget == nullptr);
    check(form_tree.root->tag() == "form");
    check(form_tree.nodes.at("form_item")->parent_elem() == form_tree.root);
    check(form_tree.nodes.at("form_label")->parent_elem() ==
          form_tree.nodes.at("form_item"));
    check(form_tree.nodes.at("form_message")->parent_elem() ==
          form_tree.nodes.at("form_item"));
    flexUI::shadcn_ir::apply_bridge_states(
        form_bridge,
        json{{"invalid", true},
             {"required", true},
             {"visible", true},
             {"disabled", true}},
        form_tree);
    if (const auto* invalid =
            form_tree.nodes.at("form_item")->attribute("data-invalid")) {
      check(*invalid == "true");
    } else {
      check(false);
    }
    if (const auto* required =
            form_tree.nodes.at("form_label")->attribute("data-required")) {
      check(*required == "true");
    } else {
      check(false);
    }
    if (const auto* described_by =
            form_tree.nodes.at("form_control")->attribute("aria-describedby")) {
      check(*described_by == "email-description email-message");
    } else {
      check(false);
    }
    if (const auto* state =
            form_tree.nodes.at("form_message")->attribute("data-state")) {
      check(*state == "visible");
    } else {
      check(false);
    }
  }

  it("builds the supported shadcn ir widget trees from component ir") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto button_component =
        load_json_file(sample_dir / "button.component.json");
    const auto dialog_component =
        load_json_file(sample_dir / "dialog.component.json");
    const auto input_component =
        load_json_file(sample_dir / "input.component.json");
    const auto textarea_component =
        load_json_file(sample_dir / "textarea.component.json");
    const auto select_component =
        load_json_file(sample_dir / "select.component.json");
    const auto tabs_component =
        load_json_file(sample_dir / "tabs.component.json");
    const auto checkbox_component =
        load_json_file(sample_dir / "checkbox.component.json");
    const auto radio_component =
        load_json_file(sample_dir / "radio.component.json");
    const auto switch_component =
        load_json_file(sample_dir / "switch.component.json");
    const auto dropdown_component =
        load_json_file(sample_dir / "dropdown.component.json");
    const auto popover_component =
        load_json_file(sample_dir / "popover.component.json");
    const auto toast_component =
        load_json_file(sample_dir / "toast.component.json");
    const auto menu_component =
        load_json_file(sample_dir / "menu.component.json");
    const auto notification_component =
        load_json_file(sample_dir / "notification.component.json");
    const auto searchbox_component =
        load_json_file(sample_dir / "searchbox.component.json");
    const auto sidebar_component =
        load_json_file(sample_dir / "sidebar.component.json");
    const auto tooltip_component =
        load_json_file(sample_dir / "tooltip.component.json");
    const auto accordion_component =
        load_json_file(sample_dir / "accordion.component.json");
    const auto toggle_group_component =
        load_json_file(sample_dir / "toggle-group.component.json");
    const auto spinner_component =
        load_json_file(sample_dir / "spinner.component.json");
    const auto toolbar_component =
        load_json_file(sample_dir / "toolbar.component.json");
    const auto listview_component =
        load_json_file(sample_dir / "listview.component.json");

    flexUI::Box button_box(nullptr);
    auto button_tree = flexUI::shadcn_ir::instantiate_component(
        button_box, button_component, json{{"text", "Save"}, {"disabled", true}});
    check(button_tree.root != nullptr);
    check(button_tree.root == button_tree.nodes.at("button_root"));
    check(button_tree.root->id() == "button__button_root");
    check(button_tree.root->attribute("role") != nullptr);
    if (const auto* role = button_tree.root->attribute("role")) {
      check(*role == "button");
    }
    check(button_tree.root->widget != nullptr);
    if (auto* widget = dynamic_cast<flexUI::ButtonWidget*>(button_tree.root->widget)) {
      check(widget->text() == "Save");
      check(widget->is_disabled());
    } else {
      check(false);
    }

    flexUI::Box dialog_box(nullptr);
    auto dialog_tree = flexUI::shadcn_ir::instantiate_component(
        dialog_box, dialog_component,
        json{{"title", "Delete item"}, {"message", "Delete permanently?"}, {"open", true}});
    check(dialog_tree.root != nullptr);
    check(dialog_tree.root == dialog_tree.nodes.at("dialog_root"));
    check(dialog_tree.nodes.size() == 8);
    check(dialog_tree.nodes.at("content")->parent_elem() == dialog_tree.root);
    check(dialog_tree.nodes.at("title")->parent_elem() == dialog_tree.nodes.at("header"));
    if (const auto* slot = dialog_tree.nodes.at("overlay")->attribute("data-slot")) {
      check(*slot == "overlay");
    } else {
      check(false);
    }
    if (auto* widget = dynamic_cast<flexUI::DialogWidget*>(dialog_tree.root->widget)) {
      check(widget->is_visible());
    } else {
      check(false);
    }

    flexUI::Box input_box(nullptr);
    auto input_tree = flexUI::shadcn_ir::instantiate_component(
        input_box, input_component,
        json{{"placeholder", "Work email"}, {"text", "dev@example.com"}, {"readonly", true}});
    if (auto* widget = dynamic_cast<flexUI::InputWidget*>(input_tree.root->widget)) {
      check(widget->placeholder() == "Work email");
      check(widget->text() == "dev@example.com");
      check(widget->is_readonly());
    } else {
      check(false);
    }

    flexUI::Box textarea_box(nullptr);
    auto textarea_tree = flexUI::shadcn_ir::instantiate_component(
        textarea_box, textarea_component,
        json{{"placeholder", "Describe issue"},
             {"text", "Line 1\nLine 2"},
             {"readonly", true},
             {"cursor_position", 4}});
    if (auto* widget =
            dynamic_cast<flexUI::TextAreaWidget*>(textarea_tree.root->widget)) {
      check(widget->placeholder() == "Describe issue");
      check(widget->text() == "Line 1\nLine 2");
      check(widget->is_readonly());
      check(widget->cursor_position() == 4);
    } else {
      check(false);
    }

    flexUI::Box select_box(nullptr);
    auto select_tree = flexUI::shadcn_ir::instantiate_component(
        select_box, select_component,
        json{{"options", json::array({"Alpha", "Beta", "Gamma"})},
             {"selected_index", 1},
             {"open", true},
             {"disabled", true}});
    if (auto* widget = dynamic_cast<flexUI::SelectWidget*>(select_tree.root->widget)) {
      check(widget->selected_index() == 1);
      check(widget->selected_value() == "Beta");
      check(widget->is_expanded());
      check(widget->is_disabled());
    } else {
      check(false);
    }

    flexUI::Box tabs_box(nullptr);
    auto tabs_tree = flexUI::shadcn_ir::instantiate_component(
        tabs_box, tabs_component,
        json{{"tabs", json::array({json{{"label", "General"}, {"id", "general"}},
                                     json{{"label", "Billing"}, {"id", "billing"}, {"disabled", true}}})},
             {"active_id", "general"}});
    if (auto* widget = dynamic_cast<flexUI::TabsWidget*>(tabs_tree.root->widget)) {
      check(widget->tabs().size() == 2);
      check(widget->active_id() == "general");
      check(widget->tabs()[1].disabled);
    } else {
      check(false);
    }

    flexUI::Box checkbox_box(nullptr);
    auto checkbox_tree = flexUI::shadcn_ir::instantiate_component(
        checkbox_box, checkbox_component,
        json{{"label", "Terms"}, {"checked", true}, {"disabled", true}});
    if (auto* widget =
            dynamic_cast<flexUI::CheckboxWidget*>(checkbox_tree.root->widget)) {
      check(widget->label() == "Terms");
      check(widget->is_checked());
      check(widget->is_disabled());
    } else {
      check(false);
    }

    flexUI::Box radio_box(nullptr);
    auto radio_tree = flexUI::shadcn_ir::instantiate_component(
        radio_box, radio_component,
        json{{"label", "SMS"},
             {"value", "sms"},
             {"group", "contact"},
             {"checked", true},
             {"disabled", true}});
    if (auto* widget = dynamic_cast<flexUI::RadioWidget*>(radio_tree.root->widget)) {
      check(widget->label() == "SMS");
      check(widget->value() == "sms");
      check(widget->group() == "contact");
      check(widget->is_checked());
      check(widget->is_disabled());
    } else {
      check(false);
    }

    flexUI::Box switch_box(nullptr);
    auto switch_tree = flexUI::shadcn_ir::instantiate_component(
        switch_box, switch_component,
        json{{"label", "Airplane"}, {"checked", true}, {"disabled", true}});
    if (auto* widget =
            dynamic_cast<flexUI::SwitchWidget*>(switch_tree.root->widget)) {
      check(widget->label() == "Airplane");
      check(widget->is_checked());
      check(widget->is_disabled());
    } else {
      check(false);
    }

    flexUI::Box dropdown_box(nullptr);
    auto dropdown_tree = flexUI::shadcn_ir::instantiate_component(
        dropdown_box, dropdown_component,
        json{{"placeholder", "Pick one"},
             {"options", json::array({json{{"label", "React"}, {"value", "react"}},
                                      json{{"label", "Vue"}, {"value", "vue"}},
                                      json{{"label", "Svelte"},
                                            {"value", "svelte"},
                                            {"disabled", true}}})},
             {"selected_value", "vue"},
             {"open", true}});
    if (auto* widget =
            dynamic_cast<flexUI::DropdownWidget*>(dropdown_tree.root->widget)) {
      check(widget->placeholder() == "Pick one");
      check(widget->options().size() == 3);
      check(widget->options()[2].disabled);
      check(widget->selected_value() == "vue");
      check(widget->selected_index() == 1);
      check(widget->is_open());
    } else {
      check(false);
    }

    flexUI::Box popover_box(nullptr);
    auto popover_tree = flexUI::shadcn_ir::instantiate_component(
        popover_box, popover_component,
        json{{"title", "Share"},
             {"content", "Invite your team"},
             {"position", "right"},
             {"open", true}});
    if (auto* widget =
            dynamic_cast<flexUI::PopoverWidget*>(popover_tree.root->widget)) {
      check(widget->is_visible());
      if (const auto* attr = popover_tree.root->attribute("data-side")) {
        check(*attr == "right");
      } else {
        check(false);
      }
      if (const auto* attr = popover_tree.root->attribute("aria-label")) {
        check(*attr == "Share");
      } else {
        check(false);
      }
    } else {
      check(false);
    }

    flexUI::Box toast_box(nullptr);
    auto toast_tree = flexUI::shadcn_ir::instantiate_component(
        toast_box, toast_component,
        json{{"message", "Saved changes"},
             {"type", "success"},
             {"duration", 4500},
             {"open", true}});
    if (auto* widget = dynamic_cast<flexUI::ToastWidget*>(toast_tree.root->widget)) {
      check(widget->message() == "Saved changes");
      check(widget->toast_type() == flexUI::ToastWidget::Type::Success);
      check(approx_eq(widget->duration(), 4500.0f, 0.001f));
      check(widget->is_visible());
    } else {
      check(false);
    }

    flexUI::Box menu_box(nullptr);
    auto menu_tree = flexUI::shadcn_ir::instantiate_component(
        menu_box, menu_component,
        json{{"open", true},
             {"x", 24},
             {"y", 40},
             {"items", json::array({
                 json{{"id", "new"}, {"label", "New File"}, {"shortcut", "Ctrl+N"}},
                 json{{"separator", true}},
                 json{{"id", "share"},
                      {"label", "Share"},
                      {"children", json::array({
                          json{{"id", "email"}, {"label", "Email"}},
                          json{{"id", "link"}, {"label", "Copy link"}, {"checked", true}}
                      })}}
             })}});
    if (auto* widget = dynamic_cast<flexUI::MenuWidget*>(menu_tree.root->widget)) {
      check(widget->is_visible());
      check(widget->items().size() == 3);
      check(widget->items()[1].separator);
      check(widget->items()[2].children.size() == 2);
      check(widget->items()[2].children[1].checked);
    } else {
      check(false);
    }

    flexUI::Box notification_box(nullptr);
    auto notification_tree = flexUI::shadcn_ir::instantiate_component(
        notification_box, notification_component,
        json{{"position", "top-right"},
             {"notifications", json::array({
                 json{{"title", "Saved"},
                      {"message", "File uploaded"},
                      {"type", "success"},
                      {"duration", 5000}}
             })}});
    if (auto* widget = dynamic_cast<flexUI::NotificationWidget*>(
            notification_tree.root->widget)) {
      if (const auto* attr = notification_tree.root->attribute("role")) {
        check(*attr == "region");
      } else {
        check(false);
      }
      if (const auto* attr = notification_tree.root->attribute("data-state")) {
        check(*attr == "open");
      } else {
        check(false);
      }
      if (const auto* attr = notification_tree.root->attribute("data-position")) {
        check(*attr == "top-right");
      } else {
        check(false);
      }
      if (const auto* attr = notification_tree.root->attribute("data-count")) {
        check(*attr == "1");
      } else {
        check(false);
      }
      if (const auto* attr = notification_tree.root->attribute("data-type")) {
        check(*attr == "success");
      } else {
        check(false);
      }
      check(widget->has_overlay());
    } else {
      check(false);
    }

    flexUI::Box searchbox_box(nullptr);
    auto searchbox_tree = flexUI::shadcn_ir::instantiate_component(
        searchbox_box, searchbox_component,
        json{{"text", "api"}, {"open", true}});
    if (auto* widget = dynamic_cast<flexUI::SearchBoxWidget*>(
            searchbox_tree.root->widget)) {
      check(widget->text() == "api");
      if (const auto* attr = searchbox_tree.root->attribute("data-state")) {
        check(*attr == "open");
      } else {
        check(false);
      }
      if (const auto* attr = searchbox_tree.root->attribute("data-result-count")) {
        check(*attr == "1");
      } else {
        check(false);
      }
      if (const auto* attr = searchbox_tree.root->attribute("data-active-id")) {
        check(*attr == "api-ref");
      } else {
        check(false);
      }
    } else {
      check(false);
    }

    flexUI::Box sidebar_box(nullptr);
    auto sidebar_tree = flexUI::shadcn_ir::instantiate_component(
        sidebar_box, sidebar_component,
        json{{"selected_id", "search"}, {"collapsed", true}});
    if (auto* widget = dynamic_cast<flexUI::SidebarWidget*>(sidebar_tree.root->widget)) {
      check(widget->selected() == "search");
      check(widget->is_collapsed());
      if (const auto* attr = sidebar_tree.root->attribute("data-state")) {
        check(*attr == "collapsed");
      } else {
        check(false);
      }
      if (const auto* attr = sidebar_tree.root->attribute("data-item-count")) {
        check(*attr == "2");
      } else {
        check(false);
      }
      if (const auto* attr = sidebar_tree.root->attribute("data-selected-id")) {
        check(*attr == "search");
      } else {
        check(false);
      }
    } else {
      check(false);
    }

    flexUI::Box tooltip_box(nullptr);
    auto tooltip_tree = flexUI::shadcn_ir::instantiate_component(
        tooltip_box, tooltip_component,
        json{{"text", "Pinned help"}, {"position", "right"}, {"open", true}});
    if (auto* widget = dynamic_cast<flexUI::TooltipWidget*>(tooltip_tree.root->widget)) {
      check(widget->text() == "Pinned help");
      check(widget->position() == flexUI::TooltipWidget::Position::Right);
      widget->update(600.0f, *tooltip_tree.root);
      if (const auto* attr = tooltip_tree.root->attribute("data-state")) {
        check(*attr == "open");
      } else {
        check(false);
      }
      if (const auto* attr = tooltip_tree.root->attribute("data-side")) {
        check(*attr == "right");
      } else {
        check(false);
      }
      if (const auto* attr = tooltip_tree.root->attribute("aria-label")) {
        check(*attr == "Pinned help");
      } else {
        check(false);
      }
    } else {
      check(false);
    }

    flexUI::Box accordion_box(nullptr);
    auto accordion_tree = flexUI::shadcn_ir::instantiate_component(
        accordion_box, accordion_component,
        json{{"allow_multiple", true},
             {"sections", json::array({json{{"title", "General"},
                                           {"id", "general"},
                                           {"content_height", 120}},
                                      json{{"title", "Billing"},
                                           {"id", "billing"},
                                           {"content_height", 96}}})},
             {"expanded_ids", json::array({"general", "billing"})}});
    if (auto* widget = dynamic_cast<flexUI::AccordionWidget*>(
            accordion_tree.root->widget)) {
      check(widget->allow_multiple());
      check(widget->is_expanded("general"));
      check(widget->is_expanded("billing"));
    } else {
      check(false);
    }

    flexUI::Box toggle_group_box(nullptr);
    auto toggle_group_tree = flexUI::shadcn_ir::instantiate_component(
        toggle_group_box, toggle_group_component,
        json{{"multi_select", true},
             {"options",
              json::array({json{{"id", "left"}, {"label", "Left"}},
                           json{{"id", "center"}, {"label", "Center"}},
                           json{{"id", "right"}, {"label", "Right"}}})},
             {"selected_indices", json::array({0, 2})}});
    if (auto* widget = dynamic_cast<flexUI::ToggleGroupWidget*>(
            toggle_group_tree.root->widget)) {
      check(widget->multi_select());
      check(widget->options().size() == 3);
      check(widget->is_selected(0));
      check(widget->is_selected(2));
    } else {
      check(false);
    }

    flexUI::Box spinner_box(nullptr);
    auto spinner_tree = flexUI::shadcn_ir::instantiate_component(
        spinner_box, spinner_component,
        json{{"variant", "bars"}, {"spinning", false}});
    if (auto* widget =
            dynamic_cast<flexUI::SpinnerWidget*>(spinner_tree.root->widget)) {
      check(widget->variant() == flexUI::SpinnerWidget::Variant::Bars);
      check_false(widget->is_spinning());
    } else {
      check(false);
    }

    flexUI::Box toolbar_box(nullptr);
    auto toolbar_tree = flexUI::shadcn_ir::instantiate_component(
        toolbar_box, toolbar_component,
        json{{"items",
              json::array({
                  json{{"type", "dropdown"},
                       {"id", "more"},
                       {"icon", "⋯"},
                       {"items", json::array({json{{"id", "prefs"},
                                                  {"label", "Preferences"}}})}},
                  json{{"type", "toggle"},
                       {"id", "bold"},
                       {"icon", "B"},
                       {"toggled", true}}
              })},
             {"open_dropdown", "more"}});
    if (auto* widget =
            dynamic_cast<flexUI::ToolbarWidget*>(toolbar_tree.root->widget)) {
      if (const auto* attr = toolbar_tree.root->attribute("data-state")) {
        check(*attr == "open");
      } else {
        check(false);
      }
      if (const auto* attr = toolbar_tree.root->attribute("data-open-id")) {
        check(*attr == "more");
      } else {
        check(false);
      }
    } else {
      check(false);
    }

    flexUI::Box listview_box(nullptr);
    auto listview_tree = flexUI::shadcn_ir::instantiate_component(
        listview_box, listview_component,
        json{{"items",
              json::array({json{{"id", "alpha"}, {"text", "Alpha"}},
                           json{{"id", "beta"}, {"text", "Beta"}, {"secondary", "API"}},
                           json{{"id", "gamma"}, {"text", "Gamma"}}})},
             {"multi_select", true},
             {"selected_indices", json::array({1, 2})}});
    if (auto* widget =
            dynamic_cast<flexUI::ListViewWidget*>(listview_tree.root->widget)) {
      check(widget->items().size() == 3);
      check(widget->items()[1].selected);
      check(widget->items()[2].selected);
      if (const auto* attr = listview_tree.root->attribute("role")) {
        check(*attr == "listbox");
      } else {
        check(false);
      }
    } else {
      check(false);
    }
  }
}

spec("shadcn ir bridge data can project host states onto instantiated trees") {
  it("applies host and behavior bridges to the supported shadcn ir widgets") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto button_component =
        load_json_file(sample_dir / "button.component.json");
    const auto button_bridge =
        load_json_file(sample_dir / "button.bridge.json");
    const auto dialog_component =
        load_json_file(sample_dir / "dialog.component.json");
    const auto dialog_bridge =
        load_json_file(sample_dir / "dialog.bridge.json");
    const auto input_component =
        load_json_file(sample_dir / "input.component.json");
    const auto input_bridge =
        load_json_file(sample_dir / "input.bridge.json");
    const auto textarea_component =
        load_json_file(sample_dir / "textarea.component.json");
    const auto textarea_bridge =
        load_json_file(sample_dir / "textarea.bridge.json");
    const auto select_component =
        load_json_file(sample_dir / "select.component.json");
    const auto select_bridge =
        load_json_file(sample_dir / "select.bridge.json");
    const auto checkbox_component =
        load_json_file(sample_dir / "checkbox.component.json");
    const auto checkbox_bridge =
        load_json_file(sample_dir / "checkbox.bridge.json");
    const auto radio_component =
        load_json_file(sample_dir / "radio.component.json");
    const auto radio_bridge = load_json_file(sample_dir / "radio.bridge.json");
    const auto switch_component =
        load_json_file(sample_dir / "switch.component.json");
    const auto switch_bridge =
        load_json_file(sample_dir / "switch.bridge.json");
    const auto slider_component =
        load_json_file(sample_dir / "slider.component.json");
    const auto slider_bridge =
        load_json_file(sample_dir / "slider.bridge.json");
    const auto calendar_component =
        load_json_file(sample_dir / "calendar.component.json");
    const auto calendar_bridge =
        load_json_file(sample_dir / "calendar.bridge.json");
    const auto dropdown_component =
        load_json_file(sample_dir / "dropdown.component.json");
    const auto dropdown_bridge =
        load_json_file(sample_dir / "dropdown.bridge.json");
    const auto popover_component =
        load_json_file(sample_dir / "popover.component.json");
    const auto popover_bridge =
        load_json_file(sample_dir / "popover.bridge.json");
    const auto toast_component =
        load_json_file(sample_dir / "toast.component.json");
    const auto toast_bridge = load_json_file(sample_dir / "toast.bridge.json");
    const auto menu_component =
        load_json_file(sample_dir / "menu.component.json");
    const auto menu_bridge = load_json_file(sample_dir / "menu.bridge.json");
    const auto searchbox_component =
        load_json_file(sample_dir / "searchbox.component.json");
    const auto searchbox_bridge =
        load_json_file(sample_dir / "searchbox.bridge.json");
    const auto sidebar_component =
        load_json_file(sample_dir / "sidebar.component.json");
    const auto sidebar_bridge =
        load_json_file(sample_dir / "sidebar.bridge.json");
    const auto tooltip_component =
        load_json_file(sample_dir / "tooltip.component.json");
    const auto tooltip_bridge =
        load_json_file(sample_dir / "tooltip.bridge.json");
    const auto accordion_component =
        load_json_file(sample_dir / "accordion.component.json");
    const auto accordion_bridge =
        load_json_file(sample_dir / "accordion.bridge.json");
    const auto toggle_group_component =
        load_json_file(sample_dir / "toggle-group.component.json");
    const auto toggle_group_bridge =
        load_json_file(sample_dir / "toggle-group.bridge.json");
    const auto spinner_component =
        load_json_file(sample_dir / "spinner.component.json");
    const auto spinner_bridge =
        load_json_file(sample_dir / "spinner.bridge.json");
    const auto toolbar_component =
        load_json_file(sample_dir / "toolbar.component.json");
    const auto toolbar_bridge =
        load_json_file(sample_dir / "toolbar.bridge.json");
    const auto listview_component =
        load_json_file(sample_dir / "listview.component.json");
    const auto listview_bridge =
        load_json_file(sample_dir / "listview.bridge.json");

    flexUI::Box button_box(nullptr);
    auto button_tree =
        flexUI::shadcn_ir::instantiate_component(button_box, button_component);
    flexUI::shadcn_ir::apply_bridge_states(
        button_bridge, json{{"disabled", true}, {"focus-visible", true}}, button_tree);
    if (const auto* attr = button_tree.root->attribute("aria-disabled")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    check(button_tree.root->has_state("disabled"));
    check(button_tree.root->has_state("focus-visible"));

    flexUI::Box dialog_box(nullptr);
    auto dialog_tree =
        flexUI::shadcn_ir::instantiate_component(dialog_box, dialog_component);
    flexUI::shadcn_ir::apply_bridge_states(
        dialog_bridge,
        json{{"open", true}, {"closed", false}, {"disabled", true}}, dialog_tree);
    if (const auto* state = dialog_tree.root->attribute("data-state")) {
      check(*state == "open");
    } else {
      check(false);
    }
    if (const auto* hidden = dialog_tree.root->attribute("aria-hidden")) {
      check(*hidden == "false");
    } else {
      check(false);
    }
    if (const auto* close_disabled = dialog_tree.nodes.at("close")->attribute("disabled")) {
      check(close_disabled->empty());
    } else {
      check(false);
    }
    check(dialog_tree.root->has_state("open"));

    flexUI::Box input_box(nullptr);
    auto input_tree =
        flexUI::shadcn_ir::instantiate_component(input_box, input_component);
    flexUI::shadcn_ir::apply_bridge_states(
        input_bridge, json{{"disabled", true}, {"read-only", true}, {"focus-visible", true}},
        input_tree);
    if (const auto* attr = input_tree.root->attribute("aria-disabled")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = input_tree.root->attribute("aria-readonly")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    check(input_tree.root->has_state("disabled"));
    check(input_tree.root->has_state("read-only"));
    check(input_tree.root->has_state("focus-visible"));

    flexUI::Box textarea_box(nullptr);
    auto textarea_tree =
        flexUI::shadcn_ir::instantiate_component(textarea_box, textarea_component);
    flexUI::shadcn_ir::apply_bridge_states(
        textarea_bridge,
        json{{"disabled", true}, {"read-only", true}, {"placeholder-shown", true}},
        textarea_tree);
    if (const auto* attr = textarea_tree.root->attribute("aria-disabled")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = textarea_tree.root->attribute("aria-readonly")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    check(textarea_tree.root->has_state("disabled"));
    check(textarea_tree.root->has_state("read-only"));
    check(textarea_tree.root->has_state("placeholder-shown"));

    flexUI::Box select_box(nullptr);
    auto select_tree =
        flexUI::shadcn_ir::instantiate_component(select_box, select_component);
    flexUI::shadcn_ir::apply_bridge_states(
        select_bridge, json{{"open", true}, {"disabled", true}, {"focus-visible", true}},
        select_tree);
    if (const auto* attr = select_tree.root->attribute("aria-expanded")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = select_tree.root->attribute("aria-disabled")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = select_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    check(select_tree.root->has_state("disabled"));
    check(select_tree.root->has_state("focus-visible"));

    flexUI::Box checkbox_box(nullptr);
    auto checkbox_tree =
        flexUI::shadcn_ir::instantiate_component(checkbox_box, checkbox_component);
    flexUI::shadcn_ir::apply_bridge_states(
        checkbox_bridge,
        json{{"checked", true}, {"disabled", true}, {"focus-visible", true}},
        checkbox_tree);
    if (const auto* attr = checkbox_tree.root->attribute("aria-checked")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = checkbox_tree.root->attribute("aria-disabled")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = checkbox_tree.root->attribute("data-state")) {
      check(*attr == "checked");
    } else {
      check(false);
    }
    check(checkbox_tree.root->has_state("checked"));
    check(checkbox_tree.root->has_state("disabled"));
    check(checkbox_tree.root->has_state("focus-visible"));

    flexUI::Box radio_box(nullptr);
    auto radio_tree = flexUI::shadcn_ir::instantiate_component(
        radio_box, radio_component,
        json{{"value", "sms"}, {"group", "contact"}});
    flexUI::shadcn_ir::apply_bridge_states(
        radio_bridge, json{{"checked", true}, {"disabled", true}}, radio_tree);
    if (const auto* attr = radio_tree.root->attribute("aria-checked")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = radio_tree.root->attribute("data-group")) {
      check(*attr == "contact");
    } else {
      check(false);
    }
    if (const auto* attr = radio_tree.root->attribute("data-value")) {
      check(*attr == "sms");
    } else {
      check(false);
    }
    check(radio_tree.root->has_state("checked"));
    check(radio_tree.root->has_state("disabled"));

    flexUI::Box switch_box(nullptr);
    auto switch_tree =
        flexUI::shadcn_ir::instantiate_component(switch_box, switch_component);
    flexUI::shadcn_ir::apply_bridge_states(
        switch_bridge,
        json{{"checked", true}, {"disabled", true}, {"focus-visible", true}},
        switch_tree);
    if (const auto* attr = switch_tree.root->attribute("aria-checked")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = switch_tree.root->attribute("data-state")) {
      check(*attr == "checked");
    } else {
      check(false);
    }
    check(switch_tree.root->has_state("checked"));
    check(switch_tree.root->has_state("disabled"));
    check(switch_tree.root->has_state("focus-visible"));

    flexUI::Box slider_box(nullptr);
    auto slider_tree = flexUI::shadcn_ir::instantiate_component(
        slider_box, slider_component,
        json{{"min", -10.5}, {"max", 125.25}, {"value", 12.75}, {"step", 0.25}});
    auto* slider = dynamic_cast<flexUI::SliderWidget*>(slider_tree.root->widget);
    check(slider != nullptr);
    check(approx_eq(slider->min(), -10.5f, 0.001f));
    check(approx_eq(slider->max(), 125.25f, 0.001f));
    check(approx_eq(slider->value(), 12.75f, 0.001f));
    check(approx_eq(slider->step(), 0.25f, 0.001f));
    flexUI::shadcn_ir::apply_bridge_states(
        slider_bridge,
        json{{"value", 12.75}, {"min", -10.5}, {"max", 125.25},
             {"disabled", true}, {"focus-visible", true}},
        slider_tree);
    if (const auto* attr = slider_tree.root->attribute("aria-valuemin")) {
      check(*attr == "-10.5");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("aria-valuemax")) {
      check(*attr == "125.25");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("aria-valuenow")) {
      check(*attr == "12.75");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("aria-disabled")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("data-state")) {
      check(*attr == "disabled");
    } else {
      check(false);
    }
    check(slider_tree.root->has_state("value"));
    check(slider_tree.root->has_state("disabled"));
    check(slider_tree.root->has_state("focus-visible"));

    flexUI::Box calendar_box(nullptr);
    auto calendar_tree = flexUI::shadcn_ir::instantiate_component(
        calendar_box, calendar_component,
        json{{"selected_date", "2024-05-06"}, {"view_date", "2024-05-01"}});
    auto* calendar =
        dynamic_cast<flexUI::CalendarWidget*>(calendar_tree.root->widget);
    check(calendar != nullptr);
    check(calendar->selected_date().year == 2024);
    check(calendar->selected_date().month == 5);
    check(calendar->selected_date().day == 6);
    check(calendar->view_date().year == 2024);
    check(calendar->view_date().month == 5);
    check(calendar->view_date().day == 1);
    flexUI::shadcn_ir::apply_bridge_states(
        calendar_bridge,
        json{{"selected_date", "2024-05-06"},
             {"view_date", "2024-05-01"},
             {"focus-visible", true}},
        calendar_tree);
    if (const auto* attr = calendar_tree.root->attribute("data-value")) {
      check(*attr == "2024-05-06");
    } else {
      check(false);
    }
    if (const auto* attr = calendar_tree.root->attribute("data-view-month")) {
      check(*attr == "5");
    } else {
      check(false);
    }
    if (const auto* attr = calendar_tree.root->attribute("data-view-year")) {
      check(*attr == "2024");
    } else {
      check(false);
    }
    check(calendar_tree.root->has_state("selected_date"));
    check(calendar_tree.root->has_state("view_date"));
    check(calendar_tree.root->has_state("focus-visible"));

    flexUI::Box dropdown_box(nullptr);
    auto dropdown_tree =
        flexUI::shadcn_ir::instantiate_component(dropdown_box, dropdown_component);
    flexUI::shadcn_ir::apply_bridge_states(
        dropdown_bridge,
        json{{"open", true}, {"focus-visible", true}, {"selected_value", "vue"}},
        dropdown_tree);
    if (const auto* attr = dropdown_tree.root->attribute("aria-expanded")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    if (const auto* attr = dropdown_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    if (const auto* attr = dropdown_tree.root->attribute("data-value")) {
      check(*attr == "vue");
    } else {
      check(false);
    }
    check(dropdown_tree.root->has_state("open"));
    check(dropdown_tree.root->has_state("focus-visible"));

    flexUI::Box popover_box(nullptr);
    auto popover_tree =
        flexUI::shadcn_ir::instantiate_component(popover_box, popover_component);
    flexUI::shadcn_ir::apply_bridge_states(
        popover_bridge, json{{"open", true}, {"position", "right"}}, popover_tree);
    if (const auto* attr = popover_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    if (const auto* attr = popover_tree.root->attribute("data-side")) {
      check(*attr == "right");
    } else {
      check(false);
    }
    if (const auto* attr = popover_tree.root->attribute("aria-expanded")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    check(popover_tree.root->has_state("open"));

    flexUI::Box toast_box(nullptr);
    auto toast_tree =
        flexUI::shadcn_ir::instantiate_component(toast_box, toast_component);
    flexUI::shadcn_ir::apply_bridge_states(
        toast_bridge, json{{"open", true}, {"type", "error"}}, toast_tree);
    if (const auto* attr = toast_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    if (const auto* attr = toast_tree.root->attribute("data-type")) {
      check(*attr == "error");
    } else {
      check(false);
    }
    if (const auto* attr = toast_tree.root->attribute("role")) {
      check(*attr == "alert");
    } else {
      check(false);
    }
    check(toast_tree.root->has_state("open"));

    flexUI::shadcn_ir::apply_bridge_states(
        button_bridge, json{{"disabled", false}, {"focus-visible", false}}, button_tree);
    if (const auto* attr = button_tree.root->attribute("aria-disabled")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    check_false(button_tree.root->has_state("disabled"));
    check_false(button_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        dialog_bridge,
        json{{"open", false}, {"closed", true}, {"disabled", false}, {"hover", false}, {"focus", false}},
        dialog_tree);
    if (const auto* state = dialog_tree.root->attribute("data-state")) {
      check(*state == "closed");
    } else {
      check(false);
    }
    if (const auto* hidden = dialog_tree.root->attribute("aria-hidden")) {
      check(*hidden == "true");
    } else {
      check(false);
    }
    check(dialog_tree.nodes.at("close")->attribute("disabled") == nullptr);
    check_false(dialog_tree.root->has_state("open"));

    flexUI::shadcn_ir::apply_bridge_states(
        input_bridge, json{{"disabled", false}, {"read-only", false}, {"focus-visible", false}},
        input_tree);
    if (const auto* attr = input_tree.root->attribute("aria-disabled")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = input_tree.root->attribute("aria-readonly")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    check_false(input_tree.root->has_state("disabled"));
    check_false(input_tree.root->has_state("read-only"));
    check_false(input_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        select_bridge, json{{"open", false}, {"disabled", false}, {"focus-visible", false}},
        select_tree);
    if (const auto* attr = select_tree.root->attribute("aria-expanded")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = select_tree.root->attribute("aria-disabled")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = select_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }
    check_false(select_tree.root->has_state("disabled"));
    check_false(select_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        checkbox_bridge,
        json{{"checked", false}, {"disabled", false}, {"focus-visible", false}},
        checkbox_tree);
    if (const auto* attr = checkbox_tree.root->attribute("aria-checked")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = checkbox_tree.root->attribute("aria-disabled")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = checkbox_tree.root->attribute("data-state")) {
      check(*attr == "unchecked");
    } else {
      check(false);
    }
    check_false(checkbox_tree.root->has_state("checked"));
    check_false(checkbox_tree.root->has_state("disabled"));
    check_false(checkbox_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        radio_bridge, json{{"checked", false}, {"disabled", false}}, radio_tree);
    if (const auto* attr = radio_tree.root->attribute("aria-checked")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = radio_tree.root->attribute("aria-disabled")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = radio_tree.root->attribute("data-state")) {
      check(*attr == "unchecked");
    } else {
      check(false);
    }
    check_false(radio_tree.root->has_state("checked"));
    check_false(radio_tree.root->has_state("disabled"));

    flexUI::shadcn_ir::apply_bridge_states(
        switch_bridge,
        json{{"checked", false}, {"disabled", false}, {"focus-visible", false}},
        switch_tree);
    if (const auto* attr = switch_tree.root->attribute("aria-checked")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = switch_tree.root->attribute("aria-disabled")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = switch_tree.root->attribute("data-state")) {
      check(*attr == "unchecked");
    } else {
      check(false);
    }
    check_false(switch_tree.root->has_state("checked"));
    check_false(switch_tree.root->has_state("disabled"));
    check_false(switch_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        slider_bridge,
        json{{"value", 37.5}, {"min", 0.5}, {"max", 99.5},
             {"disabled", false}, {"focus-visible", false}},
        slider_tree);
    if (const auto* attr = slider_tree.root->attribute("aria-valuemin")) {
      check(*attr == "0.5");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("aria-valuemax")) {
      check(*attr == "99.5");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("aria-valuenow")) {
      check(*attr == "37.5");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("aria-disabled")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = slider_tree.root->attribute("data-state")) {
      check(*attr == "enabled");
    } else {
      check(false);
    }
    check(slider_tree.root->attribute("disabled") == nullptr);
    check_false(slider_tree.root->has_state("value"));
    check_false(slider_tree.root->has_state("disabled"));
    check_false(slider_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        calendar_bridge,
        json{{"selected_date", "2024-04-22"},
             {"view_date", "2024-04-01"},
             {"focus-visible", false}},
        calendar_tree);
    if (const auto* attr = calendar_tree.root->attribute("data-value")) {
      check(*attr == "2024-04-22");
    } else {
      check(false);
    }
    if (const auto* attr = calendar_tree.root->attribute("data-view-month")) {
      check(*attr == "4");
    } else {
      check(false);
    }
    check(calendar->selected_date().year == 2024);
    check(calendar->selected_date().month == 4);
    check(calendar->selected_date().day == 22);
    check(calendar->view_date().year == 2024);
    check(calendar->view_date().month == 4);
    check(calendar->view_date().day == 1);
    check_false(calendar_tree.root->has_state("selected_date"));
    check_false(calendar_tree.root->has_state("view_date"));
    check_false(calendar_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        dropdown_bridge,
        json{{"open", false}, {"focus-visible", false}, {"selected_value", "react"}},
        dropdown_tree);
    if (const auto* attr = dropdown_tree.root->attribute("aria-expanded")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    if (const auto* attr = dropdown_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }
    check(dropdown_tree.root->attribute("data-value") == nullptr);
    check_false(dropdown_tree.root->has_state("open"));
    check_false(dropdown_tree.root->has_state("focus-visible"));

    flexUI::shadcn_ir::apply_bridge_states(
        popover_bridge, json{{"open", false}, {"position", "left"}}, popover_tree);
    if (const auto* attr = popover_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }
    if (const auto* attr = popover_tree.root->attribute("aria-expanded")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    check_false(popover_tree.root->has_state("open"));

    flexUI::shadcn_ir::apply_bridge_states(
        toast_bridge, json{{"open", false}, {"type", "default"}}, toast_tree);
    if (const auto* attr = toast_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }
    if (const auto* attr = toast_tree.root->attribute("data-type")) {
      check(*attr == "default");
    } else {
      check(false);
    }
    if (const auto* attr = toast_tree.root->attribute("role")) {
      check(*attr == "status");
    } else {
      check(false);
    }
    check_false(toast_tree.root->has_state("open"));

    flexUI::Box menu_box(nullptr);
    auto menu_tree =
        flexUI::shadcn_ir::instantiate_component(menu_box, menu_component);
    flexUI::shadcn_ir::apply_bridge_states(menu_bridge, json{{"open", true}}, menu_tree);
    if (const auto* attr = menu_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    if (const auto* attr = menu_tree.root->attribute("role")) {
      check(*attr == "menu");
    } else {
      check(false);
    }
    if (const auto* attr = menu_tree.root->attribute("aria-hidden")) {
      check(*attr == "false");
    } else {
      check(false);
    }
    check(menu_tree.root->has_state("open"));

    flexUI::shadcn_ir::apply_bridge_states(menu_bridge, json{{"open", false}}, menu_tree);
    if (const auto* attr = menu_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }
    if (const auto* attr = menu_tree.root->attribute("aria-hidden")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    check_false(menu_tree.root->has_state("open"));

    flexUI::Box searchbox_box(nullptr);
    auto searchbox_tree = flexUI::shadcn_ir::instantiate_component(
        searchbox_box, searchbox_component);
    flexUI::shadcn_ir::apply_bridge_states(
        searchbox_bridge, json{{"open", true}, {"with-query", true}}, searchbox_tree);
    if (const auto* attr = searchbox_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    if (const auto* attr = searchbox_tree.root->attribute("data-value")) {
      check(*attr == "api");
    } else {
      check(false);
    }
    if (const auto* attr = searchbox_tree.root->attribute("data-result-count")) {
      check(*attr == "1");
    } else {
      check(false);
    }
    if (const auto* attr = searchbox_tree.root->attribute("aria-activedescendant")) {
      check(*attr == "api-ref");
    } else {
      check(false);
    }
    check(searchbox_tree.root->has_state("open"));

    flexUI::shadcn_ir::apply_bridge_states(
        searchbox_bridge, json{{"open", false}, {"with-query", false}}, searchbox_tree);
    if (const auto* attr = searchbox_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }
    check(searchbox_tree.root->attribute("data-value") == nullptr);
    if (const auto* attr = searchbox_tree.root->attribute("data-result-count")) {
      check(*attr == "2");
    } else {
      check(false);
    }
    check_false(searchbox_tree.root->has_state("open"));

    flexUI::Box sidebar_box(nullptr);
    auto sidebar_tree =
        flexUI::shadcn_ir::instantiate_component(sidebar_box, sidebar_component);
    flexUI::shadcn_ir::apply_bridge_states(
        sidebar_bridge, json{{"collapsed", true}, {"selected", true}}, sidebar_tree);
    if (const auto* attr = sidebar_tree.root->attribute("data-state")) {
      check(*attr == "collapsed");
    } else {
      check(false);
    }
    if (const auto* attr = sidebar_tree.root->attribute("data-selected-id")) {
      check(*attr == "search");
    } else {
      check(false);
    }
    check(sidebar_tree.root->has_state("collapsed"));

    flexUI::shadcn_ir::apply_bridge_states(
        sidebar_bridge, json{{"collapsed", false}, {"selected", false}}, sidebar_tree);
    if (const auto* attr = sidebar_tree.root->attribute("data-state")) {
      check(*attr == "expanded");
    } else {
      check(false);
    }
    check(sidebar_tree.root->attribute("data-selected-id") == nullptr);
    check_false(sidebar_tree.root->has_state("collapsed"));

    flexUI::Box tooltip_box(nullptr);
    auto tooltip_tree =
        flexUI::shadcn_ir::instantiate_component(tooltip_box, tooltip_component);
    flexUI::shadcn_ir::apply_bridge_states(
        tooltip_bridge, json{{"open", true}, {"right", true}, {"retitled", true}},
        tooltip_tree);
    if (auto* tooltip = dynamic_cast<flexUI::TooltipWidget*>(tooltip_tree.root->widget)) {
      tooltip->update(600.0f, *tooltip_tree.root);
    }
    if (const auto* attr = tooltip_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    if (const auto* attr = tooltip_tree.root->attribute("data-side")) {
      check(*attr == "right");
    } else {
      check(false);
    }
    if (const auto* attr = tooltip_tree.root->attribute("aria-label")) {
      check(*attr == "Pinned help");
    } else {
      check(false);
    }
    check(tooltip_tree.root->has_state("open"));

    flexUI::shadcn_ir::apply_bridge_states(
        tooltip_bridge, json{{"open", false}, {"right", false}, {"retitled", false}},
        tooltip_tree);
    if (auto* tooltip = dynamic_cast<flexUI::TooltipWidget*>(tooltip_tree.root->widget)) {
      tooltip->update(200.0f, *tooltip_tree.root);
    }
    if (const auto* attr = tooltip_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }
    if (const auto* attr = tooltip_tree.root->attribute("data-side")) {
      check(*attr == "top");
    } else {
      check(false);
    }
    check(tooltip_tree.root->attribute("aria-label") == nullptr);
    check_false(tooltip_tree.root->has_state("open"));

    flexUI::Box accordion_box(nullptr);
    auto accordion_tree =
        flexUI::shadcn_ir::instantiate_component(accordion_box, accordion_component);
    flexUI::shadcn_ir::apply_bridge_states(
        accordion_bridge, json{{"open", true}, {"closed", false}}, accordion_tree);
    if (const auto* attr = accordion_tree.root->attribute("data-state")) {
      check(*attr == "open");
    } else {
      check(false);
    }
    if (const auto* attr = accordion_tree.root->attribute("data-expanded-id")) {
      check(*attr == "general");
    } else {
      check(false);
    }
    if (const auto* attr = accordion_tree.root->attribute("aria-expanded")) {
      check(*attr == "true");
    } else {
      check(false);
    }

    flexUI::Box toggle_group_box(nullptr);
    auto toggle_group_tree = flexUI::shadcn_ir::instantiate_component(
        toggle_group_box, toggle_group_component);
    flexUI::shadcn_ir::apply_bridge_states(
        toggle_group_bridge, json{{"selected", true}}, toggle_group_tree);
    if (const auto* attr = toggle_group_tree.root->attribute("data-value")) {
      check(*attr == "center");
    } else {
      check(false);
    }
    if (const auto* attr = toggle_group_tree.root->attribute("data-selected-count")) {
      check(*attr == "1");
    } else {
      check(false);
    }
    check(toggle_group_tree.root->has_state("selected"));
    flexUI::shadcn_ir::apply_bridge_states(
        toggle_group_bridge, json{{"selected", false}, {"empty", true}},
        toggle_group_tree);
    check(toggle_group_tree.root->attribute("data-value") == nullptr);
    if (const auto* attr = toggle_group_tree.root->attribute("data-state")) {
      check(*attr == "unselected");
    } else {
      check(false);
    }

    flexUI::Box spinner_box(nullptr);
    auto spinner_tree =
        flexUI::shadcn_ir::instantiate_component(spinner_box, spinner_component);
    flexUI::shadcn_ir::apply_bridge_states(
        spinner_bridge, json{{"spinning", true}, {"stopped", false}}, spinner_tree);
    if (const auto* attr = spinner_tree.root->attribute("data-variant")) {
      check(*attr == "bars");
    } else {
      check(false);
    }
    if (const auto* attr = spinner_tree.root->attribute("aria-busy")) {
      check(*attr == "true");
    } else {
      check(false);
    }
    flexUI::shadcn_ir::apply_bridge_states(
        spinner_bridge, json{{"spinning", false}, {"stopped", true}}, spinner_tree);
    if (const auto* attr = spinner_tree.root->attribute("data-state")) {
      check(*attr == "stopped");
    } else {
      check(false);
    }

    flexUI::Box toolbar_box(nullptr);
    auto toolbar_tree =
        flexUI::shadcn_ir::instantiate_component(toolbar_box, toolbar_component);
    flexUI::shadcn_ir::apply_bridge_states(
        toolbar_bridge, json{{"open", true}}, toolbar_tree);
    if (const auto* attr = toolbar_tree.root->attribute("data-open-id")) {
      check(*attr == "more");
    } else {
      check(false);
    }
    flexUI::shadcn_ir::apply_bridge_states(
        toolbar_bridge, json{{"open", false}, {"closed", true}}, toolbar_tree);
    check(toolbar_tree.root->attribute("data-open-id") == nullptr);
    if (const auto* attr = toolbar_tree.root->attribute("data-state")) {
      check(*attr == "closed");
    } else {
      check(false);
    }

    flexUI::Box listview_box(nullptr);
    auto listview_tree =
        flexUI::shadcn_ir::instantiate_component(listview_box, listview_component);
    flexUI::shadcn_ir::apply_bridge_states(
        listview_bridge, json{{"selected", true}}, listview_tree);
    if (const auto* attr = listview_tree.root->attribute("data-selected-id")) {
      check(*attr == "beta");
    } else {
      check(false);
    }
    if (const auto* attr = listview_tree.root->attribute("data-selected-count")) {
      check(*attr == "1");
    } else {
      check(false);
    }
    flexUI::shadcn_ir::apply_bridge_states(
        listview_bridge, json{{"selected", false}, {"unselected", true}},
        listview_tree);
    check(listview_tree.root->attribute("data-selected-id") == nullptr);
    if (const auto* attr = listview_tree.root->attribute("data-state")) {
      check(*attr == "unselected");
    } else {
      check(false);
    }
  }
}

spec("shadcn ir tree css and bridge integrate end to end") {
  it("styles instantiated input and button nodes through emitted selectors and reconciled bridge states") {
    const fs::path sample_dir = ir_root() / "samples";
    const auto button_component =
        load_json_file(sample_dir / "button.component.json");
    const auto button_style = load_json_file(sample_dir / "button.style.json");
    const auto button_bridge =
        load_json_file(sample_dir / "button.bridge.json");
    const auto input_component =
        load_json_file(sample_dir / "input.component.json");
    const auto input_style = load_json_file(sample_dir / "input.style.json");
    const auto input_bridge =
        load_json_file(sample_dir / "input.bridge.json");

    flexUI::Box box(nullptr);
    auto tree = flexUI::shadcn_ir::instantiate_component(
        box, button_component, json{{"text", "Ship"}, {"disabled", false}});
    flexUI::Box input_box(nullptr);
    auto input_tree = flexUI::shadcn_ir::instantiate_component(
        input_box, input_component, json{{"placeholder", "Email"}, {"text", ""}});

    box.load_css(R"(
      #root {
        --primary: #2563eb;
        --primary-foreground: #f8fafc;
        --destructive: #dc2626;
        --destructive-foreground: #fff1f2;
        --input: #334155;
        --background: #ffffff;
        --foreground: #020617;
        --accent: #e2e8f0;
        --accent-foreground: #0f172a;
        --ring: #22c55e;
      }
    )");
    box.load_css(flexUI::shadcn_ir::emit_css(
        button_component, button_style, json{{"variant", "outline"}, {"size", "sm"}}));
    input_box.load_css(R"(
      #root {
        --primary: #2563eb;
        --primary-foreground: #f8fafc;
        --input: #334155;
        --muted-foreground: #64748b;
        --ring: #22c55e;
      }
    )");
    input_box.load_css(flexUI::shadcn_ir::emit_css(
        input_component, input_style, json{{"size", "sm"}}));

    flexUI::shadcn_ir::apply_bridge_states(
        button_bridge, json{{"focus-visible", true}, {"disabled", false}}, tree);
    flexUI::shadcn_ir::apply_bridge_states(
        input_bridge, json{{"focus-visible", true}, {"disabled", false}, {"read-only", false}},
        input_tree);
    box.update();
    input_box.update();

    check(tree.root != nullptr);
    check(approx_eq(tree.root->style_.height, 32.0f, 0.001f));
    check(approx_eq(tree.root->style_.border_width[0], 1.0f, 0.001f));
    check(approx_eq(tree.root->style_.ring_width, 1.0f, 0.001f));
    check_false(tree.root->has_state("disabled"));
    if (const auto* disabled = tree.root->attribute("aria-disabled")) {
      check(*disabled == "false");
    } else {
      check(false);
    }
    check(approx_eq(input_tree.root->style_.height, 32.0f, 0.001f));
    check(approx_eq(input_tree.root->style_.border_width[0], 1.0f, 0.001f));
    check(approx_eq(input_tree.root->style_.ring_width, 1.0f, 0.001f));
    if (const auto* readonly = input_tree.root->attribute("aria-readonly")) {
      check(*readonly == "false");
    } else {
      check(false);
    }

    flexUI::shadcn_ir::apply_bridge_states(
        button_bridge, json{{"focus-visible", false}, {"disabled", true}}, tree);
    flexUI::shadcn_ir::apply_bridge_states(
        input_bridge, json{{"focus-visible", false}, {"disabled", true}, {"read-only", true}},
        input_tree);
    box.update();
    input_box.update();

    if (const auto* disabled = tree.root->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }
    check(tree.root->has_state("disabled"));
    check_false(tree.root->has_state("focus-visible"));
    check(approx_eq(tree.root->style_.opacity, 0.5f, 0.001f));
    if (const auto* disabled = input_tree.root->attribute("aria-disabled")) {
      check(*disabled == "true");
    } else {
      check(false);
    }
    if (const auto* readonly = input_tree.root->attribute("aria-readonly")) {
      check(*readonly == "true");
    } else {
      check(false);
    }
    check(input_tree.root->has_state("disabled"));
    check(input_tree.root->has_state("read-only"));
    check_false(input_tree.root->has_state("focus-visible"));
    check(approx_eq(input_tree.root->style_.opacity, 0.5f, 0.001f));
  }
}
