#pragma once

#include <memory>
#include <string_view>

namespace flexUI::tailwind {
class UtilityCatalog;
}

namespace flexUI::detail {

std::shared_ptr<const tailwind::UtilityCatalog> builtin_utility_catalog();
std::string_view default_theme_css();

}  // namespace flexUI::detail
