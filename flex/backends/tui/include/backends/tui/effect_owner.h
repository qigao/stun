/*
 * Tango native TerminalCell clear/buffer resource owner.
 *
 * This is an explicitly qualified native C11 cell-buffer operation, NOT a GPU
 * shader or a claim about Tango shadows, blur, or cross-target pixel mapping.
 */
#pragma once

#include <flex/render/effect_resource_owner.h>
#include <tui.h>

#include <cstdint>
#include <memory>
#include <thread>

namespace flex::tui_backend {

struct TangoCellClearProgram {
    effects::EffectRequirement requirement;
    effects::BackendProgramManifest manifest;
};

// Owns an actual Tango buffer returned by tui_buffer_create(). The buffer is a
// synchronous pre-submission candidate; no terminal front/back buffer or PTY is
// borrowed or mutated by its construction. The resource is owner-thread bound.
class TangoCellClearResources final : public effects::NativeEffectResources {
public:
    ~TangoCellClearResources() noexcept override;
    const effects::NativeEffectResourceIdentity& identity() const noexcept override {
        return identity_;
    }
    // Borrowed, owner-thread-only, valid until PreparedNativeEffect::reset().
    // A synchronous Flex->Tango replay may use this to build the candidate.
    tui_buffer_t* buffer_on_owner() const noexcept;

private:
    TangoCellClearResources(effects::NativeEffectResourceIdentity identity,
                            tui_buffer_t* buffer, std::thread::id thread) noexcept;
    effects::NativeEffectResourceIdentity identity_;
    tui_buffer_t* buffer_ = nullptr;
    std::thread::id owning_thread_;
    friend class TangoCellClearOwner;
};

// One explicit native binding: create a cell-sized Tango buffer and clear it
// using the production tui_buffer_clear implementation. Nothing else is
// advertised: no blur, shadow, GLSL, SPIR-V or terminal I/O program.
class TangoCellClearOwner final : public effects::NativeEffectResourceOwner {
public:
    TangoCellClearOwner(effects::ResourceScope scope,
                        std::uint32_t max_columns, std::uint32_t max_rows);
    TangoCellClearOwner(const TangoCellClearOwner&) = delete;
    TangoCellClearOwner& operator=(const TangoCellClearOwner&) = delete;

    bool on_owner_thread() const noexcept override;
    effects::BackendCapabilitySnapshot capabilities() const override;
    std::unique_ptr<effects::NativeEffectResources> prepare_resources(
        const effects::ValidatedEffectContract& contract) override;

    // Defines exactly the one native operation the production owner implements
    // and the buffer extent to be allocated. Throws for zero/oversized cells.
    TangoCellClearProgram describe_clear(
        std::uint32_t columns, std::uint32_t rows) const;

    // Must be invoked on the owner thread with the matching current ledger.
    // Existing PreparedNativeEffect bundles and charges remain live until
    // explicitly cleaned up; recovery requires a new owner/ledger generation.
    void mark_device_lost(effects::SharedResourceBudget& budget);

private:
    std::thread::id owning_thread_;
    effects::ResourceScope scope_;
    std::uint32_t max_columns_ = 0;
    std::uint32_t max_rows_ = 0;
    effects::BackendCapabilitySnapshot capabilities_;
};

} // namespace flex::tui_backend
