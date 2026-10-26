#include <atomic>
#include <future>

#include <tinytest.hpp>
#undef group

#include <backends/tui/init.h>
#include <backends/tui/transcript_window.h>
#include <backends/tui/transcript_height_index.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/stream_scroll_follow.h>
#include <tui.h>
#include <tui_transcript.h>
#include "tui_async_wait_internal.h"
#include "tui_app_internal.h"
#include "tui_terminal_internal.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr uint64_t kMessageId = 42u;
// Namespace scope avoids MSVC C++17 lambda capture of an array extent.
constexpr std::size_t kLongSessionMaxChunk = 64u;
#ifdef _WIN32
constexpr intptr_t kNoInputHandle = 0;
#else
constexpr intptr_t kNoInputHandle = -1;
#endif

bool submit_message(tui_async_queue_t* queue, uint64_t message_id,
                    uint32_t type, uint64_t generation,
                    std::string_view chunk) {
    constexpr std::size_t kMaxChunk = 32u;
    if (chunk.size() > kMaxChunk) return false;
    std::array<unsigned char, sizeof(tui_stream_packet_header_t) + kMaxChunk>
        packet{};
    const tui_stream_packet_header_t header{message_id};
    std::memcpy(packet.data(), &header, sizeof(header));
    if (!chunk.empty())
        std::memcpy(packet.data() + sizeof(header), chunk.data(), chunk.size());
    return tui_async_queue_post(queue, type, generation, packet.data(),
             sizeof(header) + chunk.size(), nullptr) == TUI_ASYNC_OK;
}

bool submit(tui_async_queue_t* queue, uint32_t type, uint64_t generation,
            std::string_view chunk) {
    return submit_message(queue, kMessageId, type, generation, chunk);
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
        owner->ended = false;
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

struct MultiStreamSlot {
    uint64_t message_id = 0u;
    flexUI::Element* host = nullptr;       // borrowed from Box, stable ID
    flexUI::MarkdownWidget* markdown = nullptr;
    bool dirty = false;
};

struct MultiStreamOwner {
    tui_transcript_t* transcript = nullptr; // owner-only C11 registry
    std::array<MultiStreamSlot, 2u> slots{};
    flexUI::Box* box = nullptr;
    flexUI::Element* viewport = nullptr;
    std::size_t views = 0u;
    std::size_t accepted = 0u;
    std::size_t stale = 0u;
    std::size_t unknown = 0u;
    bool ok = true;
};

bool multi_stream_update(const tui_async_event_t* event,
                         const void* payload, void* context) {
    auto* owner = static_cast<MultiStreamOwner*>(context);
    if (!owner || !owner->transcript || !event || !payload ||
        event->payload_size < sizeof(tui_stream_packet_header_t)) {
        if (owner) owner->ok = false;
        return false;
    }

    // C11 registry validates message identity, generation, ordering, bytes
    // and terminal states. The C++ owner adapts only accepted UI deltas.
    const auto result = tui_transcript_apply(
        owner->transcript, event, payload, nullptr);
    if (result == TUI_TRANSCRIPT_STALE) {
        ++owner->stale;
        return true;
    }
    if (result == TUI_TRANSCRIPT_UNKNOWN) {
        ++owner->unknown;
        return true;
    }
    if (result != TUI_TRANSCRIPT_OK) {
        owner->ok = false;
        return false;
    }
    ++owner->accepted;
    tui_stream_packet_header_t header{};
    std::memcpy(&header, payload, sizeof(header));
    for (auto& slot : owner->slots) {
        if (slot.message_id != header.message_id) continue;
        if (event->type == TUI_STREAM_BEGIN) {
            slot.markdown->set_markdown("");
            slot.dirty = true;
        } else if (event->type == TUI_STREAM_CHUNK) {
            const auto prefix = sizeof(tui_stream_packet_header_t);
            const auto delta = std::string_view(
                static_cast<const char*>(payload) + prefix,
                event->payload_size - prefix);
            if (slot.markdown->append_markdown(delta) !=
                flexUI::MarkdownAppendStatus::Appended) {
                owner->ok = false;
                return false;
            }
            slot.dirty = true;
        }
        return true;
    }
    owner->ok = false; // model ID without any widget mapping
    return false;
}

void multi_stream_view(const void* context, tui_buffer_t*) {
    auto* owner = const_cast<MultiStreamOwner*>(
        static_cast<const MultiStreamOwner*>(context));
    if (!owner || !owner->box || !owner->viewport) return;
    const auto follow = flexUI::StreamScrollFollow::before_content_update(
        *owner->viewport);
    for (auto& slot : owner->slots) {
        if (!slot.dirty) continue;
        slot.markdown->update(0.0f, *slot.host);
        slot.dirty = false;
    }
    owner->box->invalidate();
    owner->box->update();
    (void)follow.apply(*owner->viewport);
    ++owner->views;
}

} // namespace

spec("FlexUI MarkdownWidget through Tango async transport and cell renderer") {
    it("clips scrolling TUI text before writing header/footer cells") {
        std::unique_ptr<tui_buffer_t, decltype(&tui_buffer_destroy)> buffer(
            tui_buffer_create(14, 8), &tui_buffer_destroy);
        check(buffer != nullptr);
        if (!buffer) return;
        auto renderer = flex::tui_backend::create_buffer_renderer(buffer.get());
        check(renderer != nullptr);
        if (!renderer) return;

        const flex::Color color(1.0f, 1.0f, 1.0f);
        const auto draw = [&](const std::string& str, int col, int row) {
            renderer->draw_text(str, static_cast<float>(col * 8),
                static_cast<float>(row * 16), "", 14.0f, false, color);
        };
        const auto character = [&](int col, int row) -> uint32_t {
            return tui_buffer_at_const(buffer.get(), col, row)->ch;
        };
        renderer->begin_frame(14.0f * 8.0f, 8.0f * 16.0f, 1.0f);
        draw("HEADER", 0, 0);
        draw("FOOTER", 0, 7);

        // Transcript content scrolls under the fixed header/footer, but the
        // renderer must honor the actual clip after translations.
        renderer->save();
        renderer->clip_rect(0.0f, 2.0f * 16.0f, 14.0f * 8.0f,
                            4.0f * 16.0f);
        draw("OUTSIDE_TOP", 0, 0);
        draw("OUTSIDE_BOTTOM", 0, 7);
        draw("OUTSIDE_NEGATIVE", 0, -1);
        draw("VISIBLE", 0, 2);
        renderer->restore();

        check(character(0, 0) == 'H');
        check(character(5, 0) == 'R');
        check(character(0, 7) == 'F');
        check(character(5, 7) == 'R');
        check(character(0, 1) == ' ');
        check(character(0, 6) == ' ');
        check(character(0, 2) == 'V');

        // A horizontal clip must never publish half of a wide CJK
        // grapheme, nor draw any glyph in the preceding cell.
        renderer->save();
        renderer->clip_rect(2.0f * 8.0f, 3.0f * 16.0f,
                            5.0f * 8.0f, 3.0f * 16.0f);
        draw("A中BCDE", 0, 3);
        draw("A中B", 4, 4);
        draw("A中", 5, 5);
        renderer->restore();
        renderer->end_frame();

        check(character(0, 3) == ' ');
        check(character(1, 3) == ' ');
        check(character(2, 3) == ' ');
        check(character(3, 3) == 'B');
        check(character(6, 3) == 'E');
        check(character(4, 4) == 'A');
        const auto* wide = tui_buffer_at_const(buffer.get(), 5, 4);
        const auto* continuation = tui_buffer_at_const(buffer.get(), 6, 4);
        check(wide->ch == 0x4e2du && wide->span == TUI_CELL_SPAN_WIDE);
        check(continuation->span == TUI_CELL_SPAN_CONTINUATION);
        check(character(7, 4) == ' ');
        check(character(5, 5) == 'A');
        check(character(6, 5) == ' '); // wide glyph straddles right clip
        check(character(0, 0) == 'H' && character(0, 7) == 'F');
    }

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
        // After the only burst has been drained the real wake must retire;
        // subsequent idle turns must not spin on a stale pipe byte/event.
        check(tui_async_wait_io(queue, kNoInputHandle, 0) ==
              TUI_ASYNC_WAIT_TIMEOUT);

        // Publication before entering the kernel wait uses the mailbox
        // predicate and is not lost. The same owner completes a second frame.
        check(submit(queue, TUI_STREAM_BEGIN, 2u, {}));
        check(submit(queue, TUI_STREAM_CHUNK, 2u, "C中D"));
        check(submit(queue, TUI_STREAM_END, 2u, {}));
        check(tui_async_wait_io(queue, kNoInputHandle, 0) ==
              TUI_ASYNC_WAIT_MESSAGE);
        check(tui_app_dispatch_once(app.get()));
        check(owner.ok && owner.ended && owner.views == 2u);
        check(owner.markdown->markdown() == "C中D");
        check(contains_visible_cells(terminal->front, 'C', 'D'));
        terminal->initialized = false; // Synthetic terminal: no OS restore
    }

    it("preserves user scroll-away and independent focus across Markdown updates") {
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 160.0f);
        box.load_css(
            "#root { width:320px; height:160px; display:flex; "
            "flex-direction:column; } "
            "#scroll { width:160px; height:64px; overflow-y:scroll; } "
            "#document { width:160px; display:flex; flex-direction:column; } "
            "#focus-target { width:70px; height:20px; }");
        auto* root = box.create("div", "root");
        auto* scroll = box.create("div", "scroll");
        auto* doc = box.create_widget<flexUI::MarkdownWidget>(
            "div", "document", "");
        auto* focus = box.create("button", "focus-target");
        check(root && scroll && doc && doc->widget && focus);
        if (!root || !scroll || !doc || !doc->widget || !focus) return;
        root->append(scroll);
        scroll->append(doc);
        root->append(focus);
        box.set_root(root);
        auto* widget = static_cast<flexUI::MarkdownWidget*>(doc->widget);

        std::string initial;
        for (unsigned i = 0u; i < 40u; ++i)
            initial += "Paragraph content for scrolling\n\n";
        check(widget->append_markdown(initial) ==
              flexUI::MarkdownAppendStatus::Appended);
        widget->update(0.0f, *doc);
        box.update();
        check(scroll->can_scroll_y());
        check(scroll->max_scroll_y() > 40.0f);
        if (!scroll->can_scroll_y() || scroll->max_scroll_y() <= 40.0f)
            return;

        // A user who moved away from the tail must not be forced back by
        // an unrelated content append, layout invalidation or resize.
        // Produce a real native SGR mouse-wheel event on POSIX (the same
        // private Tango decoder used by live terminal polling), then deliver
        // the normalized input through FlexUI's retained event router.
#ifndef _WIN32
        std::unique_ptr<tui_terminal_t, decltype(&tui_terminal_destroy)> native(
            tui_terminal_create(), &tui_terminal_destroy);
        check(native != nullptr);
        if (!native) return;
        native->mouse_enabled = true;
        const char wheel_sequence[] = "\x1b[<65;3;3M";
        tui_event_t native_wheel{};
        check(tui_terminal_input_push(native.get(), wheel_sequence,
                                      sizeof(wheel_sequence) - 1u));
        const bool decoded = tui_terminal_input_next(native.get(), &native_wheel);
        check(decoded);
        check(native_wheel.type == TUI_EVENT_MOUSE_PRESS);
        check(native_wheel.key == TUI_KEY_MOUSE_WHEEL_DOWN);
        if (!decoded || native_wheel.key != TUI_KEY_MOUSE_WHEEL_DOWN) return;
        const float mx = (static_cast<float>(native_wheel.x) + 0.5f) * 8.0f;
        const float my = (static_cast<float>(native_wheel.y) + 0.5f) * 16.0f;
#else
        // The Windows console RECORD decoder is exercised in Tango Native;
        // here the cross-platform FlexUI test uses its equivalent event.
        const float mx = 20.0f, my = 40.0f;
#endif
        auto wheel = flexUI::Event::mouse_wheel(mx, my, 0.0f, -1.0f);
        box.dispatch_event(wheel);
        check(scroll->scroll_y() > 0.0f);
        if (scroll->scroll_y() <= 0.0f) return;
        box.set_focus(focus);
        check(box.focused_element() == focus);
        const float before = scroll->scroll_y();
        check(widget->append_markdown("New streaming paragraph\n\n") ==
              flexUI::MarkdownAppendStatus::Appended);
        widget->update(0.0f, *doc);
        box.invalidate();
        box.update();
        check(std::fabs(scroll->scroll_y() - before) < 0.01f);
        check(box.focused_element() == focus);
        box.set_viewport(280.0f, 150.0f);
        box.update();
        check(std::fabs(scroll->scroll_y() - before) < 0.01f);
        check(box.focused_element() == focus);
    }

    it("closes admission during a live producer and drains every accepted Markdown chunk") {
        flexUI::Box box(nullptr);
        auto* root = box.create("div", "shutdown-root");
        auto* host = box.create_widget<flexUI::MarkdownWidget>(
            "div", "shutdown-message", "");
        check(root && host && host->widget);
        if (!root || !host || !host->widget) return;
        root->append(host);
        box.set_root(root);
        auto* widget = static_cast<flexUI::MarkdownWidget*>(host->widget);
        auto* stream = tui_stream_create(kMessageId, 512u, 32u);
        check(stream != nullptr);
        if (!stream) return;
        FlexUIOwner owner{stream, widget};
        auto* app = tui_app_create(
            &owner, keyboard_noop, view_noop, input_noop);
        check(app != nullptr);
        if (!app) {
            check(tui_stream_destroy(stream) == TUI_STREAM_OK);
            return;
        }
        check(tui_app_set_async_update(app, owner_async_update));
        auto* queue = tui_app_async_queue(app);
        check(queue != nullptr);
        if (!queue) {
            tui_app_destroy(app);
            check(tui_stream_destroy(stream) == TUI_STREAM_OK);
            return;
        }
        check(submit(queue, TUI_STREAM_BEGIN, 9u, {}));

        std::promise<void> first_post;
        auto first_ready = first_post.get_future();
        std::atomic<std::size_t> accepted{0u};
        std::atomic<bool> shutdown_signaled{false};
        std::atomic<tui_async_status_t> late_status{
            TUI_ASYNC_INVALID_ARGUMENT};
        std::thread producer([&] {
            // Only copied queue payloads leave this worker. No Box/Element,
            // MarkdownWidget, or stream state is borrowed across threads.
            if (submit(queue, TUI_STREAM_CHUNK, 9u, "x"))
                accepted.fetch_add(1u, std::memory_order_relaxed);
            first_post.set_value();
            for (unsigned i = 0u; i < 80u; ++i) {
                if (!submit(queue, TUI_STREAM_CHUNK, 9u, "x")) break;
                accepted.fetch_add(1u, std::memory_order_relaxed);
            }
            while (!shutdown_signaled.load(std::memory_order_acquire))
                std::this_thread::yield();
            // Post-close admission must be explicit CLOSED, not silently
            // dropped or allowed to race owner destruction.
            std::array<unsigned char, sizeof(tui_stream_packet_header_t) + 1u>
                packet{};
            const tui_stream_packet_header_t header{kMessageId};
            std::memcpy(packet.data(), &header, sizeof(header));
            packet[sizeof(header)] = static_cast<unsigned char>('z');
            late_status.store(tui_async_queue_post(
                queue, TUI_STREAM_CHUNK, 9u, packet.data(), packet.size(),
                nullptr), std::memory_order_release);
        });
        const bool first_seen =
            first_ready.wait_for(std::chrono::seconds(2)) ==
            std::future_status::ready;
        check(first_seen);
        check(tui_async_queue_close(queue) == TUI_ASYNC_OK);
        shutdown_signaled.store(true, std::memory_order_release);
        producer.join();
        check(accepted.load() >= 1u);
        check(late_status.load(std::memory_order_acquire) == TUI_ASYNC_CLOSED);

        const auto expected = accepted.load(std::memory_order_relaxed);
        tui_async_queue_stats_t stats{};
        check(tui_async_queue_get_stats(queue, &stats) == TUI_ASYNC_OK);
        check(stats.closed && stats.queued == expected + 1u);
        check(stats.rejected_closed >= 1u);
        // The app owner must consume precisely the records admitted before
        // shutdown, then retire the queue only after worker join/quiescence.
        check(tui_app_pump_messages(app, 128u) == expected + 1u);
        check(owner.ok && owner.chunks == expected);
        check(widget->markdown() == std::string(expected, 'x'));
        widget->update(0.0f, *host);
        box.update();
        check(host->child_count() == 1u);
        if (host->child_count() == 1u)
            check(host->child_at(0u)->text() == std::string(expected, 'x'));
        check(tui_app_pump_messages(app, 128u) == 0u);
        check(tui_async_queue_get_stats(queue, &stats) == TUI_ASYNC_OK);
        check(stats.queued == 0u);
        tui_app_destroy(app);
        check(tui_stream_destroy(stream) == TUI_STREAM_OK);
    }

    it("keeps two interleaved message identities and user-tail intent isolated") {
        // One bounded Tango queue, exactly two application-owned streams and
        // two independent MarkdownWidget projections. No second parser/queue.
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 176.0f);
        box.load_css(
            "#root { display:flex; flex-direction:column; width:320px; "
            "height:176px; } "
            "#transcript { width:180px; height:72px; overflow-y:scroll; "
            "display:flex; flex-direction:column; } "
            "#message41, #message42 { width:180px; display:flex; "
            "flex-direction:column; flex-shrink:0; } "
            "#other-focus { width:70px; height:20px; }");
        auto* root = box.create("div", "root");
        auto* transcript = box.create("div", "transcript");
        auto* msg41 = box.create_widget<flexUI::MarkdownWidget>(
            "div", "message41", "");
        auto* msg42 = box.create_widget<flexUI::MarkdownWidget>(
            "div", "message42", "");
        auto* focus = box.create("button", "other-focus");
        check(root && transcript && msg41 && msg41->widget &&
              msg42 && msg42->widget && focus);
        if (!root || !transcript || !msg41 || !msg41->widget ||
            !msg42 || !msg42->widget || !focus) return;
        root->append(transcript);
        transcript->append(msg41);
        transcript->append(msg42);
        root->append(focus);
        box.set_root(root);
        box.update();
        const auto handle41 = box.handle_for(*msg41);
        const auto handle42 = box.handle_for(*msg42);

        tui_transcript_t* transcript_state = tui_transcript_create(
            2u, 4096u, 32u);
        check(transcript != nullptr);
        if (!transcript) return;
        uint64_t evicted = 999u;
        check(tui_transcript_open(transcript_state, 41u, &evicted) ==
              TUI_TRANSCRIPT_OK);
        check(evicted == 0u);
        check(tui_transcript_open(transcript_state, 42u, &evicted) ==
              TUI_TRANSCRIPT_OK);
        check(tui_transcript_count(transcript_state) == 2u);
        MultiStreamOwner owner{};
        owner.transcript = transcript_state;
        owner.box = &box;
        owner.viewport = transcript;
        owner.slots[0] = {
            41u, msg41,
            static_cast<flexUI::MarkdownWidget*>(msg41->widget), false};
        owner.slots[1] = {
            42u, msg42,
            static_cast<flexUI::MarkdownWidget*>(msg42->widget), false};
        auto* app = tui_app_create(&owner, keyboard_noop,
                                   multi_stream_view, input_noop);
        check(app != nullptr);
        if (!app) {
            check(tui_transcript_destroy(transcript_state) == TUI_TRANSCRIPT_OK);
            
            return;
        }
        auto* term = tui_app_terminal(app);
        term->width = 8;
        term->height = 2;
        term->front = tui_buffer_create(8, 2);
        term->back = tui_buffer_create(8, 2);
        check(term->front && term->back);
        if (!term->front || !term->back) {
            tui_app_destroy(app);
            check(tui_transcript_destroy(transcript_state) == TUI_TRANSCRIPT_OK);
            
            return;
        }
        term->initialized = true;
        term->color_mode = TUI_COLOR_TRUECOLOR;
        term->write_output = mock_terminal_write;
        check(tui_app_set_async_update(app, multi_stream_update));
        auto* queue = tui_app_async_queue(app);
        check(queue != nullptr);
        if (!queue) {
            term->initialized = false;
            tui_app_destroy(app);
            check(tui_transcript_destroy(transcript_state) == TUI_TRANSCRIPT_OK);
            
            return;
        }
        app->needs_redraw = false;

        check(submit_message(queue, 41u, TUI_STREAM_BEGIN, 1u, {}));
        check(submit_message(queue, 42u, TUI_STREAM_BEGIN, 1u, {}));
        std::string alpha, beta;
        for (unsigned i = 0u; i < 12u; ++i) {
            check(submit_message(queue, 41u, TUI_STREAM_CHUNK,
                                 1u, "alpha paragraph\n\n"));
            check(submit_message(queue, 42u, TUI_STREAM_CHUNK,
                                 1u, "beta paragraph\n\n"));
            alpha += "alpha paragraph\n\n";
            beta += "beta paragraph\n\n";
        }
        check(submit_message(queue, 41u, TUI_STREAM_END, 1u, {}));
        check(tui_app_dispatch_once(app));
        check(owner.ok && owner.views == 1u);
        check(owner.accepted == 27u);
        check(owner.slots[0].markdown->markdown() == alpha);
        check(owner.slots[1].markdown->markdown() == beta);
        check(transcript->can_scroll_y());
        check(transcript->max_scroll_y() > 20.0f);
        check(std::fabs(transcript->scroll_y() -
                        transcript->max_scroll_y()) <= 2.0f);
        check(box.resolve_handle(handle41) == msg41);
        check(box.resolve_handle(handle42) == msg42);
        if (!transcript->can_scroll_y() ||
            transcript->max_scroll_y() <= 20.0f) {
            term->initialized = false;
            tui_app_destroy(app);
            check(tui_transcript_destroy(transcript_state) == TUI_TRANSCRIPT_OK);
            
            return;
        }

        // A manual scroll-away and focus are durable application state, not
        // an instruction for the next message to force a tail-follow.
        check(transcript->set_scroll_offset(0.0f, 3.0f));
        const float away = transcript->scroll_y();
        box.set_focus(focus);
        check(box.focused_element() == focus);
        check(submit_message(queue, 41u, TUI_STREAM_CHUNK, 1u, "stale"));
        check(submit_message(queue, 42u, TUI_STREAM_CHUNK,
                             1u, "still active\n\n"));
        beta += "still active\n\n";
        check(submit_message(queue, 41u, TUI_STREAM_BEGIN, 2u, {}));
        check(submit_message(queue, 41u, TUI_STREAM_CHUNK,
                             2u, "fresh gen two\n\n"));
        check(submit_message(queue, 42u, TUI_STREAM_CANCEL, 1u, {}));
        check(submit_message(queue, 42u, TUI_STREAM_CHUNK, 1u, "late"));
        check(submit_message(queue, 41u, TUI_STREAM_END, 2u, {}));
        // Unknown IDs consume bounded queue admission, but no slots.
        check(submit_message(queue, 404u, TUI_STREAM_BEGIN, 1u, {}));
        check(tui_app_dispatch_once(app));
        check(owner.ok && owner.views == 2u);
        check(owner.stale == 2u && owner.unknown == 1u);
        check(owner.slots[0].markdown->markdown() == "fresh gen two\n\n");
        check(owner.slots[1].markdown->markdown() == beta);
        check(std::fabs(transcript->scroll_y() - away) <= 0.01f);
        check(box.focused_element() == focus);
        check(box.resolve_handle(handle41) == msg41);
        check(box.resolve_handle(handle42) == msg42);

        // The user explicitly returns to End. Only then may the next owner
        // publication follow new content at the bottom of the transcript.
        check(transcript->set_scroll_offset(
            transcript->scroll_x(), transcript->max_scroll_y()));
        const float previous_max = transcript->max_scroll_y();
        check(submit_message(queue, 42u, TUI_STREAM_BEGIN, 2u, {}));
        std::string gamma;
        for (unsigned i = 0u; i < 24u; ++i) {
            check(submit_message(queue, 42u, TUI_STREAM_CHUNK,
                                 2u, "gamma paragraph\n\n"));
            gamma += "gamma paragraph\n\n";
        }
        check(submit_message(queue, 42u, TUI_STREAM_END, 2u, {}));
        check(tui_app_dispatch_once(app));
        check(owner.ok && owner.views == 3u);
        check(owner.slots[1].markdown->markdown() == gamma);
        check(transcript->max_scroll_y() > previous_max);
        check(std::fabs(transcript->scroll_y() -
                        transcript->max_scroll_y()) <= 2.0f);
        check(box.focused_element() == focus);
        check(transcript->child_count() == 2u);
        check(box.resolve_handle(handle41) == msg41);
        check(box.resolve_handle(handle42) == msg42);

        // Both native stream snapshots are independently settled despite
        // sharing one global mailbox with interleaved sequence numbers.
        tui_transcript_message_t snap41{}, snap42{};
        check(tui_transcript_get(transcript_state, 41u, &snap41) ==
              TUI_TRANSCRIPT_OK);
        check(tui_transcript_get(transcript_state, 42u, &snap42) ==
              TUI_TRANSCRIPT_OK);
        check(snap41.message_id == 41u &&
              snap41.stream.generation == 2u &&
              snap41.stream.state == TUI_STREAM_DONE);
        check(snap42.message_id == 42u &&
              snap42.stream.generation == 2u &&
              snap42.stream.state == TUI_STREAM_DONE);
        tui_transcript_message_t visible[2]{};
        size_t visible_count = 99u;
        check(tui_transcript_window(transcript_state, 0u, 2u, visible, 2u,
                                    &visible_count) == TUI_TRANSCRIPT_OK);
        check(visible_count == 2u && visible[0].message_id == 41u &&
              visible[1].message_id == 42u);
        check(tui_async_queue_close(queue) == TUI_ASYNC_OK);
        term->initialized = false; // synthetic sink has no terminal mode
        tui_app_destroy(app);
        check(tui_transcript_destroy(transcript_state) == TUI_TRANSCRIPT_OK);
        
    }

    it("recycles bounded transcript slots while invalidating evicted UiHandles") {
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 160.0f);
        box.load_css(
            "#root { display:flex; flex-direction:column; width:320px; "
            "height:160px; } "
            "#window { display:flex; flex-direction:column; width:180px; "
            "height:54px; overflow-y:scroll; } "
            ".transcript-message { width:180px; flex-shrink:0; } "
            "#focus { width:80px; height:20px; }");
        auto* root = box.create("div", "root");
        auto* viewport = box.create("div", "window");
        auto* focus = box.create("button", "focus");
        check(root && viewport && focus);
        if (!root || !viewport || !focus) return;
        root->append(viewport);
        root->append(focus);
        box.set_root(root);
        flex::tui_backend::TranscriptWindow window(box, *viewport, 2u);
        check(window.slot_capacity() == 2u);
        const std::size_t initial_owned = box.owned_element_count();
        check(initial_owned <= 6u);

        tui_transcript_t* transcript_state =
            tui_transcript_create(3u, 4096u, 128u);
        check(transcript_state != nullptr);
        if (!transcript_state) return;
        std::uint64_t next_sequence = 0u;
        const auto emit = [&](std::uint64_t id, std::uint32_t type,
                              std::uint64_t generation,
                              std::string_view payload) {
            std::array<unsigned char,
                       sizeof(tui_stream_packet_header_t) + 128u> data{};
            if (payload.size() > 128u) return TUI_TRANSCRIPT_INVALID;
            const tui_stream_packet_header_t header{id};
            std::memcpy(data.data(), &header, sizeof(header));
            if (!payload.empty())
                std::memcpy(data.data() + sizeof(header),
                            payload.data(), payload.size());
            const tui_async_event_t event{
                type, generation, ++next_sequence,
                sizeof(header) + payload.size()
            };
            return tui_transcript_apply(
                transcript_state, &event, data.data(), nullptr);
        };
        std::string long_text;
        for (unsigned i = 0u; i < 14u; ++i)
            long_text += "message paragraph\n\n";
        check(long_text.size() <= 4096u);

        // Admission and UI rendering remain independent. Opening/evicting a
        // C11 stream does not create any new Element or MarkdownWidget.
        for (std::uint64_t id = 101u; id <= 103u; ++id) {
            std::uint64_t evicted = 99u;
            check(tui_transcript_open(transcript_state, id, &evicted) ==
                  TUI_TRANSCRIPT_OK);
            check(evicted == 0u);
            check(emit(id, TUI_STREAM_BEGIN, 1u, {}) ==
                  TUI_TRANSCRIPT_OK);
            for (std::size_t offset = 0u; offset < long_text.size();
                 offset += 64u) {
                const std::size_t remaining =
                    std::min<std::size_t>(64u, long_text.size() - offset);
                check(emit(id, TUI_STREAM_CHUNK, 1u,
                           std::string_view(long_text.data() + offset,
                                            remaining)) ==
                      TUI_TRANSCRIPT_OK);
            }
            check(emit(id, TUI_STREAM_END, 1u, {}) ==
                  TUI_TRANSCRIPT_OK);
        }
        using Status = flex::tui_backend::TranscriptWindow::Status;
        check(window.show(transcript_state, 0u) == Status::Updated);
        check(window.show(transcript_state, 0u) == Status::Unchanged);
        check(viewport->child_count() == 2u);
        check(window.visible_count() == 2u);
        auto* first_host = window.visible_host(101u);
        auto* held_host = window.visible_host(102u);
        check(first_host && held_host);
        if (!first_host || !held_host) {
            check(tui_transcript_destroy(transcript_state) ==
                  TUI_TRANSCRIPT_OK);
            return;
        }
        const auto retired_handle = box.handle_for(*first_host);
        const auto held_handle = box.handle_for(*held_host);
        check(box.resolve_handle(retired_handle) == first_host);
        check(box.resolve_handle(held_handle) == held_host);
        check(viewport->can_scroll_y());
        if (!viewport->can_scroll_y()) {
            check(tui_transcript_destroy(transcript_state) ==
                  TUI_TRANSCRIPT_OK);
            return;
        }

        check(viewport->set_scroll_offset(0.0f, 3.0f));
        const float away = viewport->scroll_y();
        box.set_focus(focus);
        check(box.focused_element() == focus);

        // Window shift keeps message 102 on its exact original host; only
        // message 101 is reused. Its prior UiHandle must become invalid.
        check(window.show(transcript_state, 1u) == Status::Updated);
        check(viewport->child_count() == 2u);
        check(window.visible_host(101u) == nullptr);
        check(window.visible_host(102u) == held_host);
        check(window.visible_host(103u) == first_host);
        check(box.resolve_handle(retired_handle) == nullptr);
        check(box.resolve_handle(held_handle) == held_host);
        check(std::fabs(viewport->scroll_y() - away) < 0.01f);
        check(box.focused_element() == focus);

        // A settled C11 message may now be evicted. UI handle/slot identity
        // stays associated with the two still-visible messages until the
        // caller explicitly changes the presentation window.
        std::uint64_t evicted = 99u;
        check(tui_transcript_open(transcript_state, 104u, &evicted) ==
              TUI_TRANSCRIPT_OK);
        check(evicted == 101u);
        check(emit(101u, TUI_STREAM_BEGIN, 999u, {}) ==
              TUI_TRANSCRIPT_STALE);
        check(emit(104u, TUI_STREAM_BEGIN, 1u, {}) ==
              TUI_TRANSCRIPT_OK);
        check(emit(104u, TUI_STREAM_CHUNK, 1u, "new message\n\n") ==
              TUI_TRANSCRIPT_OK);
        check(emit(104u, TUI_STREAM_END, 1u, {}) ==
              TUI_TRANSCRIPT_OK);
        check(window.show_from_id(transcript_state, 101u) ==
              Status::Unchanged);
        check(window.visible_host(102u) == held_host);
        check(window.visible_host(103u) == first_host);

        const auto old_102 = box.handle_for(*held_host);
        // Tail follows the newest two retained messages (103,104), no
        // reallocation of the fixed host slots.
        check(window.show_tail(transcript_state) == Status::Updated);
        check(viewport->child_count() == 2u);
        check(window.visible_host(102u) == nullptr);
        check(window.visible_host(103u) == first_host);
        check(window.visible_host(104u) == held_host);
        check(box.resolve_handle(old_102) == nullptr);
        check(box.focused_element() == focus);
        check(box.owned_element_count() <= initial_owned + 64u);

        // Simulate a longer bounded session. Every explicit admission
        // recycles only a settled native slot and only two UI host slots.
        for (std::uint64_t id = 105u; id <= 136u; ++id) {
            check(tui_transcript_open(transcript_state, id, &evicted) ==
                  TUI_TRANSCRIPT_OK);
            check(evicted > 0u);
            check(emit(id, TUI_STREAM_BEGIN, 1u, {}) ==
                  TUI_TRANSCRIPT_OK);
            check(emit(id, TUI_STREAM_CHUNK, 1u, "cycle\n\n") ==
                  TUI_TRANSCRIPT_OK);
            check(emit(id, TUI_STREAM_END, 1u, {}) ==
                  TUI_TRANSCRIPT_OK);
            check(window.show_tail(transcript_state) == Status::Updated);
            check(window.visible_count() == 2u);
            check(viewport->child_count() == 2u);
            check(tui_transcript_count(transcript_state) == 3u);
            check(box.owned_element_count() <= initial_owned + 64u);
        }
        check(window.visible_host(136u) != nullptr);
        check(window.visible_host(135u) != nullptr);

        // A caller-supplied message anchor above the latest retained ID
        // must clamp to the newest two slots, not silently jump back to
        // the oldest history. This was previously a hidden fallback to 0.
        check(window.show(transcript_state, 0u) == Status::Updated);
        check(window.visible_host(134u) != nullptr);
        check(window.visible_host(136u) == nullptr);
        check(window.show_from_id(transcript_state,
                  std::numeric_limits<std::uint64_t>::max()) ==
              Status::Updated);
        check(window.visible_host(134u) == nullptr);
        check(window.visible_host(135u) != nullptr);
        check(window.visible_host(136u) != nullptr);
        check(window.visible_count() == 2u);
        check(viewport->child_count() == 2u);
        check(box.owned_element_count() <= initial_owned + 64u);
        check(box.resolve_handle(retired_handle) == nullptr);
        check(box.resolve_handle(held_handle) == nullptr);
        check(box.focused_element() == focus);
        check(tui_transcript_destroy(transcript_state) ==
              TUI_TRANSCRIPT_OK);
    }

    it("keeps a bounded variable-height message anchor across reflow and eviction") {
        // Test the actual C11 transcript -> MarkdownWidget -> Box layout
        // instead of estimating offscreen heights. No full-history DOM tree.
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 176.0f);
        box.load_css(
            "#root { display:flex; flex-direction:column; width:100%; "
            "height:100%; } "
            "#anchor-window { display:flex; flex-direction:column; "
            "width:80%; height:72px; overflow-y:scroll; } "
            ".transcript-message { width:100%; flex-shrink:0; }");
        auto* root = box.create("div", "root");
        auto* viewport = box.create("div", "anchor-window");
        check(root && viewport);
        if (!root || !viewport) return;
        root->append(viewport);
        box.set_root(root);
        flex::tui_backend::TranscriptWindow window(box, *viewport, 3u);
        const std::size_t original_nodes = box.owned_element_count();
        auto* transcript = tui_transcript_create(3u, 8192u, 128u);
        check(transcript != nullptr);
        if (!transcript) return;
        std::uint64_t sequence = 0u;
        const auto emit = [&](std::uint64_t id, std::uint32_t type,
                              std::string_view chunk) {
            if (chunk.size() > 128u) return false;
            std::array<unsigned char,
                       sizeof(tui_stream_packet_header_t) + 128u> bytes{};
            const tui_stream_packet_header_t header{id};
            std::memcpy(bytes.data(), &header, sizeof(header));
            if (!chunk.empty())
                std::memcpy(bytes.data() + sizeof(header),
                            chunk.data(), chunk.size());
            const tui_async_event_t event{
                type, 1u, ++sequence, sizeof(header) + chunk.size()
            };
            return tui_transcript_apply(
                transcript, &event, bytes.data(), nullptr) ==
                TUI_TRANSCRIPT_OK;
        };
        const auto paragraphs = [](std::uint64_t id, unsigned n) {
            std::string source = "# Message " + std::to_string(id) + "\n\n";
            for (unsigned i = 0u; i < n; ++i)
                source += "Wrapped 中 **Markdown** paragraph for layout.\n\n";
            return source;
        };
        const auto send_source = [&](std::uint64_t id, const std::string& source) {
            for (std::size_t offset = 0u; offset < source.size(); offset += 96u) {
                const auto size = std::min<std::size_t>(96u, source.size() - offset);
                if (!emit(id, TUI_STREAM_CHUNK,
                          std::string_view(source.data() + offset, size)))
                    return false;
            }
            return true;
        };
        bool accepted = true;
        for (std::uint64_t id = 1u; id <= 3u; ++id) {
            std::uint64_t evicted = 99u;
            const std::string text = paragraphs(id, id == 3u ? 16u : 5u);
            accepted = accepted &&
                tui_transcript_open(transcript, id, &evicted) ==
                    TUI_TRANSCRIPT_OK && evicted == 0u &&
                emit(id, TUI_STREAM_BEGIN, {}) &&
                send_source(id, text) &&
                (id == 1u || emit(id, TUI_STREAM_END, {}));
        }
        check(accepted);
        if (!accepted) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        using Status = flex::tui_backend::TranscriptWindow::Status;
        check(window.show(transcript, 0u) == Status::Updated);
        box.update();
        auto* host2 = window.visible_host(2u);
        auto* host3 = window.visible_host(3u);
        check(host2 && host3 && viewport->can_scroll_y());
        if (!host2 || !host3 || !viewport->can_scroll_y()) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        const float requested = host2->y() + 6.0f;
        check(viewport->max_scroll_y() > requested + 2.0f);
        if (viewport->max_scroll_y() <= requested + 2.0f) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        check(viewport->set_scroll_offset(0.0f, requested));
        const auto anchor = window.capture_scroll_anchor();
        check(!anchor.follow_tail && anchor.message_id == 2u);
        check(std::fabs(anchor.within_message_y - 6.0f) < 2.0f);
        if (anchor.follow_tail || anchor.message_id != 2u) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        const float previous_top = host2->y();
        check(send_source(1u, paragraphs(1u, 12u)));
        check(window.show_preserving_anchor(transcript, anchor) ==
              Status::Updated);
        check(window.visible_host(2u) == host2);
        check(host2->y() > previous_top + 2.0f);
        check(std::fabs(viewport->scroll_y() -
                        (host2->y() + anchor.within_message_y)) < 2.0f);

        // The stored value survives a real Box/CSS reflow. The caller captures
        // it before resize; the bounded owner then repositions the same ID.
        box.set_viewport(240.0f, 176.0f);
        box.update();
        const auto resized = window.show_preserving_anchor(transcript, anchor);
        check(resized == Status::Updated || resized == Status::Unchanged);
        check(window.visible_host(2u) == host2);
        check(std::fabs(viewport->scroll_y() -
                        (host2->y() + anchor.within_message_y)) < 2.0f);

        // Only explicit follow-tail requests may override a scroll-away.
        check(viewport->set_scroll_offset(0.0f, viewport->max_scroll_y()));
        const auto following = window.capture_scroll_anchor();
        check(following.follow_tail);
        const auto tail_result = window.show_preserving_anchor(transcript, following);
        check(tail_result == Status::Updated || tail_result == Status::Unchanged);
        check(std::fabs(viewport->scroll_y() - viewport->max_scroll_y()) < 2.0f);

        // Oldest completed message is evicted while the chosen anchor (2)
        // remains retained: keep its host identity, not an absolute scrollY.
        check(emit(1u, TUI_STREAM_END, {}));
        std::uint64_t evicted = 0u;
        check(tui_transcript_open(transcript, 4u, &evicted) ==
              TUI_TRANSCRIPT_OK && evicted == 1u);
        check(emit(4u, TUI_STREAM_BEGIN, {}));
        check(send_source(4u, paragraphs(4u, 12u)));
        check(emit(4u, TUI_STREAM_END, {}));
        check(window.show_preserving_anchor(transcript, anchor) ==
              Status::Updated);
        check(window.visible_host(2u) == host2);
        check(window.visible_host(3u) == host3);
        check(std::fabs(viewport->scroll_y() -
                        (host2->y() + anchor.within_message_y)) < 2.0f);

        // Once message 2 is evicted, an old anchor clamps to oldest retained
        // #3 at its start; it must NOT force End/tail or reuse ID 2's host.
        check(tui_transcript_open(transcript, 5u, &evicted) ==
              TUI_TRANSCRIPT_OK && evicted == 2u);
        check(emit(5u, TUI_STREAM_BEGIN, {}));
        check(send_source(5u, paragraphs(5u, 10u)));
        check(emit(5u, TUI_STREAM_END, {}));
        check(window.show_preserving_anchor(transcript, anchor) ==
              Status::Updated);
        check(window.visible_host(2u) == nullptr);
        check(window.visible_host(3u) == host3);
        check(viewport->scroll_y() < 1.0f);
        check(window.visible_count() <= 3u);
        check(viewport->child_count() <= 3u);
        // Three simultaneously visible variable-height documents include
        // dozens of real paragraph/inline Markdown projection nodes.
        // Bound their *total* owned forest separately from the tighter
        // 512 tiny-message turnover test (initial + 32), and report the
        // observed value so future CI can reduce this budget if justified.
        std::printf("TUI_ANCHOR_VISIBLE box_elements=%zu "
                    "style_baselines=%zu visible_hosts=%zu\n",
                    box.owned_element_count(),
                    box.cached_style_element_count(), window.visible_count());
        // Observed three-host projection high-water was 160 nodes on
        // Linux ASan/LSan; leave bounded growth room for other compilers.
        check(box.owned_element_count() <= original_nodes + 192u);
        check(box.cached_style_element_count() <= original_nodes + 192u);
        check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
    }

    it("tracks bounded C11 message heights without stale geometry ownership") {
        // This private cache is a numeric precursor for offscreen spacers.
        // Actual heights come from two Box-owned MarkdownWidget hosts; the
        // other retained messages only carry explicit fallback estimates.
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 176.0f);
        box.load_css(
            "#root { width:320px; height:176px; display:flex; "
            "flex-direction:column; } "
            "#height-window { width:180px; height:72px; overflow-y:scroll; "
            "display:flex; flex-direction:column; } "
            ".transcript-message { width:180px; flex-shrink:0; } "
            ".tui-virtual-spacer { width:180px; flex-shrink:0; "
            "height:var(--tui-virtual-height); }");
        auto* root = box.create("div", "root");
        auto* viewport = box.create("div", "height-window");
        check(root && viewport);
        if (!root || !viewport) return;
        root->append(viewport);
        box.set_root(root);
        flex::tui_backend::TranscriptWindow window(box, *viewport, 2u);
        box.update();
        window.enable_height_tracking(4u, 24.0f);
        window.enable_virtual_spacers();
        const auto initial_owned = box.owned_element_count();
        auto* transcript = tui_transcript_create(4u, 4096u, 128u);
        check(transcript != nullptr);
        if (!transcript) return;

        std::uint64_t sequence = 0u;
        const auto emit = [&](std::uint64_t id, std::uint32_t type,
                              std::string_view text) {
            if (text.size() > 128u) return false;
            std::array<unsigned char,
                       sizeof(tui_stream_packet_header_t) + 128u> data{};
            const tui_stream_packet_header_t header{id};
            std::memcpy(data.data(), &header, sizeof(header));
            if (!text.empty())
                std::memcpy(data.data() + sizeof(header),
                            text.data(), text.size());
            const tui_async_event_t event{
                type, 1u, ++sequence, sizeof(header) + text.size()
            };
            return tui_transcript_apply(
                transcript, &event, data.data(), nullptr) ==
                TUI_TRANSCRIPT_OK;
        };
        bool accepted = true;
        for (std::uint64_t id = 10u; id <= 13u; ++id) {
            std::uint64_t evicted = 99u;
            accepted = accepted &&
                tui_transcript_open(transcript, id, &evicted) ==
                    TUI_TRANSCRIPT_OK && evicted == 0u &&
                emit(id, TUI_STREAM_BEGIN, {}) &&
                emit(id, TUI_STREAM_CHUNK, "## Typed 中\n\nBody paragraph\n\n") &&
                (id == 12u || emit(id, TUI_STREAM_END, {}));
        }
        check(accepted);
        if (!accepted) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        check(window.show(transcript, 1u) ==
              flex::tui_backend::TranscriptWindow::Status::Updated);
        box.update();
        auto* host11 = window.visible_host(11u);
        auto* host12 = window.visible_host(12u);
        check(host11 && host12 && viewport->width() > 0.0f);
        const auto* live_index = window.tracked_heights();
        check(live_index != nullptr && live_index->size() == 4u);
        check(window.virtual_spacers_enabled());
        check(viewport->child_count() == 4u);
        check(std::fabs(window.leading_spacer_height() - 24.0f) < 1.0f);
        check(std::fabs(window.trailing_spacer_height() - 24.0f) < 1.0f);
        if (live_index && host11 && host12) {
            check(live_index->lookup(11u)->measured);
            check(live_index->lookup(12u)->measured);
            check(!live_index->lookup(10u)->measured);
            check(!live_index->lookup(13u)->measured);
        }
        if (!host11 || !host12 || viewport->width() <= 0.0f) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }

        using Index = flex::tui_backend::TranscriptHeightIndex;
        Index heights(4u, 24.0f);
        const float width = viewport->width();
        check(heights.sync(transcript, width) == Index::Status::Updated);
        check(heights.size() == 4u && heights.capacity() == 4u);
        const auto first_prefix = heights.height_before(11u);
        check(first_prefix && std::fabs(*first_prefix - 24.0) < 0.01);
        const float height11 = host11->height();
        const float height12 = host12->height();
        check(height11 > 0.0f && height12 > 0.0f);
        check(heights.record_height(11u, height11));
        check(heights.record_height(12u, height12));
        const auto extents = heights.window_extent(11u, 2u);
        check(extents.has_value());
        if (extents) {
            check(!extents->leading_exact && extents->visible_exact &&
                  !extents->trailing_exact);
            check(std::fabs(extents->leading - 24.0) < 0.01);
            check(std::fabs(extents->visible -
                            static_cast<double>(height11 + height12)) < 0.01);
            check(std::fabs(extents->trailing - 24.0) < 0.01);
            check(std::fabs(extents->total -
                            (extents->leading + extents->visible +
                             extents->trailing)) < 0.01);
        }
        check(!heights.window_extent(9u, 2u).has_value());
        check(!heights.window_extent(13u, 2u).has_value());
        check(!heights.window_extent(11u, 0u).has_value());
        check(!heights.locate_offset(-1.0));
        check(!heights.locate_offset(
            std::numeric_limits<double>::quiet_NaN()));
        const auto estimated_start = heights.locate_offset(2.0);
        check(estimated_start && estimated_start->message_id == 10u &&
              estimated_start->prefix_exact &&
              !estimated_start->message_measured);
        const auto measured_middle =
            heights.locate_offset(24.0 + height11 / 2.0);
        check(measured_middle && measured_middle->message_id == 11u &&
              !measured_middle->prefix_exact &&
              measured_middle->message_measured);
        const auto last_retain =
            heights.locate_offset(heights.total_height_estimate());
        check(last_retain && last_retain->message_id == 13u &&
              !last_retain->message_measured);
        const auto height_info12 = heights.lookup(12u);
        check(height_info12 && height_info12->measured);
        const auto before12 = heights.height_before(12u);
        check(before12 &&
              std::fabs(*before12 - (24.0 + height11)) < 0.01);
        bool exact_prefix = true;
        (void)heights.height_before(12u, &exact_prefix);
        check(!exact_prefix); // ID 10 is retained, but not measured.
        check(!heights.height_before(99u));
        check(!heights.record_height(99u, 1.0f));
        check(!heights.record_height(12u,
                std::numeric_limits<float>::infinity()));

        check(heights.sync(transcript, width) == Index::Status::Unchanged);
        check(heights.lookup(12u)->measured);
        // A late chunk changes only message 12's revision: id 11's actual
        // height stays valid, while id 12 falls back until remeasured.
        check(emit(12u, TUI_STREAM_CHUNK, "Extra **new** paragraph\n\n"));
        check(heights.sync(transcript, width) == Index::Status::Updated);
        check(heights.lookup(11u)->measured);
        check(!heights.lookup(12u)->measured);
        const auto after_delta = heights.window_extent(11u, 2u);
        check(after_delta && !after_delta->visible_exact);
        if (after_delta) {
            check(std::fabs(after_delta->visible -
                            (height11 + 24.0)) < 0.01);
        }
        check(window.show(transcript, 1u) ==
              flex::tui_backend::TranscriptWindow::Status::Updated);
        box.update();
        check(live_index->lookup(12u)->measured);
        check(heights.record_height(12u, host12->height()));
        // Width invalidation is conservative: an unchanged C11 revision
        // needs a new Box measurement after wrapping changes.
        check(heights.sync(transcript, width - 20.0f) ==
              Index::Status::Updated);
        check(!heights.lookup(11u)->measured);
        check(!heights.lookup(12u)->measured);
        check(heights.record_height(11u, height11));
        // A font/CSS change invalidates the sample even if viewport width
        // and native stream sequence did not change.
        check(heights.sync(transcript, width - 20.0f, 1u) ==
              Index::Status::Updated);
        check(!heights.lookup(11u)->measured);
        check(heights.layout_revision() == 1u);
        window.set_height_layout_revision(1u);
        const auto unchanged_source = window.show(transcript, 1u);
        check(unchanged_source ==
                  flex::tui_backend::TranscriptWindow::Status::Unchanged ||
              unchanged_source ==
                  flex::tui_backend::TranscriptWindow::Status::Updated);
        check(live_index->layout_revision() == 1u);
        check(live_index->lookup(11u)->measured);
        const auto frozen_size = heights.size();
        check(heights.sync(
                  transcript, std::numeric_limits<float>::quiet_NaN(), 2u) ==
              Index::Status::Invalid);
        check(heights.size() == frozen_size);
        check(heights.layout_revision() == 1u);
        check(heights.width() == width - 20.0f);
        check(emit(12u, TUI_STREAM_END, {}));

        // Full C11 capacity admits a new message by evicting only the
        // earliest settled message. Its numeric measurements disappear.
        std::uint64_t evicted = 0u;
        check(tui_transcript_open(transcript, 14u, &evicted) ==
              TUI_TRANSCRIPT_OK && evicted == 10u);
        check(emit(14u, TUI_STREAM_BEGIN, {}));
        check(emit(14u, TUI_STREAM_CHUNK, "New retained content\n\n"));
        check(emit(14u, TUI_STREAM_END, {}));
        check(heights.sync(transcript, width - 20.0f) ==
              Index::Status::Updated);
        check(heights.size() == 4u);
        check(!heights.lookup(10u));
        check(heights.lookup(11u).has_value());
        check(heights.lookup(14u).has_value());
        check(!heights.height_before(10u));
        check(heights.height_before(11u).has_value());
        check(!heights.window_extent(10u, 2u));
        const auto now_oldest = heights.locate_offset(0.0);
        check(now_oldest && now_oldest->message_id == 11u &&
              now_oldest->prefix_exact);
        const auto evicted_plan = heights.window_extent(11u, 2u);
        check(evicted_plan && std::fabs(evicted_plan->leading) < 0.01);
        check(window.show_tail(transcript) ==
              flex::tui_backend::TranscriptWindow::Status::Updated);
        check(viewport->child_count() == 4u);
        check(window.leading_spacer_height() > 24.0f);
        check(window.trailing_spacer_height() < 1.0f);
        check(box.owned_element_count() == initial_owned ||
              box.owned_element_count() <= initial_owned + 40u);
        check(!live_index->lookup(10u));
        check(live_index->lookup(14u)->measured);
        const auto scroll_origin = window.virtual_location(0.0);
        check(scroll_origin && scroll_origin->message_id == 11u &&
              scroll_origin->prefix_exact);
        check(!window.virtual_location(-1.0).has_value());
        const auto scrolled = window.show_at_scroll_offset(transcript, 0.0);
        check(scrolled ==
                  flex::tui_backend::TranscriptWindow::Status::Updated ||
              scrolled ==
                  flex::tui_backend::TranscriptWindow::Status::Unchanged);
        check(window.visible_host(11u) != nullptr);
        check(window.visible_host(14u) == nullptr);
        check(window.leading_spacer_height() < 1.0f);
        check(viewport->child_count() == 4u);
        check(std::fabs(viewport->scroll_y()) < 1.0f);
        // A measured middle message may be selected without materializing
        // the newest history; the existing first Host is retained.
        const auto* current11 = window.visible_host(11u);
        const auto* current12 = window.visible_host(12u);
        const auto middle_start = live_index->height_before(12u);
        check(middle_start.has_value());
        if (middle_start && live_index->lookup(12u) &&
            live_index->lookup(12u)->measured) {
            const auto requested_y = *middle_start + 2.0;
            const auto mapped = window.virtual_location(requested_y);
            check(mapped && mapped->message_id == 12u &&
                  mapped->prefix_exact && mapped->message_measured);
            const auto reposition = window.show_at_scroll_offset(
                transcript, requested_y);
            check(reposition ==
                      flex::tui_backend::TranscriptWindow::Status::Updated ||
                  reposition ==
                      flex::tui_backend::TranscriptWindow::Status::Unchanged);
            check(window.visible_host(11u) == current11);
            check(window.visible_host(12u) == current12);
            check(std::fabs(viewport->scroll_y() -
                            (current12->y() + 2.0)) < 2.0);
        }
        const auto rejected_scroll = window.show_at_scroll_offset(
            transcript, std::numeric_limits<double>::quiet_NaN());
        check(rejected_scroll ==
              flex::tui_backend::TranscriptWindow::Status::Invalid);
        check(window.visible_host(11u) == current11);
        check(window.visible_host(12u) == current12);
        check(viewport->child_count() == 4u);
        // Offscreen retained ID 11 keeps a valid measured sample when its
        // generation, source revision and layout epoch have not changed.
        check(live_index->lookup(11u)->measured);

        // A smaller cache refuses oversized native retention before changing
        // any entries, without silently discarding newest message geometry.
        Index too_small(3u, 24.0f);
        check(too_small.sync(transcript, width) == Index::Status::Capacity);
        check(too_small.size() == 0u);
        // Constructor admission MUST reject impossible sizes before making
        // a vector allocation; this guards the private cache against API
        // misuse and preserves fixed-capacity ownership.
        bool rejected_capacity = false;
        try {
            Index invalid(std::numeric_limits<std::size_t>::max(), 24.0f);
        } catch (const std::invalid_argument&) {
            rejected_capacity = true;
        }
        check(rejected_capacity);
        bool rejected_fallback = false;
        try {
            Index invalid(4u, std::numeric_limits<float>::infinity());
        } catch (const std::invalid_argument&) {
            rejected_fallback = true;
        }
        check(rejected_fallback);
        bool all_measured = true;
        check(heights.total_height_estimate(&all_measured) > 0.0);
        check(!all_measured);

        // New empty C11 session, same bounded Box owner: prior offscreen
        // placeholders must collapse to zero and all old identities retire.
        const auto old_host_handle = box.handle_for(*current11);
        auto* empty_session = tui_transcript_create(4u, 4096u, 128u);
        check(empty_session != nullptr);
        if (empty_session) {
            check(window.show(empty_session, 0u) ==
                  flex::tui_backend::TranscriptWindow::Status::Updated);
            check(window.visible_count() == 0u);
            check(viewport->child_count() == 2u);
            check(std::fabs(window.leading_spacer_height()) < 1.0f);
            check(std::fabs(window.trailing_spacer_height()) < 1.0f);
            check(live_index->size() == 0u);
            check(box.resolve_handle(old_host_handle) == nullptr);
            check(tui_transcript_destroy(empty_session) ==
                  TUI_TRANSCRIPT_OK);
        }
        check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
    }

    it("resolves value-only virtual spacer heights through real FlexUI CSS") {
        // Stage-test native CSS custom-property height rendering before any
        // private transcript virtualization is enabled in the live demo.
        // Offscreen history must require only two owned spacer Elements,
        // never one Element per retained C11 message.
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 176.0f);
        box.load_css(
            "#spacer-root {display:flex;flex-direction:column;"
            "width:320px;height:176px;} "
            "#spacer-viewport {display:flex;flex-direction:column;"
            "width:160px;height:70px;overflow-y:scroll;} "
            ".tui-virtual-spacer {width:160px;flex-shrink:0;"
            "height:var(--tui-virtual-height);} "
            ".tui-visible {width:160px;flex-shrink:0;height:30px;}");
        auto* root = box.create("div", "spacer-root");
        auto* viewport = box.create("div", "spacer-viewport");
        auto* before = box.create("div");
        auto* visible = box.create("div");
        auto* after = box.create("div");
        check(root && viewport && before && visible && after);
        if (!root || !viewport || !before || !visible || !after) return;
        before->add_class("tui-virtual-spacer");
        after->add_class("tui-virtual-spacer");
        visible->add_class("tui-visible");
        before->set_custom_property("--tui-virtual-height", "120px");
        after->set_custom_property("--tui-virtual-height", "160px");
        root->append(viewport);
        viewport->append(before);
        viewport->append(visible);
        viewport->append(after);
        box.set_root(root);
        const auto owned = box.owned_element_count();
        box.update();
        check(viewport->child_count() == 3u);
        check(std::fabs(before->height() - 120.0f) < 1.0f);
        check(std::fabs(visible->height() - 30.0f) < 1.0f);
        check(std::fabs(after->height() - 160.0f) < 1.0f);
        check(viewport->max_scroll_y() >= 238.0f);
        check(viewport->set_scroll_offset(0.0f, 100.0f));
        before->set_custom_property("--tui-virtual-height", "80px");
        box.update();
        check(std::fabs(before->height() - 80.0f) < 1.0f);
        check(std::fabs(after->height() - 160.0f) < 1.0f);
        check(std::fabs(viewport->scroll_y() - 100.0f) < 1.0f);
        check(box.owned_element_count() == owned);
        check(box.cached_style_element_count() <= owned);
    }

    it("anchors GFM tables, fenced code and wide glyphs while estimates become measurements") {
        // Drive real C11 source -> MarkdownWidget -> Box layout. Message #1
        // starts outside the two-host window with only an estimated height;
        // a user scroll promotes it into an actual measured GFM document.
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 176.0f);
        box.load_css(
            "#gfm-root {width:100%;height:100%;display:flex;"
            "flex-direction:column;} "
            "#gfm-window {width:75%;height:72px;overflow-y:scroll;"
            "display:flex;flex-direction:column;} "
            ".transcript-message {width:100%;flex-shrink:0;} "
            ".tui-virtual-spacer {width:100%;flex-shrink:0;"
            "height:var(--tui-virtual-height);}");
        auto* root = box.create("div", "gfm-root");
        auto* viewport = box.create("div", "gfm-window");
        check(root && viewport);
        if (!root || !viewport) return;
        root->append(viewport);
        box.set_root(root);
        box.update();

        flex::tui_backend::TranscriptWindow window(box, *viewport, 2u);
        window.enable_height_tracking(3u, 24.0f);
        window.enable_virtual_spacers();
        auto* transcript = tui_transcript_create(3u, 8192u, 64u);
        check(transcript != nullptr);
        if (!transcript) return;

        std::uint64_t sequence = 0u;
        const auto emit = [&](std::uint64_t id, std::uint32_t type,
                              std::string_view source) {
            if (source.size() > 64u) return false;
            std::array<unsigned char,
                       sizeof(tui_stream_packet_header_t) + 64u> packet{};
            const tui_stream_packet_header_t header{id};
            std::memcpy(packet.data(), &header, sizeof(header));
            if (!source.empty())
                std::memcpy(packet.data() + sizeof(header),
                            source.data(), source.size());
            const tui_async_event_t event{
                type, 1u, ++sequence, sizeof(header) + source.size()
            };
            return tui_transcript_apply(
                transcript, &event, packet.data(), nullptr) ==
                TUI_TRANSCRIPT_OK;
        };
        const auto send = [&](std::uint64_t id, std::string_view source) {
            for (std::size_t offset = 0u; offset < source.size();
                 offset += 48u) {
                if (!emit(id, TUI_STREAM_CHUNK, source.substr(offset, 48u)))
                    return false;
            }
            return true;
        };
        const std::array<std::string, 3> sources{{
            "| 名称 | 结果 |\n| --- | --- |\n"
            "| 中文中中 | 😀 emoji |\n| longer 中文 | more 😀 emoji |\n\n",
            "```c\n// 中文 + 😀 emoji\n"
            "int calculate(void) {\n  return 42;\n}\n```\n\n",
            "### Last response\n\nParagraph one.\n\nParagraph two.\n\n"
        }};
        bool accepted = true;
        for (std::uint64_t id = 1u; id <= 3u; ++id) {
            std::uint64_t evicted = 99u;
            accepted = accepted &&
                tui_transcript_open(transcript, id, &evicted) ==
                    TUI_TRANSCRIPT_OK && evicted == 0u &&
                emit(id, TUI_STREAM_BEGIN, {}) &&
                send(id, sources[static_cast<std::size_t>(id - 1u)]) &&
                (id == 1u || emit(id, TUI_STREAM_END, {}));
        }
        check(accepted);
        if (!accepted) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }

        using Status = flex::tui_backend::TranscriptWindow::Status;
        check(window.show_tail(transcript) == Status::Updated);
        const auto* heights = window.tracked_heights();
        check(heights && heights->lookup(1u) && heights->lookup(2u));
        if (!heights || !heights->lookup(1u) || !heights->lookup(2u)) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        check(!heights->lookup(1u)->measured);
        check(heights->lookup(2u)->measured);
        check(std::fabs(window.leading_spacer_height() - 24.0f) < 1.0f);
        const auto estimated = window.virtual_location(8.0);
        check(estimated && estimated->message_id == 1u &&
              !estimated->message_measured);

        check(window.show_at_scroll_offset(transcript, 8.0) ==
              Status::Updated);
        auto* host1 = window.visible_host(1u);
        auto* host2 = window.visible_host(2u);
        check(host1 && host2);
        if (!host1 || !host2) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        const auto measured = heights->lookup(1u);
        check(measured && measured->measured &&
              measured->height > 24.0f);
        const auto resolved = window.virtual_location(8.0);
        check(resolved && resolved->message_id == 1u &&
              resolved->message_measured);
        const auto anchor = window.capture_scroll_anchor();
        check(!anchor.follow_tail && anchor.message_id == 1u);
        check(std::fabs(anchor.within_message_y - 8.0f) < 2.5f);

        // Complete a GFM source while it remains visible: line wraps,
        // table cells and code fences can all change actual Box height.
        check(send(1u,
                   "| 新增行中文 | 😀 😀 emoji |\n\n"
                   "```c\nint second(void) {\n  return 7;\n}\n```\n"));
        check(window.show_preserving_anchor(transcript, anchor) ==
              Status::Updated);
        check(window.visible_host(1u) == host1);
        check(window.visible_host(2u) == host2);
        check(heights->lookup(1u) && heights->lookup(1u)->measured);
        check(std::fabs(viewport->scroll_y() -
                        (host1->y() + anchor.within_message_y)) < 2.5f);

        // A narrower viewport invalidates all cached widths. Preserve
        // message identity/local position after the real Box reflow.
        const float previous_width = viewport->width();
        box.set_viewport(224.0f, 176.0f);
        box.update();
        check(viewport->width() + 1.0f < previous_width);
        const auto reflow = window.show_preserving_anchor(transcript, anchor);
        check(reflow == Status::Updated || reflow == Status::Unchanged);
        check(window.visible_host(1u) == host1);
        check(heights->lookup(1u) && heights->lookup(1u)->measured);
        check(std::fabs(heights->width() - viewport->width()) < 1.0f);
        check(std::fabs(viewport->scroll_y() -
                        (host1->y() + anchor.within_message_y)) < 2.5f);
        check(viewport->child_count() == 4u);
        check(emit(1u, TUI_STREAM_END, {}));
        check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
    }

    it("follows the new tail after a hidden message invalidates measured spacer height") {
        // A hidden ACTIVE message changes its generation/sequence revision.
        // Its *previously measured* short height becomes an estimated 400px
        // spacer while the two visible MarkdownWidget hosts stay identical.
        // Follow-tail must use the NEW Box max_scroll_y, not the old one.
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 176.0f);
        box.load_css(
            "#tail-root {display:flex;flex-direction:column;"
            "width:320px;height:176px;} "
            "#tail-window {display:flex;flex-direction:column;"
            "width:180px;height:60px;overflow-y:scroll;} "
            ".transcript-message {width:180px;flex-shrink:0;} "
            ".tui-virtual-spacer {width:180px;flex-shrink:0;"
            "height:var(--tui-virtual-height);}");
        auto* root = box.create("div", "tail-root");
        auto* viewport = box.create("div", "tail-window");
        check(root && viewport);
        if (!root || !viewport) return;
        root->append(viewport);
        box.set_root(root);
        box.update();
        flex::tui_backend::TranscriptWindow window(box, *viewport, 2u);
        window.enable_height_tracking(3u, 400.0f);
        window.enable_virtual_spacers();
        auto* transcript = tui_transcript_create(3u, 4096u, 128u);
        check(transcript != nullptr);
        if (!transcript) return;

        std::uint64_t sequence = 0u;
        const auto emit = [&](std::uint64_t id, std::uint32_t type,
                              std::string_view chunk) {
            if (chunk.size() > 128u) return false;
            std::array<unsigned char,
                       sizeof(tui_stream_packet_header_t) + 128u> packet{};
            const tui_stream_packet_header_t header{id};
            std::memcpy(packet.data(), &header, sizeof(header));
            if (!chunk.empty())
                std::memcpy(packet.data() + sizeof(header),
                            chunk.data(), chunk.size());
            const tui_async_event_t event{
                type, 1u, ++sequence, sizeof(header) + chunk.size()
            };
            return tui_transcript_apply(
                transcript, &event, packet.data(), nullptr) ==
                TUI_TRANSCRIPT_OK;
        };
        bool admitted = true;
        for (std::uint64_t id = 1u; id <= 3u; ++id) {
            std::uint64_t evicted = 99u;
            admitted = admitted &&
                tui_transcript_open(transcript, id, &evicted) ==
                    TUI_TRANSCRIPT_OK && evicted == 0u &&
                emit(id, TUI_STREAM_BEGIN, {}) &&
                emit(id, TUI_STREAM_CHUNK, "Tiny markdown line.\n") &&
                (id == 1u || emit(id, TUI_STREAM_END, {}));
        }
        check(admitted);
        if (!admitted) {
            check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
            return;
        }
        using Status = flex::tui_backend::TranscriptWindow::Status;
        check(window.show(transcript, 0u) == Status::Updated);
        const auto* index = window.tracked_heights();
        check(index && index->lookup(1u).has_value());
        check(index && index->lookup(1u)->measured);
        const float measured_before = index->lookup(1u)->height;
        check(measured_before < 300.0f);
        check(window.show_tail(transcript) == Status::Updated);
        auto* host2 = window.visible_host(2u);
        auto* host3 = window.visible_host(3u);
        check(host2 && host3 && viewport->child_count() == 4u);
        const float previous_max = viewport->max_scroll_y();
        check(viewport->scroll_y() >= previous_max - 1.0f);
        check(window.leading_spacer_height() < 300.0f);

        // The C11 owner still admits content for old active message #1;
        // neither visible message #2 nor #3 has changed.
        check(emit(1u, TUI_STREAM_CHUNK, "More words.\n"));
        check(window.show_tail(transcript) == Status::Updated);
        const float new_max = viewport->max_scroll_y();
        check(new_max >= previous_max + 30.0f);
        check(std::fabs(viewport->scroll_y() - new_max) < 2.0f);
        check(window.leading_spacer_height() > 399.0f);
        check(window.visible_host(1u) == nullptr);
        check(window.visible_host(2u) == host2);
        check(window.visible_host(3u) == host3);
        check(viewport->child_count() == 4u);
        check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
    }

    it("keeps opt-in virtual spacers and fixed hosts bounded for 128 messages") {
        // Exercise projection churn + explicit scroll-position rewindowing
        // with the real C11 retainer and only two permanent Markdown hosts.
        // This is an owner-only qualification, not an input-event loop.
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 176.0f);
        box.load_css(
            "#virtual-root {display:flex;flex-direction:column;"
            "width:320px;height:176px;} "
            "#virtual-window {display:flex;flex-direction:column;"
            "width:200px;height:68px;overflow-y:scroll;} "
            ".transcript-message {width:200px;flex-shrink:0;} "
            ".tui-virtual-spacer {width:200px;flex-shrink:0;"
            "height:var(--tui-virtual-height);}");
        auto* root = box.create("div", "virtual-root");
        auto* viewport = box.create("div", "virtual-window");
        check(root && viewport);
        if (!root || !viewport) return;
        root->append(viewport);
        box.set_root(root);
        box.update();
        flex::tui_backend::TranscriptWindow window(box, *viewport, 2u);
        window.enable_height_tracking(4u, 24.0f);
        window.enable_virtual_spacers();
        const auto initial_owned = box.owned_element_count();
        auto* transcript = tui_transcript_create(4u, 512u, 64u);
        check(transcript != nullptr);
        if (!transcript) return;
        std::uint64_t sequence = 0u;
        const auto emit = [&](std::uint64_t id, std::uint32_t type,
                              std::string_view text) {
            if (text.size() > 64u) return false;
            std::array<unsigned char,
                       sizeof(tui_stream_packet_header_t) + 64u> data{};
            const tui_stream_packet_header_t header{id};
            std::memcpy(data.data(), &header, sizeof(header));
            if (!text.empty())
                std::memcpy(data.data() + sizeof(header),
                            text.data(), text.size());
            const tui_async_event_t event{
                type, 1u, ++sequence, sizeof(header) + text.size()
            };
            return tui_transcript_apply(
                transcript, &event, data.data(), nullptr) ==
                TUI_TRANSCRIPT_OK;
        };
        using Status = flex::tui_backend::TranscriptWindow::Status;
        bool qualified = true;
        std::size_t peak_owned = initial_owned;
        std::size_t peak_styles = 0u;
        for (std::uint64_t id = 1u; id <= 128u; ++id) {
            std::uint64_t evicted = 0u;
            const auto admitted = tui_transcript_open(transcript, id, &evicted);
            const std::string markdown =
                "## Response " + std::to_string(id) +
                "\n\n中 **stream** with fixed slot reuse.\n\n";
            if (admitted != TUI_TRANSCRIPT_OK ||
                evicted != (id > 4u ? id - 4u : 0u) ||
                !emit(id, TUI_STREAM_BEGIN, {}) ||
                !emit(id, TUI_STREAM_CHUNK, markdown) ||
                !emit(id, TUI_STREAM_END, {})) {
                qualified = false;
                break;
            }
            const auto tail = window.show_tail(transcript);
            if (tail != Status::Updated) {
                qualified = false;
                break;
            }
            const auto* index = window.tracked_heights();
            const auto retained = std::min<std::size_t>(
                4u, static_cast<std::size_t>(id));
            check(window.slot_capacity() == 2u);
            check(window.visible_count() ==
                  std::min<std::size_t>(2u, retained));
            check(viewport->child_count() == window.visible_count() + 2u);
            check(window.visible_host(id) != nullptr);
            check(index && index->size() == retained);
            check(index && index->lookup(id).has_value());
            if (id > 4u) check(index && !index->lookup(id - 4u));
            peak_owned = std::max(peak_owned, box.owned_element_count());
            peak_styles = std::max(
                peak_styles, box.cached_style_element_count());
            check(box.owned_element_count() <= initial_owned + 64u);
            check(box.cached_style_element_count() <= initial_owned + 64u);

            // Simulate a user seeking to the oldest retained coordinate
            // while new messages arrive. The owner must not fabricate DOM.
            const auto browsed = window.show_at_scroll_offset(transcript, 0.0);
            check(browsed == Status::Updated || browsed == Status::Unchanged);
            check(viewport->child_count() == window.visible_count() + 2u);
            check(viewport->scroll_y() < 1.0f);
            const auto returned = window.show_tail(transcript);
            check(returned == Status::Updated || returned == Status::Unchanged);
            check(window.visible_host(id) != nullptr);
            check(box.owned_element_count() <= initial_owned + 64u);
            check(box.cached_style_element_count() <= initial_owned + 64u);
        }
        check(qualified);
        check(tui_transcript_count(transcript) == 4u);
        check(window.visible_count() == 2u);
        std::printf("TUI_VIRTUAL_STRESS turns=128 peak_elements=%zu "
                    "peak_styles=%zu visible_hosts=%zu\n",
                    peak_owned, peak_styles, window.visible_count());
        check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
    }

    it("bounds real transcript projection ownership over 512 owner turns") {
        // Exercise the actual C11 registry + fixed-slot FlexUI window, not a
        // fake list or benchmark-only renderer. Structural budgets are hard
        // assertions; observed p50/p95 timings establish an exact-head CI
        // baseline before choosing any machine-dependent latency threshold.
        constexpr std::uint64_t kMessages = 512u;
        constexpr std::size_t kRetained = 4u;
        constexpr std::size_t kVisible = 3u;
        constexpr std::size_t kMaxChunk = kLongSessionMaxChunk;
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 160.0f);
        box.load_css(
            "#root { width:320px; height:160px; display:flex; "
            "flex-direction:column; } "
            "#long-window { width:280px; height:96px; overflow-y:scroll; "
            "display:flex; flex-direction:column; } "
            ".transcript-message { width:280px; flex-shrink:0; }");
        auto* root = box.create("div", "root");
        auto* viewport = box.create("div", "long-window");
        check(root && viewport);
        if (!root || !viewport) return;
        root->append(viewport);
        box.set_root(root);
        flex::tui_backend::TranscriptWindow window(box, *viewport, kVisible);
        const std::size_t initial_owned = box.owned_element_count();

        auto* transcript = tui_transcript_create(kRetained, 512u, kMaxChunk);
        check(transcript != nullptr);
        if (!transcript) return;
        std::uint64_t sequence = 0u;
        const auto emit = [&](std::uint64_t id, std::uint32_t type,
                              std::string_view text) {
            if (text.size() > kMaxChunk) return false;
            std::array<unsigned char,
                       sizeof(tui_stream_packet_header_t) +
                           kLongSessionMaxChunk> data{};
            const tui_stream_packet_header_t header{id};
            std::memcpy(data.data(), &header, sizeof(header));
            if (!text.empty())
                std::memcpy(data.data() + sizeof(header),
                            text.data(), text.size());
            const tui_async_event_t event{
                type, 1u, ++sequence, sizeof(header) + text.size()
            };
            return tui_transcript_apply(
                transcript, &event, data.data(), nullptr) ==
                TUI_TRANSCRIPT_OK;
        };

        std::vector<std::int64_t> update_us;
        update_us.reserve(static_cast<std::size_t>(kMessages));
        std::size_t peak_owned = initial_owned;
        std::size_t peak_cached_styles = 0u;
        const auto session_start = std::chrono::steady_clock::now();
        bool all_accepted = true;
        for (std::uint64_t id = 1u; id <= kMessages; ++id) {
            std::uint64_t evicted = 0u;
            if (tui_transcript_open(transcript, id, &evicted) !=
                    TUI_TRANSCRIPT_OK ||
                evicted != (id > kRetained ? id - kRetained : 0u)) {
                all_accepted = false;
                break;
            }
            const std::string markdown =
                "# Response " + std::to_string(id) +
                "\n\nStreaming 中 **content**.\n";
            if (!emit(id, TUI_STREAM_BEGIN, {}) ||
                !emit(id, TUI_STREAM_CHUNK, markdown) ||
                !emit(id, TUI_STREAM_END, {})) {
                all_accepted = false;
                break;
            }

            const auto start = std::chrono::steady_clock::now();
            const auto status = window.show_tail(transcript);
            box.update();
            update_us.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start).count());
            peak_owned = std::max(peak_owned, box.owned_element_count());
            peak_cached_styles = std::max(
                peak_cached_styles, box.cached_style_element_count());
            const auto expected_visible = std::min<std::size_t>(
                kVisible, static_cast<std::size_t>(id));
            const auto expected_retained = std::min<std::size_t>(
                kRetained, static_cast<std::size_t>(id));
            check(status == flex::tui_backend::TranscriptWindow::Status::Updated);
            check(window.slot_capacity() == kVisible);
            check(window.visible_count() == expected_visible);
            check(viewport->child_count() == expected_visible);
            check(window.visible_host(id) != nullptr);
            check(tui_transcript_count(transcript) == expected_retained);
            // These budgets cover Box-owned projections and pointer-keyed
            // style baselines. LSan independently checks teardown leaks.
            check(box.owned_element_count() <= initial_owned + 32u);
            check(box.cached_style_element_count() <= initial_owned + 32u);
        }
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - session_start).count();
        check(all_accepted);
        check(update_us.size() == static_cast<std::size_t>(kMessages));
        if (!update_us.empty()) {
            std::sort(update_us.begin(), update_us.end());
            const auto p50 = update_us[update_us.size() / 2u];
            const auto p95 = update_us[
                (update_us.size() - 1u) * 95u / 100u];
            std::printf(
                "TUI_LONG_SESSION iterations=%zu elapsed_ms=%lld "
                "p50_update_us=%lld p95_update_us=%lld "
                "peak_box_elements=%zu peak_style_baselines=%zu\n",
                update_us.size(), static_cast<long long>(elapsed_ms),
                static_cast<long long>(p50),
                static_cast<long long>(p95),
                peak_owned, peak_cached_styles);
            // Coarse CI regression envelope with substantial headroom over
            // measured ASan/UBSan and LSan baselines, not a product latency
            // SLA: pathological CPU growth must fail rather than just log.
            check(elapsed_ms <= 30000);
            check(p95 <= 50000);
        }
        check(tui_transcript_destroy(transcript) == TUI_TRANSCRIPT_OK);
    }

    it("retires intermediate Markdown projections during repeated owner updates") {
        flexUI::Box box(nullptr);
        box.set_viewport(320.0f, 120.0f);
        auto* root = box.create("div", "root");
        auto* host = box.create_widget<flexUI::MarkdownWidget>(
            "div", "repeated", "");
        check(root && host && host->widget);
        if (!root || !host || !host->widget) return;
        root->append(host);
        box.set_root(root);
        auto* widget = static_cast<flexUI::MarkdownWidget*>(host->widget);
        for (unsigned i = 0u; i < 120u; ++i) {
            check(widget->append_markdown("x") ==
                  flexUI::MarkdownAppendStatus::Appended);
            widget->update(0.0f, *host);
            box.update();
            // This document has one paragraph regardless of stream length.
            // Previously detached projections and style records must not
            // accumulate across UI-owner turns.
            check(host->child_count() == 1u);
            check(box.owned_element_count() <= 32u);
            check(box.cached_style_element_count() <= 32u);
        }
        check(widget->markdown() == std::string(120u, 'x'));
        check(host->child_count() == 1u);
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
