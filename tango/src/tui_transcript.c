#include "tui_transcript.h"

#include <salts/thread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct transcript_slot {
    uint64_t message_id;
    tui_stream_t *stream;
} transcript_slot_t;

struct tui_transcript {
    const void *owner_thread;
    transcript_slot_t *slots;
    size_t count;
    size_t capacity;
    size_t text_capacity;
    size_t max_chunk_bytes;
    uint64_t highest_admitted_id;
};

static bool transcript_owner(const tui_transcript_t *t) {
    return t && t->owner_thread == cmeta_thread_current_token();
}

static size_t transcript_find(const tui_transcript_t *t, uint64_t id) {
    for (size_t i = 0u; i < t->count; ++i) {
        if (t->slots[i].message_id == id) return i;
    }
    return t->count;
}

tui_transcript_t *tui_transcript_create(size_t max_messages,
                                        size_t text_capacity,
                                        size_t max_chunk_bytes) {
    if (!max_messages || max_messages > SIZE_MAX / sizeof(transcript_slot_t) ||
        !text_capacity || !max_chunk_bytes ||
        max_chunk_bytes > SIZE_MAX - 4u ||
        text_capacity > SIZE_MAX - 4u - max_chunk_bytes)
        return NULL;
    tui_transcript_t *t = (tui_transcript_t *)calloc(1u, sizeof(*t));
    if (!t) return NULL;
    t->slots = (transcript_slot_t *)calloc(max_messages, sizeof(*t->slots));
    if (!t->slots) {
        free(t);
        return NULL;
    }
    t->owner_thread = cmeta_thread_current_token();
    t->capacity = max_messages;
    t->text_capacity = text_capacity;
    t->max_chunk_bytes = max_chunk_bytes;
    return t;
}

tui_transcript_status_t tui_transcript_open(tui_transcript_t *t,
                                           uint64_t id,
                                           uint64_t *evicted_id) {
    if (!t || !id) return TUI_TRANSCRIPT_INVALID;
    if (!transcript_owner(t)) return TUI_TRANSCRIPT_NOT_OWNER;
    if (id <= t->highest_admitted_id) return TUI_TRANSCRIPT_STALE;

    size_t remove_index = t->count;
    if (t->count == t->capacity) {
        for (size_t i = 0u; i < t->count; ++i) {
            tui_stream_snapshot_t snapshot;
            if (tui_stream_get_snapshot(t->slots[i].stream, &snapshot) !=
                TUI_STREAM_OK) return TUI_TRANSCRIPT_STREAM_REJECTED;
            if (snapshot.state == TUI_STREAM_DONE ||
                snapshot.state == TUI_STREAM_CANCELLED) {
                remove_index = i;
                break;
            }
        }
        if (remove_index == t->count) return TUI_TRANSCRIPT_FULL;
    }

    /* Allocate before invalidating the old slot: allocation failure has no
     * effect on admitted message identities, content or eviction watermark.
     */
    tui_stream_t *new_stream = tui_stream_create(
        id, t->text_capacity, t->max_chunk_bytes);
    if (!new_stream) return TUI_TRANSCRIPT_FULL;

    uint64_t removed_id = 0u;
    if (remove_index != t->count) {
        removed_id = t->slots[remove_index].message_id;
        /* destroy is infallible for a stream owned by the same UI thread. */
        if (tui_stream_destroy(t->slots[remove_index].stream) != TUI_STREAM_OK) {
            (void)tui_stream_destroy(new_stream);
            return TUI_TRANSCRIPT_STREAM_REJECTED;
        }
        for (size_t i = remove_index + 1u; i < t->count; ++i)
            t->slots[i - 1u] = t->slots[i];
        --t->count;
    }

    t->slots[t->count].message_id = id;
    t->slots[t->count].stream = new_stream;
    ++t->count;
    t->highest_admitted_id = id;
    if (evicted_id) *evicted_id = removed_id;
    return TUI_TRANSCRIPT_OK;
}

tui_transcript_status_t tui_transcript_apply(
    tui_transcript_t *t, const tui_async_event_t *event,
    const void *packet, tui_stream_status_t *stream_reason) {
    if (!t || !event || !packet ||
        event->payload_size < sizeof(tui_stream_packet_header_t))
        return TUI_TRANSCRIPT_INVALID;
    if (!transcript_owner(t)) return TUI_TRANSCRIPT_NOT_OWNER;

    tui_stream_packet_header_t header;
    memcpy(&header, packet, sizeof(header));
    if (!header.message_id) return TUI_TRANSCRIPT_INVALID;
    const size_t index = transcript_find(t, header.message_id);
    if (index == t->count)
        return header.message_id <= t->highest_admitted_id
            ? TUI_TRANSCRIPT_STALE : TUI_TRANSCRIPT_UNKNOWN;

    const tui_stream_status_t status =
        tui_stream_apply(t->slots[index].stream, event, packet);
    if (status == TUI_STREAM_OK) return TUI_TRANSCRIPT_OK;
    if (status == TUI_STREAM_STALE) return TUI_TRANSCRIPT_STALE;
    if (stream_reason) *stream_reason = status;
    return TUI_TRANSCRIPT_STREAM_REJECTED;
}

tui_transcript_status_t tui_transcript_window(
    const tui_transcript_t *t, size_t first, size_t limit,
    tui_transcript_message_t *items, size_t item_capacity,
    size_t *written) {
    if (!t || !written) return TUI_TRANSCRIPT_INVALID;
    if (!transcript_owner(t)) return TUI_TRANSCRIPT_NOT_OWNER;
    const size_t remaining = first >= t->count ? 0u : t->count - first;
    const size_t count = limit < remaining ? limit : remaining;
    if (count > item_capacity || (count && !items))
        return TUI_TRANSCRIPT_FULL;
    for (size_t i = 0u; i < count; ++i) {
        const transcript_slot_t *slot = &t->slots[first + i];
        tui_stream_snapshot_t snapshot;
        if (tui_stream_get_snapshot(slot->stream, &snapshot) != TUI_STREAM_OK)
            return TUI_TRANSCRIPT_STREAM_REJECTED;
        items[i].message_id = slot->message_id;
        items[i].stream = snapshot;
    }
    *written = count;
    return TUI_TRANSCRIPT_OK;
}

tui_transcript_status_t tui_transcript_get(
    const tui_transcript_t *t, uint64_t id, tui_transcript_message_t *out) {
    if (!t || !id || !out) return TUI_TRANSCRIPT_INVALID;
    if (!transcript_owner(t)) return TUI_TRANSCRIPT_NOT_OWNER;
    const size_t index = transcript_find(t, id);
    if (index == t->count)
        return id <= t->highest_admitted_id
            ? TUI_TRANSCRIPT_STALE : TUI_TRANSCRIPT_UNKNOWN;
    tui_stream_snapshot_t snapshot;
    if (tui_stream_get_snapshot(t->slots[index].stream, &snapshot) !=
        TUI_STREAM_OK) return TUI_TRANSCRIPT_STREAM_REJECTED;
    const tui_transcript_message_t value = {id, snapshot};
    *out = value;
    return TUI_TRANSCRIPT_OK;
}

const char *tui_transcript_text(
    const tui_transcript_t *t, uint64_t id, size_t *bytes) {
    if (!t || !id || !bytes || !transcript_owner(t)) return NULL;
    const size_t index = transcript_find(t, id);
    return index == t->count ? NULL :
           tui_stream_text(t->slots[index].stream, bytes);
}

size_t tui_transcript_count(const tui_transcript_t *t) {
    return transcript_owner(t) ? t->count : 0u;
}

tui_transcript_status_t tui_transcript_destroy(tui_transcript_t *t) {
    if (!t) return TUI_TRANSCRIPT_INVALID;
    if (!transcript_owner(t)) return TUI_TRANSCRIPT_NOT_OWNER;
    for (size_t i = 0u; i < t->count; ++i)
        (void)tui_stream_destroy(t->slots[i].stream);
    free(t->slots);
    free(t);
    return TUI_TRANSCRIPT_OK;
}
