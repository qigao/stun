/*
 * Single native TangoGL GPU resource owner over the existing gCanvas
 * presenter and real GLsync queue. The GL API stays in gl_fence.cpp.
 * No extra GL renderer, parser, draw model, CSS or global loader.
 */
#include <backends/tango_opengl/gl_presenter.h>

#include <exception>
#include <stdexcept>
#include <utility>

namespace flex::tango_opengl {

NativeGLPresentationOwner::NativeGLPresentationOwner(
    const ExternalGLCellCreateInfo& renderer,
    const NativeGLFenceConfig& fences)
    : fences_(fences),
      presenter_(create_external_gl_presenter(renderer)),
      gate_(std::make_shared<CellHostFrameGate>(*presenter_)) {
    // NativeGLFrameFences validates actual current-context identity and
    // exact GL proc availability before creating the GPU renderer.
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
    // MUST check capacity and host-current identity before ANY GPU drawing.
    fences_.require_capacity();

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
        // All gCanvas shader/glyph draw commands have now been submitted.
        // A missing GLsync is an unsafe lifetime state, never a green frame.
        fences_.record_submitted_frame(*presenter_);
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
    return fences_.retire_completed(timeout_nanoseconds);
}

void NativeGLPresentationOwner::require_capacity() const {
    if (closed_ || unfenced_failure_)
        throw std::logic_error(
            "TangoGL resource owner is retired or has an unfenced failure");
    fences_.require_capacity();
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
    // On failure the renderer stays owned and alive, never half-retired.
    fences_.close();
    gate_->retire_device(); // Retire callbacks before deleting GPU assets.
    presenter_.reset();     // Actual gCanvas GL delete, with context current.
    closed_ = true;
}

bool NativeGLPresentationOwner::closed() const noexcept {
    return closed_;
}

} // namespace flex::tango_opengl
