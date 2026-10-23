/*
 * Tango bounded asynchronous UI mailbox.
 *
 * The mailbox is a transport primitive only: producers submit copied event
 * payloads, and only the thread that creates the mailbox may dequeue them.
 * It does not call application code, mutate UI state, render, or wake a
 * platform event loop. Wake integration belongs to the app/terminal layer.
 */
#ifndef TANGO_TUI_ASYNC_H
#define TANGO_TUI_ASYNC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tui_async_queue tui_async_queue_t;

typedef enum tui_async_status {
    TUI_ASYNC_OK = 0,
    TUI_ASYNC_INVALID_ARGUMENT,
    TUI_ASYNC_FULL,
    TUI_ASYNC_CLOSED,
    TUI_ASYNC_EMPTY,
    TUI_ASYNC_NOT_OWNER,
    TUI_ASYNC_BUFFER_TOO_SMALL,
    TUI_ASYNC_SEQUENCE_EXHAUSTED,
    TUI_ASYNC_BUSY
} tui_async_status_t;

typedef struct tui_async_queue_config {
    /* Both values must be nonzero. Capacity and payload storage are fixed. */
    size_t capacity;
    size_t max_payload_bytes;
} tui_async_queue_config_t;

typedef struct tui_async_event {
    uint32_t type;
    uint64_t generation;
    uint64_t sequence;
    size_t payload_size;
} tui_async_event_t;

typedef struct tui_async_queue_stats {
    size_t capacity;
    size_t max_payload_bytes;
    size_t queued;
    size_t high_water;
    uint64_t accepted;
    uint64_t rejected_full;
    uint64_t rejected_closed;
    bool closed;
} tui_async_queue_stats_t;

/*
 * Create on the intended UI-owner thread. That thread becomes the only thread
 * allowed to call try_pop() or destroy(). The queue allocates all slot/payload
 * storage up front and never grows it after creation.
 */
tui_async_queue_t *tui_async_queue_create(
    const tui_async_queue_config_t *config);

/*
 * Multi-producer, nonblocking admission. Payload bytes are copied before this
 * call returns; the producer retains ownership of its input memory. 'type'
 * must be nonzero. A NULL payload is valid only for a zero-byte payload,
 * although the configured maximum payload size itself must be nonzero.
 *
 * On success, optional sequence_out receives a monotonically increasing
 * sequence assigned in FIFO admission order. Output parameters are unchanged
 * on rejection. FULL/CLOSED are explicit; the queue never silently drops,
 * retries, or overwrites an accepted event.
 */
tui_async_status_t tui_async_queue_post(
    tui_async_queue_t *queue, uint32_t type, uint64_t generation,
    const void *payload, size_t payload_size, uint64_t *sequence_out);

/*
 * Nonblocking owner-thread dequeue. event_out is unchanged unless OK is
 * returned. If payload storage is too small, BUFFER_TOO_SMALL is returned and
 * the event remains queued for a later retry with a larger buffer.
 */
tui_async_status_t tui_async_queue_try_pop(
    tui_async_queue_t *queue, tui_async_event_t *event_out,
    void *payload_buffer, size_t payload_capacity);

/* Close admission; already accepted events remain available to the owner. */
tui_async_status_t tui_async_queue_close(tui_async_queue_t *queue);

/* Thread-safe observational snapshot. out is unchanged on failure. */
tui_async_status_t tui_async_queue_get_stats(
    tui_async_queue_t *queue, tui_async_queue_stats_t *out);

/* True only on the thread that created the queue. */
bool tui_async_queue_is_owner(const tui_async_queue_t *queue);

/*
 * Destroy on the owner thread after close, after draining all accepted events,
 * and only after every producer/API call has quiesced. Concurrent use during
 * destroy is forbidden: callers must stop and join producers first. BUSY
 * leaves the queue intact and may be retried.
 */
tui_async_status_t tui_async_queue_destroy(tui_async_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* TANGO_TUI_ASYNC_H */
