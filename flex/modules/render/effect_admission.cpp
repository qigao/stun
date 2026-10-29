#include <flex/render/effect_admission.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace flex::effects {

ValidatedEffectContract::ValidatedEffectContract(
    EffectRequirement requirement, BackendProgramManifest manifest,
    ResourceScope scope, std::uint64_t capability_revision, ResourceCost cost)
    : requirement_(std::move(requirement)), manifest_(std::move(manifest)),
      scope_(scope), capability_revision_(capability_revision), cost_(cost) {}

namespace {

// Diagnostics must stay bounded even when the rejected input exceeds its limit.
EffectSource diagnostic_source(const EffectSource& source) {
    constexpr std::size_t max_diagnostic_text = 1024;
    return {source.label.substr(0, max_diagnostic_text),
            source.token.substr(0, max_diagnostic_text), source.offset, source.length};
}

EffectValidationResult failure(EffectError code, const EffectSource& source,
                               const char* message, std::uint32_t item = 0) {
    return {{}, {code, diagnostic_source(source), message, item}};
}

bool hash_present(const Hash256& hash) noexcept {
    return std::any_of(hash.bytes.begin(), hash.bytes.end(),
                       [](std::uint8_t b) { return b != 0; });
}

bool version_equal(const SemanticVersion& a, const SemanticVersion& b) noexcept {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

bool identity_equal(const EffectIdentity& a, const EffectIdentity& b) noexcept {
    return a.id == b.id && version_equal(a.version, b.version) &&
           a.contract_hash == b.contract_hash;
}

bool target_equal(const TargetProfile& a, const TargetProfile& b) noexcept {
    return a.kind == b.kind && a.x_unit == b.x_unit && a.y_unit == b.y_unit &&
           a.contract_hash == b.contract_hash;
}

bool surface_equal(const SurfaceContract& a, const SurfaceContract& b) noexcept {
    return a.format == b.format && a.color_space == b.color_space && a.alpha == b.alpha;
}

bool backend_valid(BackendApi api) noexcept {
    return api == BackendApi::Direct2D || api == BackendApi::OpenGL ||
           api == BackendApi::OpenGLES || api == BackendApi::Vulkan ||
           api == BackendApi::Tango || api == BackendApi::TangoOpenGL;
}

bool unit_valid(Unit unit) noexcept {
    switch (unit) {
    case Unit::Unitless: case Unit::LogicalPixel: case Unit::DevicePixel:
    case Unit::CellColumn: case Unit::CellRow: case Unit::Degrees:
    case Unit::Milliseconds: return true;
    default: return false;
    }
}

bool input_valid(EffectInput input) noexcept {
    switch (input) {
    case EffectInput::Geometry: case EffectInput::ElementContent:
    case EffectInput::Subtree: case EffectInput::Backdrop:
    case EffectInput::Texture: case EffectInput::Cells: return true;
    default: return false;
    }
}

bool composite_valid(CompositeMode mode) noexcept {
    return mode == CompositeMode::Replace || mode == CompositeMode::SourceOver ||
           mode == CompositeMode::IsolatedSubtree;
}

bool access_valid(ResourceAccess access) noexcept {
    switch (access) {
    case ResourceAccess::SampledRead: case ResourceAccess::StorageRead:
    case ResourceAccess::StorageWrite: case ResourceAccess::ColorAttachmentWrite:
    case ResourceAccess::NativeRead: case ResourceAccess::NativeWrite: return true;
    default: return false;
    }
}

bool is_write(ResourceAccess access) noexcept {
    return access == ResourceAccess::StorageWrite ||
           access == ResourceAccess::ColorAttachmentWrite ||
           access == ResourceAccess::NativeWrite;
}

bool is_read(ResourceAccess access) noexcept {
    return access == ResourceAccess::SampledRead ||
           access == ResourceAccess::StorageRead || access == ResourceAccess::NativeRead;
}

bool target_valid(const TargetProfile& target) noexcept {
    if (!hash_present(target.contract_hash)) return false;
    if (target.kind == TargetKind::NativePixel) {
        return (target.x_unit == Unit::LogicalPixel || target.x_unit == Unit::DevicePixel) &&
               target.x_unit == target.y_unit;
    }
    return target.kind == TargetKind::TerminalCell &&
           target.x_unit == Unit::CellColumn && target.y_unit == Unit::CellRow;
}

bool surface_valid(const SurfaceContract& surface, TargetKind target) noexcept {
    if (target == TargetKind::TerminalCell) {
        return surface.format == SurfaceFormat::TerminalCells &&
               (surface.color_space == ColorSpace::Terminal16 ||
                surface.color_space == ColorSpace::Terminal256 ||
                surface.color_space == ColorSpace::TerminalTrueColor) &&
               surface.alpha == AlphaMode::Opaque;
    }
    return target == TargetKind::NativePixel &&
           (surface.format == SurfaceFormat::Rgba8Unorm ||
            surface.format == SurfaceFormat::Rgba16Float) &&
           (surface.color_space == ColorSpace::SRgb ||
            surface.color_space == ColorSpace::LinearSRgb) &&
           (surface.alpha == AlphaMode::Opaque || surface.alpha == AlphaMode::Straight ||
            surface.alpha == AlphaMode::Premultiplied);
}

bool unit_for_target(Unit unit, TargetKind target) noexcept {
    if (!unit_valid(unit)) return false;
    if (target == TargetKind::TerminalCell)
        return unit != Unit::LogicalPixel && unit != Unit::DevicePixel;
    return unit != Unit::CellColumn && unit != Unit::CellRow;
}

bool stage_for_api(ArtifactKind kind, ShaderStage stage, BackendApi api) noexcept {
    if (kind == ArtifactKind::NativeBuiltin) return stage == ShaderStage::None;
    if (api == BackendApi::Direct2D)
        return kind == ArtifactKind::D2DBytecode && stage == ShaderStage::Fragment;
    const bool shader_stage = stage == ShaderStage::Vertex ||
                              stage == ShaderStage::Fragment || stage == ShaderStage::Compute;
    if (!shader_stage) return false;
    if (api == BackendApi::OpenGL || api == BackendApi::TangoOpenGL)
        return kind == ArtifactKind::GlslSource;
    if (api == BackendApi::OpenGLES) return kind == ArtifactKind::EsslSource;
    if (api == BackendApi::Vulkan) return kind == ArtifactKind::SpirV;
    return false; // Tango TTY never accepts GPU artifacts.
}

bool add_checked(std::uint64_t& total, std::uint64_t amount) noexcept {
    if (amount > (std::numeric_limits<std::uint64_t>::max)() - total) return false;
    total += amount;
    return true;
}

bool multiply_checked(std::uint64_t a, std::uint64_t b, std::uint64_t& product) noexcept {
    if (a != 0 && b > (std::numeric_limits<std::uint64_t>::max)() / a) return false;
    product = a * b;
    return true;
}

// Count raw input before traversing nested vectors. This bounds both work and
// copied metadata; it is not a claim about allocator/driver overhead.
class MetadataBudget {
public:
    explicit MetadataBudget(std::size_t limit) : remaining_(limit) {}
    bool charge(std::size_t bytes) noexcept {
        if (bytes > remaining_) return false;
        remaining_ -= bytes;
        return true;
    }
    template <class T> bool vector(const std::vector<T>& values, std::size_t count_limit) {
        return values.size() <= count_limit &&
               values.size() <= remaining_ / sizeof(T) &&
               charge(values.size() * sizeof(T));
    }
    bool string(const std::string& value, std::size_t limit) {
        return value.size() <= limit && charge(value.size());
    }
private:
    std::size_t remaining_;
};

bool identifier_valid(const std::string& value) noexcept {
    if (value.empty()) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= 33 && ch <= 126;
    });
}

bool identity_valid(const EffectIdentity& effect) noexcept {
    return identifier_valid(effect.id) && hash_present(effect.contract_hash) &&
           (effect.version.major != 0 || effect.version.minor != 0 || effect.version.patch != 0);
}

bool limits_valid(const EffectValidationLimits& l) noexcept {
    return l.max_parameters && l.max_artifacts && l.max_resources && l.max_passes &&
           l.max_uses_per_pass && l.max_dependencies_per_pass && l.max_effect_supports &&
           l.max_installed_artifacts && l.max_domain_entries && l.max_identifier_bytes &&
           l.max_source_bytes && l.max_metadata_bytes;
}

bool domain_metadata(MetadataBudget& b, const std::vector<SurfaceContract>& surfaces,
                     const std::vector<StageSupport>& stages,
                     const std::vector<SurfaceAccessSupport>& access,
                     const EffectValidationLimits& l) {
    if (!b.vector(surfaces, l.max_domain_entries) || !b.vector(stages, l.max_domain_entries) ||
        !b.vector(access, l.max_domain_entries)) return false;
    for (const auto& stage : stages)
        if (!b.string(stage.target_profile, l.max_identifier_bytes)) return false;
    return true;
}

bool metadata_fits(const EffectRequirement& req, const BackendProgramManifest& m,
                   const BackendCapabilitySnapshot& caps, const EffectValidationLimits& l) {
    MetadataBudget b(l.max_metadata_bytes);
    if (!b.charge(sizeof(req)) || !b.charge(sizeof(m)) || !b.charge(sizeof(caps)) ||
        !b.string(req.effect.id, l.max_identifier_bytes) ||
        !b.string(req.source.label, l.max_source_bytes) ||
        !b.string(req.source.token, l.max_source_bytes) ||
        !b.vector(req.parameters, l.max_parameters) ||
        !b.string(m.effect.id, l.max_identifier_bytes) ||
        !b.string(m.variant_id, l.max_identifier_bytes) ||
        !b.string(m.backend.profile, l.max_identifier_bytes) ||
        !b.vector(m.parameters, l.max_parameters) ||
        !b.vector(m.artifacts, l.max_artifacts) || !b.vector(m.resources, l.max_resources) ||
        !b.vector(m.passes, l.max_passes) ||
        !b.string(caps.backend.profile, l.max_identifier_bytes) ||
        !b.vector(caps.effects, l.max_effect_supports) ||
        !b.vector(caps.installed, l.max_installed_artifacts) ||
        !domain_metadata(b, caps.device.surfaces, caps.device.stages, caps.device.access, l))
        return false;
    for (const auto& artifact : m.artifacts) {
        if (!b.string(artifact.id, l.max_identifier_bytes) ||
            !b.string(artifact.entry_point, l.max_identifier_bytes) ||
            !b.string(artifact.target_profile, l.max_identifier_bytes) ||
            !b.string(artifact.producer, l.max_identifier_bytes) ||
            !b.string(artifact.builtin_binding, l.max_identifier_bytes)) return false;
    }
    for (const auto& pass : m.passes) {
        if (!b.vector(pass.artifacts, l.max_artifacts) ||
            !b.vector(pass.resources, l.max_uses_per_pass) ||
            !b.vector(pass.dependencies, l.max_dependencies_per_pass)) return false;
        for (const auto& artifact : pass.artifacts)
            if (!b.string(artifact, l.max_identifier_bytes)) return false;
    }
    for (const auto& support : caps.effects) {
        if (!b.string(support.effect.id, l.max_identifier_bytes) ||
            !b.string(support.variant_id, l.max_identifier_bytes) ||
            !b.vector(support.parameters, l.max_parameters) ||
            !domain_metadata(b, support.surfaces, support.stages, support.access, l)) return false;
    }
    for (const auto& artifact : caps.installed) {
        if (!b.string(artifact.variant_id, l.max_identifier_bytes) ||
            !b.string(artifact.artifact_id, l.max_identifier_bytes) ||
            !b.string(artifact.builtin_binding, l.max_identifier_bytes) ||
            !b.string(artifact.target_profile, l.max_identifier_bytes) ||
            !b.string(artifact.entry_point, l.max_identifier_bytes)) return false;
    }
    return true;
}

EffectError validate_parameters(const std::vector<ParameterRange>& params,
                                TargetKind target) noexcept {
    for (std::size_t i = 0; i < params.size(); ++i) {
        const auto& p = params[i];
        if (p.id == 0 || !std::isfinite(p.lower) || !std::isfinite(p.upper) ||
            p.lower > p.upper ||
            (p.binding != ParameterBinding::Uniform &&
             p.binding != ParameterBinding::Specialization &&
             p.binding != ParameterBinding::Structural)) return EffectError::InvalidValue;
        if (!unit_for_target(p.unit, target)) return EffectError::UnsupportedUnit;
        for (std::size_t j = 0; j < i; ++j)
            if (params[j].id == p.id) return EffectError::InvalidManifest;
    }
    return EffectError::None;
}

EffectError range_subset(const std::vector<ParameterRange>& required,
                        const std::vector<ParameterRange>& available) noexcept {
    if (required.size() != available.size()) return EffectError::UnsupportedParameterDomain;
    for (const auto& r : required) {
        const auto it = std::find_if(available.begin(), available.end(),
                                    [&](const ParameterRange& a) { return a.id == r.id; });
        if (it == available.end()) return EffectError::UnsupportedParameterDomain;
        if (r.unit != it->unit) return EffectError::UnsupportedUnit;
        if (r.binding != it->binding || r.lower < it->lower || r.upper > it->upper)
            return EffectError::UnsupportedParameterDomain;
    }
    return EffectError::None;
}

bool has_surface(const std::vector<SurfaceContract>& domain,
                 const SurfaceContract& required) noexcept {
    return std::any_of(domain.begin(), domain.end(),
                       [&](const SurfaceContract& s) { return surface_equal(s, required); });
}

bool has_stage(const std::vector<StageSupport>& domain,
               const ProgramArtifact& artifact) noexcept {
    return std::any_of(domain.begin(), domain.end(), [&](const StageSupport& s) {
        return s.kind == artifact.kind && s.stage == artifact.stage &&
               s.target_profile == artifact.target_profile;
    });
}

bool has_access(const std::vector<SurfaceAccessSupport>& domain,
                const SurfaceContract& surface, ResourceAccess required) noexcept {
    return std::any_of(domain.begin(), domain.end(), [&](const SurfaceAccessSupport& tuple) {
        return surface_equal(tuple.surface, surface) && tuple.access == required;
    });
}

bool domain_valid(const std::vector<SurfaceContract>& surfaces,
                  const std::vector<StageSupport>& stages,
                  const std::vector<SurfaceAccessSupport>& access,
                  BackendApi api, TargetKind target) noexcept {
    for (std::size_t i = 0; i < surfaces.size(); ++i) {
        if (!surface_valid(surfaces[i], target)) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (surface_equal(surfaces[i], surfaces[j])) return false;
    }
    for (std::size_t i = 0; i < stages.size(); ++i) {
        const auto& stage = stages[i];
        if (!stage_for_api(stage.kind, stage.stage, api) ||
            !identifier_valid(stage.target_profile)) return false;
        for (std::size_t j = 0; j < i; ++j) {
            const auto& previous = stages[j];
            if (stage.kind == previous.kind && stage.stage == previous.stage &&
                stage.target_profile == previous.target_profile) return false;
        }
    }
    for (std::size_t i = 0; i < access.size(); ++i) {
        const auto& tuple = access[i];
        if (!access_valid(tuple.access) || !has_surface(surfaces, tuple.surface)) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (access[j].access == tuple.access && surface_equal(access[j].surface, tuple.surface))
                return false;
    }
    return true;
}

bool fits_size(const Size3& actual, const Size3& limit) noexcept {
    return actual.x && actual.y && actual.z && actual.x <= limit.x &&
           actual.y <= limit.y && actual.z <= limit.z;
}

bool compute_fits(const EffectPass& pass, const ExecutionLimits& limit) noexcept {
    std::uint64_t xy = 0, xyz = 0;
    return fits_size(pass.dispatch, limit.max_dispatch) &&
           fits_size(pass.workgroup, limit.max_workgroup) &&
           multiply_checked(pass.workgroup.x, pass.workgroup.y, xy) &&
           multiply_checked(xy, pass.workgroup.z, xyz) &&
           xyz <= limit.max_workgroup_invocations &&
           pass.shared_memory_bytes <= limit.max_shared_memory_bytes;
}

bool default_execution(const EffectPass& pass) noexcept {
    return pass.dispatch.x == 1 && pass.dispatch.y == 1 && pass.dispatch.z == 1 &&
           pass.workgroup.x == 1 && pass.workgroup.y == 1 && pass.workgroup.z == 1 &&
           pass.shared_memory_bytes == 0;
}

} // namespace

EffectValidationResult validate_effect_contract(
    const EffectRequirement& req, const BackendProgramManifest& m,
    const BackendCapabilitySnapshot& caps, const EffectValidationLimits& limits) {
    try {
        const auto fail = [&](EffectError error, const char* message, std::uint32_t item = 0) {
            return failure(error, req.source, message, item);
        };
        if (!limits_valid(limits))
            return fail(EffectError::InvalidLimits, "all validation limits must be nonzero");
        if (!metadata_fits(req, m, caps, limits))
            return fail(EffectError::ResourceLimit, "raw metadata count or byte limit exceeded");
        if (req.source.length > (std::numeric_limits<std::size_t>::max)() - req.source.offset)
            return fail(EffectError::InvalidValue, "source range overflows");
        if (!identity_valid(req.effect) || !identity_valid(m.effect))
            return fail(EffectError::InvalidManifest, "effect identity requires a name, version and contract digest");
        if (!identity_equal(req.effect, m.effect))
            return fail(EffectError::UnsupportedEffect, "selected implementation has a different semantic effect contract");
        if (!backend_valid(m.backend.api) || !backend_valid(caps.backend.api) ||
            !identifier_valid(m.backend.profile) || !identifier_valid(caps.backend.profile) ||
            m.backend.api != caps.backend.api || m.backend.profile != caps.backend.profile)
            return fail(EffectError::UnsupportedBackend, "selected backend/API profile does not match capabilities");
        if (!target_valid(req.target) || !target_valid(m.target) || !target_valid(caps.target))
            return fail(EffectError::UnsupportedTarget, "invalid target units or missing target contract digest");
        if (!target_equal(req.target, m.target) || !target_equal(req.target, caps.target))
            return fail(EffectError::UnsupportedTarget, "target contract differs; implicit unit or target conversion is unavailable");
        // Tango TTY owns TerminalCell output. TangoOpenGL is the separate
        // pixel presentation/effect stage AFTER a cell-grid-to-pixel
        // projection, which is not performed by this metadata validator.
        if ((m.backend.api == BackendApi::Tango) != (req.target.kind == TargetKind::TerminalCell))
            return fail(EffectError::UnsupportedTarget,
                        "Tango TTY requires TerminalCell; TangoOpenGL and other GPU backends require NativePixel");
        if (caps.scope.owner_id == 0 || caps.scope.domain_id == 0 ||
            caps.scope.device_generation == 0 || caps.capability_revision == 0)
            return fail(EffectError::InvalidValue, "capabilities require a nonzero resource scope and revision");
        if (caps.status == DeviceStatus::Lost)
            return fail(EffectError::DeviceLost, "resource domain is lost");
        if (caps.status != DeviceStatus::Ready)
            return fail(EffectError::InvalidValue, "unknown device status");
        if (!identifier_valid(m.variant_id) || !hash_present(m.implementation_hash) ||
            !hash_present(m.parameter_abi_hash))
            return fail(EffectError::InvalidManifest, "variant requires implementation and parameter ABI identities");
        if (!input_valid(req.input) || !input_valid(m.input) ||
            !composite_valid(req.compositing) || !composite_valid(m.compositing))
            return fail(EffectError::InvalidValue, "unknown effect input or compositing mode");
        if (req.input != m.input || req.compositing != m.compositing)
            return fail(EffectError::UnsupportedEffect, "input or compositing semantics differ from the selected implementation");
        if (req.expanded_width == 0 || req.expanded_height == 0)
            return fail(EffectError::InvalidValue, "expanded reachable output extent must be nonzero");
        if (!surface_valid(req.input_surface, req.target.kind) ||
            !surface_valid(req.output_surface, req.target.kind) ||
            !surface_valid(m.input_surface, req.target.kind) ||
            !surface_valid(m.output_surface, req.target.kind) ||
            !surface_equal(req.input_surface, m.input_surface) ||
            !surface_equal(req.output_surface, m.output_surface))
            return fail(EffectError::UnsupportedFormat, "surface, color-space or alpha contract differs");
        for (const auto* params : {&req.parameters, &m.parameters}) {
            const auto error = validate_parameters(*params, req.target.kind);
            if (error != EffectError::None)
                return fail(error, "invalid, duplicate or target-incompatible parameter domain");
        }
        for (const auto& parameter : m.parameters) {
            if (parameter.binding != ParameterBinding::Uniform && parameter.lower != parameter.upper)
                return fail(EffectError::UnsupportedParameterDomain,
                            "one selected variant cannot cover a changing structural or specialization parameter");
        }
        auto range_error = range_subset(req.parameters, m.parameters);
        if (range_error != EffectError::None)
            return fail(range_error, "reachable parameter domain is outside the selected manifest");

        // A qualified adapter binding is unique for this semantic contract. The
        // caller supplies one selected manifest; we never rank or retry variants.
        const EffectSupport* support = nullptr;
        for (const auto& candidate : caps.effects) {
            if (!identity_valid(candidate.effect) || !identifier_valid(candidate.variant_id) ||
                !hash_present(candidate.implementation_hash) ||
                !hash_present(candidate.parameter_abi_hash) || !input_valid(candidate.input) ||
                !composite_valid(candidate.compositing))
                return fail(EffectError::InvalidManifest, "invalid qualified adapter record");
            const auto parameter_error = validate_parameters(candidate.parameters, req.target.kind);
            if (parameter_error != EffectError::None)
                return fail(parameter_error, "invalid qualified adapter parameter domain");
            if (!domain_valid(candidate.surfaces, candidate.stages, candidate.access,
                              caps.backend.api, caps.target.kind))
                return fail(EffectError::InvalidManifest, "invalid or duplicate qualified adapter domain tuple");
            if (candidate.effect.id == req.effect.id &&
                version_equal(candidate.effect.version, req.effect.version)) {
                if (support)
                    return fail(EffectError::InvalidManifest, "ambiguous qualified bindings for one effect/version");
                support = &candidate;
            }
        }
        if (!support || !identity_equal(support->effect, req.effect))
            return fail(EffectError::UnsupportedEffect, "adapter has no qualified semantic effect contract");
        if (support->variant_id != m.variant_id)
            return fail(EffectError::MissingProgramVariant, "selected variant is not the qualified adapter binding");
        if (support->implementation_hash != m.implementation_hash)
            return fail(EffectError::ArtifactHashMismatch, "implementation identity differs from the qualified binding");
        if (support->parameter_abi_hash != m.parameter_abi_hash)
            return fail(EffectError::ProgramAbiMismatch, "parameter ABI differs from the qualified binding");
        if (support->input != req.input || support->compositing != req.compositing)
            return fail(EffectError::UnsupportedEffect, "adapter does not implement this input/compositing contract");
        range_error = range_subset(m.parameters, support->parameters);
        if (range_error != EffectError::None)
            return fail(range_error, "manifest parameter domain exceeds qualified adapter support");
        if (!domain_valid(caps.device.surfaces, caps.device.stages, caps.device.access,
                          caps.backend.api, caps.target.kind))
            return fail(EffectError::InvalidManifest, "invalid or duplicate device domain tuple");
        for (const auto* surface : {&req.input_surface, &req.output_surface}) {
            if (!has_surface(support->surfaces, *surface) ||
                !has_surface(caps.device.surfaces, *surface))
                return fail(EffectError::UnsupportedFormat, "required surface tuple is absent from adapter or device support");
        }
        if (m.artifacts.empty())
            return fail(EffectError::MissingProgramVariant, "manifest has no native binding or shader artifact");
        if (m.passes.empty() || m.resources.empty())
            return fail(EffectError::InvalidPassGraph, "manifest must describe passes and whole resources");

        for (std::size_t i = 0; i < caps.installed.size(); ++i) {
            const auto& installed = caps.installed[i];
            if (!identifier_valid(installed.variant_id) || !identifier_valid(installed.artifact_id))
                return fail(EffectError::InvalidManifest, "invalid installed artifact identity");
            for (std::size_t j = 0; j < i; ++j)
                if (caps.installed[j].variant_id == installed.variant_id &&
                    caps.installed[j].artifact_id == installed.artifact_id)
                    return fail(EffectError::InvalidManifest, "duplicate installed artifact observation");
        }

        ResourceCost cost;
        for (std::size_t i = 0; i < m.artifacts.size(); ++i) {
            const auto& artifact = m.artifacts[i];
            if (!identifier_valid(artifact.id) || !identifier_valid(artifact.target_profile) ||
                !identifier_valid(artifact.producer) || !hash_present(artifact.content_hash) ||
                !hash_present(artifact.interface_abi_hash))
                return fail(EffectError::InvalidManifest, "artifact identity, toolchain, profile or digest is missing");
            for (std::size_t j = 0; j < i; ++j)
                if (m.artifacts[j].id == artifact.id)
                    return fail(EffectError::InvalidManifest, "duplicate artifact ID");
            if (!stage_for_api(artifact.kind, artifact.stage, caps.backend.api))
                return fail(EffectError::UnsupportedStage, "artifact kind/stage is not valid for the selected API");
            if (artifact.kind == ArtifactKind::NativeBuiltin) {
                if (!identifier_valid(artifact.builtin_binding) || !artifact.entry_point.empty() ||
                    artifact.byte_count != 0)
                    return fail(EffectError::InvalidManifest, "native bindings require a binding ID and no shader payload or entry point");
            } else if (!artifact.builtin_binding.empty() || artifact.byte_count == 0 ||
                       !identifier_valid(artifact.entry_point)) {
                return fail(EffectError::InvalidManifest, "shader artifact requires payload size and entry point, without a builtin binding");
            }
            if (!has_stage(support->stages, artifact) || !has_stage(caps.device.stages, artifact))
                return fail(EffectError::UnsupportedStage, "artifact stage/profile is absent from adapter or device support");
            const auto observed = std::find_if(caps.installed.begin(), caps.installed.end(),
                [&](const InstalledArtifact& installed) {
                    return installed.variant_id == m.variant_id && installed.artifact_id == artifact.id;
                });
            if (observed == caps.installed.end())
                return fail(EffectError::MissingProgramVariant, "selected artifact has no trusted loader observation");
            if (observed->observed_content_hash != artifact.content_hash ||
                observed->observed_byte_count != artifact.byte_count)
                return fail(EffectError::ArtifactHashMismatch, "loader-observed content digest or byte size differs");
            if (observed->observed_interface_abi_hash != artifact.interface_abi_hash)
                return fail(EffectError::ProgramAbiMismatch, "loader-observed artifact interface ABI differs");
            if (observed->kind != artifact.kind || observed->stage != artifact.stage ||
                observed->target_profile != artifact.target_profile ||
                observed->entry_point != artifact.entry_point ||
                observed->builtin_binding != artifact.builtin_binding)
                return fail(EffectError::InvalidManifest, "loader-observed artifact metadata differs from the selected manifest");
            if (!add_checked(cost.bytes, artifact.byte_count) || !add_checked(cost.allocations, 1))
                return fail(EffectError::ResourceLimit, "artifact accounting overflows");
        }

        std::size_t output_count = 0, output_index = 0, input_count = 0;
        for (std::size_t i = 0; i < m.resources.size(); ++i) {
            const auto& resource = m.resources[i];
            if (!resource.id || !resource.width || !resource.height ||
                (resource.role != ResourceRole::ExternalInput &&
                 resource.role != ResourceRole::Intermediate && resource.role != ResourceRole::Output))
                return fail(EffectError::InvalidValue, "invalid resource ID, role or extent", resource.id);
            for (std::size_t j = 0; j < i; ++j)
                if (m.resources[j].id == resource.id)
                    return fail(EffectError::InvalidPassGraph, "duplicate resource ID or undeclared alias", resource.id);
            if (!surface_valid(resource.surface, req.target.kind) ||
                !has_surface(support->surfaces, resource.surface) ||
                !has_surface(caps.device.surfaces, resource.surface))
                return fail(EffectError::UnsupportedFormat, "resource surface tuple is not supported", resource.id);
            if (resource.role == ResourceRole::ExternalInput &&
                !surface_equal(resource.surface, req.input_surface))
                return fail(EffectError::UnsupportedFormat, "external input differs from the semantic input surface", resource.id);
            if (resource.role == ResourceRole::ExternalInput) ++input_count;
            if (resource.role == ResourceRole::Output) {
                ++output_count;
                output_index = i;
                if (!surface_equal(resource.surface, req.output_surface))
                    return fail(EffectError::UnsupportedFormat, "output differs from the semantic output surface", resource.id);
                if (resource.width < req.expanded_width || resource.height < req.expanded_height)
                    return fail(EffectError::UnsupportedParameterDomain, "output cannot contain the expanded reachable effect bounds", resource.id);
            }
            if (resource.width > support->execution.max_width ||
                resource.height > support->execution.max_height ||
                resource.width > caps.device.execution.max_width ||
                resource.height > caps.device.execution.max_height)
                return fail(EffectError::ResourceLimit, "resource extent exceeds adapter or device limits", resource.id);
            std::uint64_t area = 0, minimum_bytes = 0;
            const std::uint64_t bytes_per_unit =
                resource.surface.format == SurfaceFormat::Rgba16Float ? 8 :
                resource.surface.format == SurfaceFormat::Rgba8Unorm ? 4 : 1;
            if (!multiply_checked(resource.width, resource.height, area) ||
                !multiply_checked(area, bytes_per_unit, minimum_bytes))
                return fail(EffectError::ResourceLimit, "resource storage arithmetic overflows", resource.id);
            if (resource.byte_count < minimum_bytes)
                return fail(EffectError::InvalidValue, "declared storage is smaller than the whole-resource minimum", resource.id);
            auto& units = req.target.kind == TargetKind::TerminalCell ? cost.cells : cost.pixels;
            if (!add_checked(units, area) || !add_checked(cost.bytes, resource.byte_count) ||
                !add_checked(cost.allocations, 1))
                return fail(EffectError::ResourceLimit, "aggregate resource accounting overflows", resource.id);
        }
        if (output_count != 1)
            return fail(EffectError::InvalidPassGraph, "this profile requires exactly one output resource");
        if (req.input != EffectInput::Geometry && input_count == 0)
            return fail(EffectError::InvalidPassGraph, "content effects require a declared external input");

        // A bounded transitive closure validates declared ordering; it does not
        // emit GPU barriers. Whole resources and one writer avoid alias/feedback
        // assumptions until a later, explicitly qualified region-aware profile.
        const std::size_t pass_count = m.passes.size();
        if (pass_count > limits.max_metadata_bytes / pass_count)
            return fail(EffectError::ResourceLimit, "pass dependency validation workspace exceeds its byte limit");
        std::vector<std::uint8_t> ancestors(pass_count * pass_count, 0);
        const auto no_writer = pass_count;
        std::vector<std::size_t> writers(m.resources.size(), no_writer);
        std::vector<bool> resource_used(m.resources.size(), false);
        std::vector<bool> artifact_used(m.artifacts.size(), false);
        for (std::size_t p = 0; p < pass_count; ++p) {
            const auto& pass = m.passes[p];
            if (!pass.id)
                return fail(EffectError::InvalidPassGraph, "pass ID must be nonzero");
            for (std::size_t previous = 0; previous < p; ++previous)
                if (m.passes[previous].id == pass.id)
                    return fail(EffectError::InvalidPassGraph, "duplicate pass ID", pass.id);
            for (std::size_t d = 0; d < pass.dependencies.size(); ++d) {
                const auto dependency = pass.dependencies[d];
                if (std::find(pass.dependencies.begin(),
                              pass.dependencies.begin() + static_cast<std::ptrdiff_t>(d), dependency) !=
                    pass.dependencies.begin() + static_cast<std::ptrdiff_t>(d))
                    return fail(EffectError::InvalidPassGraph, "duplicate pass dependency", pass.id);
                const auto found = std::find_if(m.passes.begin(), m.passes.begin() + static_cast<std::ptrdiff_t>(p),
                    [&](const EffectPass& prior) { return prior.id == dependency; });
                if (found == m.passes.begin() + static_cast<std::ptrdiff_t>(p))
                    return fail(EffectError::InvalidPassGraph, "dependency is missing, cyclic, or not in topological order", pass.id);
                const auto index = static_cast<std::size_t>(found - m.passes.begin());
                ancestors[p * pass_count + index] = 1;
                for (std::size_t a = 0; a < index; ++a)
                    ancestors[p * pass_count + a] |= ancestors[index * pass_count + a];
            }

            std::size_t vertex = 0, fragment = 0, compute = 0, native = 0;
            for (std::size_t a = 0; a < pass.artifacts.size(); ++a) {
                const auto& id = pass.artifacts[a];
                if (std::find(pass.artifacts.begin(), pass.artifacts.begin() + static_cast<std::ptrdiff_t>(a), id) !=
                    pass.artifacts.begin() + static_cast<std::ptrdiff_t>(a))
                    return fail(EffectError::InvalidPassGraph, "duplicate artifact reference in pass", pass.id);
                const auto artifact = std::find_if(m.artifacts.begin(), m.artifacts.end(),
                    [&](const ProgramArtifact& value) { return value.id == id; });
                if (artifact == m.artifacts.end())
                    return fail(EffectError::InvalidPassGraph, "pass references an unknown artifact", pass.id);
                artifact_used[static_cast<std::size_t>(artifact - m.artifacts.begin())] = true;
                switch (artifact->stage) {
                case ShaderStage::Vertex: ++vertex; break;
                case ShaderStage::Fragment: ++fragment; break;
                case ShaderStage::Compute: ++compute; break;
                case ShaderStage::None: ++native; break;
                default: return fail(EffectError::UnsupportedStage, "unknown artifact stage", pass.id);
                }
            }
            const bool gpu_api = caps.backend.api == BackendApi::OpenGL ||
                                 caps.backend.api == BackendApi::OpenGLES ||
                                 caps.backend.api == BackendApi::Vulkan ||
                                 caps.backend.api == BackendApi::TangoOpenGL;
            switch (pass.kind) {
            case PassKind::NativeBuiltin:
                if (pass.artifacts.size() != 1 || native != 1)
                    return fail(EffectError::UnsupportedStage, "native pass requires exactly one native binding", pass.id);
                break;
            case PassKind::Graphics:
                if (!gpu_api || pass.artifacts.size() != 2 || vertex != 1 || fragment != 1)
                    return fail(EffectError::UnsupportedStage, "graphics pass requires one vertex and one fragment module", pass.id);
                break;
            case PassKind::Compute:
                if (!gpu_api || pass.artifacts.size() != 1 || compute != 1)
                    return fail(EffectError::UnsupportedStage, "compute pass requires one compute module and a qualified GPU execution path", pass.id);
                if (!compute_fits(pass, support->execution) || !compute_fits(pass, caps.device.execution))
                    return fail(EffectError::ResourceLimit, "dispatch, workgroup or shared-memory domain exceeds adapter or device limits", pass.id);
                break;
            case PassKind::D2DPixelEffect:
                if (caps.backend.api != BackendApi::Direct2D || pass.artifacts.size() != 1 || fragment != 1)
                    return fail(EffectError::UnsupportedStage, "D2D pixel effect requires one D2D fragment artifact", pass.id);
                break;
            default:
                return fail(EffectError::InvalidPassGraph, "unknown pass kind", pass.id);
            }
            if (pass.kind != PassKind::Compute && !default_execution(pass))
                return fail(EffectError::InvalidManifest, "non-compute pass carries unused dispatch or workgroup data", pass.id);
            if (pass.resources.empty())
                return fail(EffectError::InvalidPassGraph, "pass has no declared resource accesses", pass.id);
            if (pass.resources.size() > support->execution.max_resource_uses_per_pass ||
                pass.resources.size() > caps.device.execution.max_resource_uses_per_pass)
                return fail(EffectError::ResourceLimit, "pass resource accesses exceed adapter or device limits", pass.id);
            std::size_t writes = 0;
            for (std::size_t u = 0; u < pass.resources.size(); ++u) {
                const auto& use = pass.resources[u];
                for (std::size_t prior = 0; prior < u; ++prior)
                    if (pass.resources[prior].resource_id == use.resource_id)
                        return fail(EffectError::InvalidPassGraph, "same-pass resource feedback or duplicate use is unsupported", pass.id);
                const auto found = std::find_if(m.resources.begin(), m.resources.end(),
                    [&](const SurfaceResource& value) { return value.id == use.resource_id; });
                if (found == m.resources.end())
                    return fail(EffectError::InvalidPassGraph, "pass references an unknown resource", pass.id);
                const auto index = static_cast<std::size_t>(found - m.resources.begin());
                resource_used[index] = true;
                if (!access_valid(use.access) || !has_access(support->access, found->surface, use.access) ||
                    !has_access(caps.device.access, found->surface, use.access))
                    return fail(EffectError::UnsupportedResourceAccess, "surface/access tuple is absent from adapter or device support", pass.id);
                const bool native_pass = pass.kind == PassKind::NativeBuiltin || pass.kind == PassKind::D2DPixelEffect;
                const bool permitted = native_pass ?
                    (use.access == ResourceAccess::NativeRead || use.access == ResourceAccess::NativeWrite) :
                    pass.kind == PassKind::Graphics ?
                    (use.access == ResourceAccess::SampledRead || use.access == ResourceAccess::ColorAttachmentWrite) :
                    (use.access == ResourceAccess::SampledRead || use.access == ResourceAccess::StorageRead ||
                     use.access == ResourceAccess::StorageWrite);
                if (!permitted)
                    return fail(EffectError::UnsupportedResourceAccess, "access kind is incompatible with this pass execution model", pass.id);
                if (is_write(use.access)) {
                    ++writes;
                    if (found->role == ResourceRole::ExternalInput || writers[index] != no_writer)
                        return fail(EffectError::InvalidPassGraph, "resource is external or already has a writer", pass.id);
                    writers[index] = p;
                } else if (is_read(use.access) && found->role != ResourceRole::ExternalInput) {
                    if (writers[index] == no_writer || writers[index] >= p ||
                        !ancestors[p * pass_count + writers[index]])
                        return fail(EffectError::InvalidPassGraph, "read lacks an initialized producer and declared dependency", pass.id);
                }
            }
            if (writes == 0)
                return fail(EffectError::InvalidPassGraph, "pass must produce a declared whole resource", pass.id);
        }
        for (std::size_t i = 0; i < m.resources.size(); ++i) {
            if (!resource_used[i] || (m.resources[i].role != ResourceRole::ExternalInput && writers[i] == no_writer))
                return fail(EffectError::InvalidPassGraph, "unused or uninitialized resource in manifest", m.resources[i].id);
        }
        if (std::find(artifact_used.begin(), artifact_used.end(), false) != artifact_used.end())
            return fail(EffectError::InvalidPassGraph, "manifest contains an artifact not used by any pass");

        // Dependencies establish ordering, not content flow. A disconnected
        // branch cannot stand in for the semantic input of a content effect.
        // Reverse topological traversal follows only actual read/producer edges.
        std::vector<bool> needed_resources(m.resources.size(), false);
        std::vector<bool> needed_passes(pass_count, false);
        needed_resources[output_index] = true;
        for (std::size_t p = pass_count; p-- > 0;) {
            bool needed = false;
            for (std::size_t r = 0; r < writers.size(); ++r)
                needed = needed || (needed_resources[r] && writers[r] == p);
            if (!needed) continue;
            needed_passes[p] = true;
            for (const auto& use : m.passes[p].resources) {
                if (!is_read(use.access)) continue;
                const auto found = std::find_if(m.resources.begin(), m.resources.end(),
                    [&](const SurfaceResource& resource) { return resource.id == use.resource_id; });
                needed_resources[static_cast<std::size_t>(found - m.resources.begin())] = true;
            }
        }
        if (std::find(needed_passes.begin(), needed_passes.end(), false) != needed_passes.end() ||
            std::find(needed_resources.begin(), needed_resources.end(), false) != needed_resources.end())
            return fail(EffectError::InvalidPassGraph, "pass or resource is disconnected from the output dataflow");

        // Freeze the actual validated inputs. Owners must not allocate from the
        // caller's mutable manifest after obtaining this proof or reservation.
        auto contract = std::shared_ptr<const ValidatedEffectContract>(
            new ValidatedEffectContract(req, m, caps.scope, caps.capability_revision, cost));
        return {std::move(contract), {}};
    } catch (const std::bad_alloc&) {
        return {{}, {EffectError::AllocationFailure, {}, {}, 0}};
    }
}

} // namespace flex::effects
