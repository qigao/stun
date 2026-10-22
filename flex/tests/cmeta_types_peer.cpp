#include "flex/core/cmeta_types.h"

namespace {

// Deliberately TU-local: descriptor addresses are not cross-TU type identity.
const cmeta_type_identity color_identity = CMETA_TYPE_ID_ATOM_INIT("flex.Color");
const cmeta_type_identity vec2_identity = CMETA_TYPE_ID_ATOM_INIT("flex.Vec2");
const cmeta_type_identity string_identity = CMETA_TYPE_ID_ATOM_INIT("flex.String");

const cmeta_type_desc color_type = {
    "peer Color", sizeof(flex::Color), alignof(flex::Color), CMETA_T_OBJECT,
    nullptr, nullptr, &color_identity};
const cmeta_type_desc vec2_type = {
    "peer Vec2", sizeof(flex::Vec2), alignof(flex::Vec2), CMETA_T_OBJECT,
    nullptr, nullptr, &vec2_identity};
const cmeta_type_desc string_type = {
    "peer String", sizeof(std::string), alignof(std::string), CMETA_T_OBJECT,
    nullptr, nullptr, &string_identity};

} // namespace

const cmeta_type_desc* flex_test_peer_color_type() noexcept { return &color_type; }
const cmeta_type_desc* flex_test_peer_vec2_type() noexcept { return &vec2_type; }
const cmeta_type_desc* flex_test_peer_string_type() noexcept { return &string_type; }
