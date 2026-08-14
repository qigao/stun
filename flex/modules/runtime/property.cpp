#include "property_internal.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <type_traits>

namespace flex {
namespace {

using detail::AnimatedPropertyDescriptor;
using detail::AnimatedPropertyValueKind;
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
    {PropertyID::Unknown, AnimatedPropertyValueKind::Scalar, 0},
    {PropertyID::X, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::Y, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::Rotation, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::Scale, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::ScaleX, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::ScaleY, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::Width, AnimatedPropertyValueKind::Scalar, kShapeTarget},
    {PropertyID::Height, AnimatedPropertyValueKind::Scalar, kShapeTarget},
    {PropertyID::Radius, AnimatedPropertyValueKind::Scalar, kShapeTarget},
    {PropertyID::Opacity, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::Visible, AnimatedPropertyValueKind::Scalar, kAllTargets},
    {PropertyID::Fill, AnimatedPropertyValueKind::Color, kShapeTarget},
    {PropertyID::FillOpacity, AnimatedPropertyValueKind::Scalar, kShapeTarget},
    {PropertyID::Stroke, AnimatedPropertyValueKind::Color, kShapeTarget},
    {PropertyID::StrokeWidth, AnimatedPropertyValueKind::Scalar, kShapeTarget},
    {PropertyID::Text, AnimatedPropertyValueKind::String, kTextTarget},
    {PropertyID::Content, AnimatedPropertyValueKind::String, kTextTarget},
    {PropertyID::FontSize, AnimatedPropertyValueKind::Scalar, kTextTarget},
    {PropertyID::TextColor, AnimatedPropertyValueKind::Color, kTextTarget},
    {PropertyID::Color, AnimatedPropertyValueKind::Color,
     static_cast<uint8_t>(kShapeTarget | kTextTarget)},
    {PropertyID::Position, AnimatedPropertyValueKind::Vec2, kAllTargets},
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

bool animated_property_supports(PropertyID id, PropertyTargetKind target) noexcept {
    const auto* descriptor = animated_property_descriptor(id);
    return descriptor && (descriptor->target_mask & target_bit(target)) != 0;
}

} // namespace detail
} // namespace flex
