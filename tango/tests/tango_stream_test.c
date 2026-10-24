/* Real Tango static library + installed Salts Unicode streaming conformance. */
#include "tui.h"
#include "tui_app_internal.h"
#include "tui_terminal_internal.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); return 1; \
} } while (0)

static tui_stream_status_t apply(
    tui_stream_t *stream, uint32_t type, uint64_t generation,
    uint64_t seq, const void *chunk, size_t length) {
    unsigned char bytes[sizeof(tui_stream_packet_header_t) + 128u];
    tui_stream_packet_header_t header = {42u};
    if (length > 128u || (length && !chunk)) return TUI_STREAM_INVALID;
    memcpy(bytes, &header, sizeof(header));
    if (length) memcpy(bytes + sizeof(header), chunk, length);
    const tui_async_event_t event = {
        type, generation, seq, sizeof(header) + length
    };
    return tui_stream_apply(stream, &event, bytes);
}

static int test_fragmented_unicode_generation_and_cancel(void) {
    tui_stream_t *stream = tui_stream_create(42u, 128u, 16u);
    tui_stream_snapshot_t stat;
    size_t bytes = 0u;
    CHECK(stream != NULL);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 1u, "A", 1u) ==
          TUI_STREAM_STALE);
    CHECK(apply(stream, TUI_STREAM_BEGIN, 1u, 2u, NULL, 0u) ==
          TUI_STREAM_OK);
    const unsigned char first[] = {0xe4u};
    const unsigned char second[] = {0xb8u, 0xadu};
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 3u, first, sizeof(first)) ==
          TUI_STREAM_OK);
    CHECK(tui_stream_get_snapshot(stream, &stat) == TUI_STREAM_OK);
    CHECK(stat.pending_utf8_bytes == 1u && stat.retained_bytes == 0u);
    CHECK(apply(stream, TUI_STREAM_END, 1u, 4u, NULL, 0u) ==
          TUI_STREAM_INVALID);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 5u, second, sizeof(second)) ==
          TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_text(stream, &bytes), "中") == 0 && bytes == 3u);

    /* This emoji is three Unicode scalars but one displayed grapheme.
     * Split its UTF-8 bytes across several real delivery boundaries. */
    const char *emoji = "👩‍💻";
    const size_t emoji_len = strlen(emoji);
    CHECK(emoji_len > 6u);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 6u, emoji, 2u) ==
          TUI_STREAM_OK);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 7u, emoji + 2u, 3u) ==
          TUI_STREAM_OK);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 8u, emoji + 5u,
                emoji_len - 5u) == TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_text(stream, &bytes), "中👩‍💻") == 0);
    CHECK(tui_grapheme_width(emoji, emoji_len) == 2);

    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 8u, "!", 1u) ==
          TUI_STREAM_STALE);
    CHECK(apply(stream, TUI_STREAM_CANCEL, 1u, 9u, NULL, 0u) ==
          TUI_STREAM_OK);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 10u, "bad", 3u) ==
          TUI_STREAM_STALE);
    CHECK(tui_stream_get_snapshot(stream, &stat) == TUI_STREAM_OK);
    CHECK(stat.state == TUI_STREAM_CANCELLED && stat.last_sequence == 9u);
    CHECK(apply(stream, TUI_STREAM_BEGIN, 2u, 11u, NULL, 0u) ==
          TUI_STREAM_OK);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 12u, "late", 4u) ==
          TUI_STREAM_STALE);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 2u, 13u, "new", 3u) ==
          TUI_STREAM_OK);
    CHECK(apply(stream, TUI_STREAM_END, 2u, 14u, NULL, 0u) ==
          TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_text(stream, &bytes), "new") == 0 && bytes == 3u);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 2u, 15u, "late", 4u) ==
          TUI_STREAM_STALE);
    CHECK(tui_stream_destroy(stream) == TUI_STREAM_OK);
    return 0;
}

static int test_capacity_control_and_grapheme_eviction(void) {
    tui_stream_t *stream = tui_stream_create(42u, 9u, 12u);
    tui_stream_snapshot_t stat;
    size_t bytes = 0u;
    CHECK(stream != NULL);
    CHECK(apply(stream, TUI_STREAM_BEGIN, 1u, 1u, NULL, 0u) ==
          TUI_STREAM_OK);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 2u, "中中中",
                strlen("中中中")) == TUI_STREAM_OK);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 3u, "A", 1u) ==
          TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_text(stream, &bytes), "中中A") == 0);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 4u, "❤️",
                strlen("❤️")) == TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_text(stream, &bytes), "A❤️") == 0);
    CHECK(tui_stream_get_snapshot(stream, &stat) == TUI_STREAM_OK);
    CHECK(stat.discarded_bytes == 9u);
    const unsigned char esc = 27u;
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 5u, &esc, 1u) ==
          TUI_STREAM_INVALID);
    CHECK(strcmp(tui_stream_text(stream, &bytes), "A❤️") == 0);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 6u, "toolongtext123",
                strlen("toolongtext123")) == TUI_STREAM_CAPACITY);
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 7u, "x\ny", 3u) ==
          TUI_STREAM_OK);
    CHECK(tui_stream_tail(stream, 1u, &bytes) != NULL);
    CHECK(bytes == 1u && strcmp(tui_stream_tail(stream, 1u, &bytes), "y") == 0);
    CHECK(tui_stream_destroy(stream) == TUI_STREAM_OK);

    tui_stream_t *tiny = tui_stream_create(42u, 2u, 16u);
    CHECK(tiny != NULL);
    CHECK(apply(tiny, TUI_STREAM_BEGIN, 1u, 1u, NULL, 0u) ==
          TUI_STREAM_OK);
    CHECK(apply(tiny, TUI_STREAM_CHUNK, 1u, 2u, "中", strlen("中")) ==
          TUI_STREAM_CAPACITY);
    CHECK(tui_stream_get_snapshot(tiny, &stat) == TUI_STREAM_OK);
    CHECK(stat.retained_bytes == 0u && stat.last_sequence == 1u);
    CHECK(tui_stream_destroy(tiny) == TUI_STREAM_OK);
    return 0;
}

typedef struct stream_ui_model {
    tui_stream_t *stream;
    size_t views;
    size_t updates;
    size_t ignored;
} stream_ui_model_t;

static bool ui_keyboard(int msg, void *model) {
    (void)msg; (void)model; return true;
}
static int ui_map(const tui_event_t *e) {
    (void)e; return -1;
}
static bool ui_stream_update(const tui_async_event_t *event,
                             const void *payload, void *model) {
    stream_ui_model_t *ui = (stream_ui_model_t *)model;
    const tui_stream_status_t status = tui_stream_apply(ui->stream,event,payload);
    if (status == TUI_STREAM_STALE || status == TUI_STREAM_WRONG_MESSAGE) {
        ++ui->ignored;
        return true;
    }
    if (status != TUI_STREAM_OK) return false;
    ++ui->updates;
    return true;
}
static void ui_stream_view(const void *model, tui_buffer_t *buf) {
    stream_ui_model_t *ui = (stream_ui_model_t *)model;
    ++ui->views;
    size_t length = 0;
    const char *txt = tui_stream_text(ui->stream, &length);
    if (txt && length) {
        /* This fixture places one bounded text run at the terminal cells. */
        (void)tui_buffer_text(buf, 0, 0, txt, TUI_WHITE, TUI_BLACK);
    }
}
static bool ui_mock_write(tui_terminal_t *term, const char *p, size_t n) {
    (void)term; (void)p; (void)n; return true;
}
static int send_ui_packet(tui_async_queue_t *queue,
                          uint32_t type, uint64_t generation,
                          const void *text, size_t size) {
    unsigned char packet[sizeof(tui_stream_packet_header_t) + 32u];
    tui_stream_packet_header_t header = {42u};
    CHECK(size <= 32u);
    memcpy(packet, &header, sizeof(header));
    if (size) memcpy(packet + sizeof(header), text, size);
    CHECK(tui_async_queue_post(queue, type, generation,
          packet, sizeof(header) + size, NULL) == TUI_ASYNC_OK);
    return 0;
}

static int test_real_async_stream_to_tango_cells(void) {
    stream_ui_model_t model = {0};
    model.stream = tui_stream_create(42u, 64u, 16u);
    CHECK(model.stream);
    tui_app_t *app = tui_app_create(&model, ui_keyboard,
                                    ui_stream_view, ui_map);
    CHECK(app);
    CHECK(tui_app_set_async_update(app, ui_stream_update));
    tui_terminal_t *term = app->terminal;
    term->width = 12;
    term->height = 1;
    term->front = tui_buffer_create(12, 1);
    term->back = tui_buffer_create(12, 1);
    CHECK(term->front && term->back);
    term->initialized = true;
    term->color_mode = TUI_COLOR_TRUECOLOR;
    term->write_output = ui_mock_write;
    app->needs_redraw = false;

    tui_async_queue_t *queue = tui_app_async_queue(app);
    CHECK(queue);
    CHECK(send_ui_packet(queue, TUI_STREAM_BEGIN, 3u, NULL, 0u) == 0);
    CHECK(send_ui_packet(queue, TUI_STREAM_CHUNK, 3u, "A", 1u) == 0);
    const unsigned char cjk1[] = {0xe4u};
    const unsigned char cjk2[] = {0xb8u,0xadu};
    CHECK(send_ui_packet(queue, TUI_STREAM_CHUNK, 3u, cjk1, sizeof(cjk1)) == 0);
    CHECK(send_ui_packet(queue, TUI_STREAM_CHUNK, 3u, cjk2, sizeof(cjk2)) == 0);
    CHECK(send_ui_packet(queue, TUI_STREAM_CHUNK, 3u, "B", 1u) == 0);
    CHECK(tui_app_dispatch_once(app));
    CHECK(model.updates == 5u && model.views == 1u);
    CHECK(tui_buffer_at_const(term->front, 0, 0)->ch == 'A');
    CHECK(tui_buffer_at_const(term->front, 1, 0)->ch == 0x4e2du);
    CHECK(tui_buffer_at_const(term->front, 1, 0)->span == TUI_CELL_SPAN_WIDE);
    CHECK(tui_buffer_at_const(term->front, 2, 0)->span ==
          TUI_CELL_SPAN_CONTINUATION);
    CHECK(tui_buffer_at_const(term->front, 3, 0)->ch == 'B');

    CHECK(send_ui_packet(queue, TUI_STREAM_CANCEL, 3u, NULL, 0u) == 0);
    CHECK(send_ui_packet(queue, TUI_STREAM_CHUNK, 3u, "Z", 1u) == 0);
    CHECK(tui_app_dispatch_once(app));
    CHECK(model.ignored == 1u);
    CHECK(tui_buffer_at_const(term->front, 3, 0)->ch == 'B');
    CHECK(model.views == 2u);
    term->initialized = false;
    tui_app_destroy(app);
    CHECK(tui_stream_destroy(model.stream) == TUI_STREAM_OK);
    return 0;
}

typedef struct stream_wrong_owner {
    tui_stream_t *stream;
    tui_stream_status_t apply_status;
    tui_stream_status_t destroy_status;
    tui_stream_status_t snapshot_status;
} stream_wrong_owner_t;

static void stream_wrong_owner_worker(void *ctx) {
    stream_wrong_owner_t *test = (stream_wrong_owner_t *)ctx;
    unsigned char bytes[sizeof(tui_stream_packet_header_t)] = {0};
    const tui_stream_packet_header_t header = {42u};
    memcpy(bytes, &header, sizeof(header));
    const tui_async_event_t event = {
        TUI_STREAM_BEGIN, 1u, 1u, sizeof(header)
    };
    tui_stream_snapshot_t out = {0};
    test->apply_status = tui_stream_apply(test->stream, &event, bytes);
    test->snapshot_status = tui_stream_get_snapshot(test->stream, &out);
    test->destroy_status = tui_stream_destroy(test->stream);
}

static int test_invalid_packet_owner_and_newline_tail(void) {
    tui_stream_t *stream = tui_stream_create(42u, 128u, 48u);
    CHECK(stream != NULL);
    stream_wrong_owner_t wrong = {0};
    wrong.stream = stream;
    cmeta_thread_t thread = NULL;
    CHECK(cmeta_thread_create(&thread, stream_wrong_owner_worker, &wrong) == 0);
    CHECK(cmeta_thread_join(&thread) == 0);
    CHECK(wrong.apply_status == TUI_STREAM_NOT_OWNER);
    CHECK(wrong.snapshot_status == TUI_STREAM_NOT_OWNER);
    CHECK(wrong.destroy_status == TUI_STREAM_NOT_OWNER);

    CHECK(apply(stream, TUI_STREAM_BEGIN, 1u, 1u, NULL, 0u) ==
          TUI_STREAM_OK);
    const unsigned char malformed[] = {0xffu, 'a', 'b', 'c'};
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 2u,
                malformed, sizeof(malformed)) == TUI_STREAM_INVALID);
    const unsigned char c1_control[] = {0xc2u, 0x85u};
    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 3u,
                c1_control, sizeof(c1_control)) == TUI_STREAM_INVALID);
    size_t bytes = 10u;
    CHECK(strcmp(tui_stream_text(stream, &bytes), "") == 0 && bytes == 0u);

    CHECK(apply(stream, TUI_STREAM_CHUNK, 1u, 4u,
                "first\nsecond\nthird", strlen("first\nsecond\nthird")) ==
          TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_tail(stream, 1u, &bytes), "third") == 0);
    CHECK(bytes == strlen("third"));
    CHECK(strcmp(tui_stream_tail(stream, 2u, &bytes), "second\nthird") == 0);
    CHECK(strcmp(tui_stream_tail(stream, 3u, &bytes),
                 "first\nsecond\nthird") == 0);

    const tui_async_event_t mismatch = {
        TUI_STREAM_CHUNK, 1u, 5u, sizeof(tui_stream_packet_header_t)
    };
    tui_stream_packet_header_t other = {99u};
    CHECK(tui_stream_apply(stream, &mismatch, &other) ==
          TUI_STREAM_WRONG_MESSAGE);
    CHECK(apply(stream, TUI_STREAM_END, 1u, 6u, NULL, 0u) ==
          TUI_STREAM_OK);
    CHECK(tui_stream_destroy(stream) == TUI_STREAM_OK);

    tui_stream_t *clusters = tui_stream_create(42u, 3u, 8u);
    CHECK(clusters != NULL);
    CHECK(apply(clusters, TUI_STREAM_BEGIN, 1u, 1u, NULL, 0u) ==
          TUI_STREAM_OK);
    CHECK(apply(clusters, TUI_STREAM_CHUNK, 1u, 2u, "e", 1u) ==
          TUI_STREAM_OK);
    const unsigned char combining[] = {0xccu,0x81u};
    CHECK(apply(clusters, TUI_STREAM_CHUNK, 1u, 3u,
                combining, sizeof(combining)) == TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_text(clusters, &bytes), "é") == 0);
    CHECK(apply(clusters, TUI_STREAM_CHUNK, 1u, 4u, "A", 1u) ==
          TUI_STREAM_OK);
    CHECK(strcmp(tui_stream_text(clusters, &bytes), "A") == 0);
    CHECK(tui_stream_destroy(clusters) == TUI_STREAM_OK);
    return 0;
}

int main(void) {
    if (test_fragmented_unicode_generation_and_cancel()) return 1;
    if (test_capacity_control_and_grapheme_eviction()) return 1;
    if (test_real_async_stream_to_tango_cells()) return 1;
    if (test_invalid_packet_owner_and_newline_tail()) return 1;
    puts("Tango streaming tests passed");
    return 0;
}
