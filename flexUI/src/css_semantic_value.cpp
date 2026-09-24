#include <flexUI/detail/css_semantic_value.h>

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace flexUI::detail {
namespace {

std::string_view trim_ascii(std::string_view value) noexcept {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::string ascii_lower_copy(std::string_view value) {
    std::string result(value);
    for (char& ch : result) {
        ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

bool is_css_wide_keyword(std::string_view value) noexcept {
    return value == "inherit" || value == "initial" || value == "unset" ||
           value == "revert" || value == "revert-layer";
}

bool contains_deferred_function(std::string_view value) noexcept {
    return value.find("var(") != std::string_view::npos ||
           value.find("calc(") != std::string_view::npos ||
           value.find("env(") != std::string_view::npos;
}

bool is_digit(char ch) noexcept {
    return ch >= '0' && ch <= '9';
}

} // namespace

CssLiteralResult<float> parse_css_number_literal(
    std::string_view raw_value) noexcept {
    const std::string_view trimmed = trim_ascii(raw_value);
    if (trimmed.empty()) {
        return {};
    }

    const std::string lowered = ascii_lower_copy(trimmed);
    if (is_css_wide_keyword(lowered) ||
        contains_deferred_function(lowered) ||
        (!lowered.empty() && lowered.back() == '%')) {
        return {CssLiteralState::Deferred, 0.0f};
    }

    // CSS <number> grammar:
    //   [+-]?([0-9]*\.[0-9]+|[0-9]+)([eE][+-]?[0-9]+)?
    // Validate the grammar before strtof so C-only spellings such as
    // hexadecimal floats, infinities and NaNs cannot enter the CSS cache.
    std::size_t i = 0;
    if (trimmed[i] == '+' || trimmed[i] == '-') {
        ++i;
    }
    if (i == trimmed.size()) {
        return {};
    }

    const std::size_t integer_begin = i;
    while (i < trimmed.size() && is_digit(trimmed[i])) {
        ++i;
    }
    const bool has_integer_digits = i > integer_begin;

    bool has_fraction_digits = false;
    if (i < trimmed.size() && trimmed[i] == '.') {
        ++i;
        const std::size_t fraction_begin = i;
        while (i < trimmed.size() && is_digit(trimmed[i])) {
            ++i;
        }
        has_fraction_digits = i > fraction_begin;
        if (!has_fraction_digits) {
            return {};
        }
    }

    if (!has_integer_digits && !has_fraction_digits) {
        return {};
    }

    if (i < trimmed.size() &&
        (trimmed[i] == 'e' || trimmed[i] == 'E')) {
        ++i;
        if (i < trimmed.size() &&
            (trimmed[i] == '+' || trimmed[i] == '-')) {
            ++i;
        }
        const std::size_t exponent_begin = i;
        while (i < trimmed.size() && is_digit(trimmed[i])) {
            ++i;
        }
        if (i == exponent_begin) {
            return {};
        }
    }

    if (i != trimmed.size()) {
        return {};
    }

    const std::string value(trimmed);
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end != value.c_str() + value.size() ||
        errno == ERANGE || !std::isfinite(parsed)) {
        return {};
    }

    return {CssLiteralState::Concrete, parsed};
}

} // namespace flexUI::detail
