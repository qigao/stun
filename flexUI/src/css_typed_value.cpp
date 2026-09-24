#include <flexUI/detail/css_typed_value.h>
#include <flexUI/detail/css_semantic_value.h>

namespace flexUI::detail {

CompiledCssLiteral compile_css_literal(
    const StylePropertyDesc* property,
    std::string_view raw_value) noexcept {
    if (!property || !property->type) return {};

    if (property->id == StylePropertyId::Opacity &&
        cmeta_type_equal(property->type, &cmeta_type_float)) {
        const auto parsed = parse_css_number_literal(raw_value);
        if (parsed.is_concrete()) {
            return {&cmeta_type_float, parsed.value};
        }
    }

    return {};
}

} // namespace flexUI::detail
