#include <tinytest.hpp>
#undef group

#include <backends/tui/init.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <tui.h>
#include "tui_async_wait_internal.h"
#include "tui_app_internal.h"
#include "tui_terminal_internal.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr uint64_t kMessageId = 42u;
#ifdef _WIN32
constexpr intptr_t kNoInputHandle = 0;
#else
constexpr intptr_t kNoInputHandle = -1;
#endif

bool submit(tui_async_queue_t* queue, uint32_t type, uint64_t generation,
            std::string_view chunk) {
    constexpr std::size_t kMaxChunk = 32u;
    if (chunk.size() > kMaxChunk) return false;
    std::array<unsigned char, sizeof(tui_stream_packet_header_t) + kMaxChunk>
        packet{};
    const tui_stream_packet_header_t header{kMessageId};
    std::memcpy(packet.data(), &header, sizeof(header));
    if (!chunk.empty())
        std::memcpy(packet.data() + sizeof(header), chunk.data(), chunk.size());
    return tui_async_queue_post(queue, type, generation, packet.data(),
             sizeof(header) + chunk.size(), nullptr) == TUI_ASYNC_OK;
}

bool contains_visible_cells(const tui_buffer_t* buffer,
                            uint32_t left = 'A', uint32_t right = 'B') {
    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x + 3 < buffer->width; ++x) {
            const auto* a = tui_buffer_at_const(buffer, x, y);
            const auto* cjk = tui_buffer_at_const(buffer, x + 1, y);
            const auto* second = tui_buffer_at_const(buffer, x + 2, y);
            const auto* b = tui_buffer_at_const(buffer, x + 3, y);
            if (a && cjk && second && b &&
                a->ch == left && cjk->ch == 0x4e2du &&
                cjk->span == TUI_CELL_SPAN_WIDE &&
                second->span == TUI_CELL_SPAN_CONTINUATION &&
                b->ch == right) return true;
        }
    }
    return false;
}


struct FlexUIOwner {
    tui_stream_t* stream = nullptr;
    flexUI::MarkdownWidget* markdown = nullptr;
    flexUI::Box* box = nullptr;
    flexUI::Element* host = nullptr;
    std::size_t views = 0u;
    std::size_t chunks = 0u;
    bool ended = false;
    bool ok = true;
};
bool keyboard_noop(int, void*) { return true; }
int input_noop(const tui_event_t*) { return -1; }
void view_noop(const void*, tui_buffer_t*) {}
bool mock_terminal_write(tui_terminal_t*, const char*, size_t) {
    return true;
}
void owner_flexui_view(const void* context, tui_buffer_t* back) {
    auto* owner = const_cast<FlexUIOwner*>(
        static_cast<const FlexUIOwner*>(context));
    if (!owner || !back || !owner->box || !owner->host ||
        !owner->markdown) {
        if (owner) owner->ok = false;
        return;
    }
    ++owner->views;
    owner->markdown->update(0.0f, *owner->host);
    owner->box->invalidate();
    owner->box->update();
}
bool owner_async_update(const tui_async_event_t* event,
                        const void* payload, void* context) {
    auto* owner = static_cast<FlexUIOwner*>(context);
    const auto status = tui_stream_apply(owner->stream, event, payload);
    if (status == TUI_STREAM_STALE) return true;
    if (status != TUI_STREAM_OK) {
        owner->ok = false;
        return false;
    }
    if (event->type == TUI_STREAM_BEGIN) {
        owner->markdown->set_markdown("");
    } else if (event->type == TUI_STREAM_CHUNK) {
        const auto prefix = sizeof(tui_stream_packet_header_t);
        if (event->payload_size < prefix) {
            owner->ok = false;
            return false;
        }
        const auto delta = std::string_view(
            static_cast<const char*>(payload) + prefix,
            event->payload_size - prefix);
        if (owner->markdown->append_markdown(delta) !=
            flexUI::MarkdownAppendStatus::Appended) {
            owner->ok = false;
            return false;
        }
        ++owner->chunks;
    } else if (event->type == TUI_STREAM_END) {
        owner->ended = true;
    }
    return true;
}

} // namespace

spec("FlexUI MarkdownWidget through Tango async transport and cell renderer") {
    it("consumes a worker-produced UTF-8 stream on one owner and draws real cells") {
        std::unique_ptr<tui_buffer_t, decltype(&tui_buffer_destroy)> buffer(
            tui_buffer_create(80, 24), &tui_buffer_destroy);
        check(buffer != nullptr);
        if (!buffer) return;

        auto renderer = flex::tui_backend::create_buffer_renderer(buffer.get());
        check(renderer != nullptr);
        check(flex::tui_backend::create_buffer_renderer(nullptr) == nullptr);
        if (!renderer) return;

        flexUI::Box box(renderer.get());
        box.set_viewport(80.0f * 8.0f, 24.0f * 16.0f);
        box.load_css(
            "#root { display: flex; flex-direction: column; width: 100%; "
            "height: 100%; } "
            "#stream-output { width: 100%; }"
        );
        auto* root = box.create("div", "root");
        auto* host = box.create_widget<flexUI::MarkdownWidget>(
            "div", "stream-output", "");
        check(root != nullptr && host != nullptr && host->widget != nullptr);
        if (!root || !host || !host->widget) return;
        root->append(host);
        box.set_root(root);
        auto* widget = static_cast<flexUI::MarkdownWidget*>(host->widget);

        tui_stream_t* stream = tui_stream_create(kMessageId, 512u, 32u);
        check(stream != nullptr);
        if (!stream) return;
        FlexUIOwner owner{stream, widget};
        tui_app_t* app = tui_app_create(
            &owner, keyboard_noop, view_noop, input_noop);
        check(app != nullptr);
        if (!app) {
            (void)tui_stream_destroy(stream);
            return;
        }
        check(tui_app_set_async_update(app, owner_async_update));
        tui_async_queue_t* queue = tui_app_async_queue(app);
        check(queue != nullptr && tui_async_queue_is_owner(queue));
        if (!queue) {
            tui_app_destroy(app);
            (void)tui_stream_destroy(stream);
            return;
        }

        bool producer_ok = false;
        std::thread worker([&] {
            // The UI model and widget are never touched from this producer.
            // Schedule real cross-thread publication after the owner begins
            // waiting on the POSIX wake pipe or Windows waitable event.
            std::this_thread::sleep_for(std::chrono::milliseconds(12));
            const char cjk[] = "中";
            producer_ok = submit(queue, TUI_STREAM_BEGIN, 1u, {}) &&
                submit(queue, TUI_STREAM_CHUNK, 1u, "A") &&
                submit(queue, TUI_STREAM_CHUNK, 1u,
                       std::string_view(cjk, 1u)) &&
                submit(queue, TUI_STREAM_CHUNK, 1u,
                       std::string_view(cjk + 1u, 2u)) &&
                submit(queue, TUI_STREAM_CHUNK, 1u, "B") &&
                submit(queue, TUI_STREAM_END, 1u, {});
        });
        // Start empty: only the queue's kernel wake endpoint can unblock
        // this owner wait; no busy polling, synthetic wake, or test condition
        // variable is involved. The queue checks the predicate under its
        // publication lock just before sleeping (post-before-wait is safe).
        const auto wake_result = tui_async_wait_io(
            queue, kNoInputHandle, 1000);
        worker.join();
        check(producer_ok);
        check(wake_result == TUI_ASYNC_WAIT_MESSAGE);

        // Use the production Tango app's owner-affine message dispatch,
        // not a test-owned queue-draining implementation. One owner turn
        // consumes the full burst, then FlexUI lays out/renders once.
        check(tui_app_pump_messages(app, 32u) == 6u);
        check(owner.ok && owner.ended && owner.chunks == 4u);

        std::size_t text_size = 0u;
        const char* text = tui_stream_text(stream, &text_size);
        check(text != nullptr);
        if (text) check(std::string(text, text_size) == "A中B");
        check(widget->markdown() == "A中B");

        // One FlexUI owner update for the entire producer burst. The real
        // MarkdownWidget publishes its C11 RichDoc-backed Element projection.
        widget->update(0.0f, *host);
        check(host->child_count() == 1u);
        if (host->child_count() == 1u) {
            const auto* p = host->child_at(0u);
            check(p->has_class(flex::Symbol("md-p")));
            check(p->text() == "A中B");
        }

        box.invalidate();
        box.update();
        // The real FlexUI RenderManager → flex_backend_tui → Tango cell
        // buffer must preserve the CJK wide-cell continuation.
        check(contains_visible_cells(buffer.get()));

        // Resize must use the same FlexUI layout pipeline and native Tango
        // renderer without recreating the message or its C11 projection.
        tui_buffer_resize(buffer.get(), 48, 16);
        check(buffer->width == 48 && buffer->height == 16);
        box.set_viewport(48.0f * 8.0f, 16.0f * 16.0f);
        box.invalidate();
        box.update();
        check(contains_visible_cells(buffer.get()));
        check(widget->markdown() == "A中B");

        // After terminal state, late data must not modify either consumer.
        const char late[] = "late";
        std::array<unsigned char, sizeof(tui_stream_packet_header_t)+4u> stale{};
        const tui_stream_packet_header_t header{kMessageId};
        std::memcpy(stale.data(), &header, sizeof(header));
        std::memcpy(stale.data()+sizeof(header), late, 4u);
        tui_stream_snapshot_t stream_snapshot{};
        check(tui_stream_get_snapshot(stream, &stream_snapshot) ==
              TUI_STREAM_OK);
        const tui_async_event_t old_chunk{
            TUI_STREAM_CHUNK, 1u, stream_snapshot.last_sequence + 1u,
            stale.size()};
        check(tui_stream_apply(stream, &old_chunk, stale.data()) ==
              TUI_STREAM_STALE);
        check(widget->markdown() == "A中B");

        tui_app_destroy(app);
        check(tui_stream_destroy(stream) == TUI_STREAM_OK);
    }

    it("runs real Tango wake, owner dispatch and FlexUI draw as one UI turn") {
        // No physical console: Tango's real terminal buffers and failure-
        // atomic terminal diff operate through a test-only output sink.
        std::unique_ptr<tui_stream_t, decltype(&tui_stream_destroy)> stream(
            tui_stream_create(kMessageId, 512u, 32u), &tui_stream_destroy);
        check(stream != nullptr);
        if (!stream) return;
        FlexUIOwner owner{};
        owner.stream = stream.get();
        std::unique_ptr<tui_app_t, decltype(&tui_app_destroy)> app(
            tui_app_create(&owner, keyboard_noop, owner_flexui_view,
                           input_noop), &tui_app_destroy);
        check(app != nullptr);
        if (!app) return;
        auto* terminal = tui_app_terminal(app.get());
        terminal->width = 80;
        terminal->height = 24;
        terminal->front = tui_buffer_create(80, 24);
        terminal->back = tui_buffer_create(80, 24);
        check(terminal->front && terminal->back);
        if (!terminal->front || !terminal->back) return;
        terminal->initialized = true;
        terminal->color_mode = TUI_COLOR_TRUECOLOR;
        terminal->write_output = mock_terminal_write;

        auto renderer = flex::tui_backend::create_renderer(terminal);
        check(renderer != nullptr);
        if (!renderer) {
            terminal->initialized = false;
            return;
        }
        flexUI::Box box(renderer.get());
        owner.box = &box;
        box.set_viewport(80.0f * 8.0f, 24.0f * 16.0f);
        box.load_css("#root { width:100%; height:100%; display:flex; "
                     "flex-direction:column; } "
                     "#stream-output { width: 100%; }");
        auto* root = box.create("div", "root");
        auto* host = box.create_widget<flexUI::MarkdownWidget>(
            "div", "stream-output", "");
        check(root && host && host->widget);
        if (!root || !host || !host->widget) {
            terminal->initialized = false;
            return;
        }
        root->append(host);
        box.set_root(root);
        owner.host = host;
        owner.markdown =
            static_cast<flexUI::MarkdownWidget*>(host->widget);
        check(tui_app_set_async_update(app.get(), owner_async_update));
        auto* queue = tui_app_async_queue(app.get());
        check(queue != nullptr);
        if (!queue) {
            terminal->initialized = false;
            return;
        }
        app->needs_redraw = false;

        bool posted = false;
        std::thread producer([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(12));
            posted = submit(queue, TUI_STREAM_BEGIN, 1u, {}) &&
                submit(queue, TUI_STREAM_CHUNK, 1u, "A") &&
                submit(queue, TUI_STREAM_CHUNK, 1u, "中") &&
                submit(queue, TUI_STREAM_CHUNK, 1u, "B") &&
                submit(queue, TUI_STREAM_END, 1u, {});
        });
        const auto wake = tui_async_wait_io(queue, kNoInputHandle, 1000);
        producer.join();
        check(posted && wake == TUI_ASYNC_WAIT_MESSAGE);
        check(tui_app_dispatch_once(app.get()));
        check(owner.ok && owner.ended && owner.chunks == 3u);
        check(owner.views == 1u);
        check(contains_visible_cells(terminal->front));
        check(!app->needs_redraw);

        // An idle owner turn must not rebuild/redraw on a periodic timer.
        check(tui_app_dispatch_once(app.get()));
        check(owner.views == 1u);

        // Publication before entering the kernel wait uses the mailbox
        // predicate and is not lost. The same owner completes a second frame.
        check(submit(queue, TUI_STREAM_BEGIN, 2u, {}));
        check(submit(queue, TUI_STREAM_CHUNK, 2u, "C中D"));
        check(submit(queue, TUI_STREAM_END, 2u, {}));
        check(tui_async_wait_io(queue, kNoInputHandle, 0) ==
              TUI_ASYNC_WAIT_MESSAGE);
        check(tui_app_dispatch_once(app.get()));
        check(owner.ok && owner.views == 2u);
        check(owner.markdown->markdown() == "C中D");
        check(contains_visible_cells(terminal->front, 'C', 'D'));
        terminal->initialized = false; // Synthetic terminal: no OS restore
    }

    it("converges incomplete GFM tables and fenced code across owner turns") {
        flexUI::Box box(nullptr);
        auto* root = box.create("div", "markdown-batches");
        auto* host = box.create_widget<flexUI::MarkdownWidget>(
            "div", "markdown", "");
        check(root && host && host->widget);
        if (!root || !host || !host->widget) return;
        root->append(host);
        box.set_root(root);
        auto* widget = static_cast<flexUI::MarkdownWidget*>(host->widget);

        const tui_async_queue_config_t config{16u, 128u};
        auto* queue = tui_async_queue_create(&config);
        auto* stream = tui_stream_create(kMessageId, 512u, 32u);
        check(queue && stream);
        if (!queue || !stream) {
            if (stream) (void)tui_stream_destroy(stream);
            if (queue) {
                (void)tui_async_queue_close(queue);
                (void)tui_async_queue_destroy(queue);
            }
            return;
        }

        const auto pump = [&](std::size_t expected) {
            std::array<unsigned char, 128u> payload{};
            tui_async_event_t event{};
            std::size_t accepted = 0u;
            for (;;) {
                auto result = tui_async_queue_try_pop(
                    queue, &event, payload.data(), payload.size());
                if (result == TUI_ASYNC_EMPTY) break;
                check(result == TUI_ASYNC_OK);
                if (result != TUI_ASYNC_OK) break;
                const auto stream_result = tui_stream_apply(
                    stream, &event, payload.data());
                if (stream_result == TUI_STREAM_STALE) continue;
                check(stream_result == TUI_STREAM_OK);
                if (stream_result != TUI_STREAM_OK) break;
                ++accepted;
                if (event.type == TUI_STREAM_BEGIN) {
                    widget->set_markdown("");
                } else if (event.type == TUI_STREAM_CHUNK) {
                    const auto prefix = sizeof(tui_stream_packet_header_t);
                    check(event.payload_size >= prefix);
                    if (event.payload_size < prefix) break;
                    const auto delta = std::string_view(
                        reinterpret_cast<const char*>(payload.data() + prefix),
                        event.payload_size - prefix);
                    check(widget->append_markdown(delta) ==
                          flexUI::MarkdownAppendStatus::Appended);
                }
            }
            check(accepted == expected);
            widget->update(0.0f, *host);
        };

        const std::string first = "# Title\n\n| H | V |\n|---|";
        const std::string second = "---|\n| 中 | A |\n\n";
        const std::string fence(3u, char(0x60));
        const std::string code_start = fence + "c\nint x = 1;\n";
        const std::string code_end = fence + "\n";
        const std::string source = first + second + code_start + code_end;

        check(submit(queue, TUI_STREAM_BEGIN, 4u, {}));
        check(submit(queue, TUI_STREAM_CHUNK, 4u, first));
        pump(2u);
        check(widget->markdown() == first);

        check(submit(queue, TUI_STREAM_CHUNK, 4u, second));
        check(submit(queue, TUI_STREAM_CHUNK, 4u, code_start));
        check(submit(queue, TUI_STREAM_CHUNK, 4u, code_end));
        check(submit(queue, TUI_STREAM_END, 4u, {}));
        pump(4u);
        check(widget->markdown() == source);
        check(host->child_count() == 3u);
        if (host->child_count() == 3u) {
            check(host->child_at(0u)->has_class(flex::Symbol("h1")));
            check(host->child_at(1u)->has_class(flex::Symbol("md-table")));
            check(host->child_at(2u)->has_class(flex::Symbol("md-code-block")));
        }
        flexUI::Box reference(nullptr);
        auto* root_reference = reference.create("div", "reference");
        auto* one_shot = reference.create_widget<flexUI::MarkdownWidget>(
            "div", "md-reference", source);
        check(root_reference && one_shot && one_shot->widget);
        if (root_reference && one_shot && one_shot->widget) {
            root_reference->append(one_shot);
            reference.set_root(root_reference);
            one_shot->widget->update(0.0f, *one_shot);
            check(host->child_count() == one_shot->child_count());
        }

        // A new generation replaces the current projection. Once cancelled,
        // a queued old chunk may still dequeue but must not reach FlexUI.
        check(submit(queue, TUI_STREAM_BEGIN, 5u, {}));
        check(submit(queue, TUI_STREAM_CHUNK, 5u, "# Cancelled\n"));
        check(submit(queue, TUI_STREAM_CANCEL, 5u, {}));
        check(submit(queue, TUI_STREAM_CHUNK, 5u, "late"));
        pump(3u);
        check(widget->markdown() == "# Cancelled\n");
        tui_stream_snapshot_t snapshot{};
        check(tui_stream_get_snapshot(stream, &snapshot) == TUI_STREAM_OK);
        check(snapshot.state == TUI_STREAM_CANCELLED);

        check(tui_async_queue_close(queue) == TUI_ASYNC_OK);
        check(tui_async_queue_destroy(queue) == TUI_ASYNC_OK);
        check(tui_stream_destroy(stream) == TUI_STREAM_OK);
    }
}
