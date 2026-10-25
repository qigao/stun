/* Bounded UI-owner transcript registry, backed by canonical tui_stream state.
 *
 * The UI owns each message ID and its projection; this C11 registry owns only
 * bounded stream buffers and admission state. It never sees FlexUI Elements,
 * parses Markdown, schedules tasks, or creates another mailbox.
 *
 * Within one session, message IDs must be strictly increasing at explicit
 * admission. The high-water ID permanently rejects evicted or old IDs without
 * an unbounded tombstone set. A new session uses a new registry/ID namespace.
 */
#ifndef TANGO_TUI_TRANSCRIPT_H
#define TANGO_TUI_TRANSCRIPT_H

#include "tui_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tui_transcript tui_transcript_t;

typedef enum tui_transcript_status {
    TUI_TRANSCRIPT_OK = 0,
    TUI_TRANSCRIPT_INVALID,
    TUI_TRANSCRIPT_NOT_OWNER,
    TUI_TRANSCRIPT_FULL,
    TUI_TRANSCRIPT_STALE,
    TUI_TRANSCRIPT_UNKNOWN,
    TUI_TRANSCRIPT_STREAM_REJECTED
} tui_transcript_status_t;

typedef struct tui_transcript_message {
    uint64_t message_id;
    tui_stream_snapshot_t stream;
} tui_transcript_message_t;

/* Zero and impossible capacities fail. Message slot storage is fixed up
 * front; each explicitly opened message allocates one bounded tui_stream.
 * All functions, including read-only enumeration, are UI-owner-only.
 */
tui_transcript_t *tui_transcript_create(size_t max_messages,
                                        size_t text_capacity,
                                        size_t max_chunk_bytes);

/* Explicit admission of a message_id > every ID previously admitted in this
 * session. When full, evicts the oldest DONE/CANCELLED entry; never evicts an
 * ACTIVE/IDLE message. The caller owns retiring its UI projection on a
 * successful eviction and must do so before reusing its UI identity.
 * evicted_id receives 0 when no eviction occurred. On failure neither the
 * registry nor evicted_id changes.
 */
tui_transcript_status_t tui_transcript_open(tui_transcript_t *transcript,
                                           uint64_t message_id,
                                           uint64_t *evicted_id);

/* Routes an already-copied, ordered Tango event by its packet message ID.
 * No implicit message creation. IDs at/below the admitted high-water mark
 * which no longer exist are STALE (permanent tombstones).
 * Future, unadmitted IDs are UNKNOWN. Underlying stream rejection is explicit
 * via STREAM_REJECTED and optional stream_reason (unchanged otherwise).
 */
tui_transcript_status_t tui_transcript_apply(
    tui_transcript_t *transcript, const tui_async_event_t *event,
    const void *packet, tui_stream_status_t *stream_reason);

/* Oldest first, clipped to [first, first+limit). A zero-length query may
 * use a NULL output. A too-small output buffer is rejected atomically.
 * All returned snapshots are values, not dangling pointers.
 */
tui_transcript_status_t tui_transcript_window(
    const tui_transcript_t *transcript, size_t first, size_t limit,
    tui_transcript_message_t *items, size_t item_capacity,
    size_t *written);

/* Get exactly one existing message or a borrowed text slice.
 * Text pointers are valid only until a successful apply/eviction/destroy and
 * never cross threads. Missing IDs do not expose old freed message contents.
 */
tui_transcript_status_t tui_transcript_get(
    const tui_transcript_t *transcript, uint64_t message_id,
    tui_transcript_message_t *out);
const char *tui_transcript_text(
    const tui_transcript_t *transcript, uint64_t message_id, size_t *bytes);

size_t tui_transcript_count(const tui_transcript_t *transcript);
tui_transcript_status_t tui_transcript_destroy(tui_transcript_t *transcript);

#ifdef __cplusplus
}
#endif
#endif /* TANGO_TUI_TRANSCRIPT_H */
