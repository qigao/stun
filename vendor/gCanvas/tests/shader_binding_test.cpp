#include "gcanvas/shader_binding.hpp"

#include <cstddef>
#include <cstdint>

namespace
{
    struct MaterialParams
    {
        float time;
        float strength;
    };

    const cmeta_type_identity material_identity =
        CMETA_TYPE_ID_ATOM_INIT("test.gcanvas.MaterialParams");
    const cmeta_type_desc material_type = {
        "MaterialParams", sizeof(MaterialParams), alignof(MaterialParams),
        CMETA_T_OBJECT, nullptr, nullptr, &material_identity};

    const cmeta_field_desc material_fields[] = {
        {"time", "float", offsetof(MaterialParams, time), sizeof(float),
         alignof(float), &cmeta_type_float, nullptr},
        {"strength", "float", offsetof(MaterialParams, strength), sizeof(float),
         alignof(float), &cmeta_type_float, nullptr},
    };
    const cmeta_struct_desc material_struct = {
        "MaterialParams", sizeof(MaterialParams), alignof(MaterialParams),
        material_fields, 2u};

    const cmeta_type_identity image_identity =
        CMETA_TYPE_ID_ATOM_INIT("test.gcanvas.ImageResource");
    const cmeta_type_desc image_type = {
        "ImageResource", sizeof(void*), alignof(void*), CMETA_T_OBJECT,
        nullptr, nullptr, &image_identity};

    gcanvas::ShaderBindingLayout valid_layout()
    {
        static const gcanvas::ShaderFieldBindingDesc gpu_fields[] = {
            {&material_fields[0], 0u, 0u, 1u},
            {&material_fields[1], 16u, 0u, 1u},
        };
        static const gcanvas::ShaderBindingDesc bindings[] = {
            {
                "material",
                gcanvas::shader_stage_fragment,
                gcanvas::ShaderResourceKind::UniformBlock,
                1u, 0u, gcanvas::shader_binding_unset, 1u,
                32u,
                &material_type,
                &material_struct,
                gpu_fields,
                2u,
            },
            {
                "source",
                gcanvas::shader_stage_fragment,
                gcanvas::ShaderResourceKind::SampledImage,
                2u, 0u, gcanvas::shader_binding_unset, 1u,
                0u,
                &image_type,
                nullptr,
                nullptr,
                0u,
            },
        };
        return {gcanvas::shader_binding_abi_version, bindings, 2u};
    }
}

int main()
{
    const gcanvas::ShaderBindingLayout layout = valid_layout();
    if (!gcanvas::shader_binding_layout_valid(layout))
        return 1;

    const std::uint64_t first_hash =
        gcanvas::shader_binding_layout_hash(layout);
    if (first_hash == 0u)
        return 2;

    static const cmeta_type_identity peer_material_identity =
        CMETA_TYPE_ID_ATOM_INIT("test.gcanvas.MaterialParams");
    static const cmeta_type_desc peer_material_type = {
        "PeerMaterialParams", sizeof(MaterialParams), alignof(MaterialParams),
        CMETA_T_OBJECT, nullptr, nullptr, &peer_material_identity};
    static const cmeta_field_desc peer_fields[] = {
        {"time", "float", offsetof(MaterialParams, time), sizeof(float),
         alignof(float), &cmeta_type_float, nullptr},
        {"strength", "float", offsetof(MaterialParams, strength), sizeof(float),
         alignof(float), &cmeta_type_float, nullptr},
    };
    static const cmeta_struct_desc peer_struct = {
        "MaterialParams", sizeof(MaterialParams), alignof(MaterialParams),
        peer_fields, 2u};
    static const gcanvas::ShaderFieldBindingDesc peer_gpu_fields[] = {
        {&peer_fields[0], 0u, 0u, 1u},
        {&peer_fields[1], 16u, 0u, 1u},
    };
    static const gcanvas::ShaderBindingDesc peer_bindings[] = {
        {
            "material",
            gcanvas::shader_stage_fragment,
            gcanvas::ShaderResourceKind::UniformBlock,
            1u, 0u, gcanvas::shader_binding_unset, 1u,
            32u,
            &peer_material_type,
            &peer_struct,
            peer_gpu_fields,
            2u,
        },
        valid_layout().bindings[1],
    };
    const gcanvas::ShaderBindingLayout peer_layout{
        gcanvas::shader_binding_abi_version, peer_bindings, 2u};

    if (!cmeta_type_equal(&material_type, &peer_material_type))
        return 3;
    if (gcanvas::shader_binding_layout_hash(peer_layout) != first_hash)
        return 4;

    auto duplicate_bindings = *layout.bindings;
    gcanvas::ShaderBindingDesc duplicated[2] = {
        layout.bindings[0], duplicate_bindings};
    duplicated[1].name = "duplicate";
    const gcanvas::ShaderBindingLayout duplicate_layout{
        gcanvas::shader_binding_abi_version, duplicated, 2u};
    const auto duplicate_result =
        gcanvas::validate_shader_binding_layout(duplicate_layout);
    if (duplicate_result.error !=
        gcanvas::ShaderBindingError::DuplicateResourceBinding)
        return 5;

    gcanvas::ShaderBindingDesc unstable = layout.bindings[1];
    static const cmeta_type_desc no_identity_type = {
        "NoIdentity", sizeof(void*), alignof(void*), CMETA_T_OBJECT,
        nullptr, nullptr, nullptr};
    unstable.type = &no_identity_type;
    const gcanvas::ShaderBindingLayout unstable_layout{
        gcanvas::shader_binding_abi_version, &unstable, 1u};
    if (gcanvas::shader_binding_layout_valid(unstable_layout))
        return 6;

    return 0;
}
