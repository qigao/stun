#pragma once

#include <flexUI/detail/style_property_registry.h>

#include <cstddef>
#include <string_view>

namespace flexUI {

using RegisteredStyleProperty = detail::StylePropertyDesc;
using RegisteredStylePropertyId = detail::StylePropertyId;

inline std::size_t registered_style_property_count() noexcept {
  return detail::style_property_count();
}

inline const RegisteredStyleProperty*
registered_style_property_at(std::size_t index) noexcept {
  return detail::style_property_at(index);
}

inline const RegisteredStyleProperty*
find_registered_style_property(std::string_view name) noexcept {
  return detail::style_property_find(name);
}

} // namespace flexUI
