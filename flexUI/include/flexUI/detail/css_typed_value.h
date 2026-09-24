#pragma once

#include <flexUI/detail/style_property_registry.h>

#include <string_view>
#include <variant>

namespace flexUI::detail {

using CompiledCssStorage = std::variant<std::monostate, float, Color>;

struct CompiledCssLiteral {
    const cmeta_type_desc* type = nullptr;
    CompiledCssStorage value{};

    bool has_value() const noexcept {
        return type != nullptr && value.index() != 0;
    }
};

CompiledCssLiteral compile_css_literal(
    const StylePropertyDesc* property,
    std::string_view raw_value) noexcept;

} // namespace flexUI::detail
