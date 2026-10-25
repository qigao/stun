#include <atomic>
#include <future>

#include <tinytest.hpp>
#undef group

#include <backends/tui/init.h>
#include <backends/tui/transcript_window.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/stream_scroll_follow.h>
#include <tui.h>
#include <tui_transcript.h>
#include "tui_async_wait_internal.h"
#include "tui_app_internal.h"
#include "tui_terminal_internal.h"

#include <array>
#include <chrono>
#include <cmath>
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
        check(box.resolve_handle(retired_handle) == nullptr);
        check(box.resolve_handle(held_handle) == nullptr);
        check(box.focused_element() == focus);
        check(tui_transcript_destroy(transcript_state) ==
              TUI_TRANSCRIPT_OK);
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
