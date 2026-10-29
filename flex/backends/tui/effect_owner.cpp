#include <backends/tui/effect_owner.h>

#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>

namespace flex::tui_backend {
using namespace flex::effects;
namespace {

constexpr std::uint64_t kMaxCells = 65536;
constexpr char kEffectId[] = "tango.cell-buffer.clear";
constexpr char kVariantId[] = "tango.clear.native.v1";
constexpr char kBindingId[] = "tango.cell-buffer.clear.v1";
constexpr char kTargetProfile[] = "terminal-cells-v1";
constexpr char kBackendProfile[] = "tango-cells-native-v1";

// SHA-256 of the explicit, versioned semantic/binding identifiers. These are
// contract identities, NOT hashes of Tango binaries or imaginary shaders.
// The binding is installed only by this real, linked production backend.
const Hash256 kSemantic{{0x77,0xa3,0x79,0x32,0x58,0x26,0xd5,0xc6,0xff,0x22,0x0b,0x6e,0x11,0x65,0x4a,0x97,0x91,0x66,0x57,0x6a,0x92,0x43,0x78,0x91,0x32,0xfe,0x4b,0xbf,0xc1,0x98,0x78,0x08}};
const Hash256 kImplementation{{0x37,0x9e,0x3c,0x7f,0x77,0x44,0x29,0xc3,0xfd,0xd9,0x4b,0xfe,0xd2,0xa2,0x10,0xa6,0xaa,0xb2,0x33,0x97,0xc6,0xda,0x9e,0x34,0x82,0x02,0xbd,0x33,0xe1,0x80,0x8a,0x39}};
const Hash256 kAbi{{0x7b,0x2e,0x85,0xa4,0x56,0x7b,0x29,0xf9,0x5c,0x17,0xe9,0x96,0x99,0x99,0xf4,0x42,0xcf,0x72,0xad,0x9f,0x4a,0xea,0x40,0x12,0xea,0xb7,0x61,0x4b,0x3a,0xfb,0xca,0x5a}};
const Hash256 kBinding{{0x82,0x80,0xc2,0xc0,0x67,0xcf,0x1f,0xd4,0x67,0x45,0x5a,0x1d,0x39,0xfc,0xf7,0x73,0x28,0x59,0xe2,0x89,0x8b,0xe6,0xc8,0x0d,0x88,0x24,0x5c,0xc8,0xc5,0x84,0xe1,0x49}};
const Hash256 kTarget{{0x05,0x5d,0x7a,0xa4,0x49,0xdc,0xb4,0xb7,0x15,0x2e,0xb7,0x13,0x8f,0x91,0x36,0x33,0xe0,0x00,0x88,0x11,0x9e,0xd0,0x44,0x54,0x52,0xff,0xa3,0x5f,0xae,0xa4,0x8b,0x0a}};

SurfaceContract cell_surface() {
    return {SurfaceFormat::TerminalCells, ColorSpace::TerminalTrueColor, AlphaMode::Opaque};
}

std::uint64_t buffer_bytes(std::uint32_t columns, std::uint32_t rows) noexcept {
    return sizeof(tui_buffer_t) + static_cast<std::uint64_t>(columns) *
           static_cast<std::uint64_t>(rows) * sizeof(tui_cell_t);
}

bool same_surface(const SurfaceContract& a, const SurfaceContract& b) noexcept {
    return a.format == b.format && a.color_space == b.color_space &&
           a.alpha == b.alpha;
}

} // namespace

TangoCellClearResources::TangoCellClearResources(
    NativeEffectResourceIdentity identity, tui_buffer_t* buffer,
    std::thread::id thread) noexcept
    : identity_(std::move(identity)), buffer_(buffer), owning_thread_(thread) {}

TangoCellClearResources::~TangoCellClearResources() noexcept {
    // The standalone synchronous cell buffer must retire on its owner. Do not
    // silently destroy backend resources from a worker or queued callback.
    if (std::this_thread::get_id() != owning_thread_) std::terminate();
    tui_buffer_destroy(buffer_);
    buffer_ = nullptr;
}

tui_buffer_t* TangoCellClearResources::buffer_on_owner() const noexcept {
    return std::this_thread::get_id() == owning_thread_ ? buffer_ : nullptr;
}

TangoCellClearOwner::TangoCellClearOwner(
    ResourceScope scope, std::uint32_t max_columns, std::uint32_t max_rows)
    : owning_thread_(std::this_thread::get_id()), scope_(scope),
      max_columns_(max_columns), max_rows_(max_rows) {
    if (!scope.owner_id || !scope.domain_id || !scope.device_generation ||
        !max_columns || !max_rows ||
        max_columns > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) ||
        max_rows > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) ||
        static_cast<std::uint64_t>(max_columns) * max_rows > kMaxCells) {
        throw std::invalid_argument(
            "Tango native clear owner requires bounded cell dimensions and resource scope");
    }

    capabilities_.backend = {BackendApi::Tango, kBackendProfile};
    capabilities_.target = {
        TargetKind::TerminalCell, Unit::CellColumn, Unit::CellRow, kTarget};
    capabilities_.scope = scope_;
    capabilities_.capability_revision = 1;
    capabilities_.status = DeviceStatus::Ready;

    EffectSupport support;
    support.effect = {kEffectId, {1, 0, 0}, kSemantic};
    support.variant_id = kVariantId;
    support.implementation_hash = kImplementation;
    support.parameter_abi_hash = kAbi;
    support.input = EffectInput::Geometry;
    support.compositing = CompositeMode::Replace;
    support.surfaces = {cell_surface()};
    support.stages = {{ArtifactKind::NativeBuiltin, ShaderStage::None, kTargetProfile}};
    support.access = {{cell_surface(), ResourceAccess::NativeWrite}};
    support.execution.max_width = max_columns_;
    support.execution.max_height = max_rows_;
    support.execution.max_resource_uses_per_pass = 1;
    capabilities_.effects = {support};
    capabilities_.device.surfaces = support.surfaces;
    capabilities_.device.stages = support.stages;
    capabilities_.device.access = support.access;
    capabilities_.device.execution = support.execution;

    // The exact native clear function is linked into flex_backend_tui.
    // This is a native binding observation, not a shader file observation.
    InstalledArtifact installed;
    installed.variant_id = kVariantId;
    installed.artifact_id = "tango-clear";
    installed.kind = ArtifactKind::NativeBuiltin;
    installed.stage = ShaderStage::None;
    installed.target_profile = kTargetProfile;
    installed.observed_content_hash = kBinding;
    installed.observed_interface_abi_hash = kAbi;
    installed.observed_byte_count = 0;
    installed.builtin_binding = kBindingId;
    capabilities_.installed = {installed};
}

bool TangoCellClearOwner::on_owner_thread() const noexcept {
    return std::this_thread::get_id() == owning_thread_;
}

BackendCapabilitySnapshot TangoCellClearOwner::capabilities() const {
    if (!on_owner_thread()) {
        throw std::logic_error("Tango native clear capabilities are owner-thread only");
    }
    return capabilities_;
}

TangoCellClearProgram TangoCellClearOwner::describe_clear(
    std::uint32_t columns, std::uint32_t rows) const {
    if (!on_owner_thread()) {
        throw std::logic_error("Tango native clear description is owner-thread only");
    }
    if (columns == 0 || rows == 0 || columns > max_columns_ ||
        rows > max_rows_ || static_cast<std::uint64_t>(columns) * rows > kMaxCells) {
        throw std::invalid_argument("Tango clear extent exceeds its qualified cell domain");
    }
    TangoCellClearProgram program;
    program.requirement.effect = {kEffectId, {1, 0, 0}, kSemantic};
    program.requirement.target = capabilities_.target;
    program.requirement.input = EffectInput::Geometry;
    program.requirement.compositing = CompositeMode::Replace;
    program.requirement.input_surface = cell_surface();
    program.requirement.output_surface = cell_surface();
    program.requirement.expanded_width = columns;
    program.requirement.expanded_height = rows;
    program.requirement.source = {"tango-native", "tango-cell-clear", 0, 0};

    auto& manifest = program.manifest;
    manifest.effect = program.requirement.effect;
    manifest.variant_id = kVariantId;
    manifest.backend = capabilities_.backend;
    manifest.target = capabilities_.target;
    manifest.implementation_hash = kImplementation;
    manifest.parameter_abi_hash = kAbi;
    manifest.input = program.requirement.input;
    manifest.compositing = program.requirement.compositing;
    manifest.input_surface = cell_surface();
    manifest.output_surface = cell_surface();
    ProgramArtifact artifact;
    artifact.id = "tango-clear";
    artifact.kind = ArtifactKind::NativeBuiltin;
    artifact.stage = ShaderStage::None;
    artifact.target_profile = kTargetProfile;
    artifact.producer = "tango";
    artifact.builtin_binding = kBindingId;
    artifact.content_hash = kBinding;
    artifact.interface_abi_hash = kAbi;
    manifest.artifacts = {artifact};

    // The entire native storage includes the tui_buffer_t allocation and
    // its separate tui_cell_t array; there is no alias or terminal borrowing.
    manifest.resources = {{
        1, ResourceRole::Output, cell_surface(),
        columns, rows, buffer_bytes(columns, rows)}};
    EffectPass pass;
    pass.id = 1;
    pass.kind = PassKind::NativeBuiltin;
    pass.artifacts = {"tango-clear"};
    pass.resources = {{1, ResourceAccess::NativeWrite}};
    manifest.passes = {std::move(pass)};
    return program;
}

std::unique_ptr<NativeEffectResources> TangoCellClearOwner::prepare_resources(
    const ValidatedEffectContract& contract) {
    if (!on_owner_thread()) {
        throw std::logic_error("Tango buffer resources must be prepared on their owner thread");
    }
    if (capabilities_.status != DeviceStatus::Ready ||
        contract.scope() != scope_ ||
        contract.capability_revision() != capabilities_.capability_revision) {
        throw std::logic_error("Tango clear owner has a lost or stale device generation");
    }
    const auto& m = contract.manifest();
    if (m.resources.size() != 1 || m.passes.size() != 1 ||
        m.artifacts.size() != 1 || !m.parameters.empty() ||
        m.resources.front().role != ResourceRole::Output) {
        throw std::invalid_argument("Tango clear requires exactly one whole native output");
    }
    const auto columns = m.resources.front().width;
    const auto rows = m.resources.front().height;
    const auto expected = describe_clear(columns, rows);
    const auto& artifact = m.artifacts.front();
    const auto& pass = m.passes.front();
    const auto& out = m.resources.front();
    const auto& requirement = contract.requirement();
    if (contract.effect().id != kEffectId ||
        contract.effect().version.major != 1 ||
        contract.effect().version.minor != 0 ||
        contract.effect().version.patch != 0 ||
        contract.effect().contract_hash != kSemantic ||
        contract.variant_id() != kVariantId ||
        contract.implementation_hash() != kImplementation ||
        contract.parameter_abi_hash() != kAbi ||
        contract.backend().api != BackendApi::Tango ||
        contract.backend().profile != kBackendProfile ||
        requirement.input != EffectInput::Geometry ||
        requirement.compositing != CompositeMode::Replace ||
        !same_surface(requirement.output_surface, cell_surface()) ||
        requirement.expanded_width != columns ||
        requirement.expanded_height != rows ||
        m.input != EffectInput::Geometry ||
        m.compositing != CompositeMode::Replace ||
        m.target.contract_hash != expected.manifest.target.contract_hash ||
        artifact.kind != ArtifactKind::NativeBuiltin ||
        artifact.stage != ShaderStage::None ||
        artifact.id != "tango-clear" ||
        artifact.builtin_binding != kBindingId ||
        artifact.content_hash != kBinding ||
        artifact.interface_abi_hash != kAbi ||
        artifact.target_profile != kTargetProfile ||
        pass.id != 1 || pass.kind != PassKind::NativeBuiltin ||
        pass.artifacts.size() != 1 || pass.artifacts.front() != "tango-clear" ||
        pass.resources.size() != 1 || pass.resources.front().resource_id != 1 ||
        pass.resources.front().access != ResourceAccess::NativeWrite ||
        out.id != 1 || !same_surface(out.surface, cell_surface()) ||
        out.byte_count != buffer_bytes(columns, rows) ||
        contract.cost().bytes != out.byte_count ||
        contract.cost().cells != static_cast<std::uint64_t>(columns) * rows ||
        contract.cost().pixels != 0 || contract.cost().allocations != 2) {
        throw std::invalid_argument("Tango owner rejected a different native clear implementation");
    }

    std::unique_ptr<tui_buffer_t, decltype(&tui_buffer_destroy)> candidate(
        tui_buffer_create(static_cast<int>(columns), static_cast<int>(rows)),
        &tui_buffer_destroy);
    if (!candidate) throw std::bad_alloc();
    // Real terminal-cell operation; an actual C11 Tango buffer is cleared.
    tui_buffer_clear(candidate.get());
    NativeEffectResourceIdentity identity{
        contract.scope(), contract.backend(), contract.target(),
        contract.variant_id(), contract.implementation_hash(),
        contract.parameter_abi_hash(), contract.cost()};
    return std::unique_ptr<NativeEffectResources>(
        new TangoCellClearResources(std::move(identity), candidate.release(), owning_thread_));
}

void TangoCellClearOwner::mark_device_lost(SharedResourceBudget& budget) {
    if (!on_owner_thread()) {
        throw std::logic_error("Tango device loss is owner-thread only");
    }
    if (budget.snapshot().scope != scope_) {
        throw std::invalid_argument("Tango device loss cannot mutate another resource ledger");
    }
    capabilities_.status = DeviceStatus::Lost;
    ++capabilities_.capability_revision;
    budget.mark_device_lost();
}

} // namespace flex::tui_backend
