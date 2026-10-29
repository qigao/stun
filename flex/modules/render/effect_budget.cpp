#include <flex/render/effect_admission.h>

#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

namespace flex::effects {

bool operator==(const Hash256& a, const Hash256& b) noexcept {
    return a.bytes == b.bytes;
}
bool operator!=(const Hash256& a, const Hash256& b) noexcept {
    return !(a == b);
}
bool operator==(const ResourceScope& a, const ResourceScope& b) noexcept {
    return a.owner_id == b.owner_id && a.domain_id == b.domain_id &&
           a.device_generation == b.device_generation;
}
bool operator!=(const ResourceScope& a, const ResourceScope& b) noexcept {
    return !(a == b);
}

namespace detail {

struct EffectBudgetState {
    EffectBudgetState(ResourceScope resource_scope, ResourceBudgetLimits budget_limits)
        : scope(resource_scope), limits(budget_limits) {}

    const ResourceScope scope;
    const ResourceBudgetLimits limits;
    std::mutex mutex;
    ResourceCost reserved;
    std::size_t reservations = 0;
    DeviceStatus status = DeviceStatus::Ready;
};

} // namespace detail

namespace {

bool valid_scope(const ResourceScope& scope) noexcept {
    return scope.owner_id != 0 && scope.domain_id != 0 &&
           scope.device_generation != 0;
}

bool fits(std::uint64_t used, std::uint64_t added, std::uint64_t limit) noexcept {
    return used <= limit && added <= limit - used;
}

bool fits(const ResourceCost& used, const ResourceCost& added,
          const ResourceCost& limit) noexcept {
    return fits(used.bytes, added.bytes, limit.bytes) &&
           fits(used.pixels, added.pixels, limit.pixels) &&
           fits(used.cells, added.cells, limit.cells) &&
           fits(used.allocations, added.allocations, limit.allocations);
}

void add(ResourceCost& target, const ResourceCost& value) noexcept {
    target.bytes += value.bytes;
    target.pixels += value.pixels;
    target.cells += value.cells;
    target.allocations += value.allocations;
}

void subtract(ResourceCost& target, const ResourceCost& value) noexcept {
    target.bytes -= value.bytes;
    target.pixels -= value.pixels;
    target.cells -= value.cells;
    target.allocations -= value.allocations;
}

EffectPreparationResult failure(EffectError error, const EffectSource& source,
                                const char* message) {
    return {{}, {error, source, message, 0}};
}

} // namespace

SharedResourceBudget::SharedResourceBudget(ResourceScope scope,
                                           ResourceBudgetLimits limits) {
    if (!valid_scope(scope) || limits.max_reservations == 0) {
        throw std::invalid_argument(
            "resource budget requires nonzero owner/domain/generation and reservation limit");
    }
    state_ = std::make_shared<detail::EffectBudgetState>(scope, limits);
}

ResourceBudgetSnapshot SharedResourceBudget::snapshot() const {
    // Moved-from handles are inert. They cannot create a new accounting domain.
    if (!state_) return {};
    std::lock_guard<std::mutex> lock(state_->mutex);
    return {state_->scope, state_->limits.capacity, state_->reserved,
            state_->reservations, state_->limits.max_reservations, state_->status};
}

void SharedResourceBudget::mark_device_lost() noexcept {
    if (!state_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    // Device loss stops new reservations. Existing charges are held until the
    // old owner has completed the corresponding native cleanup/retirement.
    state_->status = DeviceStatus::Lost;
}

EffectReservation::EffectReservation(
    std::shared_ptr<detail::EffectBudgetState> state,
    ResourceScope scope, ResourceCost cost) noexcept
    : state_(std::move(state)), scope_(scope), cost_(cost) {}

EffectReservation::~EffectReservation() {
    release();
}

EffectReservation::EffectReservation(EffectReservation&& other) noexcept
    : state_(std::move(other.state_)), scope_(other.scope_), cost_(other.cost_) {
    other.scope_ = {};
    other.cost_ = {};
}

EffectReservation& EffectReservation::operator=(EffectReservation&& other) noexcept {
    if (this != &other) {
        release();
        state_ = std::move(other.state_);
        scope_ = other.scope_;
        cost_ = other.cost_;
        other.scope_ = {};
        other.cost_ = {};
    }
    return *this;
}

void EffectReservation::release() noexcept {
    auto state = std::move(state_);
    if (!state) return;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        subtract(state->reserved, cost_);
        --state->reservations;
    }
    scope_ = {};
    cost_ = {};
}

EffectPreparationResult prepare_effect_admission(
    const ValidatedEffectContract& contract, SharedResourceBudget& budget,
    const BackendCapabilitySnapshot& current_capabilities) {
    try {
        if (!budget.state_) {
            return failure(EffectError::InvalidValue, contract.source(),
                           "resource budget handle is empty");
        }
        const auto& current = current_capabilities;
        const auto& required_scope = contract.scope();
        if (current.scope.owner_id != required_scope.owner_id ||
            current.scope.domain_id != required_scope.domain_id) {
            return failure(EffectError::ScopeMismatch, contract.source(),
                           "current capabilities belong to another owner or resource domain");
        }
        if (current.scope.device_generation != required_scope.device_generation) {
            return failure(EffectError::StaleGeneration, contract.source(),
                           "current capabilities belong to another device generation");
        }
        if (current.status != DeviceStatus::Ready) {
            return failure(current.status == DeviceStatus::Lost ? EffectError::DeviceLost
                                                                : EffectError::InvalidValue,
                           contract.source(), "current resource domain is not ready");
        }
        if (current.capability_revision != contract.capability_revision() ||
            current.backend.api != contract.backend().api ||
            current.backend.profile != contract.backend().profile ||
            current.target.kind != contract.target().kind ||
            current.target.x_unit != contract.target().x_unit ||
            current.target.y_unit != contract.target().y_unit ||
            current.target.contract_hash != contract.target().contract_hash) {
            return failure(EffectError::StaleCapabilities, contract.source(),
                           "current capability revision or profile differs from validation");
        }
        const auto state = budget.state_;
        EffectError error = EffectError::None;
        const char* message = nullptr;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            const auto& required = contract.scope();
            if (required.owner_id != state->scope.owner_id ||
                required.domain_id != state->scope.domain_id) {
                error = EffectError::ScopeMismatch;
                message = "validated contract belongs to another resource owner or domain";
            } else if (required.device_generation != state->scope.device_generation) {
                error = EffectError::StaleGeneration;
                message = "validated contract belongs to another device generation";
            } else if (state->status == DeviceStatus::Lost) {
                error = EffectError::DeviceLost;
                message = "resource domain is lost; outstanding reservations remain charged";
            } else if (state->reservations >= state->limits.max_reservations ||
                       !fits(state->reserved, contract.cost(), state->limits.capacity)) {
                error = EffectError::BudgetExceeded;
                message = "aggregate resource budget or reservation count would be exceeded";
            } else {
                // Check and charge all dimensions under the same lock. No
                // operation after this point allocates or can partially fail.
                add(state->reserved, contract.cost());
                ++state->reservations;
                return {EffectReservation(state, state->scope, contract.cost()), {}};
            }
        }
        return failure(error, contract.source(), message);
    } catch (const std::bad_alloc&) {
        return {{}, {EffectError::AllocationFailure, {}, {}, 0}};
    }
}

} // namespace flex::effects
