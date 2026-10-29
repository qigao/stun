#include <flex/render/effect_admission.h>
#include <flex/render/effect_resource_owner.h>

#include <tinytest.hpp>

#include <array>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

namespace fx = flex::effects;

static_assert(!std::is_copy_constructible_v<fx::ValidatedEffectContract>);
static_assert(!std::is_move_constructible_v<fx::ValidatedEffectContract>);
static_assert(!std::is_copy_assignable_v<fx::ValidatedEffectContract>);
static_assert(!std::is_move_assignable_v<fx::ValidatedEffectContract>);
static_assert(!std::is_default_constructible_v<fx::ValidatedEffectContract>);
static_assert(!std::is_copy_constructible_v<fx::EffectReservation>);
static_assert(!std::is_copy_assignable_v<fx::EffectReservation>);
static_assert(std::is_nothrow_move_constructible_v<fx::EffectReservation>);
static_assert(std::is_nothrow_move_assignable_v<fx::EffectReservation>);

fx::Hash256 effect_test_hash(std::uint8_t value) {
    fx::Hash256 result;
    result.bytes.fill(value);
    return result;
}

bool effect_cost_equal(const fx::ResourceCost& a, const fx::ResourceCost& b) {
    return a.bytes == b.bytes && a.pixels == b.pixels &&
           a.cells == b.cells && a.allocations == b.allocations;
}

fx::ResourceCost effect_cost_times(const fx::ResourceCost& cost, std::uint64_t count) {
    return {cost.bytes * count, cost.pixels * count,
            cost.cells * count, cost.allocations * count};
}

bool effect_validation_rejected(const fx::EffectValidationResult& result) {
    return !result.contract && result.diagnostic.code != fx::EffectError::None &&
           !result.diagnostic.message.empty();
}

bool effect_validation_error(const fx::EffectValidationResult& result,
                             fx::EffectError error) {
    return effect_validation_rejected(result) && result.diagnostic.code == error;
}

bool effect_preparation_error(const fx::EffectPreparationResult& result,
                              fx::EffectError error) {
    return !result.reservation && result.diagnostic.code == error &&
           !result.diagnostic.message.empty();
}

struct EffectFixture {
    fx::EffectRequirement requirement;
    fx::BackendProgramManifest manifest;
    fx::BackendCapabilitySnapshot capabilities;
};

// These are synthetic qualified domains and trusted-loader observations.
// Passing a fixture does not qualify any production renderer or shader binary.
void effect_test_install_observations(EffectFixture& fixture) {
    fixture.capabilities.installed.clear();
    for (const auto& artifact : fixture.manifest.artifacts) {
        fx::InstalledArtifact installed;
        installed.variant_id = fixture.manifest.variant_id;
        installed.artifact_id = artifact.id;
        installed.kind = artifact.kind;
        installed.stage = artifact.stage;
        installed.target_profile = artifact.target_profile;
        installed.entry_point = artifact.entry_point;
        installed.observed_content_hash = artifact.content_hash;
        installed.observed_interface_abi_hash = artifact.interface_abi_hash;
        installed.observed_byte_count = artifact.byte_count;
        installed.builtin_binding = artifact.builtin_binding;
        fixture.capabilities.installed.push_back(std::move(installed));
    }
}

void effect_test_qualify_fixture(EffectFixture& fixture,
                                const std::vector<fx::ResourceAccess>& access) {
    auto& caps = fixture.capabilities;
    caps.backend = fixture.manifest.backend;
    caps.target = fixture.manifest.target;
    caps.scope = {11, 22, 3};
    caps.capability_revision = 7;
    caps.status = fx::DeviceStatus::Ready;
    fx::EffectSupport support;
    support.effect = fixture.manifest.effect;
    support.variant_id = fixture.manifest.variant_id;
    support.implementation_hash = fixture.manifest.implementation_hash;
    support.parameter_abi_hash = fixture.manifest.parameter_abi_hash;
    support.input = fixture.manifest.input;
    support.compositing = fixture.manifest.compositing;
    support.parameters = fixture.manifest.parameters;
    support.surfaces = {fixture.manifest.input_surface};
    for (const auto& artifact : fixture.manifest.artifacts) {
        support.stages.push_back({artifact.kind, artifact.stage, artifact.target_profile});
    }
    for (const auto usage : access) {
        support.access.push_back({fixture.manifest.input_surface, usage});
    }
    support.execution.max_width = 128;
    support.execution.max_height = 128;
    support.execution.max_resource_uses_per_pass = 8;
    support.execution.max_dispatch = {8, 8, 8};
    support.execution.max_workgroup = {8, 8, 8};
    support.execution.max_workgroup_invocations = 64;
    support.execution.max_shared_memory_bytes = 128;
    caps.effects = {support};
    caps.device.surfaces = support.surfaces;
    caps.device.stages = support.stages;
    caps.device.access = support.access;
    caps.device.execution = support.execution;
    effect_test_install_observations(fixture);
}

EffectFixture effect_graphics_fixture(fx::BackendApi api = fx::BackendApi::OpenGL) {
    EffectFixture fixture;
    auto& requirement = fixture.requirement;
    requirement.effect = {"test.blur", {1, 0, 0}, effect_test_hash(1)};
    requirement.target = {fx::TargetKind::NativePixel, fx::Unit::DevicePixel,
                          fx::Unit::DevicePixel, effect_test_hash(2)};
    requirement.input = fx::EffectInput::ElementContent;
    requirement.compositing = fx::CompositeMode::SourceOver;
    requirement.input_surface =
        {fx::SurfaceFormat::Rgba8Unorm, fx::ColorSpace::SRgb, fx::AlphaMode::Premultiplied};
    requirement.output_surface = requirement.input_surface;
    requirement.expanded_width = 4;
    requirement.expanded_height = 3;
    requirement.parameters = {{1, fx::Unit::DevicePixel,
                               fx::ParameterBinding::Uniform, 0.0, 4.0}};
    requirement.source = {"fixture.ui.xml", "blur-[4px]", 37, 10};
    auto& manifest = fixture.manifest;
    manifest.effect = requirement.effect;
    manifest.variant_id = "test.variant";
    manifest.backend = {api, "fixture-profile"};
    manifest.target = requirement.target;
    manifest.implementation_hash = effect_test_hash(3);
    manifest.parameter_abi_hash = effect_test_hash(4);
    manifest.input = requirement.input;
    manifest.compositing = requirement.compositing;
    manifest.input_surface = requirement.input_surface;
    manifest.output_surface = requirement.output_surface;
    manifest.parameters = {{1, fx::Unit::DevicePixel,
                            fx::ParameterBinding::Uniform, 0.0, 8.0}};
    fx::ArtifactKind kind = fx::ArtifactKind::GlslSource;
    std::string shader_profile = "glsl410-core";
    if (api == fx::BackendApi::OpenGLES) {
        kind = fx::ArtifactKind::EsslSource;
        shader_profile = "essl300-es";
    } else if (api == fx::BackendApi::Vulkan) {
        kind = fx::ArtifactKind::SpirV;
        shader_profile = "spirv10";
    }
    for (unsigned index = 0; index != 2; ++index) {
        fx::ProgramArtifact artifact;
        artifact.id = index == 0 ? "vertex" : "fragment";
        artifact.kind = kind;
        artifact.stage = index == 0 ? fx::ShaderStage::Vertex : fx::ShaderStage::Fragment;
        artifact.entry_point = "main";
        artifact.target_profile = shader_profile;
        artifact.producer = "test-fixture";
        artifact.content_hash = effect_test_hash(static_cast<std::uint8_t>(10 + index));
        artifact.interface_abi_hash = manifest.parameter_abi_hash;
        artifact.byte_count = index == 0 ? 16 : 24;
        manifest.artifacts.push_back(std::move(artifact));
    }
    manifest.resources = {
        {1, fx::ResourceRole::ExternalInput, requirement.input_surface, 4, 3, 48},
        {2, fx::ResourceRole::Output, requirement.output_surface, 4, 3, 48},
    };
    fx::EffectPass pass;
    pass.id = 1;
    pass.kind = fx::PassKind::Graphics;
    pass.artifacts = {"vertex", "fragment"};
    pass.resources = {{1, fx::ResourceAccess::SampledRead},
                      {2, fx::ResourceAccess::ColorAttachmentWrite}};
    manifest.passes = {pass};
    effect_test_qualify_fixture(
        fixture, {fx::ResourceAccess::SampledRead, fx::ResourceAccess::ColorAttachmentWrite});
    return fixture;
}

EffectFixture effect_builtin_fixture(fx::BackendApi api) {
    auto fixture = effect_graphics_fixture();
    auto& requirement = fixture.requirement;
    auto& manifest = fixture.manifest;
    manifest.backend = {api, "fixture-native"};
    if (api == fx::BackendApi::Tango) {
        requirement.target = {fx::TargetKind::TerminalCell, fx::Unit::CellColumn,
                              fx::Unit::CellRow, effect_test_hash(5)};
        requirement.input = fx::EffectInput::Cells;
        requirement.compositing = fx::CompositeMode::Replace;
        requirement.input_surface = {fx::SurfaceFormat::TerminalCells,
                                     fx::ColorSpace::TerminalTrueColor, fx::AlphaMode::Opaque};
        requirement.output_surface = requirement.input_surface;
        manifest.target = requirement.target;
        manifest.input = requirement.input;
        manifest.compositing = requirement.compositing;
        manifest.input_surface = requirement.input_surface;
        manifest.output_surface = requirement.output_surface;
        for (auto& resource : manifest.resources) {
            resource.surface = requirement.output_surface;
            resource.byte_count = 12;  // Fixture owner declares one byte per cell.
        }
    }
    requirement.parameters.clear();
    manifest.parameters.clear();
    fx::ProgramArtifact artifact;
    artifact.id = "native";
    artifact.kind = fx::ArtifactKind::NativeBuiltin;
    artifact.stage = fx::ShaderStage::None;
    artifact.target_profile = "native-v1";
    artifact.producer = "test-fixture";
    artifact.builtin_binding = "test.native.binding";
    artifact.content_hash = effect_test_hash(12);  // Binding identity, not shader bytes.
    artifact.interface_abi_hash = manifest.parameter_abi_hash;
    manifest.artifacts = {artifact};
    manifest.passes.front().kind = fx::PassKind::NativeBuiltin;
    manifest.passes.front().artifacts = {"native"};
    manifest.passes.front().resources = {
        {1, fx::ResourceAccess::NativeRead}, {2, fx::ResourceAccess::NativeWrite}};
    effect_test_qualify_fixture(
        fixture, {fx::ResourceAccess::NativeRead, fx::ResourceAccess::NativeWrite});
    return fixture;
}

EffectFixture effect_d2d_pixel_fixture() {
    auto fixture = effect_builtin_fixture(fx::BackendApi::Direct2D);
    auto& artifact = fixture.manifest.artifacts.front();
    artifact.id = "pixel-effect";
    artifact.kind = fx::ArtifactKind::D2DBytecode;
    artifact.stage = fx::ShaderStage::Fragment;
    artifact.target_profile = "ps_4_0";
    artifact.entry_point = "main";
    artifact.builtin_binding.clear();
    artifact.byte_count = 32;
    fixture.manifest.passes.front().kind = fx::PassKind::D2DPixelEffect;
    fixture.manifest.passes.front().artifacts = {artifact.id};
    effect_test_qualify_fixture(
        fixture, {fx::ResourceAccess::NativeRead, fx::ResourceAccess::NativeWrite});
    return fixture;
}

EffectFixture effect_compute_fixture() {
    auto fixture = effect_graphics_fixture(fx::BackendApi::Vulkan);
    auto artifact = fixture.manifest.artifacts.front();
    artifact.id = "compute";
    artifact.stage = fx::ShaderStage::Compute;
    artifact.byte_count = 32;
    fixture.manifest.artifacts = {artifact};
    auto& pass = fixture.manifest.passes.front();
    pass.kind = fx::PassKind::Compute;
    pass.artifacts = {"compute"};
    pass.resources = {{1, fx::ResourceAccess::StorageRead},
                      {2, fx::ResourceAccess::StorageWrite}};
    pass.dispatch = {2, 3, 1};
    pass.workgroup = {4, 4, 1};
    pass.shared_memory_bytes = 64;
    effect_test_qualify_fixture(
        fixture, {fx::ResourceAccess::StorageRead, fx::ResourceAccess::StorageWrite});
    return fixture;
}

EffectFixture effect_two_pass_fixture() {
    auto fixture = effect_graphics_fixture();
    fixture.manifest.resources[1].role = fx::ResourceRole::Intermediate;
    fixture.manifest.resources.push_back(
        {3, fx::ResourceRole::Output, fixture.requirement.output_surface, 4, 3, 48});
    auto pass = fixture.manifest.passes.front();
    pass.id = 2;
    pass.resources = {{2, fx::ResourceAccess::SampledRead},
                      {3, fx::ResourceAccess::ColorAttachmentWrite}};
    pass.dependencies = {1};
    fixture.manifest.passes.push_back(pass);
    return fixture;
}

fx::EffectValidationResult effect_validate(const EffectFixture& fixture,
                                           const fx::EffectValidationLimits& limits = {}) {
    return fx::validate_effect_contract(
        fixture.requirement, fixture.manifest, fixture.capabilities, limits);
}

fx::SharedResourceBudget effect_test_budget(const fx::ValidatedEffectContract& contract,
                                           std::uint64_t copies = 1,
                                           std::size_t reservations = 64) {
    return {contract.scope(), {effect_cost_times(contract.cost(), copies), reservations}};
}

}  // namespace

spec("Effect metadata admission keeps backend profiles and semantic contracts distinct") {
    it("validates each GPU API's own vertex and fragment artifacts") {
        for (auto api : {fx::BackendApi::OpenGL, fx::BackendApi::OpenGLES,
                         fx::BackendApi::Vulkan}) {
            const auto fixture = effect_graphics_fixture(api);
            const auto result = effect_validate(fixture);
            check(result.contract != nullptr);
            if (!result.contract) continue;
            check(result.contract->backend().api == api);
            check(effect_cost_equal(result.contract->cost(), {136, 24, 0, 4}));
        }
    }

    it("admits native bindings without pretending they contain shaders") {
        const auto d2d = effect_validate(effect_builtin_fixture(fx::BackendApi::Direct2D));
        const auto tango = effect_validate(effect_builtin_fixture(fx::BackendApi::Tango));
        check(d2d.contract != nullptr);
        check(tango.contract != nullptr);
        if (d2d.contract) check(effect_cost_equal(d2d.contract->cost(), {96, 24, 0, 3}));
        if (tango.contract) check(effect_cost_equal(tango.contract->cost(), {24, 0, 24, 3}));
    }

    it("admits a D2D fragment effect through its native pass rather than fake vertices") {
        const auto result = effect_validate(effect_d2d_pixel_fixture());
        check(result.contract != nullptr);
        if (result.contract) check(effect_cost_equal(result.contract->cost(), {128, 24, 0, 3}));
    }

    it("admits an explicitly qualified compute path and a declared two-pass graph") {
        const auto compute = effect_validate(effect_compute_fixture());
        const auto graph = effect_validate(effect_two_pass_fixture());
        check(compute.contract != nullptr);
        check(graph.contract != nullptr);
        if (compute.contract) check(effect_cost_equal(compute.contract->cost(), {128, 24, 0, 3}));
        if (graph.contract) check(effect_cost_equal(graph.contract->cost(), {184, 36, 0, 5}));
    }

    it("rejects cross-API artifact kinds even when loader observations agree") {
        for (auto api : {fx::BackendApi::OpenGL, fx::BackendApi::OpenGLES,
                         fx::BackendApi::Vulkan}) {
            auto fixture = effect_graphics_fixture(api);
            const auto wrong = api == fx::BackendApi::OpenGL
                                   ? fx::ArtifactKind::EsslSource
                                   : fx::ArtifactKind::GlslSource;
            for (auto& artifact : fixture.manifest.artifacts) artifact.kind = wrong;
            for (auto& stage : fixture.capabilities.effects.front().stages) stage.kind = wrong;
            for (auto& stage : fixture.capabilities.device.stages) stage.kind = wrong;
            effect_test_install_observations(fixture);
            check(effect_validation_rejected(effect_validate(fixture)));
        }
    }

    it("rejects shader-bearing Tango and generic D2D graphics passes") {
        auto tango = effect_builtin_fixture(fx::BackendApi::Tango);
        tango.manifest.artifacts.front().kind = fx::ArtifactKind::SpirV;
        tango.manifest.artifacts.front().stage = fx::ShaderStage::Compute;
        tango.manifest.artifacts.front().entry_point = "main";
        tango.manifest.artifacts.front().builtin_binding.clear();
        tango.manifest.artifacts.front().byte_count = 16;
        tango.manifest.passes.front().kind = fx::PassKind::Compute;
        effect_test_qualify_fixture(
            tango, {fx::ResourceAccess::NativeRead, fx::ResourceAccess::NativeWrite});
        check(effect_validation_rejected(effect_validate(tango)));

        auto d2d = effect_graphics_fixture(fx::BackendApi::Direct2D);
        for (auto& artifact : d2d.manifest.artifacts) artifact.kind = fx::ArtifactKind::D2DBytecode;
        effect_test_qualify_fixture(
            d2d, {fx::ResourceAccess::SampledRead, fx::ResourceAccess::ColorAttachmentWrite});
        check(effect_validation_rejected(effect_validate(d2d)));
    }

    it("rejects transparent terminal surfaces even when both domains claim support") {
        auto fixture = effect_builtin_fixture(fx::BackendApi::Tango);
        fixture.requirement.input_surface.alpha = fx::AlphaMode::Straight;
        fixture.requirement.output_surface.alpha = fx::AlphaMode::Straight;
        fixture.manifest.input_surface = fixture.requirement.input_surface;
        fixture.manifest.output_surface = fixture.requirement.output_surface;
        for (auto& resource : fixture.manifest.resources) {
            resource.surface.alpha = fx::AlphaMode::Straight;
        }
        effect_test_qualify_fixture(
            fixture, {fx::ResourceAccess::NativeRead, fx::ResourceAccess::NativeWrite});
        check(effect_validation_rejected(effect_validate(fixture)));
    }

    it("rejects GPU access and vertex stages in the D2D pixel-effect contract") {
        auto access = effect_d2d_pixel_fixture();
        access.manifest.passes.front().resources.front().access = fx::ResourceAccess::SampledRead;
        access.capabilities.effects.front().access.push_back(
            {access.manifest.input_surface, fx::ResourceAccess::SampledRead});
        access.capabilities.device.access.push_back(
            {access.manifest.input_surface, fx::ResourceAccess::SampledRead});
        check(effect_validation_rejected(effect_validate(access)));
        auto vertex = effect_d2d_pixel_fixture();
        vertex.manifest.artifacts.front().stage = fx::ShaderStage::Vertex;
        vertex.capabilities.effects.front().stages.front().stage = fx::ShaderStage::Vertex;
        vertex.capabilities.device.stages.front().stage = fx::ShaderStage::Vertex;
        effect_test_install_observations(vertex);
        check(effect_validation_rejected(effect_validate(vertex)));
        auto compute = effect_d2d_pixel_fixture();
        compute.manifest.passes.front().kind = fx::PassKind::Compute;
        compute.manifest.artifacts.front().stage = fx::ShaderStage::Compute;
        effect_test_qualify_fixture(
            compute, {fx::ResourceAccess::NativeRead, fx::ResourceAccess::NativeWrite});
        check(effect_validation_rejected(effect_validate(compute)));
    }

    it("intersects adapter and device stages independently") {
        for (bool remove_adapter : {false, true}) {
            auto fixture = effect_compute_fixture();
            if (remove_adapter) fixture.capabilities.effects.front().stages.clear();
            else fixture.capabilities.device.stages.clear();
            check(effect_validation_error(effect_validate(fixture), fx::EffectError::UnsupportedStage));
        }
    }

    it("intersects surface tuples and resource access on both sides") {
        for (bool change_adapter : {false, true}) {
            auto surface = effect_graphics_fixture();
            auto& formats = change_adapter ? surface.capabilities.effects.front().surfaces
                                           : surface.capabilities.device.surfaces;
            formats = {
                {fx::SurfaceFormat::Rgba8Unorm, fx::ColorSpace::LinearSRgb,
                 fx::AlphaMode::Premultiplied},
                {fx::SurfaceFormat::Rgba16Float, fx::ColorSpace::SRgb,
                 fx::AlphaMode::Straight},
            };
            auto& format_access = change_adapter ? surface.capabilities.effects.front().access
                                                  : surface.capabilities.device.access;
            format_access = {
                {formats[0], fx::ResourceAccess::SampledRead},
                {formats[0], fx::ResourceAccess::ColorAttachmentWrite},
                {formats[1], fx::ResourceAccess::SampledRead},
                {formats[1], fx::ResourceAccess::ColorAttachmentWrite},
            };
            check(effect_validation_error(effect_validate(surface), fx::EffectError::UnsupportedFormat));
            auto access = effect_graphics_fixture();
            auto& allowed = change_adapter ? access.capabilities.effects.front().access
                                           : access.capabilities.device.access;
            allowed = {{access.manifest.input_surface, fx::ResourceAccess::SampledRead}};
            check(effect_validation_error(effect_validate(access),
                                          fx::EffectError::UnsupportedResourceAccess));
        }
    }

    it("does not combine one format's read support with another format's write support") {
        for (bool restrict_adapter : {false, true}) {
            auto fixture = effect_compute_fixture();
            const auto input = fixture.requirement.input_surface;
            const fx::SurfaceContract output = {
                fx::SurfaceFormat::Rgba16Float, fx::ColorSpace::LinearSRgb,
                fx::AlphaMode::Premultiplied};
            fixture.requirement.output_surface = output;
            fixture.manifest.output_surface = output;
            fixture.manifest.resources.back().surface = output;
            fixture.manifest.resources.back().byte_count = 96;
            fixture.manifest.passes.front().resources.front().access =
                fx::ResourceAccess::SampledRead;
            fixture.capabilities.effects.front().surfaces = {input, output};
            fixture.capabilities.device.surfaces = {input, output};
            const std::vector<fx::SurfaceAccessSupport> tuples = {
                {input, fx::ResourceAccess::SampledRead},
                {output, fx::ResourceAccess::StorageWrite},
            };
            fixture.capabilities.effects.front().access = tuples;
            fixture.capabilities.device.access = tuples;
            const auto valid = effect_validate(fixture);
            check(valid.contract != nullptr);
            if (valid.contract) {
                check(effect_cost_equal(valid.contract->cost(), {176, 24, 0, 3}));
            }

            // Keep the valid format and access separately present, but never
            // qualify their unsupported combination on the restricted side.
            fixture.requirement.output_surface = input;
            fixture.manifest.output_surface = input;
            fixture.manifest.resources.back().surface = input;
            fixture.manifest.resources.back().byte_count = 48;
            auto& unrestricted = restrict_adapter ? fixture.capabilities.device.access
                                                  : fixture.capabilities.effects.front().access;
            unrestricted.push_back({input, fx::ResourceAccess::StorageWrite});
            check(effect_validation_error(effect_validate(fixture),
                                          fx::EffectError::UnsupportedResourceAccess));
        }
    }

    it("rejects unknown profiles and explicit target or unit mismatches") {
        auto unknown = effect_graphics_fixture();
        unknown.manifest.backend.api = static_cast<fx::BackendApi>(255);
        unknown.capabilities.backend = unknown.manifest.backend;
        check(effect_validation_rejected(effect_validate(unknown)));
        auto target = effect_graphics_fixture();
        target.requirement.target.contract_hash = effect_test_hash(99);
        check(effect_validation_rejected(effect_validate(target)));
        auto unit = effect_builtin_fixture(fx::BackendApi::Tango);
        unit.requirement.target.x_unit = fx::Unit::DevicePixel;
        check(effect_validation_rejected(effect_validate(unit)));
    }

    it("keeps ordinary blur, backdrop input and isolated composition distinct") {
        auto input = effect_graphics_fixture();
        input.requirement.input = fx::EffectInput::Backdrop;
        check(effect_validation_rejected(effect_validate(input)));
        auto compositing = effect_graphics_fixture();
        compositing.requirement.compositing = fx::CompositeMode::IsolatedSubtree;
        check(effect_validation_rejected(effect_validate(compositing)));
        auto version = effect_graphics_fixture();
        ++version.requirement.effect.version.minor;
        check(effect_validation_rejected(effect_validate(version)));
        auto schema = effect_graphics_fixture();
        schema.requirement.effect.contract_hash = effect_test_hash(99);
        check(effect_validation_rejected(effect_validate(schema)));
    }

    it("requires one unambiguous installed artifact and exact identity observations") {
        auto missing = effect_graphics_fixture();
        missing.capabilities.installed.pop_back();
        check(effect_validation_error(effect_validate(missing), fx::EffectError::MissingProgramVariant));
        auto duplicate = effect_graphics_fixture();
        duplicate.capabilities.installed.push_back(duplicate.capabilities.installed.front());
        check(effect_validation_rejected(effect_validate(duplicate)));
        auto hash = effect_graphics_fixture();
        hash.capabilities.installed.front().observed_content_hash = effect_test_hash(99);
        check(effect_validation_error(effect_validate(hash), fx::EffectError::ArtifactHashMismatch));
        auto abi = effect_graphics_fixture();
        abi.capabilities.installed.front().observed_interface_abi_hash = effect_test_hash(99);
        check(effect_validation_error(effect_validate(abi), fx::EffectError::ProgramAbiMismatch));
    }

    it("rejects installed kind stage profile entry and byte-count drift") {
        for (unsigned change = 0; change != 5; ++change) {
            auto fixture = effect_graphics_fixture();
            auto& observed = fixture.capabilities.installed.front();
            if (change == 0) observed.stage = fx::ShaderStage::Compute;
            if (change == 1) observed.target_profile = "another-profile";
            if (change == 2) observed.entry_point = "another-entry";
            if (change == 3) ++observed.observed_byte_count;
            if (change == 4) observed.kind = fx::ArtifactKind::EsslSource;
            check(effect_validation_rejected(effect_validate(fixture)));
        }
    }

    it("requires the selected variant and its implementation and parameter ABI") {
        auto missing = effect_graphics_fixture();
        missing.capabilities.effects.front().variant_id = "another.variant";
        check(effect_validation_rejected(effect_validate(missing)));
        auto duplicate = effect_graphics_fixture();
        duplicate.capabilities.effects.push_back(duplicate.capabilities.effects.front());
        check(effect_validation_rejected(effect_validate(duplicate)));
        auto implementation = effect_graphics_fixture();
        implementation.capabilities.effects.front().implementation_hash = effect_test_hash(99);
        check(effect_validation_rejected(effect_validate(implementation)));
        auto abi = effect_graphics_fixture();
        abi.capabilities.effects.front().parameter_abi_hash = effect_test_hash(99);
        check(effect_validation_error(effect_validate(abi), fx::EffectError::ProgramAbiMismatch));
    }

    it("checks the complete reachable parameter domain and binding category") {
        auto boundary = effect_graphics_fixture();
        boundary.requirement.parameters.front().upper = 8.0;
        check(effect_validate(boundary).contract != nullptr);
        boundary.requirement.parameters.front().upper = 8.01;
        check(effect_validation_error(effect_validate(boundary),
                                      fx::EffectError::UnsupportedParameterDomain));
        auto adapter = effect_graphics_fixture();
        adapter.capabilities.effects.front().parameters.front().upper = 3.0;
        check(effect_validation_error(effect_validate(adapter),
                                      fx::EffectError::UnsupportedParameterDomain));
        auto binding = effect_graphics_fixture();
        binding.requirement.parameters.front().binding = fx::ParameterBinding::Structural;
        check(effect_validation_rejected(effect_validate(binding)));
        auto units = effect_graphics_fixture();
        units.requirement.parameters.front().unit = fx::Unit::LogicalPixel;
        check(effect_validation_rejected(effect_validate(units)));
    }

    it("rejects nonfinite reversed and duplicate parameter ranges") {
        for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(), -1.0}) {
            auto fixture = effect_graphics_fixture();
            fixture.requirement.parameters.front().upper = invalid;
            check(effect_validation_rejected(effect_validate(fixture)));
        }
        auto duplicate = effect_graphics_fixture();
        duplicate.requirement.parameters.push_back(duplicate.requirement.parameters.front());
        check(effect_validation_rejected(effect_validate(duplicate)));
    }

    it("admits only one selected structural or specialization value per manifest") {
        for (const auto binding : {fx::ParameterBinding::Structural,
                                   fx::ParameterBinding::Specialization}) {
            auto fixture = effect_graphics_fixture();
            fixture.requirement.parameters.front().binding = binding;
            fixture.requirement.parameters.front().upper = 1.0;
            fixture.manifest.parameters.front().binding = binding;
            fixture.manifest.parameters.front().upper = 1.0;
            fixture.capabilities.effects.front().parameters.front().binding = binding;
            check(effect_validation_error(effect_validate(fixture),
                                          fx::EffectError::UnsupportedParameterDomain));

            // Even a singleton request cannot make a changing implementation
            // specialization or graph structure into one concrete variant.
            fixture.requirement.parameters.front().upper = 0.0;
            check(effect_validation_error(effect_validate(fixture),
                                          fx::EffectError::UnsupportedParameterDomain));
            fixture.manifest.parameters.front().upper = 0.0;
            // The adapter may support a wider family of concrete variants.
            check(effect_validate(fixture).contract != nullptr);
            fixture.requirement.parameters.front().upper = 1.0;
            check(effect_validation_error(effect_validate(fixture),
                                          fx::EffectError::UnsupportedParameterDomain));
        }
    }

    it("requires resources to cover the expanded output and format storage") {
        auto expanded = effect_graphics_fixture();
        expanded.requirement.expanded_width = 5;
        check(effect_validation_rejected(effect_validate(expanded)));
        auto empty = effect_graphics_fixture();
        empty.requirement.expanded_height = 0;
        check(effect_validation_rejected(effect_validate(empty)));
        auto bytes = effect_graphics_fixture();
        bytes.manifest.resources.back().byte_count = 47;
        check(effect_validation_rejected(effect_validate(bytes)));
        auto zero_resource = effect_graphics_fixture();
        zero_resource.manifest.resources.back().width = 0;
        check(effect_validation_rejected(effect_validate(zero_resource)));
    }

    it("checks adapter and device extent and compute execution limits independently") {
        for (bool change_adapter : {false, true}) {
            auto extent = effect_graphics_fixture();
            auto& dimensions = change_adapter ? extent.capabilities.effects.front().execution
                                              : extent.capabilities.device.execution;
            dimensions.max_width = 3;
            check(effect_validation_rejected(effect_validate(extent)));
            for (unsigned change = 0; change != 4; ++change) {
                auto compute = effect_compute_fixture();
                auto& limits = change_adapter ? compute.capabilities.effects.front().execution
                                              : compute.capabilities.device.execution;
                if (change == 0) limits.max_dispatch.y = 2;
                if (change == 1) limits.max_workgroup.x = 3;
                if (change == 2) limits.max_workgroup_invocations = 15;
                if (change == 3) limits.max_shared_memory_bytes = 63;
                check(effect_validation_rejected(effect_validate(compute)));
            }
        }
    }

    it("rejects zero compute dimensions and overflowing workgroup products") {
        auto zero = effect_compute_fixture();
        zero.manifest.passes.front().dispatch.x = 0;
        check(effect_validation_rejected(effect_validate(zero)));
        auto overflow = effect_compute_fixture();
        const auto maximum = (std::numeric_limits<std::uint32_t>::max)();
        overflow.manifest.passes.front().workgroup = {maximum, maximum, maximum};
        overflow.capabilities.effects.front().execution.max_workgroup = {maximum, maximum, maximum};
        overflow.capabilities.device.execution.max_workgroup = {maximum, maximum, maximum};
        overflow.capabilities.effects.front().execution.max_workgroup_invocations =
            (std::numeric_limits<std::uint64_t>::max)();
        overflow.capabilities.device.execution.max_workgroup_invocations =
            (std::numeric_limits<std::uint64_t>::max)();
        check(effect_validation_rejected(effect_validate(overflow)));
    }
}

spec("Effect pass graphs and validation work are bounded") {
    it("requires external input for every non-geometry input contract") {
        for (const auto input : {fx::EffectInput::ElementContent, fx::EffectInput::Subtree,
                                 fx::EffectInput::Backdrop, fx::EffectInput::Texture,
                                 fx::EffectInput::Cells}) {
            auto fixture = input == fx::EffectInput::Cells
                ? effect_builtin_fixture(fx::BackendApi::Tango) : effect_graphics_fixture();
            fixture.requirement.input = input;
            fixture.manifest.input = input;
            fixture.capabilities.effects.front().input = input;
            fixture.manifest.resources.erase(fixture.manifest.resources.begin());
            auto& uses = fixture.manifest.passes.front().resources;
            uses.erase(uses.begin());
            check(effect_validation_error(effect_validate(fixture), fx::EffectError::InvalidPassGraph));
        }
    }

    it("permits an explicitly qualified geometry generator with no external input") {
        auto fixture = effect_graphics_fixture();
        fixture.requirement.effect.id = "test.geometry-generator";
        fixture.requirement.effect.contract_hash = effect_test_hash(25);
        fixture.manifest.effect = fixture.requirement.effect;
        fixture.capabilities.effects.front().effect = fixture.requirement.effect;
        fixture.requirement.input = fx::EffectInput::Geometry;
        fixture.manifest.input = fx::EffectInput::Geometry;
        fixture.capabilities.effects.front().input = fx::EffectInput::Geometry;
        fixture.manifest.resources.erase(fixture.manifest.resources.begin());
        fixture.manifest.passes.front().resources.erase(
            fixture.manifest.passes.front().resources.begin());
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (result.contract) check(effect_cost_equal(result.contract->cost(), {88, 12, 0, 3}));
    }

    it("accepts an initialized read through a transitive producer dependency") {
        auto fixture = effect_two_pass_fixture();
        fixture.manifest.resources.back().role = fx::ResourceRole::Intermediate;
        fixture.manifest.resources.push_back(
            {4, fx::ResourceRole::Output, fixture.requirement.output_surface, 4, 3, 48});
        auto pass = fixture.manifest.passes.back();
        pass.id = 3;
        pass.dependencies = {2};
        pass.resources = {{2, fx::ResourceAccess::SampledRead},
                          {3, fx::ResourceAccess::SampledRead},
                          {4, fx::ResourceAccess::ColorAttachmentWrite}};
        fixture.manifest.passes.push_back(pass);
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (result.contract) check(effect_cost_equal(result.contract->cost(), {232, 48, 0, 6}));
    }

    it("requires an earlier producer and an explicit dependency for intermediate reads") {
        auto no_dependency = effect_two_pass_fixture();
        no_dependency.manifest.passes.back().dependencies.clear();
        check(effect_validation_error(effect_validate(no_dependency), fx::EffectError::InvalidPassGraph));
        auto uninitialized = effect_graphics_fixture();
        uninitialized.manifest.resources.front().role = fx::ResourceRole::Intermediate;
        check(effect_validation_error(effect_validate(uninitialized), fx::EffectError::InvalidPassGraph));
    }

    it("rejects detached computation even when the output pass depends on its completion") {
        for (const bool order_after_branch : {false, true}) {
            auto fixture = effect_two_pass_fixture();
            fixture.manifest.resources.back().role = fx::ResourceRole::Intermediate;
            fixture.manifest.resources.push_back(
                {4, fx::ResourceRole::Output, fixture.requirement.output_surface, 4, 3, 48});
            auto pass = fixture.manifest.passes.back();
            pass.id = 3;
            pass.dependencies = order_after_branch ? std::vector<std::uint32_t>{2}
                                                    : std::vector<std::uint32_t>{};
            pass.resources = {{1, fx::ResourceAccess::SampledRead},
                              {4, fx::ResourceAccess::ColorAttachmentWrite}};
            fixture.manifest.passes.push_back(pass);
            // Resources 2 and 3 are produced and used in a valid ordered chain,
            // but neither contributes data to the final output resource 4.
            check(effect_validation_error(effect_validate(fixture), fx::EffectError::InvalidPassGraph));
        }
    }

    it("rejects external reads in dead branches and spare writes in live passes") {
        auto dead_input = effect_two_pass_fixture();
        dead_input.manifest.passes.back().resources = {
            {3, fx::ResourceAccess::ColorAttachmentWrite}};
        // Its declared dependency is the only connection to the input reader.
        check(effect_validation_error(effect_validate(dead_input), fx::EffectError::InvalidPassGraph));

        auto spare_write = effect_graphics_fixture();
        spare_write.manifest.resources.push_back(
            {3, fx::ResourceRole::Intermediate, spare_write.requirement.output_surface, 4, 3, 48});
        spare_write.manifest.passes.front().resources.push_back(
            {3, fx::ResourceAccess::ColorAttachmentWrite});
        check(effect_validation_error(effect_validate(spare_write), fx::EffectError::InvalidPassGraph));
    }

    it("rejects unknown self and forward dependencies rather than silently reordering") {
        for (auto dependency : {1u, 2u, 99u}) {
            auto fixture = effect_two_pass_fixture();
            fixture.manifest.passes.front().dependencies = {dependency};
            check(effect_validation_error(effect_validate(fixture), fx::EffectError::InvalidPassGraph));
        }
    }

    it("rejects feedback duplicate IDs extra writers and missing output") {
        auto feedback = effect_graphics_fixture();
        feedback.manifest.passes.front().resources.push_back({2, fx::ResourceAccess::SampledRead});
        check(effect_validation_error(effect_validate(feedback), fx::EffectError::InvalidPassGraph));
        auto duplicate = effect_graphics_fixture();
        duplicate.manifest.resources.back().id = duplicate.manifest.resources.front().id;
        check(effect_validation_rejected(effect_validate(duplicate)));
        auto writer = effect_two_pass_fixture();
        writer.manifest.passes.back().resources.push_back({2, fx::ResourceAccess::ColorAttachmentWrite});
        check(effect_validation_error(effect_validate(writer), fx::EffectError::InvalidPassGraph));
        auto output = effect_graphics_fixture();
        output.manifest.resources.back().role = fx::ResourceRole::Intermediate;
        check(effect_validation_rejected(effect_validate(output)));
    }

    it("rejects unused resources and artifacts instead of admitting an unrelated payload") {
        auto resource = effect_graphics_fixture();
        auto extra = resource.manifest.resources.front();
        extra.id = 99;
        resource.manifest.resources.push_back(extra);
        check(effect_validation_rejected(effect_validate(resource)));
        auto artifact = effect_graphics_fixture();
        auto module = artifact.manifest.artifacts.front();
        module.id = "unused";
        artifact.manifest.artifacts.push_back(module);
        effect_test_install_observations(artifact);
        check(effect_validation_rejected(effect_validate(artifact)));
    }

    it("rejects cost addition and surface size multiplication overflow") {
        auto bytes = effect_graphics_fixture();
        bytes.manifest.artifacts.front().byte_count =
            (std::numeric_limits<std::uint64_t>::max)();
        effect_test_install_observations(bytes);
        check(effect_validation_rejected(effect_validate(bytes)));
        auto pixels = effect_graphics_fixture();
        const auto maximum = (std::numeric_limits<std::uint32_t>::max)();
        for (auto& resource : pixels.manifest.resources) {
            resource.width = resource.height = maximum;
            resource.byte_count = (std::numeric_limits<std::uint64_t>::max)();
        }
        pixels.capabilities.effects.front().execution.max_width = maximum;
        pixels.capabilities.effects.front().execution.max_height = maximum;
        pixels.capabilities.device.execution.max_width = maximum;
        pixels.capabilities.device.execution.max_height = maximum;
        check(effect_validation_rejected(effect_validate(pixels)));
    }

    it("rejects zero validation limits") {
        const std::array<std::size_t fx::EffectValidationLimits::*, 12> fields = {
            &fx::EffectValidationLimits::max_parameters,
            &fx::EffectValidationLimits::max_artifacts,
            &fx::EffectValidationLimits::max_resources,
            &fx::EffectValidationLimits::max_passes,
            &fx::EffectValidationLimits::max_uses_per_pass,
            &fx::EffectValidationLimits::max_dependencies_per_pass,
            &fx::EffectValidationLimits::max_effect_supports,
            &fx::EffectValidationLimits::max_installed_artifacts,
            &fx::EffectValidationLimits::max_domain_entries,
            &fx::EffectValidationLimits::max_identifier_bytes,
            &fx::EffectValidationLimits::max_source_bytes,
            &fx::EffectValidationLimits::max_metadata_bytes,
        };
        const auto fixture = effect_graphics_fixture();
        for (auto field : fields) {
            fx::EffectValidationLimits limits;
            limits.*field = 0;
            check(effect_validation_error(effect_validate(fixture, limits), fx::EffectError::InvalidLimits));
        }
    }

    it("enforces artifact resource pass use and domain count boundaries") {
        const auto fixture = effect_graphics_fixture();
        fx::EffectValidationLimits limits;
        limits.max_parameters = 1;
        limits.max_artifacts = 2;
        limits.max_resources = 2;
        limits.max_passes = 1;
        limits.max_uses_per_pass = 2;
        limits.max_effect_supports = 1;
        limits.max_installed_artifacts = 2;
        limits.max_domain_entries = 2;
        check(effect_validate(fixture, limits).contract != nullptr);
        for (auto field : {&fx::EffectValidationLimits::max_artifacts,
                           &fx::EffectValidationLimits::max_resources,
                           &fx::EffectValidationLimits::max_uses_per_pass,
                           &fx::EffectValidationLimits::max_installed_artifacts,
                           &fx::EffectValidationLimits::max_domain_entries}) {
            auto smaller = limits;
            smaller.*field = 1;
            check(effect_validation_error(effect_validate(fixture, smaller), fx::EffectError::ResourceLimit));
        }
        check(effect_validation_error(effect_validate(effect_two_pass_fixture(), limits),
                                      fx::EffectError::ResourceLimit));
    }

    it("bounds identifiers source diagnostics and aggregate metadata") {
        const auto fixture = effect_graphics_fixture();
        for (auto field : {&fx::EffectValidationLimits::max_identifier_bytes,
                           &fx::EffectValidationLimits::max_source_bytes,
                           &fx::EffectValidationLimits::max_metadata_bytes}) {
            fx::EffectValidationLimits limits;
            limits.*field = 1;
            check(effect_validation_error(effect_validate(fixture, limits), fx::EffectError::ResourceLimit));
        }
    }

    it("keeps rejected oversized source diagnostics bounded and preserves the original span") {
        auto fixture = effect_graphics_fixture();
        fixture.requirement.source.label = "source:" + std::string(8192, 'L');
        fixture.requirement.source.token = "effect:" + std::string(8192, 'T');
        fixture.requirement.source.offset = 4096;
        fixture.requirement.source.length = 8192;
        const auto result = effect_validate(fixture);
        check(effect_validation_error(result, fx::EffectError::ResourceLimit));
        const auto& source = result.diagnostic.source;
        check(!source.label.empty() && source.label.size() <= 1024);
        check(!source.token.empty() && source.token.size() <= 1024);
        check(source.label == fixture.requirement.source.label.substr(0, source.label.size()));
        check(source.token == fixture.requirement.source.token.substr(0, source.token.size()));
        check(source.offset == 4096);
        check(source.length == 8192);
    }

    it("freezes the full checked requirement manifest and diagnostic source") {
        auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        fixture.requirement.expanded_width = 99;
        fixture.requirement.parameters.front().upper = 99;
        fixture.requirement.source.token = "mutated";
        fixture.manifest.artifacts.front().content_hash = effect_test_hash(99);
        fixture.manifest.resources.clear();
        fixture.manifest.passes.clear();
        fixture.manifest.variant_id = "mutated";
        check(result.contract->requirement().expanded_width == 4);
        check(result.contract->requirement().parameters.front().upper == 4.0);
        check(result.contract->source().token == "blur-[4px]");
        check(result.contract->manifest().resources.size() == 2);
        check(result.contract->manifest().passes.size() == 1);
        check(result.contract->manifest().artifacts.front().content_hash == effect_test_hash(10));
        check(result.contract->variant_id() == "test.variant");
        auto failed = effect_graphics_fixture();
        failed.capabilities.installed.front().observed_content_hash = effect_test_hash(99);
        const auto failure = effect_validate(failed);
        check(effect_validation_error(failure, fx::EffectError::ArtifactHashMismatch));
        check(failure.diagnostic.source.label == "fixture.ui.xml");
        check(failure.diagnostic.source.token == "blur-[4px]");
        check(failure.diagnostic.source.offset == 37);
        check(failure.diagnostic.source.length == 10);
        check(effect_cost_equal(result.contract->cost(), {136, 24, 0, 4}));
    }
}

spec("Effect reservations enforce one real shared resource ledger") {
    it("reserves every cost dimension atomically and releases it by RAII") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        auto budget = effect_test_budget(*result.contract);
        {
            auto prepared = fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
            check(bool(prepared));
            check(budget.snapshot().reservations == 1);
            check(effect_cost_equal(budget.snapshot().reserved, {136, 24, 0, 4}));
            check(prepared.reservation.scope() == fixture.capabilities.scope);
        }
        check(budget.snapshot().reservations == 0);
        check(effect_cost_equal(budget.snapshot().reserved, {}));
    }

    it("shares accounting across copied handles instead of minting another capacity") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        auto budget = effect_test_budget(*result.contract);
        auto shared = budget;
        auto first = fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
        check(bool(first));
        const auto second = fx::prepare_effect_admission(*result.contract, shared, fixture.capabilities);
        check(effect_preparation_error(second, fx::EffectError::BudgetExceeded));
        check(shared.snapshot().reservations == 1);
        check(effect_cost_equal(shared.snapshot().reserved, result.contract->cost()));
        first.reservation.release();
        check(shared.snapshot().reservations == 0);
        auto again = fx::prepare_effect_admission(*result.contract, shared, fixture.capabilities);
        check(bool(again));
    }

    it("fails any single insufficient dimension without partially charging others") {
        for (auto fixture : {effect_graphics_fixture(), effect_builtin_fixture(fx::BackendApi::Tango)}) {
            const auto result = effect_validate(fixture);
            check(result.contract != nullptr);
            if (!result.contract) continue;
            for (auto field : {&fx::ResourceCost::bytes, &fx::ResourceCost::pixels,
                               &fx::ResourceCost::cells, &fx::ResourceCost::allocations}) {
                if (result.contract->cost().*field == 0) continue;
                auto capacity = result.contract->cost();
                --(capacity.*field);
                fx::SharedResourceBudget budget(result.contract->scope(), {capacity, 64});
                const auto prepared =
                    fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
                check(effect_preparation_error(prepared, fx::EffectError::BudgetExceeded));
                check(budget.snapshot().reservations == 0);
                check(effect_cost_equal(budget.snapshot().reserved, {}));
            }
        }
    }

    it("enforces the reservation count even when all byte capacities would fit") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        auto budget = effect_test_budget(*result.contract, 2, 1);
        auto first = fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
        check(bool(first));
        const auto second = fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
        check(effect_preparation_error(second, fx::EffectError::BudgetExceeded));
        check(effect_cost_equal(budget.snapshot().reserved, result.contract->cost()));
    }

    it("moves lease ownership and makes explicit release idempotent") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        auto budget = effect_test_budget(*result.contract);
        auto prepared = fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
        check(bool(prepared));
        fx::EffectReservation lease(std::move(prepared.reservation));
        check(bool(lease));
        check(!prepared.reservation);
        check(budget.snapshot().reservations == 1);
        auto& self_alias = lease;
        lease = std::move(self_alias);
        check(bool(lease));
        check(budget.snapshot().reservations == 1);
        lease.release();
        lease.release();
        prepared.reservation.release();
        check(!lease);
        check(budget.snapshot().reservations == 0);
        check(effect_cost_equal(budget.snapshot().reserved, {}));
    }

    it("releases a move-assignment destination exactly once before taking the source") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        auto budget = effect_test_budget(*result.contract, 2);
        auto first = fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
        auto second = fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
        check(bool(first));
        check(bool(second));
        check(budget.snapshot().reservations == 2);
        first.reservation = std::move(second.reservation);
        check(!second.reservation);
        check(budget.snapshot().reservations == 1);
        check(effect_cost_equal(budget.snapshot().reserved, result.contract->cost()));
        first.reservation.release();
        check(budget.snapshot().reservations == 0);
    }

    it("rechecks capability scope generation profile revision and lost status") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        auto budget = effect_test_budget(*result.contract);
        for (unsigned change = 0; change != 7; ++change) {
            auto current = fixture.capabilities;
            fx::EffectError expected = fx::EffectError::StaleCapabilities;
            if (change == 0) { ++current.scope.owner_id; expected = fx::EffectError::ScopeMismatch; }
            if (change == 1) { ++current.scope.domain_id; expected = fx::EffectError::ScopeMismatch; }
            if (change == 2) { ++current.scope.device_generation; expected = fx::EffectError::StaleGeneration; }
            if (change == 3) ++current.capability_revision;
            if (change == 4) current.backend.profile = "changed-profile";
            if (change == 5) current.target.contract_hash = effect_test_hash(99);
            if (change == 6) { current.status = fx::DeviceStatus::Lost; expected = fx::EffectError::DeviceLost; }
            const auto prepared = fx::prepare_effect_admission(*result.contract, budget, current);
            check(effect_preparation_error(prepared, expected));
            check(budget.snapshot().reservations == 0);
            check(effect_cost_equal(budget.snapshot().reserved, {}));
        }
    }

    it("rejects another ledger owner domain or generation even with matching current capabilities") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        for (unsigned change = 0; change != 3; ++change) {
            auto scope = result.contract->scope();
            if (change == 0) ++scope.owner_id;
            if (change == 1) ++scope.domain_id;
            if (change == 2) ++scope.device_generation;
            fx::SharedResourceBudget budget(scope, {result.contract->cost(), 64});
            const auto prepared =
                fx::prepare_effect_admission(*result.contract, budget, fixture.capabilities);
            check(effect_preparation_error(
                prepared, change == 2 ? fx::EffectError::StaleGeneration : fx::EffectError::ScopeMismatch));
            check(budget.snapshot().reservations == 0);
        }
    }

    it("keeps lost-generation charges until the old owner explicitly retires them") {
        const auto old_fixture = effect_graphics_fixture();
        const auto old_contract = effect_validate(old_fixture);
        check(old_contract.contract != nullptr);
        if (!old_contract.contract) return;
        auto old_budget = effect_test_budget(*old_contract.contract, 2);
        auto old_lease =
            fx::prepare_effect_admission(*old_contract.contract, old_budget, old_fixture.capabilities);
        check(bool(old_lease));
        old_budget.mark_device_lost();
        old_budget.mark_device_lost();
        check(old_budget.snapshot().status == fx::DeviceStatus::Lost);
        check(old_budget.snapshot().reservations == 1);
        check(effect_cost_equal(old_budget.snapshot().reserved, old_contract.contract->cost()));
        const auto blocked =
            fx::prepare_effect_admission(*old_contract.contract, old_budget, old_fixture.capabilities);
        check(effect_preparation_error(blocked, fx::EffectError::DeviceLost));

        auto new_fixture = old_fixture;
        ++new_fixture.capabilities.scope.device_generation;
        const auto new_contract = effect_validate(new_fixture);
        check(new_contract.contract != nullptr);
        if (!new_contract.contract) return;
        auto new_budget = effect_test_budget(*new_contract.contract);
        auto new_lease =
            fx::prepare_effect_admission(*new_contract.contract, new_budget, new_fixture.capabilities);
        check(bool(new_lease));
        old_lease.reservation.release();
        check(old_budget.snapshot().reservations == 0);
        check(new_budget.snapshot().reservations == 1);
        check(effect_cost_equal(new_budget.snapshot().reserved, new_contract.contract->cost()));
    }

    it("rejects invalid ledger identities and count limits but permits zero capacity") {
        const fx::ResourceScope scope{11, 22, 3};
        for (unsigned field = 0; field != 3; ++field) {
            auto invalid = scope;
            if (field == 0) invalid.owner_id = 0;
            if (field == 1) invalid.domain_id = 0;
            if (field == 2) invalid.device_generation = 0;
            check_throws_as(fx::SharedResourceBudget(invalid, fx::ResourceBudgetLimits{}),
                            std::invalid_argument);
        }
        fx::ResourceBudgetLimits invalid;
        invalid.max_reservations = 0;
        check_throws_as(fx::SharedResourceBudget(scope, invalid), std::invalid_argument);
        fx::SharedResourceBudget zero(scope, {});
        check(effect_cost_equal(zero.snapshot().capacity, {}));
        const auto fixture = effect_graphics_fixture();
        const auto contract = effect_validate(fixture);
        check(contract.contract != nullptr);
        if (!contract.contract) return;
        const auto prepared =
            fx::prepare_effect_admission(*contract.contract, zero, fixture.capabilities);
        check(effect_preparation_error(prepared, fx::EffectError::BudgetExceeded));
    }

    it("atomically grants the final capacity to only one concurrent candidate") {
        const auto fixture = effect_graphics_fixture();
        const auto result = effect_validate(fixture);
        check(result.contract != nullptr);
        if (!result.contract) return;
        auto budget = effect_test_budget(*result.contract);
        std::array<fx::EffectPreparationResult, 2> prepared;
        std::mutex gate;
        std::condition_variable changed;
        unsigned waiting = 0;
        bool start = false;
        const auto worker = [&](unsigned index, fx::SharedResourceBudget shared) {
            {
                std::unique_lock<std::mutex> lock(gate);
                ++waiting;
                changed.notify_all();
                changed.wait(lock, [&] { return start; });
            }
            prepared[index] =
                fx::prepare_effect_admission(*result.contract, shared, fixture.capabilities);
        };
        std::thread first(worker, 0, budget);
        std::thread second(worker, 1, budget);
        {
            std::unique_lock<std::mutex> lock(gate);
            changed.wait(lock, [&] { return waiting == 2; });
            start = true;
        }
        changed.notify_all();
        first.join();
        second.join();
        check(unsigned(bool(prepared[0])) + unsigned(bool(prepared[1])) == 1);
        check(effect_preparation_error(prepared[0], fx::EffectError::BudgetExceeded) ||
              effect_preparation_error(prepared[1], fx::EffectError::BudgetExceeded));
        check(budget.snapshot().reservations == 1);
        check(effect_cost_equal(budget.snapshot().reserved, result.contract->cost()));
        prepared[0].reservation.release();
        prepared[1].reservation.release();
        check(budget.snapshot().reservations == 0);
        check(effect_cost_equal(budget.snapshot().reserved, {}));
    }
}


namespace {

struct NativeOwnerTestState {
    fx::SharedResourceBudget budget;
    int prepare_attempts = 0;
    int constructed = 0;
    int released = 0;
    int released_while_charged = 0;

    explicit NativeOwnerTestState(fx::SharedResourceBudget ledger)
        : budget(std::move(ledger)) {}
};

fx::NativeEffectResourceIdentity native_test_identity(
    const fx::ValidatedEffectContract& contract) {
    return {contract.scope(), contract.backend(), contract.target(),
            contract.variant_id(), contract.implementation_hash(),
            contract.parameter_abi_hash(), contract.cost()};
}

class CountingNativeResources final : public fx::NativeEffectResources {
public:
    CountingNativeResources(fx::NativeEffectResourceIdentity id,
                            std::shared_ptr<NativeOwnerTestState> state)
        : identity_(std::move(id)), state_(std::move(state)) {
        ++state_->constructed;
    }
    ~CountingNativeResources() noexcept override {
        const auto remaining = state_->budget.snapshot();
        if (remaining.reservations > 0 &&
            remaining.reserved.allocations >= identity_.charged_cost.allocations) {
            ++state_->released_while_charged;
        }
        ++state_->released;
    }
    const fx::NativeEffectResourceIdentity& identity() const noexcept override {
        return identity_;
    }
private:
    fx::NativeEffectResourceIdentity identity_;
    std::shared_ptr<NativeOwnerTestState> state_;
};

enum class NativeOwnerMode {
    Good, NullResult, ThrowRuntime, ThrowAlloc,
    WrongScope, WrongGeneration, WrongVariant, WrongBackend,
    WrongHash, WrongAbi, WrongCost,
    ChangeRevision, ChangeGeneration, DeviceLost, LedgerLost
};

class CountingNativeOwner final : public fx::NativeEffectResourceOwner {
public:
    fx::BackendCapabilitySnapshot current;
    std::shared_ptr<NativeOwnerTestState> state;
    NativeOwnerMode mode = NativeOwnerMode::Good;
    mutable unsigned snapshot_calls = 0;
    const std::thread::id owning_thread = std::this_thread::get_id();

    CountingNativeOwner(fx::BackendCapabilitySnapshot capabilities,
                        std::shared_ptr<NativeOwnerTestState> shared,
                        NativeOwnerMode action = NativeOwnerMode::Good)
        : current(std::move(capabilities)), state(std::move(shared)), mode(action) {}

    bool on_owner_thread() const noexcept override {
        return std::this_thread::get_id() == owning_thread;
    }

    fx::BackendCapabilitySnapshot capabilities() const override {
        ++snapshot_calls;
        auto result = current;
        if (snapshot_calls > 1) {
            if (mode == NativeOwnerMode::ChangeRevision)
                ++result.capability_revision;
            if (mode == NativeOwnerMode::ChangeGeneration)
                ++result.scope.device_generation;
            if (mode == NativeOwnerMode::DeviceLost)
                result.status = fx::DeviceStatus::Lost;
        }
        return result;
    }

    std::unique_ptr<fx::NativeEffectResources> prepare_resources(
        const fx::ValidatedEffectContract& contract) override {
        ++state->prepare_attempts;
        check(state->budget.snapshot().reservations > 0);
        auto id = native_test_identity(contract);
        switch (mode) {
        case NativeOwnerMode::WrongScope: ++id.scope.domain_id; break;
        case NativeOwnerMode::WrongGeneration: ++id.scope.device_generation; break;
        case NativeOwnerMode::WrongVariant: id.variant_id += ".wrong"; break;
        case NativeOwnerMode::WrongBackend: id.backend.profile += ".wrong"; break;
        case NativeOwnerMode::WrongHash: id.implementation_hash.bytes[0] ^= 0x42; break;
        case NativeOwnerMode::WrongAbi: id.parameter_abi_hash.bytes[0] ^= 0x24; break;
        case NativeOwnerMode::WrongCost: ++id.charged_cost.bytes; break;
        default: break;
        }
        if (mode == NativeOwnerMode::NullResult) return {};
        if (mode == NativeOwnerMode::ThrowAlloc) throw std::bad_alloc();
        if (mode == NativeOwnerMode::ThrowRuntime) {
            // Partial allocations must be reclaimed by the backend owner itself
            // before the exception crosses the generic preparation boundary.
            auto partial = std::make_unique<CountingNativeResources>(id, state);
            (void)partial;
            throw std::runtime_error("native artifact loader failed after allocation");
        }
        auto resources = std::make_unique<CountingNativeResources>(
            std::move(id), state);
        if (mode == NativeOwnerMode::LedgerLost) {
            state->budget.mark_device_lost();
        }
        return resources;
    }
};

std::shared_ptr<NativeOwnerTestState> native_test_state(
    const fx::ValidatedEffectContract& contract, std::uint64_t copies = 1) {
    return std::make_shared<NativeOwnerTestState>(effect_test_budget(contract, copies));
}

bool native_prep_error(const fx::NativeEffectPreparationResult& result,
                       fx::EffectError error) {
    return !result && result.diagnostic.code == error &&
           !result.diagnostic.message.empty() &&
           !result.prepared;
}

} // namespace

spec("Native backend effect resources preserve admission lease ownership") {
    it("owns the resource and shared cost until explicit pre-submission cleanup") {
        for (auto api : {fx::BackendApi::OpenGL, fx::BackendApi::OpenGLES,
                         fx::BackendApi::Vulkan, fx::BackendApi::Direct2D,
                         fx::BackendApi::Tango}) {
            auto fixture = api == fx::BackendApi::OpenGL ||
                                   api == fx::BackendApi::OpenGLES ||
                                   api == fx::BackendApi::Vulkan
                ? effect_graphics_fixture(api) : effect_builtin_fixture(api);
            const auto validated = effect_validate(fixture);
            check(bool(validated));
            if (!validated) continue;
            auto shared = native_test_state(*validated.contract);
            CountingNativeOwner owner(fixture.capabilities, shared);
            auto prepared = fx::prepare_native_effect(
                validated.contract, shared->budget, owner);
            check(bool(prepared));
            check(prepared.prepared.contract() == validated.contract.get());
            check(prepared.prepared.identity() != nullptr);
            check(prepared.prepared.scope() == fixture.capabilities.scope);
            check(shared->prepare_attempts == 1);
            check(shared->budget.snapshot().reservations == 1);
            check(effect_cost_equal(shared->budget.snapshot().reserved,
                                    validated.contract->cost()));
            prepared.prepared.reset();
            prepared.prepared.reset();
            check(shared->released == 1);
            check(shared->released_while_charged == 1);
            check(shared->budget.snapshot().reservations == 0);
            check(effect_cost_equal(shared->budget.snapshot().reserved, {}));
        }
    }

    it("rejects incomplete owner results without minting successful resources") {
        const auto fixture = effect_graphics_fixture();
        const auto checked = effect_validate(fixture);
        check(bool(checked));
        if (!checked) return;
        for (auto mode : {NativeOwnerMode::NullResult, NativeOwnerMode::ThrowRuntime,
                          NativeOwnerMode::ThrowAlloc}) {
            auto shared = native_test_state(*checked.contract);
            CountingNativeOwner owner(fixture.capabilities, shared, mode);
            const auto result =
                fx::prepare_native_effect(checked.contract, shared->budget, owner);
            check(native_prep_error(result, mode == NativeOwnerMode::ThrowAlloc
                ? fx::EffectError::AllocationFailure : fx::EffectError::NativePreparationFailed));
            check(shared->budget.snapshot().reservations == 0);
            check(effect_cost_equal(shared->budget.snapshot().reserved, {}));
            check(shared->constructed == shared->released);
            check(shared->released_while_charged == shared->released);
        }
        auto shared = native_test_state(*checked.contract);
        CountingNativeOwner owner(fixture.capabilities, shared);
        const auto missing = fx::prepare_native_effect({}, shared->budget, owner);
        check(native_prep_error(missing, fx::EffectError::InvalidValue));
        check(shared->prepare_attempts == 0);
        check(owner.snapshot_calls == 0);
    }

    it("rejects every mismatched native scope, variant, ABI, digest or cost") {
        const auto fixture = effect_graphics_fixture();
        const auto checked = effect_validate(fixture);
        check(bool(checked));
        if (!checked) return;
        for (auto mode : {NativeOwnerMode::WrongScope, NativeOwnerMode::WrongGeneration,
                          NativeOwnerMode::WrongVariant, NativeOwnerMode::WrongBackend,
                          NativeOwnerMode::WrongHash, NativeOwnerMode::WrongAbi,
                          NativeOwnerMode::WrongCost}) {
            auto shared = native_test_state(*checked.contract);
            CountingNativeOwner owner(fixture.capabilities, shared, mode);
            const auto result =
                fx::prepare_native_effect(checked.contract, shared->budget, owner);
            auto expected = fx::EffectError::NativeIdentityMismatch;
            if (mode == NativeOwnerMode::WrongScope) expected = fx::EffectError::ScopeMismatch;
            if (mode == NativeOwnerMode::WrongGeneration) expected = fx::EffectError::StaleGeneration;
            if (mode == NativeOwnerMode::WrongHash) expected = fx::EffectError::ArtifactHashMismatch;
            if (mode == NativeOwnerMode::WrongAbi) expected = fx::EffectError::ProgramAbiMismatch;
            check(native_prep_error(result, expected));
            check(shared->constructed == 1);
            check(shared->released == 1);
            check(shared->released_while_charged == 1);
            check(shared->budget.snapshot().reservations == 0);
        }
    }

    it("destroys a candidate before releasing its budget after device changes") {
        const auto fixture = effect_graphics_fixture();
        const auto checked = effect_validate(fixture);
        check(bool(checked));
        if (!checked) return;
        for (auto mode : {NativeOwnerMode::ChangeRevision,
                          NativeOwnerMode::ChangeGeneration,
                          NativeOwnerMode::DeviceLost,
                          NativeOwnerMode::LedgerLost}) {
            auto shared = native_test_state(*checked.contract);
            CountingNativeOwner owner(fixture.capabilities, shared, mode);
            const auto result =
                fx::prepare_native_effect(checked.contract, shared->budget, owner);
            auto expected = mode == NativeOwnerMode::ChangeRevision ?
                fx::EffectError::StaleCapabilities :
                mode == NativeOwnerMode::ChangeGeneration ?
                    fx::EffectError::StaleGeneration : fx::EffectError::DeviceLost;
            check(native_prep_error(result, expected));
            check(shared->constructed == 1);
            check(shared->released == 1);
            check(shared->released_while_charged == 1);
            check(shared->budget.snapshot().reservations == 0);
            if (mode == NativeOwnerMode::LedgerLost)
                check(shared->budget.snapshot().status == fx::DeviceStatus::Lost);
        }
    }

    it("refuses insufficient budgets before invoking the native owner") {
        const auto fixture = effect_graphics_fixture();
        const auto checked = effect_validate(fixture);
        check(bool(checked));
        if (!checked) return;
        fx::SharedResourceBudget ledger(checked.contract->scope(),
                                        {{0, 0, 0, 0}, 1});
        auto shared = std::make_shared<NativeOwnerTestState>(ledger);
        CountingNativeOwner owner(fixture.capabilities, shared);
        const auto result = fx::prepare_native_effect(checked.contract, ledger, owner);
        check(native_prep_error(result, fx::EffectError::BudgetExceeded));
        check(shared->prepare_attempts == 0);
        check(shared->budget.snapshot().reservations == 0);
    }

    it("holds both old and candidate costs until each native owner retires") {
        const auto fixture = effect_graphics_fixture();
        const auto checked = effect_validate(fixture);
        check(bool(checked));
        if (!checked) return;
        auto shared = native_test_state(*checked.contract, 2);
        CountingNativeOwner owner(fixture.capabilities, shared);
        auto active = fx::prepare_native_effect(checked.contract, shared->budget, owner);
        auto candidate = fx::prepare_native_effect(checked.contract, shared->budget, owner);
        check(bool(active));
        check(bool(candidate));
        check(shared->budget.snapshot().reservations == 2);
        active.prepared = std::move(candidate.prepared);
        check(bool(active));
        check(!candidate.prepared);
        check(shared->released == 1);
        check(shared->released_while_charged == 1);
        check(shared->budget.snapshot().reservations == 1);
        auto& self = active.prepared;
        active.prepared = std::move(self);
        check(bool(active.prepared));
        active.prepared.reset();
        check(shared->released == 2);
        check(shared->released_while_charged == 2);
        check(shared->budget.snapshot().reservations == 0);
    }

    it("rejects a call from a non-owner thread before querying or reserving") {
        const auto fixture = effect_graphics_fixture();
        const auto checked = effect_validate(fixture);
        check(bool(checked));
        if (!checked) return;
        auto shared = native_test_state(*checked.contract);
        CountingNativeOwner owner(fixture.capabilities, shared);
        fx::NativeEffectPreparationResult result;
        std::thread foreign([&] {
            result = fx::prepare_native_effect(
                checked.contract, shared->budget, owner);
        });
        foreign.join();
        check(native_prep_error(result, fx::EffectError::WrongOwnerThread));
        check(owner.snapshot_calls == 0);
        check(shared->prepare_attempts == 0);
        check(shared->budget.snapshot().reservations == 0);
    }

    it("keeps resources independently alive after the temporary factory dies") {
        const auto fixture = effect_graphics_fixture();
        const auto checked = effect_validate(fixture);
        check(bool(checked));
        if (!checked) return;
        auto shared = native_test_state(*checked.contract);
        fx::NativeEffectPreparationResult result;
        {
            CountingNativeOwner owner(fixture.capabilities, shared);
            result = fx::prepare_native_effect(checked.contract, shared->budget, owner);
        }
        check(bool(result));
        check(shared->released == 0);
        result.prepared.reset();
        check(shared->released == 1);
        check(shared->budget.snapshot().reservations == 0);
    }
}
