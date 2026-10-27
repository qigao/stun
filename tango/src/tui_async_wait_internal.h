/* Tango-private wake/io multiplexing. The queue remains the source of truth. */
#ifndef TANGO_TUI_ASYNC_WAIT_INTERNAL_H
#define TANGO_TUI_ASYNC_WAIT_INTERNAL_H
#include "tui_async.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum tui_async_wait_result {
    TUI_ASYNC_WAIT_MESSAGE = 1,
    TUI_ASYNC_WAIT_INPUT,
    TUI_ASYNC_WAIT_TIMEOUT,
    TUI_ASYNC_WAIT_INTERRUPTED,
    TUI_ASYNC_WAIT_ERROR
} tui_async_wait_result_t;
/* native_input: POSIX fd (-1 disables input), Windows HANDLE (0 disables).
 * timeout_ms: -1 = indefinite; zero = nonblocking.
 * This wait never invokes a user callback or consumes an application message.
 */
tui_async_wait_result_t tui_async_wait_io(
    tui_async_queue_t *queue, intptr_t native_input, int timeout_ms);
#ifndef _WIN32
/* POSIX only: borrowed nonblocking wake pipe writer for SIGWINCH.
 * Resolve on the queue owner. The terminal handler may only write one
 * byte (async-signal-safe); it must never lock, post, or access queue state.
 * Disarm before queue destruction. No public Tango API is introduced. */
int tui_async_queue_signal_wake_fd(tui_async_queue_t *queue);
#endif
#ifdef __cplusplus
}
#endif
#endif
