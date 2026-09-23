#include "gcanvas/shader_binding.hpp"

#include <cstring>

namespace gcanvas
{
    namespace
    {
        constexpr std::uint64_t fnv_offset = 14695981039346656037ull;
        constexpr std::uint64_t fnv_prime = 1099511628211ull;

        bool nonempty(const char* value) noexcept
        {
            return value != nullptr && value[0] != '\0';
        }

        bool valid_stage_mask(ShaderStageMask stages) noexcept
        {
            return stages != 0u && (stages & ~shader_stage_all_graphics) == 0u;
        }

        bool valid_kind(ShaderResourceKind kind) noexcept
        {
            switch (kind)
            {
            case ShaderResourceKind::UniformBlock:
            case ShaderResourceKind::SampledImage:
            case ShaderResourceKind::Sampler:
            case ShaderResourceKind::CombinedImageSampler:
            case ShaderResourceKind::StageInput:
            case ShaderResourceKind::StageOutput:
                return true;
            }
            return false;
        }

        bool stable_type(const cmeta_type_desc* type) noexcept
        {
            if (!cmeta_type_desc_valid(type))
                return false;
            const cmeta_type_identity* identity = cmeta_type_identity_of(type);
            return identity != nullptr && cmeta_type_identity_valid(identity);
        }

        bool valid_struct(const cmeta_struct_desc* desc) noexcept
        {
            if (desc == nullptr || !nonempty(desc->name) || desc->size == 0u ||
                desc->align == 0u)
                return false;
            if (desc->field_count != 0u && desc->fields == nullptr)
                return false;

            for (std::size_t index = 0; index < desc->field_count; ++index)
            {
                const cmeta_field_desc& field = desc->fields[index];
                if (!nonempty(field.name) || field.size == 0u || field.align == 0u ||
                    !stable_type(field.type))
                    return false;
                if (field.offset > desc->size ||
                    field.size > desc->size - field.offset)
                    return false;
            }
            return true;
        }

        ShaderBindingValidation fail(ShaderBindingError error,
                                     std::size_t binding,
                                     std::size_t field =
                                         static_cast<std::size_t>(-1)) noexcept
        {
            return {error, binding, field};
        }

        bool is_resource(ShaderResourceKind kind) noexcept
        {
            return kind == ShaderResourceKind::UniformBlock ||
                   kind == ShaderResourceKind::SampledImage ||
                   kind == ShaderResourceKind::Sampler ||
                   kind == ShaderResourceKind::CombinedImageSampler;
        }

        bool is_stage_io(ShaderResourceKind kind) noexcept
        {
            return kind == ShaderResourceKind::StageInput ||
                   kind == ShaderResourceKind::StageOutput;
        }

        bool same_stage_location(const ShaderBindingDesc& left,
                                 const ShaderBindingDesc& right) noexcept
        {
            if (!is_stage_io(left.kind) || left.kind != right.kind ||
                left.location != right.location)
                return false;
            return (left.stages & right.stages) != 0u;
        }

        void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept
        {
            hash ^= value;
            hash *= fnv_prime;
        }

        void hash_u64(std::uint64_t& hash, std::uint64_t value) noexcept
        {
            for (unsigned shift = 0; shift < 64; shift += 8)
                hash_byte(hash, static_cast<std::uint8_t>((value >> shift) & 0xffu));
        }

        void hash_string(std::uint64_t& hash, const char* value) noexcept
        {
            if (value == nullptr)
            {
                hash_u64(hash, 0u);
                return;
            }

            const std::size_t length = std::strlen(value);
            hash_u64(hash, static_cast<std::uint64_t>(length));
            for (std::size_t index = 0; index < length; ++index)
                hash_byte(hash, static_cast<std::uint8_t>(value[index]));
        }

        bool hash_identity(std::uint64_t& hash,
                           const cmeta_type_identity* identity) noexcept
        {
            if (!cmeta_type_identity_valid(identity))
                return false;

            hash_u64(hash, static_cast<std::uint64_t>(identity->form));
            switch (identity->form)
            {
            case CMETA_TYPE_ATOM:
                hash_string(hash, identity->stable_atom_id);
                return true;

            case CMETA_TYPE_POINTER:
            case CMETA_TYPE_CONST:
                return hash_identity(hash, identity->base);

            case CMETA_TYPE_APPLY:
                if (!cmeta_generic_desc_valid(identity->constructor))
                    return false;
                hash_string(hash, identity->constructor->stable_id);
                hash_u64(hash, static_cast<std::uint64_t>(identity->arity));
                for (std::size_t index = 0; index < identity->arity; ++index)
                    if (!hash_identity(hash, identity->args[index]))
                        return false;
                return true;
            }
            return false;
        }

        bool hash_type(std::uint64_t& hash,
                       const cmeta_type_desc* type) noexcept
        {
            if (!stable_type(type))
                return false;
            return hash_identity(hash, cmeta_type_identity_of(type));
        }
    }

    ShaderBindingValidation
    validate_shader_binding_layout(const ShaderBindingLayout& layout) noexcept
    {
        if (layout.abi_version != shader_binding_abi_version)
            return fail(ShaderBindingError::InvalidAbiVersion, 0u);
        if (layout.binding_count != 0u && layout.bindings == nullptr)
            return fail(ShaderBindingError::MissingBindings, 0u);

        for (std::size_t index = 0; index < layout.binding_count; ++index)
        {
            const ShaderBindingDesc& binding = layout.bindings[index];
            if (!nonempty(binding.name))
                return fail(ShaderBindingError::InvalidName, index);
            if (!valid_stage_mask(binding.stages))
                return fail(ShaderBindingError::InvalidStageMask, index);
            if (!valid_kind(binding.kind))
                return fail(ShaderBindingError::InvalidResourceKind, index);
            if (!cmeta_type_desc_valid(binding.type))
                return fail(ShaderBindingError::InvalidType, index);
            if (!stable_type(binding.type))
                return fail(ShaderBindingError::MissingStableTypeIdentity, index);
            if (binding.array_count == 0u)
                return fail(ShaderBindingError::InvalidPlacement, index);

            if (is_resource(binding.kind))
            {
                if (binding.set == shader_binding_unset ||
                    binding.binding == shader_binding_unset ||
                    binding.location != shader_binding_unset)
                    return fail(ShaderBindingError::InvalidPlacement, index);
            }
            else
            {
                if (binding.set != shader_binding_unset ||
                    binding.binding != shader_binding_unset ||
                    binding.location == shader_binding_unset)
                    return fail(ShaderBindingError::InvalidPlacement, index);
            }

            if (binding.kind == ShaderResourceKind::UniformBlock)
            {
                if (!valid_struct(binding.struct_type) || binding.gpu_size == 0u ||
                    binding.type->size != binding.struct_type->size ||
                    binding.type->align != binding.struct_type->align)
                    return fail(ShaderBindingError::InvalidStructLayout, index);
                if (binding.field_count != 0u && binding.fields == nullptr)
                    return fail(ShaderBindingError::InvalidField, index);

                for (std::size_t field_index = 0;
                     field_index < binding.field_count; ++field_index)
                {
                    const ShaderFieldBindingDesc& gpu_field =
                        binding.fields[field_index];
                    if (gpu_field.field == nullptr ||
                        !nonempty(gpu_field.field->name) ||
                        !stable_type(gpu_field.field->type) ||
                        gpu_field.array_count == 0u ||
                        gpu_field.gpu_offset >= binding.gpu_size)
                        return fail(ShaderBindingError::InvalidField,
                                    index, field_index);

                    if (gpu_field.array_count > 1u && gpu_field.gpu_stride == 0u)
                        return fail(ShaderBindingError::InvalidField,
                                    index, field_index);

                    const cmeta_field_desc* semantic_field =
                        cmeta_struct_find_field(binding.struct_type,
                                                gpu_field.field->name);
                    if (semantic_field == nullptr ||
                        !cmeta_type_equal(semantic_field->type,
                                          gpu_field.field->type))
                        return fail(ShaderBindingError::InvalidField,
                                    index, field_index);

                    for (std::size_t prior = 0; prior < field_index; ++prior)
                    {
                        if (std::strcmp(binding.fields[prior].field->name,
                                        gpu_field.field->name) == 0)
                            return fail(ShaderBindingError::InvalidField,
                                        index, field_index);
                    }
                }
            }
            else if (binding.struct_type != nullptr ||
                     binding.fields != nullptr ||
                     binding.field_count != 0u ||
                     binding.gpu_size != 0u)
            {
                return fail(ShaderBindingError::InvalidStructLayout, index);
            }

            for (std::size_t prior = 0; prior < index; ++prior)
            {
                const ShaderBindingDesc& previous = layout.bindings[prior];
                if (is_resource(binding.kind) && is_resource(previous.kind) &&
                    binding.set == previous.set &&
                    binding.binding == previous.binding)
                    return fail(ShaderBindingError::DuplicateResourceBinding,
                                index);
                if (same_stage_location(binding, previous))
                    return fail(ShaderBindingError::DuplicateStageLocation,
                                index);
            }
        }

        return {};
    }

    bool shader_binding_layout_valid(const ShaderBindingLayout& layout) noexcept
    {
        return static_cast<bool>(validate_shader_binding_layout(layout));
    }

    std::uint64_t
    shader_binding_layout_hash(const ShaderBindingLayout& layout) noexcept
    {
        if (!shader_binding_layout_valid(layout))
            return 0u;

        std::uint64_t hash = fnv_offset;
        hash_u64(hash, layout.abi_version);
        hash_u64(hash, static_cast<std::uint64_t>(layout.binding_count));

        for (std::size_t index = 0; index < layout.binding_count; ++index)
        {
            const ShaderBindingDesc& binding = layout.bindings[index];
            hash_string(hash, binding.name);
            hash_u64(hash, binding.stages);
            hash_u64(hash, static_cast<std::uint64_t>(binding.kind));
            hash_u64(hash, binding.set);
            hash_u64(hash, binding.binding);
            hash_u64(hash, binding.location);
            hash_u64(hash, binding.array_count);
            hash_u64(hash, static_cast<std::uint64_t>(binding.gpu_size));
            if (!hash_type(hash, binding.type))
                return 0u;

            if (binding.struct_type != nullptr)
            {
                hash_string(hash, binding.struct_type->name);
                hash_u64(hash,
                         static_cast<std::uint64_t>(binding.field_count));
                for (std::size_t field_index = 0;
                     field_index < binding.field_count; ++field_index)
                {
                    const ShaderFieldBindingDesc& field =
                        binding.fields[field_index];
                    hash_string(hash, field.field->name);
                    if (!hash_type(hash, field.field->type))
                        return 0u;
                    hash_u64(hash,
                             static_cast<std::uint64_t>(field.gpu_offset));
                    hash_u64(hash,
                             static_cast<std::uint64_t>(field.gpu_stride));
                    hash_u64(hash, field.array_count);
                }
            }
        }

        return hash == 0u ? 1u : hash;
    }
}
