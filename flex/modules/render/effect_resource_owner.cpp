#include <flex/render/effect_resource_owner.h>

#include <new>
#include <stdexcept>
#include <utility>

namespace flex::effects {
namespace {

bool same_backend(const BackendProfile& a, const BackendProfile& b) noexcept {
    return a.api == b.api && a.profile == b.profile;
}

bool same_target(const TargetProfile& a, const TargetProfile& b) noexcept {
    return a.kind == b.kind && a.x_unit == b.x_unit &&
           a.y_unit == b.y_unit && a.contract_hash == b.contract_hash;
}

bool same_cost(const ResourceCost& a, const ResourceCost& b) noexcept {
    return a.bytes == b.bytes && a.pixels == b.pixels &&
           a.cells == b.cells && a.allocations == b.allocations;
}

NativeEffectPreparationResult failure(EffectError code, const EffectSource& source,
                                      const char* message) {
    return {{}, {code, source, message, 0}};
}

EffectError changed_capabilities(
    const ValidatedEffectContract& contract,
    const BackendCapabilitySnapshot& initial,
    const BackendCapabilitySnapshot& final,
    const SharedResourceBudget& budget) {
    if (final.scope.owner_id != contract.scope().owner_id ||
        final.scope.domain_id != contract.scope().domain_id)
        return EffectError::ScopeMismatch;
    if (final.scope.device_generation != contract.scope().device_generation)
        return EffectError::StaleGeneration;
    if (final.status != DeviceStatus::Ready ||
        budget.snapshot().status != DeviceStatus::Ready)
        return EffectError::DeviceLost;
    if (final.capability_revision != contract.capability_revision() ||
        final.capability_revision != initial.capability_revision ||
        !same_backend(final.backend, contract.backend()) ||
        !same_backend(final.backend, initial.backend) ||
        !same_target(final.target, contract.target()) ||
        !same_target(final.target, initial.target))
        return EffectError::StaleCapabilities;
    return EffectError::None;
}

EffectError wrong_identity(const ValidatedEffectContract& contract,
                           const NativeEffectResourceIdentity& actual) noexcept {
    if (actual.scope.owner_id != contract.scope().owner_id ||
        actual.scope.domain_id != contract.scope().domain_id)
        return EffectError::ScopeMismatch;
    if (actual.scope.device_generation != contract.scope().device_generation)
        return EffectError::StaleGeneration;
    if (!same_backend(actual.backend, contract.backend()) ||
        !same_target(actual.target, contract.target()) ||
        actual.variant_id != contract.variant_id())
        return EffectError::NativeIdentityMismatch;
    if (actual.implementation_hash != contract.implementation_hash())
        return EffectError::ArtifactHashMismatch;
    if (actual.parameter_abi_hash != contract.parameter_abi_hash())
        return EffectError::ProgramAbiMismatch;
    if (!same_cost(actual.charged_cost, contract.cost()))
        return EffectError::NativeIdentityMismatch;
    return EffectError::None;
}

} // namespace

PreparedNativeEffect::PreparedNativeEffect(
    std::shared_ptr<const ValidatedEffectContract> contract,
    EffectReservation reservation,
    std::unique_ptr<NativeEffectResources> resources) noexcept
    : reservation_(std::move(reservation)),
      resources_(std::move(resources)), contract_(std::move(contract)) {}

PreparedNativeEffect::~PreparedNativeEffect() noexcept {
    reset();
}

PreparedNativeEffect::PreparedNativeEffect(PreparedNativeEffect&& other) noexcept
    : reservation_(std::move(other.reservation_)),
      resources_(std::move(other.resources_)),
      contract_(std::move(other.contract_)) {}

PreparedNativeEffect& PreparedNativeEffect::operator=(
    PreparedNativeEffect&& other) noexcept {
    if (this != &other) {
        reset();
        reservation_ = std::move(other.reservation_);
        resources_ = std::move(other.resources_);
        contract_ = std::move(other.contract_);
    }
    return *this;
}

void PreparedNativeEffect::reset() noexcept {
    // Native destructor must synchronously finish cleanup before the budget
    // can be credited back. No release of in-flight GPU resources is allowed.
    resources_.reset();
    reservation_.release();
    contract_.reset();
}

NativeEffectPreparationResult prepare_native_effect(
    std::shared_ptr<const ValidatedEffectContract> contract,
    SharedResourceBudget& budget, NativeEffectResourceOwner& owner) {
    if (!contract) {
        return failure(EffectError::InvalidValue, {},
                       "native preparation requires a validated contract");
    }
    const EffectSource& source = contract->source();
    if (!owner.on_owner_thread()) {
        return failure(EffectError::WrongOwnerThread, source,
                       "native resources must be prepared on their backend owner thread");
    }
    try {
        const BackendCapabilitySnapshot initial = owner.capabilities();
        auto reservation = prepare_effect_admission(*contract, budget, initial);
        if (!reservation) return {{}, std::move(reservation.diagnostic)};

        // A null bundle is NEVER a successful or optional effect. The native
        // owner must itself release any partial resources before returning.
        std::unique_ptr<NativeEffectResources> resources =
            owner.prepare_resources(*contract);
        if (!resources) {
            return failure(EffectError::NativePreparationFailed, source,
                           "backend did not produce a complete native resource bundle");
        }

        const auto identity_error = wrong_identity(*contract, resources->identity());
        if (identity_error != EffectError::None) {
            // Reverse destruction order is guaranteed: resources before lease.
            resources.reset();
            return failure(identity_error, source,
                           "prepared native resource identity or cost does not match admission");
        }

        if (!owner.on_owner_thread()) {
            resources.reset();
            return failure(EffectError::WrongOwnerThread, source,
                           "native resource owner thread changed during preparation");
        }
        const BackendCapabilitySnapshot current = owner.capabilities();
        const auto final_error =
            changed_capabilities(*contract, initial, current, budget);
        if (final_error != EffectError::None) {
            resources.reset();
            return failure(final_error, source,
                           "backend capabilities or resource domain changed during preparation");
        }

        return {PreparedNativeEffect(std::move(contract),
                                     std::move(reservation.reservation),
                                     std::move(resources)), {}};
    } catch (const std::bad_alloc&) {
        return failure(EffectError::AllocationFailure, source,
                       "native effect preparation could not allocate");
    } catch (const std::exception&) {
        return failure(EffectError::NativePreparationFailed, source,
                       "native resource owner failed during preparation");
    }
}

} // namespace flex::effects
