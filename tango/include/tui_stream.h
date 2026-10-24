/* Tango UI-owner streaming text state.
 * Salts::Unicode is the sole UTF-8/grapheme authority; this module only
 * decides capacity, application generations, and retained view ownership. */
#ifndef TANGO_TUI_STREAM_H
#define TANGO_TUI_STREAM_H
#include "tui_async.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct tui_stream tui_stream_t;

enum {
    TUI_STREAM_BEGIN = 0x1001,
    TUI_STREAM_CHUNK = 0x1002,
    TUI_STREAM_END = 0x1003,
    TUI_STREAM_CANCEL = 0x1004
};

typedef enum tui_stream_state {
    TUI_STREAM_IDLE = 0,
    TUI_STREAM_ACTIVE,
    TUI_STREAM_DONE,
    TUI_STREAM_CANCELLED
} tui_stream_state_t;

typedef enum tui_stream_status {
    TUI_STREAM_OK = 0,
    TUI_STREAM_STALE,
    TUI_STREAM_WRONG_MESSAGE,
    TUI_STREAM_INVALID,
    TUI_STREAM_CAPACITY,
    TUI_STREAM_NOT_OWNER
} tui_stream_status_t;

/* The mailbox payload starts with the native in-process, nonpersistent ID.
 * CHUNK appends exactly payload_size-sizeof(message_id) data bytes.
 * Control events must have exactly sizeof(message_id) bytes. */
typedef struct tui_stream_packet_header {
    uint64_t message_id;
} tui_stream_packet_header_t;

typedef struct tui_stream_snapshot {
    uint64_t message_id;
    uint64_t generation;
    uint64_t last_sequence;
    size_t retained_bytes;
    size_t discarded_bytes;
    size_t pending_utf8_bytes;
    tui_stream_state_t state;
} tui_stream_snapshot_t;

/* Create on UI owner. Byte capacities must be nonzero and do not grow. */
tui_stream_t *tui_stream_create(uint64_t message_id, size_t text_capacity,
                                size_t max_chunk_bytes);
/* Owner-only; copied async payload is borrowed just for this call.
 * INVALID/CAPACITY/STALE never modify the retained visible text. */
tui_stream_status_t tui_stream_apply(tui_stream_t *stream,
                                     const tui_async_event_t *event,
                                     const void *packet);
/* Borrowed, NUL-terminated canonical UTF-8, valid until next apply/destroy.
 * Text capacity eviction always happens at Unicode grapheme boundaries. */
const char *tui_stream_text(const tui_stream_t *stream, size_t *bytes);
/* Borrowed suffix beginning at a newline boundary (or oldest retained byte),
 * at most max_lines newline-delimited lines, no terminal output. */
const char *tui_stream_tail(const tui_stream_t *stream, size_t max_lines,
                            size_t *bytes);
tui_stream_status_t tui_stream_get_snapshot(const tui_stream_t *stream,
                                            tui_stream_snapshot_t *out);
void tui_stream_destroy(tui_stream_t *stream);
#ifdef __cplusplus
}
#endif
#endif
