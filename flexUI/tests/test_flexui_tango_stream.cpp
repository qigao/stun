#include <tinytest.hpp>
#undef group

#include <backends/tui/init.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <tui.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr uint64_t kMessageId = 42u;

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

bool contains_visible_cells(const tui_buffer_t* buffer) {
    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x + 3 < buffer->width; ++x) {
            const auto* a = tui_buffer_at_const(buffer, x, y);
            const auto* cjk = tui_buffer_at_const(buffer, x + 1, y);
            const auto* second = tui_buffer_at_const(buffer, x + 2, y);
            const auto* b = tui_buffer_at_const(buffer, x + 3, y);
            if (a && cjk && second && b &&
                a->ch == 'A' && cjk->ch == 0x4e2du &&
                cjk->span == TUI_CELL_SPAN_WIDE &&
                second->span == TUI_CELL_SPAN_CONTINUATION &&
                b->ch == 'B') return true;
        }
    }
    return false;
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

        const tui_async_queue_config_t config{32u, 128u};
        tui_async_queue_t* queue = tui_async_queue_create(&config);
        tui_stream_t* stream = tui_stream_create(kMessageId, 512u, 32u);
        check(queue != nullptr && stream != nullptr);
        if (!queue || !stream) {
            if (stream) (void)tui_stream_destroy(stream);
            if (queue) {
                (void)tui_async_queue_close(queue);
                (void)tui_async_queue_destroy(queue);
            }
            return;
        }
        check(tui_async_queue_is_owner(queue));

        bool producer_ok = false;
        std::thread worker([&] {
            // The UI Model and widget are never touched from this producer.
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
        worker.join();
        check(producer_ok);

        std::array<unsigned char, 128u> payload{};
        tui_async_event_t event{};
        std::size_t accepted_chunks = 0u;
        bool ended = false;
        for (;;) {
            const auto result = tui_async_queue_try_pop(
                queue, &event, payload.data(), payload.size());
            if (result == TUI_ASYNC_EMPTY) break;
            check(result == TUI_ASYNC_OK);
            if (result != TUI_ASYNC_OK) break;

            // Order, generation, UTF-8 and terminal validity are checked by
            // the same production Tango stream state as the plain-text UI.
            const auto stream_result = tui_stream_apply(
                stream, &event, payload.data());
            check(stream_result == TUI_STREAM_OK);
            if (stream_result != TUI_STREAM_OK) break;
            if (event.type == TUI_STREAM_BEGIN) {
                widget->set_markdown("");
            } else if (event.type == TUI_STREAM_CHUNK) {
                const std::size_t prefix = sizeof(tui_stream_packet_header_t);
                check(event.payload_size >= prefix);
                if (event.payload_size < prefix) break;
                const auto delta = std::string_view(
                    reinterpret_cast<const char*>(payload.data() + prefix),
                    event.payload_size - prefix);
                check(widget->append_markdown(delta) ==
                      flexUI::MarkdownAppendStatus::Appended);
                ++accepted_chunks;
            } else if (event.type == TUI_STREAM_END) {
                ended = true;
            }
        }
        check(ended && accepted_chunks == 4u);

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

        // After terminal state, late data must not modify either consumer.
        const char late[] = "late";
        std::array<unsigned char, sizeof(tui_stream_packet_header_t)+4u> stale{};
        const tui_stream_packet_header_t header{kMessageId};
        std::memcpy(stale.data(), &header, sizeof(header));
        std::memcpy(stale.data()+sizeof(header), late, 4u);
        const tui_async_event_t old_chunk{
            TUI_STREAM_CHUNK, 1u, event.sequence + 1u, stale.size()};
        check(tui_stream_apply(stream, &old_chunk, stale.data()) ==
              TUI_STREAM_STALE);
        check(widget->markdown() == "A中B");

        check(tui_async_queue_close(queue) == TUI_ASYNC_OK);
        check(tui_async_queue_destroy(queue) == TUI_ASYNC_OK);
        check(tui_stream_destroy(stream) == TUI_STREAM_OK);
    }
}
