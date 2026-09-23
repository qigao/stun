#ifndef GCANVAS_SHADER_BINDING_HPP
#define GCANVAS_SHADER_BINDING_HPP

#include <cmeta/cmeta.h>
#include <cmeta/struct.h>

#include <cstddef>
#include <cstdint>

namespace gcanvas
{
    constexpr std::uint32_t shader_binding_abi_version = 1u;
    constexpr std::uint32_t shader_binding_unset = UINT32_MAX;

    using ShaderStageMask = std::uint32_t;
    constexpr ShaderStageMask shader_stage_vertex = 1u << 0;
    constexpr ShaderStageMask shader_stage_fragment = 1u << 1;
    constexpr ShaderStageMask shader_stage_all_graphics =
        shader_stage_vertex | shader_stage_fragment;

    enum class ShaderResourceKind : std::uint8_t
    {
        UniformBlock,
        SampledImage,
        Sampler,
        CombinedImageSampler,
        StageInput,
        StageOutput
    };

    struct ShaderFieldBindingDesc
    {
        const cmeta_field_desc* field = nullptr;
        std::size_t gpu_offset = 0;
        std::size_t gpu_stride = 0;
        std::uint32_t array_count = 1;
    };

    struct ShaderBindingDesc
    {
        const char* name = nullptr;
        ShaderStageMask stages = 0;
        ShaderResourceKind kind = ShaderResourceKind::UniformBlock;

        std::uint32_t set = shader_binding_unset;
        std::uint32_t binding = shader_binding_unset;
        std::uint32_t location = shader_binding_unset;
        std::uint32_t array_count = 1;

        std::size_t gpu_size = 0;

        const cmeta_type_desc* type = nullptr;
        const cmeta_struct_desc* struct_type = nullptr;
        const ShaderFieldBindingDesc* fields = nullptr;
        std::size_t field_count = 0;
    };

    struct ShaderBindingLayout
    {
        std::uint32_t abi_version = shader_binding_abi_version;
        const ShaderBindingDesc* bindings = nullptr;
        std::size_t binding_count = 0;
    };

    enum class ShaderBindingError : std::uint8_t
    {
        None,
        InvalidAbiVersion,
        MissingBindings,
        InvalidName,
        InvalidStageMask,
        InvalidResourceKind,
        InvalidType,
        MissingStableTypeIdentity,
        InvalidPlacement,
        InvalidStructLayout,
        InvalidField,
        DuplicateResourceBinding,
        DuplicateStageLocation
    };

    struct ShaderBindingValidation
    {
        ShaderBindingError error = ShaderBindingError::None;
        std::size_t binding_index = static_cast<std::size_t>(-1);
        std::size_t field_index = static_cast<std::size_t>(-1);

        explicit operator bool() const noexcept
        {
            return error == ShaderBindingError::None;
        }
    };

    ShaderBindingValidation
    validate_shader_binding_layout(const ShaderBindingLayout& layout) noexcept;

    bool shader_binding_layout_valid(const ShaderBindingLayout& layout) noexcept;

    /**
     * Returns a deterministic semantic/layout hash.
     *
     * The hash is independent of descriptor addresses and is derived from:
     * - the Shader binding ABI version;
     * - CMeta stable semantic type identity;
     * - semantic field names/types;
     * - GPU stage/set/binding/location/offset/stride placement.
     *
     * Invalid layouts return 0.
     */
    std::uint64_t
    shader_binding_layout_hash(const ShaderBindingLayout& layout) noexcept;
}

#endif // GCANVAS_SHADER_BINDING_HPP
