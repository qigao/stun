#pragma once

#include <flexUI/types.h>

#include <cstdint>
#include <string_view>

namespace flexUI::detail {

enum class CssLiteralState : std::uint8_t {
    Invalid,
    Concrete,
    Deferred,
};

template <typename T>
struct CssLiteralResult {
    CssLiteralState state = CssLiteralState::Invalid;
    T value{};

    bool is_concrete() const noexcept {
        return state == CssLiteralState::Concrete;
    }

    bool is_deferred() const noexcept {
        return state == CssLiteralState::Deferred;
    }
};

CssLiteralResult<float> parse_css_number_literal(
    std::string_view raw_value) noexcept;

CssLiteralResult<Color> parse_css_color_literal(
    std::string_view raw_value) noexcept;

CssLiteralResult<Visibility> parse_css_visibility_literal(
    std::string_view raw_value) noexcept;

CssLiteralResult<float> parse_css_angle_literal(
    std::string_view raw_value) noexcept;

CssLiteralResult<float> parse_css_context_independent_length_literal(
    std::string_view raw_value) noexcept;

CssLiteralResult<float> parse_css_border_width_literal(
    std::string_view raw_value) noexcept;

} // namespace flexUI::detail
