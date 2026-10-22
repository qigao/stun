#include "property_internal.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <type_traits>

namespace flex {
namespace {

using detail::AnimatedPropertyDescriptor;
using detail::PropertyTargetKind;

constexpr uint8_t target_bit(PropertyTargetKind target) {
    return static_cast<uint8_t>(target);
}

constexpr uint8_t kAllTargets =
    target_bit(PropertyTargetKind::Group) |
    target_bit(PropertyTargetKind::Shape) |
    target_bit(PropertyTargetKind::Text) |
    target_bit(PropertyTargetKind::Image) |
    target_bit(PropertyTargetKind::Svg) |
    target_bit(PropertyTargetKind::Instance) |
    target_bit(PropertyTargetKind::Solo);
constexpr uint8_t kShapeTarget = target_bit(PropertyTargetKind::Shape);
constexpr uint8_t kTextTarget = target_bit(PropertyTargetKind::Text);

constexpr std::array<AnimatedPropertyDescriptor, 22> kAnimatedProperties = {{
    {PropertyID::Unknown, nullptr, 0},
    {PropertyID::X, &cmeta_type_float, kAllTargets},
    {PropertyID::Y, &cmeta_type_float, kAllTargets},
    {PropertyID::Rotation, &cmeta_type_float, kAllTargets},
    {PropertyID::Scale, &cmeta_type_float, kAllTargets},
    {PropertyID::ScaleX, &cmeta_type_float, kAllTargets},
    {PropertyID::ScaleY, &cmeta_type_float, kAllTargets},
    {PropertyID::Width, &cmeta_type_float, kShapeTarget},
    {PropertyID::Height, &cmeta_type_float, kShapeTarget},
    {PropertyID::Radius, &cmeta_type_float, kShapeTarget},
    {PropertyID::Opacity, &cmeta_type_float, kAllTargets},
    {PropertyID::Visible, &cmeta_type_float, kAllTargets},
    {PropertyID::Fill, &cmeta_type_color, kShapeTarget},
    {PropertyID::FillOpacity, &cmeta_type_float, kShapeTarget},
    {PropertyID::Stroke, &cmeta_type_color, kShapeTarget},
    {PropertyID::StrokeWidth, &cmeta_type_float, kShapeTarget},
    {PropertyID::Text, &cmeta_type_string, kTextTarget},
    {PropertyID::Content, &cmeta_type_string, kTextTarget},
    {PropertyID::FontSize, &cmeta_type_float, kTextTarget},
    {PropertyID::TextColor, &cmeta_type_color, kTextTarget},
    {PropertyID::Color, &cmeta_type_color,
     static_cast<uint8_t>(kShapeTarget | kTextTarget)},
    {PropertyID::Position, &cmeta_type_vec2, kAllTargets},
}};

struct PropertyName {
    std::string_view name;
    PropertyID id;
};

constexpr std::array<PropertyName, 24> kPropertyNames = {{
    {"color", PropertyID::Color},
    {"content", PropertyID::Content},
    {"fill", PropertyID::Fill},
    {"fill.opacity", PropertyID::FillOpacity},
    {"fontSize", PropertyID::FontSize},
    {"font_size", PropertyID::FontSize},
    {"height", PropertyID::Height},
    {"opacity", PropertyID::Opacity},
    {"position", PropertyID::Position},
    {"radius", PropertyID::Radius},
    {"rotation", PropertyID::Rotation},
    {"scale", PropertyID::Scale},
    {"scaleX", PropertyID::ScaleX},
    {"scaleY", PropertyID::ScaleY},
    {"scale_x", PropertyID::ScaleX},
    {"scale_y", PropertyID::ScaleY},
    {"stroke", PropertyID::Stroke},
    {"stroke.width", PropertyID::StrokeWidth},
    {"text", PropertyID::Text},
    {"text.color", PropertyID::TextColor},
    {"visible", PropertyID::Visible},
    {"width", PropertyID::Width},
    {"x", PropertyID::X},
    {"y", PropertyID::Y},
}};

constexpr bool property_names_are_sorted() {
    for (size_t index = 1; index < kPropertyNames.size(); ++index) {
        if (!(kPropertyNames[index - 1].name < kPropertyNames[index].name)) {
            return false;
        }
    }
    return true;
}

static_assert(property_names_are_sorted(), "Property name table must stay sorted");
static_assert(kAnimatedProperties.size() ==
                  static_cast<size_t>(PropertyID::Position) + 1,
              "Property descriptors must cover every PropertyID");

} // namespace

PropertyID get_property_id(const char* property) {
    if (!property || !property[0]) {
        return PropertyID::Unknown;
    }

    const std::string_view name(property);
    const auto it = std::lower_bound(
        kPropertyNames.begin(), kPropertyNames.end(), name,
        [](const PropertyName& entry, std::string_view candidate) {
            return entry.name < candidate;
        });
    return it != kPropertyNames.end() && it->name == name
               ? it->id
               : PropertyID::Unknown;
}

namespace detail {

const AnimatedPropertyDescriptor* animated_property_descriptor(PropertyID id) noexcept {
    using IdType = std::underlying_type_t<PropertyID>;
    const auto index = static_cast<IdType>(id);
    if (index <= 0 || static_cast<size_t>(index) >= kAnimatedProperties.size()) {
        return nullptr;
    }
    const auto& descriptor = kAnimatedProperties[static_cast<size_t>(index)];
    return descriptor.id == id ? &descriptor : nullptr;
}

const cmeta_type_desc* anim_value_type(const AnimValue& value) noexcept {
    return std::visit(
        [](const auto& current) -> const cmeta_type_desc* {
            using T = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<T, float>) {
                return &cmeta_type_float;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return &cmeta_type_string;
            } else if constexpr (std::is_same_v<T, Color>) {
                return &cmeta_type_color;
            } else if constexpr (std::is_same_v<T, Vec2>) {
                return &cmeta_type_vec2;
            } else {
                return nullptr;
            }
        },
        value);
}

bool animated_property_accepts(PropertyID id, const AnimValue& value) noexcept {
    const auto* descriptor = animated_property_descriptor(id);
    const auto* value_type = anim_value_type(value);
    return descriptor && descriptor->type && value_type &&
           cmeta_type_equal(descriptor->type, value_type);
}

bool animated_property_supports(PropertyID id, PropertyTargetKind target) noexcept {
    const auto* descriptor = animated_property_descriptor(id);
    return descriptor && (descriptor->target_mask & target_bit(target)) != 0;
}

} // namespace detail
} // namespace flex
