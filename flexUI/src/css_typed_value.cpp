#include <flexUI/detail/css_typed_value.h>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string>

namespace flexUI::detail {
namespace {

std::string trim_copy(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

bool is_deferred_css_value(std::string_view value) {
    return value.find("var(") != std::string_view::npos ||
           value.find("calc(") != std::string_view::npos ||
           value.find("env(") != std::string_view::npos ||
           value.find("currentColor") != std::string_view::npos ||
           value.find("currentcolor") != std::string_view::npos;
}

bool parse_strict_float(std::string_view raw, float& out) {
    const std::string value = trim_copy(raw);
    if (value.empty() || is_deferred_css_value(value)) return false;

    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end == value.c_str() || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }
    while (end && *end != '\0' &&
           (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        ++end;
    }
    if (end && *end != '\0') return false;

    out = parsed;
    return true;
}

} // namespace

CompiledCssLiteral compile_css_literal(
    const StylePropertyDesc* property,
    std::string_view raw_value) noexcept {
    if (!property || !property->type) return {};

    if (property->id == StylePropertyId::Opacity &&
        cmeta_type_equal(property->type, &cmeta_type_float)) {
        float value = 0.0f;
        if (parse_strict_float(raw_value, value)) {
            return {&cmeta_type_float, value};
        }
    }

    return {};
}

} // namespace flexUI::detail
