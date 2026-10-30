#include <backends/tango_opengl/gl_presenter.h>
#include <flex/render/engines/gcanvas.h>

#include <stdexcept>
#include <glad/glad.h>
#include <exception>
#include <limits>
#include <thread>
#include <vector>
#include <utility>

namespace flex::tango_opengl {

std::unique_ptr<CellPixelPresenter> create_external_gl_presenter(
    const ExternalGLCellCreateInfo& info) {
    if (!info.get_proc_address || info.font_family.empty() ||
        info.font_path.empty() || info.bold_font_path.empty()) {
        throw std::invalid_argument(
            "TangoOpenGL requires the current GL context/loader and explicit regular/bold font files");
    }
    // Use the existing gCanvas OpenGL shaders/atlas context without
    // registering the general OpenGL factory as TangoOpenGL.
    auto pixel = render::engines::gcanvas::create_external_opengl_renderer(
        info.get_proc_address, info.proc_user_data);
    if (!pixel) {
        throw std::runtime_error("TangoOpenGL failed to create gCanvas GL renderer");
    }
    if (!pixel->register_font(info.font_family, info.font_path) ||
        !pixel->register_font(info.font_family + "-bold", info.bold_font_path)) {
        throw std::runtime_error(
            "TangoOpenGL cannot register the exact requested regular/bold fonts");
    }
    return std::make_unique<CellPixelPresenter>(std::move(pixel));
}


struct NativeGLFrameFences::Impl final {
    using FenceSyncFn = PFNGLFENCESYNCPROC;
    using ClientWaitSyncFn = PFNGLCLIENTWAITSYNCPROC;
    using DeleteSyncFn = PFNGLDELETESYNCPROC;
    using FlushFn = PFNGLFLUSHPROC;
    using GetErrorFn = PFNGLGETERRORPROC;

    struct Pending {
        GLsync fence = nullptr;
        std::uint64_t submission = 0;
        std::uint64_t revision = 0;
        std::uint64_t device = 0;
    };

    NativeGLFenceConfig config{};
    const std::thread::id owner = std::this_thread::get_id();
    FenceSyncFn fence_sync = nullptr;
    ClientWaitSyncFn client_wait = nullptr;
    DeleteSyncFn delete_sync = nullptr;
    FlushFn flush = nullptr;
    GetErrorFn get_error = nullptr;
    std::vector<Pending> pending;
    std::uint64_t last_recorded_submission = 0;
    std::uint64_t retired_submission = 0;
    bool closed = false;

    void require_current() const {
        if (std::this_thread::get_id() != owner)
            throw std::logic_error(
                "TangoGL GPU fence accessed outside its original UI/GL thread");
        if (!config.is_context_current(config.host_user_data))
            throw std::logic_error(
                "TangoGL GPU fence accessed with missing/foreign native GL context");
    }

    void require_open() const {
        require_current();
        if (closed) throw std::logic_error("TangoGL GPU fence owner already retired");
    }

    template <typename Fn>
    Fn load(const char* symbol) {
        const auto address = config.get_proc_address(config.proc_user_data, symbol);
        if (!address)
            throw std::runtime_error(
                std::string("TangoGL GL 3.2 fence entry point missing: ") + symbol);
        return reinterpret_cast<Fn>(address);
    }

    explicit Impl(const NativeGLFenceConfig& info) : config(info) {
        if (!config.get_proc_address || !config.is_context_current ||
            config.device_generation == 0 ||
            config.max_in_flight == 0 || config.max_in_flight > 8)
            throw std::invalid_argument(
                "TangoGL GPU fence requires bounded in-flight capacity and host identity");
        require_current();
        fence_sync = load<FenceSyncFn>("glFenceSync");
        client_wait = load<ClientWaitSyncFn>("glClientWaitSync");
        delete_sync = load<DeleteSyncFn>("glDeleteSync");
        flush = load<FlushFn>("glFlush");
        get_error = load<GetErrorFn>("glGetError");
        pending.reserve(config.max_in_flight);
    }

    ~Impl() noexcept {
        // Do NOT delete unsignaled GLsyncs, or free their owner with an
        // uncompleted GPU frame. Violating explicit drain/close is fatal;
        // no implicit glFinish, unbounded wait, or context-less GL deletion.
        if (!pending.empty()) std::terminate();
    }
};

NativeGLFrameFences::NativeGLFrameFences(const NativeGLFenceConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

NativeGLFrameFences::~NativeGLFrameFences() noexcept = default;

void NativeGLFrameFences::require_capacity() const {
    impl_->require_open();
    if (impl_->pending.size() >= impl_->config.max_in_flight)
        throw std::length_error(
            "TangoGL GPU pending fence capacity exhausted; retire before drawing");
    if (impl_->last_recorded_submission ==
        (std::numeric_limits<std::uint64_t>::max)())
        throw std::overflow_error("TangoGL GPU submission identity exhausted");
}

void NativeGLFrameFences::record_submitted_frame(
    const CellPixelPresenter& presenter) {
    require_capacity();
    if (!presenter.on_owner_thread())
        throw std::logic_error("TangoGL GPU source is not on its UI/GL owner");
    const auto* plan = presenter.last_submitted_plan();
    const auto submission = presenter.submission_id();
    if (!plan || !plan->grid_revision() ||
        !submission || submission <= impl_->last_recorded_submission)
        throw std::logic_error(
            "TangoGL GPU fence requires a distinct completed native pixel submission");
    if (impl_->get_error() != GL_NO_ERROR)
        throw std::runtime_error(
            "TangoGL GPU frame has an outstanding native GL error before fencing");
    GLsync sync = impl_->fence_sync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!sync)
        throw std::runtime_error("TangoGL GPU glFenceSync returned no native sync");
    // Record the GLsync immediately, BEFORE examining errors or flushing.
    // An error cannot turn an owned GPU resource into an untracked leak.
    impl_->pending.push_back({
        sync, submission, plan->grid_revision(), impl_->config.device_generation});
    impl_->last_recorded_submission = submission;
    if (impl_->get_error() != GL_NO_ERROR)
        throw std::runtime_error("TangoGL GPU glFenceSync reported a native error");
    impl_->flush();
    if (impl_->get_error() != GL_NO_ERROR)
        throw std::runtime_error("TangoGL GPU glFlush failed after fence publication");
}

NativeGLFencePoll NativeGLFrameFences::retire_completed(
    std::uint64_t timeout_nanoseconds) {
    impl_->require_open();
    // A single poll must have a bounded delay; callers may retry rather
    // than blocking the UI owner forever on an unresponsive GL driver.
    constexpr std::uint64_t kMaxPollNs = 100000000ULL; // 100ms
    if (timeout_nanoseconds > kMaxPollNs)
        throw std::invalid_argument("TangoGL fence poll exceeds 100ms budget");
    while (!impl_->pending.empty()) {
        const auto item = impl_->pending.front();
        if (item.device != impl_->config.device_generation ||
            !item.submission || !item.revision ||
            item.submission <= impl_->retired_submission)
            throw std::logic_error(
                "TangoGL fence owner detected an invalid device/submission generation");
        const GLenum waited = impl_->client_wait(
            item.fence, GL_SYNC_FLUSH_COMMANDS_BIT,
            static_cast<GLuint64>(timeout_nanoseconds));
        if (waited == GL_TIMEOUT_EXPIRED) return NativeGLFencePoll::Busy;
        if (waited != GL_ALREADY_SIGNALED && waited != GL_CONDITION_SATISFIED)
            throw std::runtime_error(
                "TangoGL native GL client sync wait failed; resource retirement unsafe");
        if (impl_->get_error() != GL_NO_ERROR)
            throw std::runtime_error(
                "TangoGL native GL client sync produced an error");
        impl_->delete_sync(item.fence);
        if (impl_->get_error() != GL_NO_ERROR)
            throw std::runtime_error(
                "TangoGL native GL sync retirement produced an error");
        impl_->retired_submission = item.submission;
        impl_->pending.erase(impl_->pending.begin());
    }
    return NativeGLFencePoll::Drained;
}

void NativeGLFrameFences::close() {
    impl_->require_open();
    if (!impl_->pending.empty())
        throw std::logic_error(
            "TangoGL GPU renderer cannot retire while native fences are in flight");
    impl_->closed = true;
}

std::size_t NativeGLFrameFences::in_flight() const {
    if (std::this_thread::get_id() != impl_->owner)
        throw std::logic_error("TangoGL GPU fence count is owner-only");
    return impl_->pending.size();
}

std::uint64_t NativeGLFrameFences::last_retired_submission() const {
    if (std::this_thread::get_id() != impl_->owner)
        throw std::logic_error("TangoGL GPU retired submission ID is owner-only");
    return impl_->retired_submission;
}

std::uint64_t NativeGLFrameFences::device_generation() const noexcept {
    return impl_->config.device_generation;
}

} // namespace flex::tango_opengl
