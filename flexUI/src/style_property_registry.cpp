#include <flexUI/detail/style_property_registry.h>

#include <flex/core/cmeta_types.h>
#include <flexUI/element.h>

#include <array>
#include <string_view>

namespace flexUI::detail {
namespace {

template <typename T, T ComputedStyle::*Member>
bool read_member(const ComputedStyle& style, void* out) {
    if (!out) return false;
    *static_cast<T*>(out) = style.*Member;
    return true;
}

template <typename T, T ComputedStyle::*Member>
bool write_member(ComputedStyle& style, const void* value) {
    if (!value) return false;
    style.*Member = *static_cast<const T*>(value);
    return true;
}

bool read_shadow_offset_x(const ComputedStyle& style, void* out) {
    if (!out) return false;
    *static_cast<float*>(out) = style.shadow.offset_x;
    return true;
}
bool write_shadow_offset_x(ComputedStyle& style, const void* value) {
    if (!value) return false;
    style.shadow.offset_x = *static_cast<const float*>(value);
    return true;
}
bool read_shadow_offset_y(const ComputedStyle& style, void* out) {
    if (!out) return false;
    *static_cast<float*>(out) = style.shadow.offset_y;
    return true;
}
bool write_shadow_offset_y(ComputedStyle& style, const void* value) {
    if (!value) return false;
    style.shadow.offset_y = *static_cast<const float*>(value);
    return true;
}
bool read_shadow_blur(const ComputedStyle& style, void* out) {
    if (!out) return false;
    *static_cast<float*>(out) = style.shadow.blur_radius;
    return true;
}
bool write_shadow_blur(ComputedStyle& style, const void* value) {
    if (!value) return false;
    style.shadow.blur_radius = *static_cast<const float*>(value);
    return true;
}
bool read_shadow_spread(const ComputedStyle& style, void* out) {
    if (!out) return false;
    *static_cast<float*>(out) = style.shadow.spread_radius;
    return true;
}
bool write_shadow_spread(ComputedStyle& style, const void* value) {
    if (!value) return false;
    style.shadow.spread_radius = *static_cast<const float*>(value);
    return true;
}
bool read_shadow_color(const ComputedStyle& style, void* out) {
    if (!out) return false;
    *static_cast<Color*>(out) = style.shadow.color;
    return true;
}
bool write_shadow_color(ComputedStyle& style, const void* value) {
    if (!value) return false;
    style.shadow.color = *static_cast<const Color*>(value);
    return true;
}

constexpr std::uint32_t kTransition =
    STYLE_PROPERTY_ANIMATABLE | STYLE_PROPERTY_TRANSITIONABLE;
constexpr std::uint32_t kAnimOnly = STYLE_PROPERTY_ANIMATABLE;
constexpr std::uint32_t kPaint = STYLE_IMPACT_PAINT;
constexpr std::uint32_t kTransformImpact =
    STYLE_IMPACT_PAINT | STYLE_IMPACT_HIT_TEST;
constexpr std::uint32_t kOpacityImpact =
    STYLE_IMPACT_PAINT | STYLE_IMPACT_COMPOSITE;
constexpr std::uint32_t kVisibilityImpact =
    STYLE_IMPACT_PAINT | STYLE_IMPACT_HIT_TEST;

const cmeta_type_identity kVisibilityIdentity =
    CMETA_TYPE_ID_ATOM_INIT("flexUI.Visibility");
const cmeta_type_desc kVisibilityType = {
    "flexUI::Visibility", sizeof(Visibility), alignof(Visibility),
    CMETA_T_INTEGER, nullptr, nullptr, &kVisibilityIdentity};
constexpr std::uint32_t kFontImpact =
    STYLE_IMPACT_TEXT_LAYOUT | STYLE_IMPACT_LAYOUT | STYLE_IMPACT_PAINT;

const std::array<StylePropertyDesc, 24> kProperties = {{
    {StylePropertyId::Opacity, "opacity", &cmeta_type_float, kTransition,
     kOpacityImpact, nullptr,
     read_member<float, &ComputedStyle::opacity>,
     write_member<float, &ComputedStyle::opacity>},
    {StylePropertyId::Visibility, "visibility", &kVisibilityType,
     STYLE_PROPERTY_INHERITED | STYLE_PROPERTY_DISCRETE,
     kVisibilityImpact, nullptr,
     read_member<Visibility, &ComputedStyle::visibility>,
     write_member<Visibility, &ComputedStyle::visibility>},
    {StylePropertyId::BackgroundColor, "background-color", &flex::cmeta_type_color,
     kTransition, kPaint, nullptr,
     read_member<Color, &ComputedStyle::background_color>,
     write_member<Color, &ComputedStyle::background_color>},
    {StylePropertyId::TextColor, "color", &flex::cmeta_type_color,
     kAnim | STYLE_PROPERTY_INHERITED, kPaint, nullptr,
     read_member<Color, &ComputedStyle::text_color>,
     write_member<Color, &ComputedStyle::text_color>},
    {StylePropertyId::BorderColor, "border-color", &flex::cmeta_type_color,
     kTransition, kPaint, nullptr,
     read_member<Color, &ComputedStyle::border_color>,
     write_member<Color, &ComputedStyle::border_color>},
    {StylePropertyId::FontSize, "font-size", &cmeta_type_float,
     kAnim | STYLE_PROPERTY_INHERITED, kFontImpact, nullptr,
     read_member<float, &ComputedStyle::font_size>,
     write_member<float, &ComputedStyle::font_size>},
    {StylePropertyId::TransformX, "transform-x", &cmeta_type_float, kTransition,
     kTransformImpact, "transform",
     read_member<float, &ComputedStyle::transform_x>,
     write_member<float, &ComputedStyle::transform_x>},
    {StylePropertyId::TransformY, "transform-y", &cmeta_type_float, kTransition,
     kTransformImpact, "transform",
     read_member<float, &ComputedStyle::transform_y>,
     write_member<float, &ComputedStyle::transform_y>},
    {StylePropertyId::TransformScale, "transform-scale", &cmeta_type_float, kTransition,
     kTransformImpact, "transform",
     read_member<float, &ComputedStyle::transform_scale>,
     write_member<float, &ComputedStyle::transform_scale>},
    {StylePropertyId::TransformScaleX, "transform-scale-x", &cmeta_type_float, kTransition,
     kTransformImpact, "transform",
     read_member<float, &ComputedStyle::transform_scale_x>,
     write_member<float, &ComputedStyle::transform_scale_x>},
    {StylePropertyId::TransformScaleY, "transform-scale-y", &cmeta_type_float, kTransition,
     kTransformImpact, "transform",
     read_member<float, &ComputedStyle::transform_scale_y>,
     write_member<float, &ComputedStyle::transform_scale_y>},
    {StylePropertyId::TransformRotate, "transform-rotate", &cmeta_type_float, kTransition,
     kTransformImpact, "transform",
     read_member<float, &ComputedStyle::transform_rotate>,
     write_member<float, &ComputedStyle::transform_rotate>},
    {StylePropertyId::OutlineWidth, "outline-width", &cmeta_type_float, kTransition,
     kPaint, nullptr,
     read_member<float, &ComputedStyle::outline_width>,
     write_member<float, &ComputedStyle::outline_width>},
    {StylePropertyId::OutlineOffset, "outline-offset", &cmeta_type_float, kTransition,
     kPaint, nullptr,
     read_member<float, &ComputedStyle::outline_offset>,
     write_member<float, &ComputedStyle::outline_offset>},
    {StylePropertyId::OutlineColor, "outline-color", &flex::cmeta_type_color, kTransition,
     kPaint, nullptr,
     read_member<Color, &ComputedStyle::outline_color>,
     write_member<Color, &ComputedStyle::outline_color>},
    {StylePropertyId::RingWidth, "ring-width", &cmeta_type_float, kTransition,
     kPaint, nullptr,
     read_member<float, &ComputedStyle::ring_width>,
     write_member<float, &ComputedStyle::ring_width>},
    {StylePropertyId::RingOffset, "ring-offset", &cmeta_type_float, kTransition,
     kPaint, nullptr,
     read_member<float, &ComputedStyle::ring_offset>,
     write_member<float, &ComputedStyle::ring_offset>},
    {StylePropertyId::RingColor, "ring-color", &flex::cmeta_type_color, kTransition,
     kPaint, nullptr,
     read_member<Color, &ComputedStyle::ring_color>,
     write_member<Color, &ComputedStyle::ring_color>},
    {StylePropertyId::RingOffsetColor, "ring-offset-color", &flex::cmeta_type_color,
     kTransition, kPaint, nullptr,
     read_member<Color, &ComputedStyle::ring_offset_color>,
     write_member<Color, &ComputedStyle::ring_offset_color>},
    {StylePropertyId::BoxShadowOffsetX, "box-shadow-offset-x", &cmeta_type_float,
     kTransition, kPaint, "box-shadow", read_shadow_offset_x, write_shadow_offset_x},
    {StylePropertyId::BoxShadowOffsetY, "box-shadow-offset-y", &cmeta_type_float,
     kTransition, kPaint, "box-shadow", read_shadow_offset_y, write_shadow_offset_y},
    {StylePropertyId::BoxShadowBlur, "box-shadow-blur", &cmeta_type_float,
     kTransition, kPaint, "box-shadow", read_shadow_blur, write_shadow_blur},
    {StylePropertyId::BoxShadowSpread, "box-shadow-spread", &cmeta_type_float,
     kTransition, kPaint, "box-shadow", read_shadow_spread, write_shadow_spread},
    {StylePropertyId::BoxShadowColor, "box-shadow-color", &flex::cmeta_type_color,
     kTransition, kPaint, "box-shadow", read_shadow_color, write_shadow_color},
}};

struct PropertyName {
    std::string_view name;
    StylePropertyId id;
};

constexpr std::array<PropertyName, 28> kNames = {{
    {"background-color", StylePropertyId::BackgroundColor},
    {"border-color", StylePropertyId::BorderColor},
    {"box-shadow-blur", StylePropertyId::BoxShadowBlur},
    {"box-shadow-color", StylePropertyId::BoxShadowColor},
    {"box-shadow-offset-x", StylePropertyId::BoxShadowOffsetX},
    {"box-shadow-offset-y", StylePropertyId::BoxShadowOffsetY},
    {"box-shadow-spread", StylePropertyId::BoxShadowSpread},
    {"color", StylePropertyId::TextColor},
    {"font-size", StylePropertyId::FontSize},
    {"fontSize", StylePropertyId::FontSize},
    {"opacity", StylePropertyId::Opacity},
    {"outline-color", StylePropertyId::OutlineColor},
    {"outline-offset", StylePropertyId::OutlineOffset},
    {"outline-width", StylePropertyId::OutlineWidth},
    {"ring-color", StylePropertyId::RingColor},
    {"ring-offset", StylePropertyId::RingOffset},
    {"ring-offset-color", StylePropertyId::RingOffsetColor},
    {"ring-width", StylePropertyId::RingWidth},
    {"text-color", StylePropertyId::TextColor},
    {"transform-rotate", StylePropertyId::TransformRotate},
    {"transform-scale", StylePropertyId::TransformScale},
    {"transform-scale-x", StylePropertyId::TransformScaleX},
    {"transform-scale-y", StylePropertyId::TransformScaleY},
    {"transform-x", StylePropertyId::TransformX},
    {"transform-y", StylePropertyId::TransformY},
    {"transformX", StylePropertyId::TransformX},
    {"transformY", StylePropertyId::TransformY},
    {"visibility", StylePropertyId::Visibility},
}};

} // namespace

std::size_t style_property_count() noexcept {
    return kProperties.size();
}

const StylePropertyDesc* style_property_at(std::size_t index) noexcept {
    return index < kProperties.size() ? &kProperties[index] : nullptr;
}

const StylePropertyDesc* style_property_descriptor(StylePropertyId id) noexcept {
    if (id == StylePropertyId::Unknown) return nullptr;
    for (const auto& property : kProperties) {
        if (property.id == id) return &property;
    }
    return nullptr;
}

const StylePropertyDesc* style_property_find(std::string_view name) noexcept {
    for (const auto& entry : kNames) {
        if (entry.name == name) return style_property_descriptor(entry.id);
    }
    return nullptr;
}

bool style_property_read(const StylePropertyDesc& property,
                         const ComputedStyle& style,
                         const cmeta_type_desc* expected_type,
                         void* out_value) noexcept {
    return property.read && property.type && expected_type && out_value &&
           cmeta_type_equal(property.type, expected_type) &&
           property.read(style, out_value);
}

bool style_property_read_transition(const StylePropertyDesc& property,
                                    const Element& element,
                                    const ComputedStyle& style,
                                    const cmeta_type_desc* expected_type,
                                    void* out_value) noexcept {
    if (!property.type || !expected_type || !out_value ||
        !cmeta_type_equal(property.type, expected_type)) {
        return false;
    }

    if (property.id == StylePropertyId::BackgroundColor) {
        if (!cmeta_type_equal(property.type, &flex::cmeta_type_color)) {
            return false;
        }
        *static_cast<Color*>(out_value) =
            element.is_widget_owned()
                ? style.background_color
                : style.get_variable_color("--bg", style.background_color);
        return true;
    }

    return style_property_read(property, style, expected_type, out_value);
}

bool style_property_matches_transition(
    const StylePropertyDesc& property,
    std::string_view transition_name) noexcept {
    if ((property.flags & STYLE_PROPERTY_TRANSITIONABLE) == 0u ||
        transition_name.empty()) {
        return false;
    }

    if (transition_name == "all") {
        return true;
    }

    if (property.transition_group && property.transition_group[0] != '\0') {
        return transition_name == property.transition_group;
    }

    return property.css_name && transition_name == property.css_name;
}

bool style_property_write(const StylePropertyDesc& property,
                          ComputedStyle& style,
                          const cmeta_type_desc* supplied_type,
                          const void* value) noexcept {
    return property.write && property.type && supplied_type && value &&
           cmeta_type_equal(property.type, supplied_type) &&
           property.write(style, value);
}

void mark_style_impact(Element& element, std::uint32_t impact) noexcept {
    const bool text_layout =
        (impact & STYLE_IMPACT_TEXT_LAYOUT) != 0u;
    const bool layout =
        text_layout || (impact & STYLE_IMPACT_LAYOUT) != 0u;
    const bool hit_test =
        (impact & STYLE_IMPACT_HIT_TEST) != 0u;
    const bool visual =
        (impact & (STYLE_IMPACT_PAINT | STYLE_IMPACT_COMPOSITE)) != 0u;

    // Text measurement changes invalidate content-derived bounds in addition
    // to layout. mark_layout_dirty() owns the Box layout + paint notification.
    if (text_layout) {
        element.mark_dirty(flex::DirtyFlags::Content |
                           flex::DirtyFlags::Bounds);
    }

    if (layout) {
        element.mark_layout_dirty();
    }

    // Hit-test changes need fresh bounds/world-bounds even when they do not
    // participate in layout (for example transform-family properties).
    if (hit_test) {
        element.mark_dirty(flex::DirtyFlags::Bounds);
    }

    // Keep Visual explicit. mark_layout_dirty() notifies Box painting but does
    // not itself set the Element Visual bit.
    if (visual || hit_test) {
        element.mark_paint_dirty();
    }
}

} // namespace flexUI::detail
