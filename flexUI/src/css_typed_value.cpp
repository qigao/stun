#include <flexUI/detail/css_typed_value.h>
#include <flexUI/detail/css_semantic_value.h>
#include <flexUI/detail/css_length.h>

#include <flex/core/cmeta_types.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>

namespace flexUI::detail {
namespace {

std::string normalize_css_name(std::string_view value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return normalized;
}

std::vector<std::string> split_transform_components(std::string_view raw_value) {
    std::string normalized(raw_value);
    int paren_depth = 0;
    for (char& ch : normalized) {
        if (ch == '(') {
            ++paren_depth;
        } else if (ch == ')' && paren_depth > 0) {
            --paren_depth;
        } else if (ch == ',' && paren_depth == 0) {
            ch = ' ';
        }
    }
    return split_css_tokens(normalized);
}

bool append_float_write(CompiledPropertyWriteList& out,
                        StylePropertyId id,
                        float value) noexcept {
    if (out.count >= out.writes.size()) {
        return false;
    }
    const auto* property = style_property_descriptor(id);
    if (!property ||
        !cmeta_type_equal(property->type, &cmeta_type_float)) {
        return false;
    }
    out.writes[out.count++] = {
        property, {&cmeta_type_float, value}
    };
    return true;
}

} // namespace

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

    if (property->id == StylePropertyId::Visibility) {
        const auto parsed = parse_css_visibility_literal(raw_value);
        if (parsed.is_concrete()) {
            return {property->type, parsed.value};
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

CompiledPropertyWriteList compile_css_property_writes(
    std::string_view property_name,
    std::string_view raw_value) noexcept {
    CompiledPropertyWriteList out;
    const std::string property = normalize_css_name(property_name);
    const std::string value = normalize_css_name(raw_value);
    if (property.empty() || value.empty() || value == "none") {
        return out;
    }

    const auto args = split_transform_components(raw_value);

    if (property == "translate") {
        if (args.empty() || args.size() > 2) {
            return {};
        }
        const auto x =
            parse_css_context_independent_length_literal(args[0]);
        if (!x.is_concrete()) {
            return {};
        }
        CssLiteralResult<float> y{
            CssLiteralState::Concrete, 0.0f};
        if (args.size() == 2) {
            y = parse_css_context_independent_length_literal(args[1]);
            if (!y.is_concrete()) {
                return {};
            }
        }
        if (!append_float_write(out, StylePropertyId::TransformX, x.value) ||
            !append_float_write(out, StylePropertyId::TransformY, y.value)) {
            return {};
        }
        return out;
    }

    if (property == "scale") {
        if (args.empty() || args.size() > 2) {
            return {};
        }
        const auto x = parse_css_number_literal(args[0]);
        if (!x.is_concrete()) {
            return {};
        }
        CssLiteralResult<float> y = x;
        if (args.size() == 2) {
            y = parse_css_number_literal(args[1]);
            if (!y.is_concrete()) {
                return {};
            }
        }
        const float uniform =
            std::abs(x.value - y.value) < 0.0001f ? x.value : 1.0f;
        if (!append_float_write(out, StylePropertyId::TransformScale, uniform) ||
            !append_float_write(out, StylePropertyId::TransformScaleX, x.value) ||
            !append_float_write(out, StylePropertyId::TransformScaleY, y.value)) {
            return {};
        }
        return out;
    }

    if (property == "rotate") {
        if (args.size() != 1) {
            return {};
        }
        const auto angle = parse_css_angle_literal(args[0]);
        if (!angle.is_concrete() ||
            !append_float_write(out, StylePropertyId::TransformRotate,
                                angle.value)) {
            return {};
        }
        return out;
    }

    return out;
}

} // namespace flexUI::detail
