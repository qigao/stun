/* Real interactive Tango + FlexUI Streaming demo.
 * q/Esc quit, c cancel, arrows/wheel browse, Home oldest, End follow tail.
 * Optional CI-only --headless-smoke uses a synthetic terminal output sink.
 * No new event loop, queue, Markdown parser, or cross-thread UI mutation.
 */
#include <backends/tui/init.h>
#include <backends/tui/transcript_window.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/label_widget.h>
#include <tui.h>
#include <tui_transcript.h>

#if defined(STUN_TUI_DEMO_HEADLESS_TEST)
#include "tui_app_internal.h"
#include "tui_terminal_internal.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr std::size_t kRetained = 12u;
constexpr std::size_t kVisible = 5u;
constexpr std::size_t kMaxChunk = 128u;
constexpr float kCellWidth = 8.0f;
constexpr float kCellHeight = 16.0f;

// The installed-SDK PTY fixture checks restoration before macOS revokes
// the controlling tty on process exit. No new Tango ABI or event loop.
#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && !defined(_WIN32)
bool same_pty_termios(const termios& lhs, const termios& rhs) {
    return lhs.c_iflag == rhs.c_iflag &&
           lhs.c_oflag == rhs.c_oflag &&
           lhs.c_cflag == rhs.c_cflag &&
           lhs.c_lflag == rhs.c_lflag &&
           std::memcmp(lhs.c_cc, rhs.c_cc, sizeof(lhs.c_cc)) == 0 &&
           cfgetispeed(&lhs) == cfgetispeed(&rhs) &&
           cfgetospeed(&lhs) == cfgetospeed(&rhs);
}
#endif

enum Command { Quit = 1, Older, Newer, Tail, Oldest, Cancel };

struct Demo {
    tui_app_t* app = nullptr;
    tui_transcript_t* transcript = nullptr;
    std::unique_ptr<flex::Renderer> renderer;
    std::unique_ptr<flexUI::Box> box;
    std::unique_ptr<flex::tui_backend::TranscriptWindow> window;
    flexUI::Element* viewport = nullptr; // borrowed while Box is alive
    flexUI::LabelWidget* status = nullptr;
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> cancel_target{0u};
    std::atomic<bool> producer_done{false};
    std::uint64_t active_message = 0u;
    // UI-owner-only progress, corresponding to an accepted END/CANCEL.
    // Unlike latest BEGIN, this marks an actually settled message.
    std::uint64_t last_settled_message = 0u;
    std::uint64_t anchor = 0u;
    std::size_t accepted = 0u;
    std::size_t rejected = 0u;
    bool following = true;
    bool valid = true;
#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && defined(_WIN32)
    // Optional CI-only owner-cell diagnostic; no callbacks, event transport
    // or terminal output are changed. At most 96 bounded lines are written.
    std::string frame_report_path;
    unsigned frame_report_count = 0u;
    unsigned frame_seen_count = 0u;
#endif
};

#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && defined(_WIN32)
void report_owned_ui_cells(Demo& d, const tui_buffer_t* buffer) {
    // Sample the *whole* session, including the last retained messages.
    // Avoid many synchronous file appends that change ConPTY scheduling,
    // and preserve a strict maximum of 96 diagnostic records.
    if (d.frame_report_path.empty() || d.frame_report_count >= 96u ||
        !buffer || buffer->height <= 0 || buffer->width <= 0) return;
    if ((d.frame_seen_count++ % 8u) != 0u) return;
    const auto row_ascii = [&](int row) {
        std::string result;
        if (row < 0 || row >= buffer->height) return result;
        const int width = std::min(buffer->width, 54);
        for (int x = 0; x < width; ++x) {
            const auto* cell = tui_buffer_at_const(buffer, x, row);
            result.push_back(cell->ch >= 0x20u && cell->ch < 0x7fu ?
                static_cast<char>(cell->ch) : '?');
        }
        return result;
    };
    const auto contains = [&](const char* needle) {
        const auto length = std::strlen(needle);
        if (length > static_cast<std::size_t>(buffer->width)) return false;
        for (int y = 0; y < buffer->height; ++y)
            for (int x = 0; x + static_cast<int>(length) <= buffer->width; ++x) {
                bool equal = true;
                for (std::size_t k = 0; k < length; ++k) {
                    if (tui_buffer_at_const(buffer, x + static_cast<int>(k), y)->ch !=
                            static_cast<std::uint32_t>(
                                static_cast<unsigned char>(needle[k]))) {
                        equal = false;
                        break;
                    }
                }
                if (equal) return true;
            }
        return false;
    };
    FILE* report = std::fopen(d.frame_report_path.c_str(), "a");
    if (!report) return;
    const auto top = row_ascii(0);
    const auto lower = row_ascii(buffer->height - 2);
    const auto bottom = row_ascii(buffer->height - 1);
    (void)std::fprintf(report,
        "accepted=%zu settled=%llu retained=%zu width=%d height=%d "
        "header=%d footer=%d top='%s' lower='%s' bottom='%s'\n",
        d.accepted,
        static_cast<unsigned long long>(d.last_settled_message),
        tui_transcript_count(d.transcript),
        buffer->width, buffer->height,
        contains("FlexUI + Tango") ? 1 : 0,
        contains("Retained ") ? 1 : 0,
        top.c_str(), lower.c_str(), bottom.c_str());
    (void)std::fclose(report);
    ++d.frame_report_count;
}
#endif

int map_input(const tui_event_t* e) {
    if (!e) return -1;
    if (e->type == TUI_EVENT_MOUSE_PRESS) {
        if (e->key == TUI_KEY_MOUSE_WHEEL_UP) return Older;
        if (e->key == TUI_KEY_MOUSE_WHEEL_DOWN) return Newer;
    }
    if (e->type != TUI_EVENT_KEY) return -1;
    if (e->key == TUI_KEY_ESCAPE || e->ch == 'q' ||
        tui_event_is_ctrl(e, 'c')) return Quit;
    if (e->ch == 'c') return Cancel;
    switch (e->key) {
        case TUI_KEY_UP: case TUI_KEY_PAGE_UP: return Older;
        case TUI_KEY_DOWN: case TUI_KEY_PAGE_DOWN: return Newer;
        case TUI_KEY_HOME: return Oldest;
        case TUI_KEY_END: return Tail;
        default: return -1;
    }
}

std::uint64_t id_at(tui_transcript_t* t, std::size_t index) {
    tui_transcript_message_t value{};
    std::size_t written = 0u;
    return tui_transcript_window(t, index, 1u, &value, 1u, &written) ==
        TUI_TRANSCRIPT_OK && written == 1u ? value.message_id : 0u;
}

bool on_key(int command, void* ptr) {
    auto& d = *static_cast<Demo*>(ptr);
    if (command == Quit) {
        d.stop.store(true, std::memory_order_release);
        return false;
    }
    if (command == Cancel) {
        if (d.active_message)
            d.cancel_target.store(d.active_message, std::memory_order_release);
        return true;
    }
    if (command == Tail) {
        d.following = true;
        d.anchor = 0u;
        return true;
    }
    const std::size_t count = tui_transcript_count(d.transcript);
    if (!count) return true;
    if (command == Oldest) {
        d.following = false;
        d.anchor = id_at(d.transcript, 0u);
        return true;
    }
    if (command != Older && command != Newer) return true;
    const std::size_t tail_start = count > kVisible ? count - kVisible : 0u;
    std::size_t first = tail_start;
    if (!d.following && d.anchor) {
        for (std::size_t i = 0u; i < count; ++i) {
            if (id_at(d.transcript, i) >= d.anchor) {
                first = i;
                break;
            }
        }
    }
    if (command == Older && first) --first;
    if (command == Newer && first < tail_start) ++first;
    d.following = first >= tail_start;
    d.anchor = d.following ? 0u : id_at(d.transcript, first);
    return true;
}

bool on_async(const tui_async_event_t* event, const void* packet, void* ptr) {
    auto& d = *static_cast<Demo*>(ptr);
    if (!event || !packet ||
        event->payload_size < sizeof(tui_stream_packet_header_t))
        return d.valid = false;
    tui_stream_packet_header_t header{};
    std::memcpy(&header, packet, sizeof(header));
    if (event->type == TUI_STREAM_BEGIN) {
        tui_transcript_message_t value{};
        const auto found = tui_transcript_get(
            d.transcript, header.message_id, &value);
        if (found == TUI_TRANSCRIPT_UNKNOWN) {
            std::uint64_t evicted = 0u;
            if (tui_transcript_open(d.transcript, header.message_id,
                                    &evicted) != TUI_TRANSCRIPT_OK)
                return d.valid = false;
        } else if (found != TUI_TRANSCRIPT_OK) {
            ++d.rejected;
            return true;
        }
    }
    const auto status = tui_transcript_apply(
        d.transcript, event, packet, nullptr);
    if (status == TUI_TRANSCRIPT_STALE ||
        status == TUI_TRANSCRIPT_UNKNOWN) {
        ++d.rejected;
        return true;
    }
    if (status != TUI_TRANSCRIPT_OK) return d.valid = false;
    ++d.accepted;
    if (event->type == TUI_STREAM_BEGIN)
        d.active_message = header.message_id;
    else if (event->type == TUI_STREAM_END ||
             event->type == TUI_STREAM_CANCEL) {
        d.last_settled_message = header.message_id;
        if (d.active_message == header.message_id)
            d.active_message = 0u;
    }
    return true;
}

void on_view(const void* ptr, tui_buffer_t* back) {
    auto& d = *const_cast<Demo*>(static_cast<const Demo*>(ptr));
    try {
        const auto* term = tui_app_terminal(d.app);
        if (!term || !d.window || !d.box) {
            d.valid = false;
            return;
        }
        const int w = tui_terminal_width(term);
        const int h = tui_terminal_height(term);
        if (w > 0 && h > 0)
            d.box->set_viewport(w * kCellWidth, h * kCellHeight);
        const auto status = d.following
            ? d.window->show_tail(d.transcript)
            : d.window->show_from_id(d.transcript, d.anchor);
        using WindowStatus = flex::tui_backend::TranscriptWindow::Status;
        if (status == WindowStatus::QueryRejected ||
            status == WindowStatus::Invalid) {
            d.valid = false;
            (void)tui_async_queue_close(tui_app_async_queue(d.app));
            return;
        }
        if (d.status) {
            d.status->set_text("Retained " +
                std::to_string(tui_transcript_count(d.transcript)) +
                "/12 | " + (d.following ? "tail" : "scrollback") +
                " | " + std::to_string(w) + "x" + std::to_string(h) +
                " | settled=" + std::to_string(d.last_settled_message) +
                " | q quit, c cancel, arrows/wheel browse, End follow");
        }
        d.box->update();
#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && defined(_WIN32)
        report_owned_ui_cells(d, back);
#endif
    } catch (const std::exception&) {
        d.valid = false;
        (void)tui_async_queue_close(tui_app_async_queue(d.app));
    }
}

bool initialize(Demo& d) {
    d.app = tui_app_create(&d, on_key, on_view, map_input);
    if (!d.app || !tui_app_set_async_update(d.app, on_async)) return false;
    d.transcript = tui_transcript_create(kRetained, 8192u, kMaxChunk);
    if (!d.transcript) return false;
    d.renderer = flex::tui_backend::create_renderer(tui_app_terminal(d.app));
    if (!d.renderer) return false;
    d.box = std::make_unique<flexUI::Box>(d.renderer.get());
    d.box->set_viewport(640.0f, 384.0f);
    d.box->load_css(R"(
        #root { width:100%;height:100%;display:flex;
                flex-direction:column;background-color:#151923; }
        #title { height:28px;flex-shrink:0;color:#7aa2f7;
                 font-size:16px; }
        #transcript { width:100%;flex:1;min-height:0px;
                      overflow-y:scroll;display:flex;
                      flex-direction:column; }
        .transcript-message { width:100%;flex-shrink:0;
                              color:#d3daed;font-size:14px; }
        #status { height:26px;flex-shrink:0;font-size:12px;
                  color:#95a2b8; }
    )");
    auto* root = d.box->create("div", "root");
    auto* heading = d.box->create_widget<flexUI::LabelWidget>(
        "div", "title", "FlexUI + Tango  |  bounded Streaming chat");
    auto* viewport = d.box->create("div", "transcript");
    auto* footer = d.box->create_widget<flexUI::LabelWidget>(
        "div", "status", "Streaming...");
    if (!root || !heading || !viewport || !footer || !footer->widget)
        return false;
    root->append(heading);
    root->append(viewport);
    root->append(footer);
    d.box->set_root(root);
    d.viewport = viewport;
    d.status = static_cast<flexUI::LabelWidget*>(footer->widget);
    d.window = std::make_unique<flex::tui_backend::TranscriptWindow>(
        *d.box, *viewport, kVisible);
    return true;
}

void shutdown(Demo& d) {
    // Every worker/API call is quiescent before the native owner is freed.
    d.window.reset();
    d.viewport = nullptr;
    d.box.reset();
    d.renderer.reset();
    if (d.transcript) (void)tui_transcript_destroy(d.transcript);
    d.transcript = nullptr;
    if (d.app) tui_app_destroy(d.app);
    d.app = nullptr;
}

bool publish(Demo& d, std::uint64_t id, std::uint32_t type,
             std::string_view text) {
    if (text.size() > kMaxChunk) return false;
    std::array<unsigned char, sizeof(tui_stream_packet_header_t) + kMaxChunk>
        packet{};
    const tui_stream_packet_header_t header{id};
    std::memcpy(packet.data(), &header, sizeof(header));
    if (!text.empty())
        std::memcpy(packet.data() + sizeof(header),
                    text.data(), text.size());
    auto* queue = tui_app_async_queue(d.app);
    if (!queue) return false;
    while (!d.stop.load(std::memory_order_acquire)) {
        const auto result = tui_async_queue_post(
            queue, type, 1u, packet.data(), sizeof(header) + text.size(),
            nullptr);
        if (result == TUI_ASYNC_OK) return true;
        if (result != TUI_ASYNC_FULL) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    return false;
}

void producer(Demo& d, std::uint64_t first_id,
              std::uint64_t final_id, std::chrono::milliseconds pause) {
    for (std::uint64_t id = first_id; id <= final_id &&
         !d.stop.load(std::memory_order_acquire); ++id) {
        if (!publish(d, id, TUI_STREAM_BEGIN, {})) break;
        const std::string content =
            "### Response " + std::to_string(id) +
            "\n\nThis **Markdown** arrives from a worker through the "
            "native Tango event loop.\n\n"
            "- Chinese: 中\n- Renderer: terminal cells\n"
            "- Retention is bounded; old messages are recycled.\n\n";
        bool cancelled = false;
        for (std::size_t offset = 0u; offset < content.size();
             offset += 24u) {
            if (d.cancel_target.load(std::memory_order_acquire) == id) {
                cancelled = true;
                break;
            }
            const std::size_t n = std::min<std::size_t>(
                24u, content.size() - offset);
            if (!publish(d, id, TUI_STREAM_CHUNK,
                         std::string_view(content.data() + offset, n))) {
                d.stop.store(true, std::memory_order_release);
                break;
            }
            if (pause.count()) std::this_thread::sleep_for(pause);
        }
        if (d.stop.load(std::memory_order_acquire)) break;
        if (!publish(d, id, cancelled ? TUI_STREAM_CANCEL :
                                       TUI_STREAM_END, {})) break;
        if (cancelled)
            d.cancel_target.store(0u, std::memory_order_release);
    }
    d.producer_done.store(true, std::memory_order_release);
}

#if defined(STUN_TUI_DEMO_HEADLESS_TEST)
bool fake_write(tui_terminal_t*, const char*, size_t) { return true; }

int headless_smoke() {
    Demo d;
    if (!initialize(d)) { shutdown(d); return 2; }
    auto* term = tui_app_terminal(d.app);
    term->width = 80;
    term->height = 24;
    term->front = tui_buffer_create(80, 24);
    term->back = tui_buffer_create(80, 24);
    if (!term->front || !term->back) { shutdown(d); return 3; }
    term->initialized = true;
    term->color_mode = TUI_COLOR_TRUECOLOR;
    term->write_output = fake_write;

    std::thread worker(producer, std::ref(d), 1u, 22u,
                       std::chrono::milliseconds(1));
    bool okay = true;
    std::size_t frames = 0u;
    for (std::size_t attempt = 0u; attempt < 8000u; ++attempt) {
        if (!tui_app_dispatch_once(d.app)) { okay = false; break; }
        ++frames;
        tui_async_queue_stats_t stats{};
        if (tui_async_queue_get_stats(tui_app_async_queue(d.app),
                                      &stats) != TUI_ASYNC_OK) {
            okay = false;
            break;
        }
        if (d.producer_done.load(std::memory_order_acquire) &&
            stats.queued == 0u) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (attempt == 7999u) okay = false;
    }
    worker.join();
    const bool at_tail = d.window && d.window->visible_count() == kVisible &&
        d.window->visible_host(22u) != nullptr &&
        d.viewport && d.viewport->can_scroll_y() &&
        std::abs(d.viewport->max_scroll_y() - d.viewport->scroll_y()) <= 2.0f;
    bool drew_text = false;
    const auto rendered = [&]() -> bool {
        if (!term->front || !term->front->cells) return false;
        const std::size_t n = static_cast<std::size_t>(term->front->width) *
                              static_cast<std::size_t>(term->front->height);
        for (std::size_t i = 0u; i < n; ++i) {
            if (term->front->cells[i].ch != 0u &&
                term->front->cells[i].ch != static_cast<std::uint32_t>(' '))
                return true;
        }
        return false;
    };
    drew_text = rendered();
    const bool bounded = d.valid && drew_text &&
        tui_transcript_count(d.transcript) == kRetained &&
        d.accepted > 40u && d.rejected == 0u;

    // Home must show the *start* of the oldest retained message even if the
    // previously visible window was at the end of the document.
    (void)on_key(Oldest, &d);
    on_view(&d, tui_terminal_buffer(term));
    const bool browsed = !d.following && d.window &&
        d.window->visible_host(11u) != nullptr && d.viewport &&
        d.viewport->scroll_y() <= 0.01f;

    // One more producer starts after the view is anchored to an old message.
    // Message 23 evicts message 11, but must not force the user's history
    // window to jump to the tail. Trigger c while the worker is active.
    d.producer_done.store(false, std::memory_order_release);
    const std::size_t before_cancel = d.accepted;
    std::thread next(producer, std::ref(d), 23u, 23u,
                     std::chrono::milliseconds(25));
    bool cancelled_from_ui = false;
    bool settled = false;
    for (std::size_t attempt = 0u; attempt < 2000u; ++attempt) {
        if (!tui_app_dispatch_once(d.app)) { okay = false; break; }
        ++frames;
        if (!cancelled_from_ui && d.active_message == 23u &&
            d.accepted >= before_cancel + 2u) {
            cancelled_from_ui = on_key(Cancel, &d);
        }
        tui_async_queue_stats_t stats{};
        if (tui_async_queue_get_stats(tui_app_async_queue(d.app),
                                      &stats) != TUI_ASYNC_OK) {
            okay = false;
            break;
        }
        if (d.producer_done.load(std::memory_order_acquire) &&
            stats.queued == 0u) {
            settled = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    d.stop.store(true, std::memory_order_release);
    next.join();
    tui_transcript_message_t cancelled{};
    const bool did_cancel = cancelled_from_ui && settled &&
        tui_transcript_get(d.transcript, 23u, &cancelled) ==
            TUI_TRANSCRIPT_OK &&
        cancelled.stream.state == TUI_STREAM_CANCELLED;
    const bool anchored = !d.following && d.window &&
        d.window->visible_host(12u) != nullptr &&
        d.window->visible_host(23u) == nullptr;

    // End must jump to the newest cell-visible message and bottom of the
    // viewport, even if the window content hasn't changed since its last
    // display (same-frame navigation / repeated End).
    (void)on_key(Tail, &d);
    on_view(&d, tui_terminal_buffer(term));
    const bool returned = d.following && d.window &&
        d.window->visible_host(23u) != nullptr && d.viewport &&
        std::abs(d.viewport->max_scroll_y() - d.viewport->scroll_y()) <= 2.0f;
    (void)on_key(Oldest, &d);
    on_view(&d, tui_terminal_buffer(term));
    const bool returned_oldest = !d.following &&
        d.window->visible_host(12u) != nullptr &&
        d.viewport->scroll_y() <= 0.01f;

    // Reflow the actual terminal's two cell buffers after a size change.
    // The TUI renderer must borrow the current back buffer each frame,
    // and the Box viewport must agree with the new cell geometry.
    term->width = 52;
    term->height = 16;
    // Exercise the same private pre-view buffer reconfiguration that
    // tui_app_dispatch_once uses for a real POSIX PTY or Win32 ConPTY resize.
    const bool prepared = tui_terminal_prepare_frame(term);
    if (prepared) on_view(&d, tui_terminal_buffer(term));
    const bool resized = prepared && d.box &&
        term->front->width == 52 && term->front->height == 16 &&
        term->back->width == 52 && term->back->height == 16 &&
        d.box->viewport_width() == 52.0f * kCellWidth &&
        d.box->viewport_height() == 16.0f * kCellHeight &&
        tui_terminal_render(term) && rendered();
    term->initialized = false; // synthetic output never changed OS console
    shutdown(d);
    if (!okay || !at_tail || !bounded || !browsed || !did_cancel ||
        !anchored || !returned || !returned_oldest || !resized ||
        frames < 3u)
        return 4;
    std::puts("Tango/FlexUI streaming demo headless smoke passed");
    return 0;
}
#endif

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--help") == 0) {
        std::puts("flexui_tui_streaming_demo [--help | --headless-smoke]\n"
                  "q/Esc quit, c cancel, Up/Down or wheel browse, "
                  "Home oldest, End follow tail");
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "--headless-smoke") == 0) {
#if defined(STUN_TUI_DEMO_HEADLESS_TEST)
        return headless_smoke();
#else
        std::fputs("headless smoke requires CI-only test build\n", stderr);
        return 2;
#endif
    }
    bool frame_diagnostic = false;
#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && defined(_WIN32)
    frame_diagnostic = argc == 3 &&
        std::strcmp(argv[1], "--frame-report") == 0;
#endif
    if (argc != 1 && !frame_diagnostic) return 2;
    Demo d;
#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && defined(_WIN32)
    if (frame_diagnostic) d.frame_report_path = argv[2];
#endif
    try {
        if (!initialize(d)) { shutdown(d); return 1; }
#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && !defined(_WIN32)
        const bool verify_pty =
            std::getenv("STUN_TUI_PTY_TERMIOS_CHECK") != nullptr;
        struct termios original_pty{};
        if (verify_pty && tcgetattr(STDIN_FILENO, &original_pty) != 0) {
            shutdown(d);
            return 1;
        }
#endif
        std::thread worker(producer, std::ref(d), 1u, 48u,
                           std::chrono::milliseconds(28));
        tui_app_run(d.app);
        d.stop.store(true, std::memory_order_release);
        worker.join();
        bool success = d.valid;
#if defined(STUN_TUI_DEMO_HEADLESS_TEST) && !defined(_WIN32)
        if (verify_pty) {
            // tui_app_run has completed terminal_cleanup(), but STDIN is
            // still an open controlling PTY: no post-exit ENOTTY race.
            struct termios restored_pty{};
            const bool restored =
                tcgetattr(STDIN_FILENO, &restored_pty) == 0 &&
                same_pty_termios(original_pty, restored_pty);
            if (restored) {
                if (std::fputs("TANGO_PTY_TERMIOS_RESTORED\n", stdout) < 0 ||
                    std::fflush(stdout) != 0)
                    success = false;
            } else {
                success = false;
            }
        }
#endif
        shutdown(d);
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        d.stop.store(true, std::memory_order_release);
        std::fprintf(stderr, "Streaming TUI: %s\n", e.what());
        shutdown(d);
        return 1;
    }
}
