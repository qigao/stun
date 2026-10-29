#pragma once

#include <flexUI/computed_style.h>
#include <flexUI/style_property_id.h>

#include <cmeta/cmeta.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace flexUI {
class Element;
}

namespace flexUI::detail {

enum StylePropertyFlags : std::uint32_t {
    STYLE_PROPERTY_NONE = 0u,
    STYLE_PROPERTY_INHERITED = 1u << 0,
    STYLE_PROPERTY_ANIMATABLE = 1u << 1,
    STYLE_PROPERTY_TRANSITIONABLE = 1u << 2,
    STYLE_PROPERTY_DISCRETE = 1u << 3,
};

enum StylePropertyImpact : std::uint32_t {
    STYLE_IMPACT_NONE = 0u,
    STYLE_IMPACT_LAYOUT = 1u << 0,
    STYLE_IMPACT_TEXT_LAYOUT = 1u << 1,
    STYLE_IMPACT_PAINT = 1u << 2,
    STYLE_IMPACT_HIT_TEST = 1u << 3,
    STYLE_IMPACT_COMPOSITE = 1u << 4,
};

using StylePropertyReadFn = bool (*)(const ComputedStyle&, void*);
using StylePropertyWriteFn = bool (*)(ComputedStyle&, const void*);

struct StylePropertyDesc {
    StylePropertyId id = StylePropertyId::Unknown;
    const char* css_name = nullptr;
    const cmeta_type_desc* type = nullptr;
    std::uint32_t flags = STYLE_PROPERTY_NONE;
    std::uint32_t impact = STYLE_IMPACT_NONE;
    const char* transition_group = nullptr;
    StylePropertyReadFn read = nullptr;
    StylePropertyWriteFn write = nullptr;
};

std::size_t style_property_count() noexcept;
const StylePropertyDesc* style_property_at(std::size_t index) noexcept;
const StylePropertyDesc* style_property_descriptor(StylePropertyId id) noexcept;
const StylePropertyDesc* style_property_find(std::string_view name) noexcept;

// Native style values require matching storage as well as semantic identity.
bool style_value_type_matches(const cmeta_type_desc* expected,
                              const cmeta_type_desc* supplied) noexcept;

bool style_property_accepts_type(const StylePropertyDesc& property,
                                 const cmeta_type_desc* type) noexcept;

bool style_property_read(const StylePropertyDesc& property,
                         const ComputedStyle& style,
                         const cmeta_type_desc* expected_type,
                         void* out_value) noexcept;

bool style_property_write(const StylePropertyDesc& property,
                          ComputedStyle& style,
                          const cmeta_type_desc* supplied_type,
                          const void* value) noexcept;

/**
 * Apply FlexUI-owned invalidation semantics for a union of StylePropertyImpact
 * flags. CMeta remains the semantic type source; dirty behavior stays in
 * FlexUI.
 */
void mark_style_impact(Element& element, std::uint32_t impact) noexcept;

inline void mark_style_property_dirty(
    Element& element, const StylePropertyDesc& property) noexcept {
    mark_style_impact(element, property.impact);
}

} // namespace flexUI::detail
