/*
 * Single native TangoGL GPU resource owner over the existing gCanvas
 * presenter and real GLsync queue. The GL API stays in gl_fence.cpp.
 * No extra GL renderer, parser, draw model, CSS or global loader.
 */
#include <backends/tango_opengl/gl_presenter.h>

#include <exception>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <utility>

namespace flex::tango_opengl {

ExternalGLCellCreateInfo NativeGLPresentationOwner::instrumented_info(
    const ExternalGLCellCreateInfo& source,
    NativeGLPresentationOwner* owner) {
    if (!owner || source.native_mutation_guard ||
        source.native_mutation_user_data)
        throw std::invalid_argument(
            "TangoGL combined owner requires its sole native GL mutation guard");
    auto result = source;
    result.native_mutation_guard = &NativeGLPresentationOwner::admit_native_mutation;
    result.native_mutation_user_data = owner;
    return result;
}

bool NativeGLPresentationOwner::admit_native_mutation(
    void* user_data, ::gcanvas::NativeGpuMutation operation,
    std::uint64_t, std::size_t bytes) noexcept {
    auto* self = static_cast<NativeGLPresentationOwner*>(user_data);
    if (!self) return false;
    try {
        // Guard is installed before ANY gCanvas atlas/shader creation.
        // Access only members constructed before presenter_, including
        // this original context identity and the real native fence owner.
        if (!self->host_config_.is_context_current ||
            !self->host_config_.is_context_current(
                self->host_config_.host_user_data) ||
            self->fences_.in_flight() != 0) {
            ++self->mutation_audit_.denied;
            return false;
        }
        switch (operation) {
        case ::gcanvas::NativeGpuMutation::TextureUpload:
            // This is a cumulative CPU texture-payload bound, not an
            // estimate of VRAM/driver allocations. Admission is checked
            // before glGenTextures/glTexImage2D in the real gCanvas path.
            if (bytes > self->host_config_.max_total_texture_upload_bytes ||
                self->mutation_audit_.uploaded_payload_bytes >
                    self->host_config_.max_total_texture_upload_bytes - bytes) {
                ++self->mutation_audit_.denied;
                return false;
            }
            ++self->mutation_audit_.texture_uploads;
            self->mutation_audit_.uploaded_payload_bytes += bytes;
            break;
        case ::gcanvas::NativeGpuMutation::TextureUpdate:
            ++self->mutation_audit_.texture_updates;
            break;
        case ::gcanvas::NativeGpuMutation::TextureDelete:
            ++self->mutation_audit_.texture_deletes;
            break;
        case ::gcanvas::NativeGpuMutation::ProgramCreate:
            ++self->mutation_audit_.program_creates;
            break;
        case ::gcanvas::NativeGpuMutation::ProgramDelete:
            ++self->mutation_audit_.program_deletes;
            break;
        default:
            ++self->mutation_audit_.denied;
            return false;
        }
        return true;
    } catch (...) {
        ++self->mutation_audit_.denied;
        return false;
    }
}

NativeGLPresentationOwner::NativeGLPresentationOwner(
    const ExternalGLCellCreateInfo& renderer,
    const NativeGLFenceConfig& fences)
    : host_config_(fences),
      fences_(fences),
      presenter_(create_external_gl_presenter(
          instrumented_info(renderer, this))),
      gate_(std::make_shared<CellHostFrameGate>(*presenter_)) {
    // NativeGLFrameFences validates actual current-context identity and
    // exact GL proc availability before creating the GPU renderer.
    if (!host_config_.max_total_texture_upload_bytes ||
        host_config_.max_total_texture_upload_bytes > (1024ull * 1024ull * 1024ull))
        throw std::invalid_argument(
            "TangoGL native texture upload payload limit must be bounded to at most 1 GiB");
    if (!presenter_ || !presenter_->on_owner_thread())
        throw std::logic_error(
            "TangoGL native owner requires an actual same-thread pixel renderer");
    if (renderer.get_proc_address != fences.get_proc_address ||
        renderer.proc_user_data != fences.proc_user_data)
        throw std::invalid_argument(
            "TangoGL pixel renderer and GPU fence owner must share one GL loader");
}

NativeGLPresentationOwner::~NativeGLPresentationOwner() noexcept {
    if (closed_) return;
    try {
        // If still in flight, NEVER run the presenter destructor and delete
        // its atlas/shader resources. A fatal failure is safer than a UAF.
        if (unfenced_failure_ || fences_.in_flight() != 0)
            std::terminate();
        close(); // Permitted only while the exact original GL context lives.
    } catch (...) {
        std::terminate();
    }
}

void NativeGLPresentationOwner::present_and_fence(
    const tui_grid_snapshot_t* snapshot, const CellPixelMetrics& metrics) {
    if (closed_ || unfenced_failure_ || !presenter_)
        throw std::logic_error(
            "TangoGL retired/unfenced GPU resource owner refuses presentation");
    // Check the native context, fence limit AND resource-mutation barrier
    // before gCanvas can edit any texture, glyph atlas or shader state.
    // Without per-resource GPU leases this particular presenter is serial.
    require_capacity();

    const auto before = presenter_->submission_id();
    try {
        presenter_->present(snapshot, metrics);
    } catch (...) {
        // Pre-submit refusal preserves the last successfully painted plan.
        // On any post-start native failure the presenter clears its plan.
        // With no previous submission the failure phase is unprovable,
        // so fail closed instead of deleting potentially in-flight assets.
        if (before == 0 || presenter_->submission_id() != before) {
            unfenced_failure_ = true;
            gate_->invalidate_geometry();
        }
        throw;
    }

    try {
        // Read actual GL-backed texture names and shader program from the
        // existing gCanvas Context, validated using glIsTexture/glIsProgram.
        // It requires the exact owning native context and a completed draw.
        const auto native_ids = presenter_->native_gpu_resource_ids();
        if (native_ids.empty() || native_ids.size() > 64u)
            throw std::length_error(
                "TangoGL native GPU resource lease inventory exceeds budget");
        const auto submission = presenter_->submission_id();
        const auto* plan = presenter_->last_submitted_plan();
        if (!submission || !plan || !plan->grid_revision())
            throw std::logic_error(
                "TangoGL resource leasing requires an actual completed C11 frame");

        bool shader = false;
        bool font_atlas = false;
        std::vector<NativeGLResourceLease> staged;
        staged.reserve(native_ids.size());
        for (const auto& id : native_ids) {
            if (!id.native_name)
                throw std::logic_error("TangoGL native resource lease has zero GPU ID");
            if (id.kind == NativeRendererResourceKind::ShaderProgram) {
                if (shader) throw std::logic_error("TangoGL has duplicate native shader");
                shader = true;
            } else if (id.kind == NativeRendererResourceKind::GlyphAtlas) {
                font_atlas = true;
            }
            staged.push_back({id.kind, id.native_name,
                              fences_.device_generation(), plan->grid_revision(),
                              submission, false});
        }
        if (!shader || !font_atlas)
            throw std::logic_error(
                "TangoGL actual GPU inventory is missing shader or glyph atlas");

        // A GLsync after draw follows all resource uses in this native
        // context. Assign leases only after the real fence was published;
        // vector::swap is noexcept and cannot leave partial resource IDs.
        fences_.record_submitted_frame(*presenter_);
        resource_leases_.swap(staged);
    } catch (...) {
        unfenced_failure_ = true;
        gate_->invalidate_geometry();
        throw;
    }
}

NativeGLFencePoll NativeGLPresentationOwner::retire_completed(
    std::uint64_t timeout_nanoseconds) {
    if (closed_)
        throw std::logic_error("TangoGL owner cannot poll after retirement");
    const auto outcome = fences_.retire_completed(timeout_nanoseconds);
    if (outcome == NativeGLFencePoll::Drained) {
        const auto completed = fences_.last_retired_submission();
        for (auto& lease : resource_leases_) {
            if (lease.device_generation != fences_.device_generation() ||
                !lease.submission_id || lease.submission_id > completed)
                throw std::logic_error(
                    "TangoGL GPU object retired outside its native device/fence generation");
            lease.gpu_completed = true;
        }
    }
    return outcome;
}

void NativeGLPresentationOwner::require_capacity() const {
    if (closed_ || unfenced_failure_)
        throw std::logic_error(
            "TangoGL resource owner is retired or has an unfenced failure");
    // Preserve the NativeGLFrameFences exact-context/owner/capacity gate.
    // Even if the configured queue has capacity for multiple GLsyncs, the
    // shared gCanvas glyph atlas and texture state cannot be mutated while
    // ANY prior GPU submission still references those resources. No
    // fallback glFinish or guessed GPU completion; callers explicitly
    // retire native fences before presenting another frame.
    fences_.require_capacity();
    if (fences_.in_flight() != 0)
        throw std::logic_error(
            "TangoGL cannot mutate shared atlas/texture/shader while a prior GPU frame is in flight");
    for (const auto& lease : resource_leases_) {
        if (!lease.gpu_completed)
            throw std::logic_error(
                "TangoGL cannot mutate a native GPU object whose resource lease is not retired");
    }
}

std::size_t NativeGLPresentationOwner::in_flight() const {
    return fences_.in_flight();
}

std::uint64_t NativeGLPresentationOwner::last_retired_submission() const {
    return fences_.last_retired_submission();
}

std::uint64_t NativeGLPresentationOwner::device_generation() const noexcept {
    return fences_.device_generation();
}

const CellPixelPresenter& NativeGLPresentationOwner::submitted_presenter() const {
    if (closed_ || !presenter_)
        throw std::logic_error(
            "TangoGL retired resource owner no longer has a pixel presenter");
    return *presenter_;
}

std::vector<NativeGLResourceLease>
NativeGLPresentationOwner::gpu_resource_leases() const {
    (void)fences_.in_flight(); // Owner-thread-only access.
    return resource_leases_;
}

std::shared_ptr<CellHostFrameGate>
NativeGLPresentationOwner::input_gate() const noexcept {
    return gate_;
}

void NativeGLPresentationOwner::close() {
    if (closed_) return;
    if (unfenced_failure_)
        throw std::logic_error(
            "TangoGL cannot destroy gCanvas assets after unfenced native failure");
    // Requires the ORIGINAL native context + owner and zero in-flight GLsync.
    // Also reject deleting any actual gCanvas shader/atlas/image resource
    // whose last GPU usage has not been proven completed by its native fence.
    for (const auto& lease : resource_leases_)
        if (!lease.gpu_completed)
            throw std::logic_error(
                "TangoGL cannot destroy a shader/atlas/image before its GPU lease retires");
    // On failure the renderer stays owned and alive, never half-retired.
    fences_.close();
    gate_->retire_device(); // Retire callbacks before deleting GPU assets.
    presenter_.reset();     // Actual gCanvas GL delete, with context current.
    closed_ = true;
}

bool NativeGLPresentationOwner::closed() const noexcept {
    return closed_;
}

NativeGLMutationAudit NativeGLPresentationOwner::native_mutation_audit() const {
    (void)fences_.in_flight(); // Native owner-thread-only audit read.
    return mutation_audit_;
}

} // namespace flex::tango_opengl
