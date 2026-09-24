#include <flexUI/detail/css_typed_value.h>
#include <flexUI/detail/css_semantic_value.h>

#include <flex/core/cmeta_types.h>

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

    const bool safe_color_property =
        property->id == StylePropertyId::BackgroundColor ||
        property->id == StylePropertyId::OutlineColor ||
        property->id == StylePropertyId::RingColor ||
        property->id == StylePropertyId::RingOffsetColor;
    if (safe_color_property &&
        cmeta_type_equal(property->type, &flex::cmeta_type_color)) {
        const auto parsed = parse_css_color_literal(raw_value);
        if (parsed.is_concrete()) {
            return {&flex::cmeta_type_color, parsed.value};
        }
    }

    const bool safe_effect_length =
        property->id == StylePropertyId::OutlineWidth ||
        property->id == StylePropertyId::OutlineOffset ||
        property->id == StylePropertyId::RingWidth ||
        property->id == StylePropertyId::RingOffset;
    if (safe_effect_length &&
        cmeta_type_equal(property->type, &cmeta_type_float)) {
        const auto parsed =
            property->id == StylePropertyId::OutlineWidth
                ? parse_css_border_width_literal(raw_value)
                : parse_css_context_independent_length_literal(raw_value);
        if (parsed.is_concrete()) {
            return {&cmeta_type_float, parsed.value};
        }
    }

    return {};
}

} // namespace flexUI::detail
