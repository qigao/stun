#include "../modules/runtime/property_internal.h"
#include "flex/core/cmeta_types.h"

#include <tinytest.hpp>

#include <string>

using namespace flex;

const cmeta_type_desc* flex_test_peer_color_type() noexcept;
const cmeta_type_desc* flex_test_peer_vec2_type() noexcept;
const cmeta_type_desc* flex_test_peer_string_type() noexcept;

suite("Flex animation CMeta property types") {
    it("publishes canonical semantic descriptors") {
        check_true(cmeta_type_desc_valid(&cmeta_type_color));
        check_true(cmeta_type_desc_valid(&cmeta_type_vec2));
        check_true(cmeta_type_desc_valid(&cmeta_type_string));

        check_true(cmeta_type_equal(
            detail::animated_property_descriptor(PropertyID::Opacity)->type,
            &cmeta_type_float));
        check_true(cmeta_type_equal(
            detail::animated_property_descriptor(PropertyID::Fill)->type,
            &cmeta_type_color));
        check_true(cmeta_type_equal(
            detail::animated_property_descriptor(PropertyID::Position)->type,
            &cmeta_type_vec2));
        check_true(cmeta_type_equal(
            detail::animated_property_descriptor(PropertyID::Text)->type,
            &cmeta_type_string));
    }

    it("matches AnimValue alternatives through CMeta") {
        check_true(detail::animated_property_accepts(
            PropertyID::Opacity, AnimValue{0.5f}));
        check_true(detail::animated_property_accepts(
            PropertyID::Fill, AnimValue{Color{1, 0, 0, 1}}));
        check_true(detail::animated_property_accepts(
            PropertyID::Position, AnimValue{Vec2{2, 3}}));
        check_true(detail::animated_property_accepts(
            PropertyID::Text, AnimValue{std::string{"hello"}}));

        check_false(detail::animated_property_accepts(
            PropertyID::Fill, AnimValue{1.0f}));
        check_false(detail::animated_property_accepts(
            PropertyID::Position, AnimValue{Color{}}));
    }

    it("uses stable identity rather than descriptor address") {
        static const cmeta_type_identity peer_identity =
            CMETA_TYPE_ID_ATOM_INIT("flex.Color");
        static const cmeta_type_desc peer_color = {
            "peer Color", sizeof(Color), alignof(Color), CMETA_T_OBJECT,
            nullptr, nullptr, &peer_identity};

        check(&peer_color != &cmeta_type_color);
        check_true(cmeta_type_equal(&peer_color, &cmeta_type_color));
    }

    it("accepts semantic type identities from a separate translation unit") {
        const auto* color = flex_test_peer_color_type();
        const auto* vec2 = flex_test_peer_vec2_type();
        const auto* string = flex_test_peer_string_type();

        check_not_equal(color, &cmeta_type_color);
        check_not_equal(vec2, &cmeta_type_vec2);
        check_not_equal(string, &cmeta_type_string);
        check_true(cmeta_type_equal(color, &cmeta_type_color));
        check_true(cmeta_type_equal(vec2, &cmeta_type_vec2));
        check_true(cmeta_type_equal(string, &cmeta_type_string));
        check_false(cmeta_type_equal(color, vec2));
    }

    it("keeps semantic identity distinct from native storage layout") {
        auto wrong_shape = *flex_test_peer_color_type();
        wrong_shape.size += sizeof(float);
        check_true(cmeta_type_equal(&wrong_shape, &cmeta_type_color));
        check_not_equal(wrong_shape.size, cmeta_type_color.size);
    }
};
