/* Native C11 bounded transcript registry / eviction and tombstone conformance. */
#include "tui_transcript.h"
#include <salts/thread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); return 1; \
} } while (0)

static tui_transcript_status_t send_packet(
    tui_transcript_t *t, uint64_t id, uint32_t type,
    uint64_t generation, uint64_t sequence,
    const char *chunk, size_t size) {
    unsigned char packet[sizeof(tui_stream_packet_header_t) + 64u];
    const tui_stream_packet_header_t header = {id};
    if (size > 64u || (size && !chunk)) return TUI_TRANSCRIPT_INVALID;
    memcpy(packet, &header, sizeof(header));
    if (size) memcpy(packet + sizeof(header), chunk, size);
    const tui_async_event_t event = {
        type, generation, sequence, sizeof(header) + size
    };
    return tui_transcript_apply(t, &event, packet, NULL);
}

typedef struct wrong_owner {
    tui_transcript_t *transcript;
    tui_transcript_status_t get;
    tui_transcript_status_t opened;
    tui_transcript_status_t destroy;
    size_t count;
} wrong_owner_t;

static void wrong_owner_worker(void *argument) {
    wrong_owner_t *w = (wrong_owner_t *)argument;
    tui_transcript_message_t snapshot = {0};
    w->get = tui_transcript_get(w->transcript, 41u, &snapshot);
    w->opened = tui_transcript_open(w->transcript, 45u, NULL);
    w->count = tui_transcript_count(w->transcript);
    w->destroy = tui_transcript_destroy(w->transcript);
}

static int test_bounded_registry_and_permanent_tombstones(void) {
    CHECK(tui_transcript_create(0u, 64u, 32u) == NULL);
    CHECK(tui_transcript_create(SIZE_MAX, 64u, 32u) == NULL);
    CHECK(tui_transcript_create(2u, SIZE_MAX, 32u) == NULL);
    CHECK(tui_transcript_create(2u, 64u, SIZE_MAX) == NULL);
    tui_transcript_t *t = tui_transcript_create(2u, 64u, 32u);
    CHECK(t != NULL);

    uint64_t evicted = 777u;
    CHECK(tui_transcript_open(t, 41u, &evicted) == TUI_TRANSCRIPT_OK);
    CHECK(evicted == 0u);
    CHECK(tui_transcript_open(t, 42u, &evicted) == TUI_TRANSCRIPT_OK);
    CHECK(tui_transcript_count(t) == 2u);
    CHECK(tui_transcript_open(t, 41u, &evicted) == TUI_TRANSCRIPT_STALE);
    CHECK(tui_transcript_open(t, 40u, &evicted) == TUI_TRANSCRIPT_STALE);
    CHECK(tui_transcript_open(t, 43u, &evicted) == TUI_TRANSCRIPT_FULL);
    CHECK(evicted == 0u);

    wrong_owner_t wrong = {t, 0, 0, 0, 1u};
    cmeta_thread_t thread = NULL;
    CHECK(cmeta_thread_create(&thread, wrong_owner_worker, &wrong) == 0);
    CHECK(cmeta_thread_join(&thread) == 0);
    CHECK(wrong.get == TUI_TRANSCRIPT_NOT_OWNER);
    CHECK(wrong.opened == TUI_TRANSCRIPT_NOT_OWNER);
    CHECK(wrong.destroy == TUI_TRANSCRIPT_NOT_OWNER && wrong.count == 0u);

    CHECK(send_packet(t, 41u, TUI_STREAM_BEGIN, 1u, 1u, NULL, 0u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(send_packet(t, 42u, TUI_STREAM_BEGIN, 1u, 2u, NULL, 0u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(send_packet(t, 41u, TUI_STREAM_CHUNK, 1u, 3u, "A", 1u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(send_packet(t, 42u, TUI_STREAM_CHUNK, 1u, 4u, "中", 3u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(send_packet(t, 41u, TUI_STREAM_END, 1u, 5u, NULL, 0u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(send_packet(t, 41u, TUI_STREAM_CHUNK, 1u, 6u, "late", 4u) ==
          TUI_TRANSCRIPT_STALE);
    CHECK(send_packet(t, 99u, TUI_STREAM_BEGIN, 1u, 7u, NULL, 0u) ==
          TUI_TRANSCRIPT_UNKNOWN);
    size_t bytes = 0u;
    const char *view = tui_transcript_text(t, 42u, &bytes);
    CHECK(view != NULL && bytes == 3u && strcmp(view, "中") == 0);

    tui_transcript_message_t before[2] = {{0}};
    size_t count = 99u;
    CHECK(tui_transcript_window(t, 0u, 2u, before, 1u, &count) ==
          TUI_TRANSCRIPT_FULL);
    CHECK(count == 99u && before[0].message_id == 0u);
    CHECK(tui_transcript_window(t, 0u, 2u, before, 2u, &count) ==
          TUI_TRANSCRIPT_OK);
    CHECK(count == 2u && before[0].message_id == 41u &&
          before[1].message_id == 42u);
    CHECK(before[0].stream.state == TUI_STREAM_DONE &&
          before[1].stream.state == TUI_STREAM_ACTIVE);

    /* Admit a newer ID only by evicting the oldest settled message.
     * No retained allocation or rejected open can resurrect an old ID. */
    CHECK(tui_transcript_open(t, 43u, &evicted) == TUI_TRANSCRIPT_OK);
    CHECK(evicted == 41u && tui_transcript_count(t) == 2u);
    CHECK(tui_transcript_text(t, 41u, &bytes) == NULL);
    CHECK(send_packet(t, 41u, TUI_STREAM_BEGIN, 200u, 8u, NULL, 0u) ==
          TUI_TRANSCRIPT_STALE);
    CHECK(tui_transcript_open(t, 41u, &evicted) == TUI_TRANSCRIPT_STALE);
    CHECK(tui_transcript_get(t, 41u, &before[0]) == TUI_TRANSCRIPT_STALE);
    CHECK(tui_transcript_window(t, 0u, 2u, before, 2u, &count) ==
          TUI_TRANSCRIPT_OK);
    CHECK(before[0].message_id == 42u && before[1].message_id == 43u);

    CHECK(tui_transcript_open(t, 44u, &evicted) == TUI_TRANSCRIPT_FULL);
    CHECK(evicted == 41u);
    CHECK(send_packet(t, 42u, TUI_STREAM_CANCEL, 1u, 9u, NULL, 0u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(tui_transcript_open(t, 44u, &evicted) == TUI_TRANSCRIPT_OK);
    CHECK(evicted == 42u);
    CHECK(send_packet(t, 42u, TUI_STREAM_CHUNK, 1u, 10u, "late", 4u) ==
          TUI_TRANSCRIPT_STALE);
    CHECK(tui_transcript_window(t, 0u, 1u, before, 1u, &count) ==
          TUI_TRANSCRIPT_OK);
    CHECK(count == 1u && before[0].message_id == 43u);
    CHECK(tui_transcript_window(t, 100u, 5u, NULL, 0u, &count) ==
          TUI_TRANSCRIPT_OK && count == 0u);

    /* Route one valid generation, reject invalid control and preserve text. */
    CHECK(send_packet(t, 43u, TUI_STREAM_BEGIN, 2u, 11u, NULL, 0u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(send_packet(t, 43u, TUI_STREAM_CHUNK, 2u, 12u, "Z", 1u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(send_packet(t, 43u, TUI_STREAM_END, 2u, 13u, NULL, 0u) ==
          TUI_TRANSCRIPT_OK);
    CHECK(strcmp(tui_transcript_text(t, 43u, &bytes), "Z") == 0);
    CHECK(tui_transcript_open(t, 45u, &evicted) == TUI_TRANSCRIPT_OK);
    CHECK(evicted == 43u);
    CHECK(tui_transcript_destroy(t) == TUI_TRANSCRIPT_OK);
    return 0;
}

int main(void) {
    return test_bounded_registry_and_permanent_tombstones();
}
