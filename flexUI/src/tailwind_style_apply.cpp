#include <flexUI/detail/tailwind_style_apply.h>

#include <flexUI/detail/style_property_registry.h>
#include <flex/core/cmeta_types.h>

#include <cmath>
#include <new>
#include <type_traits>
#include <utility>

namespace flexUI::detail {
namespace {

bool supported_paint_property(StylePropertyId property) noexcept {
    return property == StylePropertyId::Opacity ||
           property == StylePropertyId::BackgroundColor ||
           property == StylePropertyId::TextColor ||
           property == StylePropertyId::BorderColor;
}

bool normalized_channel(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

bool valid_value(float value) noexcept {
    // The only scalar in this profile is opacity.
    return normalized_channel(value);
}

bool valid_value(const flex::Color& value) noexcept {
    return normalized_channel(value.r) && normalized_channel(value.g) &&
           normalized_channel(value.b) && normalized_channel(value.a);
}

template <typename T>
const cmeta_type_desc* value_type() noexcept {
    if constexpr (std::is_same_v<T, float>) {
        return &cmeta_type_float;
    } else {
        static_assert(std::is_same_v<T, flex::Color>,
                      "Every native value requires its canonical CMeta type");
        return &flex::cmeta_type_color;
    }
}

} // namespace

NativeWriteResult write_native_declaration(
    const tailwind::NativeDeclaration& declaration,
    ComputedStyle& candidate) noexcept {
    using tailwind::NativeError;
    const auto* property = style_property_descriptor(declaration.property);
    if (!property || !supported_paint_property(declaration.property)) {
        return {NativeError::UnsupportedValue, 0};
    }
    if (declaration.value.valueless_by_exception()) {
        return {NativeError::PropertyTypeMismatch, 0};
    }

    return std::visit([&](const auto& value) -> NativeWriteResult {
        using Value = std::decay_t<decltype(value)>;
        const auto* supplied_type = value_type<Value>();
        // Derive the supplied type from storage, never from property->type.
        if (!style_property_accepts_type(*property, supplied_type)) {
            return {NativeError::PropertyTypeMismatch, 0};
        }
        if (!valid_value(value)) {
            return {NativeError::InvalidValue, 0};
        }
        if (!style_property_write(*property, candidate, supplied_type, &value)) {
            return {NativeError::PropertyTypeMismatch, 0};
        }
        return {NativeError::None, property->impact};
    }, declaration.value);
}

} // namespace flexUI::detail

namespace flexUI::tailwind {
namespace {

const char* write_error_message(NativeError error) noexcept {
    switch (error) {
    case NativeError::UnsupportedValue:
        return "Declaration is outside the native paint profile";
    case NativeError::PropertyTypeMismatch:
        return "Declaration storage does not match the registered property type";
    case NativeError::InvalidValue:
        return "Declaration value must be finite and within its supported range";
    default:
        return "Native declaration could not be applied";
    }
}

} // namespace

NativeStyleResult apply_native_style(
    const NativeProgram& program,
    const ComputedStyle& base,
    const NativeState& state) {
    try {
        ComputedStyle candidate = base;
        std::uint32_t impact = 0;
        for (const auto& rule : program.rules()) {
            if (!rule.condition.matches(state)) continue;
            const auto result = detail::write_native_declaration(
                rule.declaration, candidate);
            if (!result) {
                return {std::nullopt, 0,
                        {result.error, rule.source,
                         write_error_message(result.error)}};
            }
            impact |= result.impact;
        }
        return {std::move(candidate), impact, {}};
    } catch (const std::bad_alloc&) {
        // Reporting allocation failure must not allocate another message.
        return {std::nullopt, 0, {NativeError::AllocationFailure, {}, {}}};
    }
}

} // namespace flexUI::tailwind
