#pragma once

#include "flexUI/plugin_host.h"

#include <turbo_vstr.h>

#include <algorithm>
#include <string_view>

namespace flexUI::plugin_host_detail {

inline bool valid_plugin_identifier(std::string_view value) noexcept {
  if (value.empty() || value.front() < 'a' || value.front() > 'z') {
    return false;
  }
  return std::all_of(value.begin() + 1, value.end(), [](char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9') || character == '.' ||
           character == '_' || character == '-';
  });
}

inline bool valid_utf8_text(std::string_view value) noexcept {
  return value.find('\0') == std::string_view::npos &&
         (value.empty() ||
          vstr_utf8_valid(vstr_from_buf(value.data(), value.size())) != 0);
}

inline bool valid_plugin_host_limits(const PluginHostLimits &limits) noexcept {
  return limits.max_plugins != 0 && limits.max_in_flight_per_plugin != 0 &&
         limits.max_in_flight_per_plugin <=
             PluginHostLimits::kMaximumMaxInFlightPerPlugin &&
         limits.max_plugin_identifier_bytes != 0 &&
         limits.max_plugin_version_bytes != 0 &&
         limits.max_error_message_bytes != 0 &&
         limits.max_error_message_bytes <=
             PluginHostLimits::kDefaultMaxErrorMessageBytes &&
         limits.max_manifest_bytes != 0 &&
         limits.max_manifest_bytes <=
             PluginHostLimits::kMaximumMaxManifestBytes &&
         limits.max_dependencies_per_plugin != 0 &&
         limits.max_library_path_bytes != 0 &&
         limits.registry.max_services != 0 &&
         limits.registry.max_operations_per_service != 0 &&
         limits.registry.max_identifier_bytes != 0 &&
         limits.registry.max_payload_bytes != 0 &&
         limits.completion.max_payload_bytes != 0 &&
         limits.completion.max_error_code_bytes != 0 &&
         limits.completion.max_error_message_bytes != 0 &&
         limits.completion.max_total_string_bytes != 0;
}

} // namespace flexUI::plugin_host_detail
