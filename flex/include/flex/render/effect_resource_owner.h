#pragma once

#include <flex/render/effect_admission.h>

#include <memory>
#include <string>

namespace flex::effects {

// Identity of a COMPLETE backend-owned native resource bundle. It is supplied
// by the adapter after creating the resources, never inferred from the
// generic RendererCapabilities booleans.
struct NativeEffectResourceIdentity {
    ResourceScope scope;
    BackendProfile backend;
    TargetProfile target;
    std::string variant_id;
    Hash256 implementation_hash;
    Hash256 parameter_abi_hash;
    // Must be the full conservative cost already reserved by the admission
    // ledger. It is NOT a measurement of GPU driver allocations.
    ResourceCost charged_cost;
};

// A real backend implementation owns the shader/pipeline/surfaces or native
// terminal resources. Its destructor MUST finish synchronous cleanup before
// returning and be noexcept. A partially failed prepare MUST clean up its own
// partial resources before returning null or throwing. This pre-submission API
// does not authorize destruction of resources referenced by an in-flight frame.
class NativeEffectResources {
public:
    virtual ~NativeEffectResources() noexcept = default;
    virtual const NativeEffectResourceIdentity& identity() const noexcept = 0;
};

// The backend owner is authoritative for qualified capability snapshots and
// for creating the native resources. The factory is called on its owner thread;
// the returned resource bundle must own the lifetime of any backend state it
// needs for destruction, independently of this factory object's lifetime.
class NativeEffectResourceOwner {
public:
    virtual ~NativeEffectResourceOwner() = default;
    // No native allocation or capability query may run off the owner thread.
    virtual bool on_owner_thread() const noexcept = 0;
    virtual BackendCapabilitySnapshot capabilities() const = 0;
    virtual std::unique_ptr<NativeEffectResources> prepare_resources(
        const ValidatedEffectContract& contract) = 0;
};

// Holds both native resources and the corresponding ledger reservation.
// The native resource destructor always runs BEFORE the reservation is
// released, including move assignment and error cleanup. Keep this object
// alive until the backend has established its own frame-completion/retirement
// proof. No generic submission/retirement is implemented in this M2 slice.
struct NativeEffectPreparationResult;

class PreparedNativeEffect final {
public:
    PreparedNativeEffect() noexcept = default;
    ~PreparedNativeEffect() noexcept;
    PreparedNativeEffect(const PreparedNativeEffect&) = delete;
    PreparedNativeEffect& operator=(const PreparedNativeEffect&) = delete;
    PreparedNativeEffect(PreparedNativeEffect&& other) noexcept;
    PreparedNativeEffect& operator=(PreparedNativeEffect&& other) noexcept;

    explicit operator bool() const noexcept {
        return bool(resources_) && bool(reservation_) && bool(contract_);
    }
    const ValidatedEffectContract* contract() const noexcept {
        return contract_.get();
    }
    const NativeEffectResourceIdentity* identity() const noexcept {
        return resources_ ? &resources_->identity() : nullptr;
    }
    // C++ backend-only borrowed native bundle. Lifetime is bounded by this
    // PreparedNativeEffect; backend consumers must enforce their owner thread
    // and must not retain the handle beyond synchronous completion.
    const NativeEffectResources* native_resources() const noexcept {
        return resources_.get();
    }
    const ResourceScope& scope() const noexcept { return reservation_.scope(); }
    void reset() noexcept;

private:
    PreparedNativeEffect(
        std::shared_ptr<const ValidatedEffectContract> contract,
        EffectReservation reservation,
        std::unique_ptr<NativeEffectResources> resources) noexcept;
    // Deliberate order: destroyed reverse-to-declaration. reset() also makes
    // the ordering explicit in the move-assignment and error paths.
    EffectReservation reservation_;
    std::unique_ptr<NativeEffectResources> resources_;
    std::shared_ptr<const ValidatedEffectContract> contract_;
    friend struct NativeEffectPreparationResult;
    friend NativeEffectPreparationResult prepare_native_effect(
        std::shared_ptr<const ValidatedEffectContract>,
        SharedResourceBudget&, NativeEffectResourceOwner&);
};

struct NativeEffectPreparationResult {
    PreparedNativeEffect prepared;
    EffectDiagnostic diagnostic;
    explicit operator bool() const noexcept { return bool(prepared); }
};

// One candidate, BEFORE any frame submission:
//  (1) Query current authoritative owner snapshot.
//  (2) Atomically reserve its declared cost on the shared domain ledger.
//  (3) Create real native resources through the qualified owner.
//  (4) Verify the complete resource-bundle identity and re-query capabilities.
// On any failure: destroy native resources FIRST, then release the reservation.
// This is not an artifact loader, shader qualifier, UI publication transaction,
// completion fence, resource reaper or provider fallback.
NativeEffectPreparationResult prepare_native_effect(
    std::shared_ptr<const ValidatedEffectContract> contract,
    SharedResourceBudget& budget, NativeEffectResourceOwner& owner);

} // namespace flex::effects
