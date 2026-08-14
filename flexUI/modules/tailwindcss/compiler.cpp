#include <flexUI/tailwindcss.h>

#include "tailwindcss_internal.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace flexUI::tailwind {

namespace detail {

std::string replace_all(std::string text, const std::string& needle,
                        const std::string& replacement) {
  std::size_t pos = 0;
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
  return it != pseudos.end() ? it->second : ":" + state;
}

std::string quote_json_value(const nlohmann::json& value) {
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

}  // namespace detail

namespace {

std::string escape_class_name(const std::string& value) {
  std::ostringstream out;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    const bool is_name_char = std::isalnum(ch) || ch == '_' || ch == '-';
    const bool needs_hex_leading_escape =
        (i == 0 && std::isdigit(ch)) ||
        (i == 1 && value[0] == '-' && std::isdigit(ch));

    if (is_name_char && !needs_hex_leading_escape) {
      out << static_cast<char>(ch);
    } else if (needs_hex_leading_escape || ch < 0x20 || ch == 0x7f) {
      out << '\\' << std::uppercase << std::hex << static_cast<int>(ch)
          << std::nouppercase << std::dec << ' ';
    } else {
      out << '\\' << static_cast<char>(ch);
    }
  }
  return out.str();
}

std::string utility_selector(const std::string& class_token,
                             const nlohmann::json& token_ir) {
  const std::string base_selector = "." + escape_class_name(class_token);
  std::string selector = token_ir.value("selector", base_selector);
  if (selector.find('&') != std::string::npos) {
    selector = detail::replace_all(std::move(selector), "&", base_selector);
  }

  if (token_ir.contains("when")) {
    for (const auto& condition : token_ir.at("when")) {
      const std::string type = condition.value("type", std::string());
      if (type == "state") {
        selector +=
            detail::pseudo_for_state(condition.value("name", std::string()));
      } else if (type == "attr") {
        const std::string name = condition.value("name", std::string());
        if (name.empty()) {
          continue;
        }
        if (condition.contains("equals") || condition.contains("value")) {
          const auto& expected = condition.contains("equals")
                                     ? condition.at("equals")
                                     : condition.at("value");
          selector += "[" + name + "=\"" +
                      detail::quote_json_value(expected) + "\"]";
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

void emit_rule(std::ostringstream& out, const nlohmann::json& tokens,
               const std::string& class_token,
               const std::string& utility_token,
               std::unordered_set<std::string>& visiting) {
  const auto token_it = tokens.find(utility_token);
  if (token_it == tokens.end() || !token_it->is_object()) {
    return;
  }

  const auto& token_ir = *token_it;
  if (token_ir.value("kind", std::string()) == "macro") {
    if (!token_ir.contains("expand") || !token_ir.at("expand").is_array() ||
        !visiting.insert(utility_token).second) {
      return;
    }
    for (const auto& expanded : token_ir.at("expand")) {
      if (expanded.is_string()) {
        emit_rule(out, tokens, class_token, expanded.get<std::string>(),
                  visiting);
      }
    }
    visiting.erase(utility_token);
    return;
  }

  if (!token_ir.contains("decls") || !token_ir.at("decls").is_object()) {
    return;
  }

  const bool has_media = token_ir.contains("media") &&
                         token_ir.at("media").is_string() &&
                         !token_ir.at("media").get<std::string>().empty();
  if (has_media) {
    out << "@media " << token_ir.at("media").get<std::string>() << " {\n";
  }
  const std::string indent = has_media ? "  " : "";
  out << indent << utility_selector(class_token, token_ir) << " {\n";
  for (auto it = token_ir.at("decls").begin();
       it != token_ir.at("decls").end(); ++it) {
    out << indent << "  " << it.key() << ": "
        << it.value().get<std::string>() << ";\n";
  }
  out << indent << "}\n";
  if (has_media) {
    out << "}\n";
  }
}

const nlohmann::json& catalog_tokens(const nlohmann::json& utility_catalog) {
  if (!utility_catalog.contains("tokens") ||
      !utility_catalog.at("tokens").is_object()) {
    throw std::runtime_error(
        "utility catalog must contain an object named 'tokens'");
  }
  return utility_catalog.at("tokens");
}

}  // namespace

std::string emit_css(const nlohmann::json& utility_catalog,
                     const std::vector<std::string>& class_tokens) {
  std::ostringstream out;
  const auto& tokens = catalog_tokens(utility_catalog);
  std::unordered_set<std::string> emitted;
  for (const auto& class_token : class_tokens) {
    if (emitted.insert(class_token).second) {
      std::unordered_set<std::string> visiting;
      emit_rule(out, tokens, class_token, class_token, visiting);
    }
  }
  return out.str();
}

std::vector<std::string> find_missing_tokens(
    const nlohmann::json& utility_catalog,
    const std::vector<std::string>& class_tokens) {
  const auto& tokens = catalog_tokens(utility_catalog);
  std::vector<std::string> missing;
  std::unordered_set<std::string> seen;
  for (const auto& class_token : class_tokens) {
    if (seen.insert(class_token).second &&
        tokens.find(class_token) == tokens.end()) {
      missing.push_back(class_token);
    }
  }
  return missing;
}

}  // namespace flexUI::tailwind
