#include "tui_stream.h"

#include <salts/thread.h>
#include <salts_unicode.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct tui_stream {
    const void *owner_thread;
    uint64_t message_id;
    uint64_t generation;
    uint64_t last_sequence;
    char *text;
    char *scratch;
    size_t capacity;
    size_t max_chunk;
    size_t length;
    size_t discarded;
    unsigned char partial[3];
    size_t partial_size;
    tui_stream_state_t state;
};

static bool stream_owner(const tui_stream_t *stream) {
    return stream && stream->owner_thread == cmeta_thread_current_token();
}

tui_stream_t *tui_stream_create(uint64_t message_id, size_t text_capacity,
                                size_t max_chunk_bytes) {
    if (!message_id || !text_capacity || !max_chunk_bytes ||
        text_capacity > SIZE_MAX - max_chunk_bytes - 4u)
        return NULL;
    tui_stream_t *stream = (tui_stream_t *)calloc(1u, sizeof(*stream));
    if (!stream) return NULL;
    stream->text = (char *)malloc(text_capacity + 1u);
    stream->scratch = (char *)malloc(text_capacity + max_chunk_bytes + 4u);
    if (!stream->text || !stream->scratch) {
        free(stream->text);
        free(stream->scratch);
        free(stream);
        return NULL;
    }
    stream->text[0] = '\0';
    stream->owner_thread = cmeta_thread_current_token();
    stream->message_id = message_id;
    stream->capacity = text_capacity;
    stream->max_chunk = max_chunk_bytes;
    return stream;
}

static tui_stream_status_t stream_append(tui_stream_t *stream,
                                         const unsigned char *bytes,
                                         size_t byte_count) {
    if (byte_count > stream->max_chunk)
        return TUI_STREAM_CAPACITY;
    /* Work in the preallocated staging arena. Published text and any
     * incomplete UTF-8 bytes remain untouched until validation finishes. */
    const size_t combined = stream->partial_size + byte_count;
    if (stream->length)
        memcpy(stream->scratch, stream->text, stream->length);
    char *fresh = stream->scratch + stream->length;
    if (stream->partial_size)
        memcpy(fresh, stream->partial, stream->partial_size);
    if (byte_count)
        memcpy(fresh + stream->partial_size, bytes, byte_count);

    const vstr input = vstr_from_buf(fresh, combined);
    size_t cursor = 0u;
    while (cursor < combined) {
        salts_unicode_scalar scalar;
        const size_t before = cursor;
        if (salts_unicode_utf8_next(input, &cursor, &scalar) != SALTS_UNICODE_OK) {
            /* Only up to three terminal bytes can remain uncommitted.
             * Canonical Salts decides validity; no second UTF-8 decoder. */
            if (combined - before > 3u) return TUI_STREAM_INVALID;
            cursor = before;
            break;
        }
        if ((scalar.value < 0x20u && scalar.value != '\n') ||
            (scalar.value >= 0x7fu && scalar.value <= 0x9fu))
            return TUI_STREAM_INVALID;
    }

    const size_t complete = cursor;
    const size_t total = stream->length + complete;
    /* The newly formed last grapheme may span old/new CHUNK boundaries.
     * Recompute retention on the entire candidate text so eviction never
     * leaves a detached combining mark or half of a ZWJ emoji. */
    const vstr candidate = vstr_from_buf(stream->scratch, total);
    size_t remove = 0u;
    while (total - remove > stream->capacity) {
        const size_t previous = remove;
        vstr grapheme;
        if (salts_unicode_grapheme_next(candidate, &remove, &grapheme) !=
                SALTS_UNICODE_OK || remove <= previous)
            return TUI_STREAM_INVALID;
    }
    if (total > 0u && remove == total) return TUI_STREAM_CAPACITY;

    /* A successful append always publishes valid, complete UTF-8 only. */
    const size_t retained = total - remove;
    if (retained)
        memmove(stream->text, stream->scratch + remove, retained);
    stream->text[retained] = '\0';
    stream->length = retained;
    stream->discarded = remove > SIZE_MAX - stream->discarded
        ? SIZE_MAX : stream->discarded + remove;
    stream->partial_size = combined - complete;
    if (stream->partial_size)
        memcpy(stream->partial, fresh + complete, stream->partial_size);
    return TUI_STREAM_OK;
}

tui_stream_status_t tui_stream_apply(tui_stream_t *stream,
                                     const tui_async_event_t *event,
                                     const void *packet) {
    if (!stream || !event || !packet ||
        event->payload_size < sizeof(tui_stream_packet_header_t))
        return TUI_STREAM_INVALID;
    if (!stream_owner(stream)) return TUI_STREAM_NOT_OWNER;
    tui_stream_packet_header_t header;
    memcpy(&header, packet, sizeof(header));
    if (header.message_id != stream->message_id)
        return TUI_STREAM_WRONG_MESSAGE;
    if (!event->sequence || event->sequence <= stream->last_sequence)
        return TUI_STREAM_STALE;
    if (event->type != TUI_STREAM_CHUNK &&
        event->payload_size != sizeof(header))
        return TUI_STREAM_INVALID;

    if (event->type == TUI_STREAM_BEGIN) {
        if (!event->generation || event->generation <= stream->generation)
            return TUI_STREAM_STALE;
        stream->generation = event->generation;
        stream->last_sequence = event->sequence;
        stream->state = TUI_STREAM_ACTIVE;
        stream->length = 0u;
        stream->partial_size = 0u;
        stream->discarded = 0u;
        stream->text[0] = '\0';
        return TUI_STREAM_OK;
    }
    if (event->generation != stream->generation ||
        stream->state != TUI_STREAM_ACTIVE)
        return TUI_STREAM_STALE;
    if (event->type == TUI_STREAM_CANCEL) {
        stream->partial_size = 0u;
        stream->state = TUI_STREAM_CANCELLED;
    } else if (event->type == TUI_STREAM_END) {
        if (stream->partial_size) return TUI_STREAM_INVALID;
        stream->state = TUI_STREAM_DONE;
    } else if (event->type == TUI_STREAM_CHUNK) {
        const unsigned char *bytes =
            (const unsigned char *)packet + sizeof(header);
        const size_t size = event->payload_size - sizeof(header);
        tui_stream_status_t status = stream_append(stream, bytes, size);
        if (status != TUI_STREAM_OK) return status;
    } else {
        return TUI_STREAM_INVALID;
    }
    stream->last_sequence = event->sequence;
    return TUI_STREAM_OK;
}

const char *tui_stream_text(const tui_stream_t *stream, size_t *bytes) {
    if (!stream || !bytes || !stream_owner(stream)) return NULL;
    *bytes = stream->length;
    return stream->text;
}

const char *tui_stream_tail(const tui_stream_t *stream, size_t max_lines,
                            size_t *bytes) {
    if (!stream || !bytes || !max_lines || !stream_owner(stream)) return NULL;
    size_t start = 0u, lines = 0u;
    for (size_t i = stream->length; i > 0u; --i) {
        if (stream->text[i - 1u] == '\n' && ++lines == max_lines) {
            start = i;
            break;
        }
    }
    *bytes = stream->length - start;
    return stream->text + start;
}

tui_stream_status_t tui_stream_get_snapshot(const tui_stream_t *stream,
                                            tui_stream_snapshot_t *out) {
    if (!stream || !out) return TUI_STREAM_INVALID;
    if (!stream_owner(stream)) return TUI_STREAM_NOT_OWNER;
    *out = (tui_stream_snapshot_t){
        stream->message_id, stream->generation, stream->last_sequence,
        stream->length, stream->discarded, stream->partial_size, stream->state
    };
    return TUI_STREAM_OK;
}

tui_stream_status_t tui_stream_destroy(tui_stream_t *stream) {
    if (!stream) return TUI_STREAM_INVALID;
    if (!stream_owner(stream)) return TUI_STREAM_NOT_OWNER;
    free(stream->text);
    free(stream->scratch);
    free(stream);
    return TUI_STREAM_OK;
}
