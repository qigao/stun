#pragma once

#include <nlohmann/json_fwd.hpp>

#include <string>

namespace flexUI::tailwind::detail {

std::string replace_all(std::string text, const std::string& needle,
                        const std::string& replacement);
std::string pseudo_for_state(const std::string& state);
std::string quote_json_value(const nlohmann::json& value);

}  // namespace flexUI::tailwind::detail
