#pragma once

#include "flex/core/types.h"

#include <cmeta/cmeta.h>

#include <string>

namespace flex {

// Canonical semantic identities for Flex animation/style value types.
// These descriptors describe meaning and ABI-visible object shape; runtime
// animation storage remains AnimValue and is intentionally a separate concern.
inline const cmeta_type_identity cmeta_color_identity =
    CMETA_TYPE_ID_ATOM_INIT("flex.Color");
inline const cmeta_type_desc cmeta_type_color = {
    "flex::Color", sizeof(Color), alignof(Color), CMETA_T_OBJECT,
    nullptr, nullptr, &cmeta_color_identity};

inline const cmeta_type_identity cmeta_vec2_identity =
    CMETA_TYPE_ID_ATOM_INIT("flex.Vec2");
inline const cmeta_type_desc cmeta_type_vec2 = {
    "flex::Vec2", sizeof(Vec2), alignof(Vec2), CMETA_T_OBJECT,
    nullptr, nullptr, &cmeta_vec2_identity};

inline const cmeta_type_identity cmeta_string_identity =
    CMETA_TYPE_ID_ATOM_INIT("flex.String");
inline const cmeta_type_desc cmeta_type_string = {
    "std::string", sizeof(std::string), alignof(std::string), CMETA_T_OBJECT,
    nullptr, nullptr, &cmeta_string_identity};

} // namespace flex
