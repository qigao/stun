#pragma once

#include <flexUI/detail/style_property_registry.h>

#include <array>
#include <cstddef>
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

struct CompiledPropertyWrite {
    const StylePropertyDesc* property = nullptr;
    CompiledCssLiteral value{};
};

struct CompiledPropertyWriteList {
    std::array<CompiledPropertyWrite, 3> writes{};
    std::size_t count = 0;

    bool empty() const noexcept { return count == 0; }
    std::size_t size() const noexcept { return count; }

    const CompiledPropertyWrite& operator[](std::size_t index) const noexcept {
        return writes[index];
    }
};

CompiledCssLiteral compile_css_literal(
    const StylePropertyDesc* property,
    std::string_view raw_value) noexcept;

CompiledPropertyWriteList compile_css_property_writes(
    std::string_view property_name,
    std::string_view raw_value) noexcept;

} // namespace flexUI::detail
