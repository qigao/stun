#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace flex::effects {

// CPU contracts only. These types do not assert that any production backend,
// artifact loader, GPU execution path, or terminal effect has been qualified.
struct Hash256 {
    std::array<std::uint8_t, 32> bytes{};
};
bool operator==(const Hash256& a, const Hash256& b) noexcept;
bool operator!=(const Hash256& a, const Hash256& b) noexcept;

struct SemanticVersion {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t patch = 0;
};

struct EffectIdentity {
    std::string id;
    SemanticVersion version;
    // Stable semantic/parameter-schema identity, not a descriptor address.
    Hash256 contract_hash;
};

enum class BackendApi : std::uint8_t {
    // Tango denotes the existing TTY/cell-output profile. Preserve its
    // identity; it never accepts GPU shader modules.
    Unknown, Direct2D, OpenGL, OpenGLES, Vulkan, Tango,
    // Separate OpenGL presentation of an already-projected Tango cell grid.
    // Pixel effects here require independently qualified GLSL artifacts.
    // This does NOT qualify a production Tango OpenGL renderer.
    TangoOpenGL,
};
struct BackendProfile {
    BackendApi api = BackendApi::Unknown;
    std::string profile;
};

enum class TargetKind : std::uint8_t { Unknown, NativePixel, TerminalCell };
enum class Unit : std::uint8_t {
    Unknown, Unitless, LogicalPixel, DevicePixel, CellColumn, CellRow,
    Degrees, Milliseconds,
};
struct TargetProfile {
    TargetKind kind = TargetKind::Unknown;
    Unit x_unit = Unit::Unknown;
    Unit y_unit = Unit::Unknown;
    // Includes the host's declared unit/metrics policy. No implicit conversion
    // between logical pixels, device pixels, rows, or columns is performed.
    Hash256 contract_hash;
};

enum class SurfaceFormat : std::uint8_t {
    Unknown, Rgba8Unorm, Rgba16Float, TerminalCells,
};
enum class ColorSpace : std::uint8_t {
    Unknown, SRgb, LinearSRgb, Terminal16, Terminal256, TerminalTrueColor,
};
enum class AlphaMode : std::uint8_t { Unknown, Opaque, Straight, Premultiplied };
struct SurfaceContract {
    SurfaceFormat format = SurfaceFormat::Unknown;
    ColorSpace color_space = ColorSpace::Unknown;
    AlphaMode alpha = AlphaMode::Unknown;
};

enum class EffectInput : std::uint8_t {
    Unknown, Geometry, ElementContent, Subtree, Backdrop, Texture, Cells,
};
enum class CompositeMode : std::uint8_t {
    Unknown, Replace, SourceOver, IsolatedSubtree,
};
enum class ParameterBinding : std::uint8_t { Uniform, Specialization, Structural };
struct ParameterRange {
    std::uint32_t id = 0;
    Unit unit = Unit::Unknown;
    ParameterBinding binding = ParameterBinding::Uniform;
    double lower = 0.0;
    double upper = 0.0;
};

struct EffectSource {
    std::string label;
    std::string token;
    std::size_t offset = 0;
    std::size_t length = 0;
};

struct ResourceScope {
    std::uint64_t owner_id = 0;
    std::uint64_t domain_id = 0;
    std::uint64_t device_generation = 0;
};
bool operator==(const ResourceScope& a, const ResourceScope& b) noexcept;
bool operator!=(const ResourceScope& a, const ResourceScope& b) noexcept;

struct ResourceCost {
    // Conservative metadata accounting: sum all declared surface bytes and
    // artifact bytes, including ExternalInput; sum pixel/cell areas over all
    // resources; allocations = resources + artifacts. A builtin consumes a
    // program/binding slot even with zero payload bytes. This does not measure
    // driver allocation footprint or deduplicate identity-shared resources.
    std::uint64_t bytes = 0;
    std::uint64_t pixels = 0;
    std::uint64_t cells = 0;
    std::uint64_t allocations = 0;
};

enum class ArtifactKind : std::uint8_t {
    Unknown, NativeBuiltin, D2DBytecode, GlslSource, EsslSource, SpirV,
};
enum class ShaderStage : std::uint8_t { None, Vertex, Fragment, Compute };
struct ProgramArtifact {
    std::string id;
    ArtifactKind kind = ArtifactKind::Unknown;
    ShaderStage stage = ShaderStage::None;
    std::string entry_point;
    std::string target_profile;
    std::string producer;
    // Native builtins have a binding ID and no pretend shader payload/entry.
    std::string builtin_binding;
    Hash256 content_hash;
    Hash256 interface_abi_hash;
    std::uint64_t byte_count = 0;
};

enum class ResourceRole : std::uint8_t { ExternalInput, Intermediate, Output };
struct SurfaceResource {
    std::uint32_t id = 0;
    ResourceRole role = ResourceRole::Intermediate;
    SurfaceContract surface;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t byte_count = 0;
};
enum class ResourceAccess : std::uint8_t {
    SampledRead, StorageRead, StorageWrite, ColorAttachmentWrite,
    NativeRead, NativeWrite,
};
struct SurfaceAccessSupport {
    SurfaceContract surface;
    ResourceAccess access = ResourceAccess::SampledRead;
};
struct ResourceUse {
    std::uint32_t resource_id = 0;
    ResourceAccess access = ResourceAccess::SampledRead;
};
enum class PassKind : std::uint8_t {
    NativeBuiltin, Graphics, Compute,
    // One Direct2D custom pixel-transform binding: exactly one Fragment-stage
    // D2DBytecode artifact with native read/write access. No fabricated vertex
    // shader; D2D vertex/compute/custom internal graphs are outside this slice.
    D2DPixelEffect,
};
struct Size3 {
    std::uint32_t x = 1;
    std::uint32_t y = 1;
    std::uint32_t z = 1;
};
struct EffectPass {
    std::uint32_t id = 0;
    PassKind kind = PassKind::NativeBuiltin;
    std::vector<std::string> artifacts;
    std::vector<ResourceUse> resources;
    std::vector<std::uint32_t> dependencies;
    Size3 dispatch;
    Size3 workgroup;
    std::uint64_t shared_memory_bytes = 0;
};

struct EffectRequirement {
    EffectIdentity effect;
    TargetProfile target;
    EffectInput input = EffectInput::Unknown;
    CompositeMode compositing = CompositeMode::Unknown;
    SurfaceContract input_surface;
    SurfaceContract output_surface;
    // Nonzero, maximum reachable output after transforms, DPI mapping, and
    // filter expansion. Supplied by the caller; validation does not derive
    // geometry. Counts use device pixels for NativePixel or columns/rows for
    // TerminalCell. At least one declared Output must cover this entire extent.
    std::uint32_t expanded_width = 0;
    std::uint32_t expanded_height = 0;
    // The caller supplies the conservative reachable parameter domain, not
    // merely the current hover/animation sample. Range extraction is separate.
    std::vector<ParameterRange> parameters;
    EffectSource source;
};

struct BackendProgramManifest {
    EffectIdentity effect;
    std::string variant_id;
    BackendProfile backend;
    TargetProfile target;
    Hash256 implementation_hash;
    Hash256 parameter_abi_hash;
    EffectInput input = EffectInput::Unknown;
    CompositeMode compositing = CompositeMode::Unknown;
    SurfaceContract input_surface;
    SurfaceContract output_surface;
    std::vector<ParameterRange> parameters;
    std::vector<ProgramArtifact> artifacts;
    // First slice: whole resources, unique IDs, no aliasing or feedback. Passes
    // must be declared in topological order; all dependencies precede a pass.
    std::vector<SurfaceResource> resources;
    std::vector<EffectPass> passes;
};

struct StageSupport {
    // NativeBuiltin + None denotes a native binding capability, not a GPU
    // shader stage. The tuple must exist independently for adapter and device.
    ArtifactKind kind = ArtifactKind::Unknown;
    ShaderStage stage = ShaderStage::None;
    std::string target_profile;
};
struct ExecutionLimits {
    // Conservative common limits for every qualified tuple in this record.
    // A broader per-format/per-stage limit must not be inferred from them.
    std::uint32_t max_width = 0;
    std::uint32_t max_height = 0;
    std::uint32_t max_resource_uses_per_pass = 0;
    Size3 max_dispatch{0, 0, 0};
    Size3 max_workgroup{0, 0, 0};
    std::uint64_t max_workgroup_invocations = 0;
    std::uint64_t max_shared_memory_bytes = 0;
};
struct EffectSupport {
    EffectIdentity effect;
    std::string variant_id;
    Hash256 implementation_hash;
    Hash256 parameter_abi_hash;
    EffectInput input = EffectInput::Unknown;
    CompositeMode compositing = CompositeMode::Unknown;
    std::vector<ParameterRange> parameters;
    // Allowed tuples, not independent masks that create unsupported products.
    std::vector<SurfaceContract> surfaces;
    std::vector<StageSupport> stages;
    // Each complete format/color/alpha + access tuple is explicitly qualified;
    // separate format and usage lists must not create unsupported combinations.
    std::vector<SurfaceAccessSupport> access;
    ExecutionLimits execution;
};

struct DeviceSupport {
    std::vector<SurfaceContract> surfaces;
    std::vector<StageSupport> stages;
    // Device observations qualify these exact surface/access tuples.
    std::vector<SurfaceAccessSupport> access;
    ExecutionLimits execution;
};

struct InstalledArtifact {
    std::string variant_id;
    std::string artifact_id;
    ArtifactKind kind = ArtifactKind::Unknown;
    ShaderStage stage = ShaderStage::None;
    std::string target_profile;
    std::string entry_point;
    // Observations supplied by the trusted backend loader/owner. Validation
    // compares metadata and digests with the selected artifact. It does not
    // load, hash, reflect, or compile native artifacts itself.
    Hash256 observed_content_hash;
    Hash256 observed_interface_abi_hash;
    std::uint64_t observed_byte_count = 0;
    std::string builtin_binding;
};
enum class DeviceStatus : std::uint8_t { Ready, Lost };
struct BackendCapabilitySnapshot {
    BackendProfile backend;
    TargetProfile target;
    ResourceScope scope;
    std::uint64_t capability_revision = 0;
    DeviceStatus status = DeviceStatus::Lost;
    // Qualified ADAPTER implementation domains only. Device support below is
    // intersected independently; neither side can promote the other's limits.
    // Empty means unsupported.
    // The legacy RendererCapabilities booleans cannot populate these records.
    std::vector<EffectSupport> effects;
    DeviceSupport device;
    std::vector<InstalledArtifact> installed;
};

enum class EffectError : std::uint8_t {
    None, InvalidLimits, InvalidManifest, InvalidValue, UnsupportedEffect,
    UnsupportedBackend, UnsupportedTarget, UnsupportedUnit, UnsupportedFormat,
    UnsupportedParameterDomain, UnsupportedStage, UnsupportedResourceAccess,
    InvalidPassGraph, MissingProgramVariant, ArtifactHashMismatch,
    ProgramAbiMismatch, ResourceLimit, BudgetExceeded, ScopeMismatch,
    StaleGeneration, StaleCapabilities, DeviceLost, AllocationFailure,
    NativePreparationFailed, NativeIdentityMismatch, WrongOwnerThread,
};
struct EffectDiagnostic {
    EffectError code = EffectError::None;
    EffectSource source;
    std::string message;
    std::uint32_t item = 0;
};
struct EffectValidationLimits {
    std::size_t max_parameters = 16;
    std::size_t max_artifacts = 16;
    std::size_t max_resources = 64;
    std::size_t max_passes = 32;
    std::size_t max_uses_per_pass = 64;
    std::size_t max_dependencies_per_pass = 32;
    std::size_t max_effect_supports = 64;
    std::size_t max_installed_artifacts = 256;
    std::size_t max_domain_entries = 32;
    std::size_t max_identifier_bytes = 128;
    std::size_t max_source_bytes = 1024;
    std::size_t max_metadata_bytes = 128 * 1024;
};

class ValidatedEffectContract;
struct EffectValidationResult {
    std::shared_ptr<const ValidatedEffectContract> contract;
    EffectDiagnostic diagnostic;
    explicit operator bool() const noexcept { return bool(contract); }
};
EffectValidationResult validate_effect_contract(
    const EffectRequirement& requirement,
    const BackendProgramManifest& selected_manifest,
    const BackendCapabilitySnapshot& capabilities,
    const EffectValidationLimits& limits = {});

class ValidatedEffectContract final {
public:
    ValidatedEffectContract(const ValidatedEffectContract&) = delete;
    ValidatedEffectContract& operator=(const ValidatedEffectContract&) = delete;
    ValidatedEffectContract(ValidatedEffectContract&&) = delete;
    ValidatedEffectContract& operator=(ValidatedEffectContract&&) = delete;

    // Own the complete checked inputs. Later owner preparation must consume
    // these frozen records rather than re-reading a caller's mutable manifest.
    const EffectRequirement& requirement() const noexcept { return requirement_; }
    const BackendProgramManifest& manifest() const noexcept { return manifest_; }
    const EffectIdentity& effect() const noexcept { return manifest_.effect; }
    const BackendProfile& backend() const noexcept { return manifest_.backend; }
    const TargetProfile& target() const noexcept { return manifest_.target; }
    const ResourceScope& scope() const noexcept { return scope_; }
    std::uint64_t capability_revision() const noexcept { return capability_revision_; }
    const std::string& variant_id() const noexcept { return manifest_.variant_id; }
    const Hash256& implementation_hash() const noexcept { return manifest_.implementation_hash; }
    const Hash256& parameter_abi_hash() const noexcept { return manifest_.parameter_abi_hash; }
    const ResourceCost& cost() const noexcept { return cost_; }
    const EffectSource& source() const noexcept { return requirement_.source; }

private:
    ValidatedEffectContract(EffectRequirement requirement,
        BackendProgramManifest manifest, ResourceScope scope,
        std::uint64_t capability_revision, ResourceCost cost);
    EffectRequirement requirement_;
    BackendProgramManifest manifest_;
    ResourceScope scope_;
    std::uint64_t capability_revision_;
    ResourceCost cost_;
    friend EffectValidationResult validate_effect_contract(
        const EffectRequirement&, const BackendProgramManifest&,
        const BackendCapabilitySnapshot&, const EffectValidationLimits&);
};

struct ResourceBudgetLimits {
    ResourceCost capacity;
    std::size_t max_reservations = 64;
};
struct ResourceBudgetSnapshot {
    ResourceScope scope;
    ResourceCost capacity;
    ResourceCost reserved;
    std::size_t reservations = 0;
    std::size_t max_reservations = 0;
    DeviceStatus status = DeviceStatus::Lost;
};

namespace detail { struct EffectBudgetState; }
class SharedResourceBudget;
struct EffectPreparationResult;

// This move-only lease owns budget accounting, not GPU resources. Keep it alive
// through candidate/live/cached/in-flight/retiring ownership. Release only after
// the backend owner has actually retired resources or completed failure cleanup.
class EffectReservation final {
public:
    EffectReservation() noexcept = default;
    ~EffectReservation();
    EffectReservation(const EffectReservation&) = delete;
    EffectReservation& operator=(const EffectReservation&) = delete;
    EffectReservation(EffectReservation&& other) noexcept;
    EffectReservation& operator=(EffectReservation&& other) noexcept;
    explicit operator bool() const noexcept { return bool(state_); }
    const ResourceCost& cost() const noexcept { return cost_; }
    const ResourceScope& scope() const noexcept { return scope_; }
    void release() noexcept;

private:
    EffectReservation(std::shared_ptr<detail::EffectBudgetState> state,
                      ResourceScope scope, ResourceCost cost) noexcept;
    std::shared_ptr<detail::EffectBudgetState> state_;
    ResourceScope scope_;
    ResourceCost cost_;
    friend EffectPreparationResult prepare_effect_admission(
        const ValidatedEffectContract&, SharedResourceBudget&,
        const BackendCapabilitySnapshot&);
};

// One immutable owner/domain/generation per shared ledger. Copying this handle
// shares the same aggregate accounting; it never creates an independent budget.
// Rebuilt devices use a new ledger. Lost ledgers retain outstanding charges.
// The owner must create one authoritative ledger per scope and share it with all
// participants. Construction rejects invalid scopes/count limits with
// std::invalid_argument; resource capacity dimensions may legitimately be zero.
class SharedResourceBudget final {
public:
    SharedResourceBudget(ResourceScope scope, ResourceBudgetLimits limits);
    ResourceBudgetSnapshot snapshot() const;
    void mark_device_lost() noexcept;

private:
    std::shared_ptr<detail::EffectBudgetState> state_;
    friend EffectPreparationResult prepare_effect_admission(
        const ValidatedEffectContract&, SharedResourceBudget&,
        const BackendCapabilitySnapshot&);
};

struct EffectPreparationResult {
    EffectReservation reservation;
    EffectDiagnostic diagnostic;
    explicit operator bool() const noexcept { return bool(reservation); }
};

// Atomic shared-ledger reservation after CPU contract validation. Success does
// not create shaders/surfaces, execute effects, prepare a RenderPlan, or publish
// UI state. Current capability identity/revision and ledger scope are rechecked;
// any capability-domain or installed-artifact change must advance that revision.
// Native allocation and a final revision check remain owner work.
EffectPreparationResult prepare_effect_admission(
    const ValidatedEffectContract& contract, SharedResourceBudget& budget,
    const BackendCapabilitySnapshot& current_capabilities);

} // namespace flex::effects
