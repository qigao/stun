#pragma once

#include <cmeta/cmeta.h>

#include <array>

inline std::array<cmeta_type_desc, 3> incompatible_native_types(
    const cmeta_type_desc& canonical) {
    std::array<cmeta_type_desc, 3> types{canonical, canonical, canonical};
    ++types[0].size;
    ++types[1].align;
    types[2].kind = canonical.kind == CMETA_T_FLOAT
                        ? CMETA_T_OBJECT : CMETA_T_FLOAT;
    return types;
}

const cmeta_type_desc* flex_test_peer_color_type() noexcept;
