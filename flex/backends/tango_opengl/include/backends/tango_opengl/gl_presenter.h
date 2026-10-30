/*
 * Dedicated Tango cell-grid OpenGL presenter host.
 * This is NOT the general Flex OpenGL renderer factory and does not
 * register RendererBackend::TangoOpenGL before real application acceptance.
 */
#pragma once
#include <backends/tango_opengl/cell_presenter.h>
#include <backends/tango_opengl/cell_input.h>
#include <backends/opengl/init.h>
#include <gcanvas/context.hpp>

#include <memory>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace flex::tango_opengl {

struct ExternalGLCellCreateInfo {
    // Host-owned, compatible OpenGL context MUST be current during create,
    // present and destruction. The callback user_data must outlive this
    // presenter. The host owns window buffer swapping.
    opengl_backend::OpenGLProcLoader get_proc_address = nullptr;
    void* proc_user_data = nullptr;
    // Optional actual GL operation admission; the callback is borrowed
    // until the renderer and its native GPU resources are destroyed.
    // The strict combined owner installs its own fence/current-context guard.
    ::gcanvas::NativeGpuMutationGuard native_mutation_guard = nullptr;
    void* native_mutation_user_data = nullptr;
    // Both regular and bold font files are mandatory. Empty/missing paths
    // must fail before a frame instead of silently falling back to gCanvas's
    // default font or pretending that regular glyphs are bold glyphs.
    std::string font_family = "tango-monospace";
    std::string font_path;
    std::string bold_font_path;
};

// Owns gCanvas's real GL renderer and shader/glyph resources. It draws
// cell-pixel frame plans; native GPU/in-flight retirement and app/window
// qualification remain explicit #342 gates.
std::unique_ptr<CellPixelPresenter> create_external_gl_presenter(
    const ExternalGLCellCreateInfo& info);


/*
 * Bounded native OpenGL submission-fence owner for the existing gCanvas GL
 * presenter. No second renderer, global GL loader, or fallback synchronization.
 *
 * The host must explicitly check capacity BEFORE drawing, then record the
 * exact successful presenter submission on the SAME current native context.
 * The fence protects the GCanvas renderer and its glyph/atlas resources from
 * being destroyed until the GPU has signaled every GLsync. The input gate is
 * a separate CPU acceptance contract: publication does not imply a GPU fence
 * has completed. A context-lost recovery path is NOT implemented here.
 */
struct NativeGLFenceConfig final {
    opengl_backend::OpenGLProcLoader get_proc_address = nullptr;
    void* proc_user_data = nullptr;
    // Synchronous host check must verify the ORIGINAL exact GL context,
    // never just that some other GL context is current.
    bool (*is_context_current)(void* user_data) = nullptr;
    void* host_user_data = nullptr;
    std::uint64_t device_generation = 0;
    std::size_t max_in_flight = 0;
    // Conservative cumulative native texture upload payload bound. This
    // caps base image bytes consumed by this owner, NOT driver VRAM/mipmaps.
    // A strict finite default keeps this resource path bounded even when
    // hosts do not provide an explicit smaller profile.
    std::size_t max_total_texture_upload_bytes = 1024ull * 1024ull * 1024ull;
};

enum class NativeGLFencePoll { Drained, Busy };

class NativeGLFrameFences final {
public:
    explicit NativeGLFrameFences(const NativeGLFenceConfig& config);
    ~NativeGLFrameFences() noexcept;
    NativeGLFrameFences(const NativeGLFrameFences&) = delete;
    NativeGLFrameFences& operator=(const NativeGLFrameFences&) = delete;

    // Fail closed when the queue is full or the GL context/owner has changed.
    void require_capacity() const;
    // Insert a real GL_SYNC_GPU_COMMANDS_COMPLETE fence after successful
    // native renderer end_frame(). Frames are identified by source revision,
    // monotonically increasing presenter submission ID and device generation.
    void record_submitted_frame(const CellPixelPresenter& presenter);
    // Wait up to this bounded per-fence time (nanoseconds); a timeout keeps
    // the uncompleted GLsync owned and returns Busy. No glFinish fallback.
    NativeGLFencePoll retire_completed(std::uint64_t timeout_nanoseconds);
    // Can close only after every actual fence was observed signaled and
    // deleted while the ORIGINAL GL context is current. No implicit draining.
    void close();
    std::size_t in_flight() const;
    std::uint64_t last_retired_submission() const;
    std::uint64_t device_generation() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};


/*
 * Single native GL resource lifetime owner, layered on the existing
 * gCanvas pixel presenter + exact native GPU fences. No other renderer.
 *
 * Neither the mutable presenter nor NativeGLFrameFences can escape; a caller
 * can only submit through present_and_fence(). The exposed presenter view
 * is const, for inspecting submitted geometry. The input gate is retained
 * by shared_ptr solely so callbacks can be detached safely and old gates
 * can refuse use after the native presenter has been destroyed.
 *
 * close() must succeed with the ORIGINAL GL context current after all fences
 * have signaled. It is the ONLY method allowed to delete gCanvas GPU assets.
 * Destruction with outstanding GPU work or lost context terminates rather
 * than freeing unsafe GL objects. This is not surprise device-loss recovery.
 */
struct NativeGLResourceLease final {
    NativeRendererResourceKind kind{};
    std::uint64_t native_name = 0;
    std::uint64_t device_generation = 0;
    std::uint64_t source_revision = 0;
    std::uint64_t submission_id = 0;
    bool gpu_completed = false;
};

// Bind actual gCanvas GPU texture/atlas/shader names (not a guessed draw
// recipe) to the exact native GLsync's submission and device generation.
struct NativeGLMutationAudit final {
    std::uint64_t texture_uploads = 0;
    std::uint64_t texture_updates = 0;
    std::uint64_t texture_deletes = 0;
    std::uint64_t program_creates = 0;
    std::uint64_t program_deletes = 0;
    std::uint64_t denied = 0;
    // Sum of CPU payload byte extents for accepted native texture uploads.
    // This does not include driver/mipmap/compression overhead and is not
    // a complete GPU VRAM or peak-allocation budget.
    std::uint64_t uploaded_payload_bytes = 0;
};

class NativeGLPresentationOwner final {
public:
    NativeGLPresentationOwner(
        const ExternalGLCellCreateInfo& renderer,
        const NativeGLFenceConfig& fences);
    ~NativeGLPresentationOwner() noexcept;
    NativeGLPresentationOwner(const NativeGLPresentationOwner&) = delete;
    NativeGLPresentationOwner& operator=(const NativeGLPresentationOwner&) = delete;

    // Strict serial shared-resource mutation policy: no second gCanvas
    // draw while a prior native GLsync is owned, even with spare capacity.
    // Only explicit real fence retirement permits glyph/atlas/texture edits.
    // Per-resource overlapping frame leases remain a future separate gate.
    void present_and_fence(
        const tui_grid_snapshot_t* snapshot, const CellPixelMetrics& metrics);
    NativeGLFencePoll retire_completed(std::uint64_t timeout_nanoseconds);
    // Checks native fence budget plus the stricter shared-atlas serial gate.
    void require_capacity() const;
    std::size_t in_flight() const;
    std::uint64_t last_retired_submission() const;
    std::uint64_t device_generation() const noexcept;
    const CellPixelPresenter& submitted_presenter() const;
    // Snapshot is copied; no caller can delete or mutate a GPU resource.
    // Native names are meaningful ONLY with their recorded generation.
    std::vector<NativeGLResourceLease> gpu_resource_leases() const;
    std::shared_ptr<CellHostFrameGate> input_gate() const noexcept;
    void close();
    bool closed() const noexcept;
    NativeGLMutationAudit native_mutation_audit() const;

private:
    static ExternalGLCellCreateInfo instrumented_info(
        const ExternalGLCellCreateInfo& source,
        NativeGLPresentationOwner* owner);
    static bool admit_native_mutation(
        void* user_data, ::gcanvas::NativeGpuMutation operation,
        std::uint64_t native_name, std::size_t bytes) noexcept;
    NativeGLFenceConfig host_config_;
    NativeGLFrameFences fences_;
    NativeGLMutationAudit mutation_audit_{};
    std::unique_ptr<CellPixelPresenter> presenter_;
    std::shared_ptr<CellHostFrameGate> gate_;
    bool closed_ = false;
    std::vector<NativeGLResourceLease> resource_leases_;
    // If paint or GLsync publication fails after native GL work may have
    // begun, no successful fence proves those commands completed.
    bool unfenced_failure_ = false;
};

} // namespace flex::tango_opengl
