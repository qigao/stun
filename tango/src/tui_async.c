/*
 * Tango bounded asynchronous UI mailbox.
 *
 * Queue state is protected by the canonical Salts::Platform mutex. Producer
 * threads only copy bounded event data; dequeue is restricted to the captured
 * UI-owner thread. No callback is invoked while the lock is held.
 */
#include "tui_async.h"
#include "tui_async_wait_internal.h"

#include <salts/thread.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#endif

typedef struct tui_async_slot {
    uint32_t type;
    uint64_t generation;
    uint64_t sequence;
    size_t payload_size;
} tui_async_slot_t;

struct tui_async_queue {
    cmeta_mutex_t mutex;
    const void *owner_thread;
    tui_async_slot_t *slots;
    unsigned char *payloads;
    size_t capacity;
    size_t max_payload_bytes;
    size_t head;
    size_t count;
    size_t high_water;
    uint64_t next_sequence;
    uint64_t accepted;
    uint64_t rejected_full;
    uint64_t rejected_closed;
    bool closed;
    bool wake_failed;
#ifdef _WIN32
    HANDLE wake_event;
#else
    int wake_read;
    int wake_write;
#endif
};

static void increment_saturated(uint64_t *value) {
    if (*value != UINT64_MAX) ++*value;
}

/* The notification is level-triggered by the protected queue predicate.
 * POSIX pipe bytes and the Windows manual-reset Event only represent the
 * empty->nonempty/close transition. They never own an application message. */
static void wake_locked(tui_async_queue_t *queue) {
#ifdef _WIN32
    if (!SetEvent(queue->wake_event)) queue->wake_failed = true;
#else
    const unsigned char byte = 1u;
    ssize_t n;
    do { n = write(queue->wake_write, &byte, 1u); }
    while (n < 0 && errno == EINTR);
    if (n != 1 && !(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)))
        queue->wake_failed = true;
#endif
}

#ifndef _WIN32
static bool set_fd_flags(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    const int descriptor_flags = fcntl(fd, F_GETFD, 0);
    return flags >= 0 && descriptor_flags >= 0 &&
           fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 &&
           fcntl(fd, F_SETFD, descriptor_flags | FD_CLOEXEC) == 0;
}
#endif

static bool queue_owner(const tui_async_queue_t *queue) {
    return queue != NULL &&
           queue->owner_thread == cmeta_thread_current_token();
}

tui_async_queue_t *tui_async_queue_create(
    const tui_async_queue_config_t *config) {
    tui_async_queue_t *queue;

    if (config == NULL || config->capacity == 0u ||
        config->max_payload_bytes == 0u ||
        config->capacity > SIZE_MAX / sizeof(tui_async_slot_t) ||
        config->capacity > SIZE_MAX / config->max_payload_bytes) {
        return NULL;
    }

    queue = (tui_async_queue_t *)calloc(1u, sizeof(*queue));
    if (queue == NULL) return NULL;

#ifndef _WIN32
    queue->wake_read = -1;
    queue->wake_write = -1;
#endif
    cmeta_mutex_init(&queue->mutex);
    if (queue->mutex == NULL) {
        free(queue);
        return NULL;
    }

    queue->slots = (tui_async_slot_t *)calloc(
        config->capacity, sizeof(*queue->slots));
    queue->payloads = (unsigned char *)malloc(
        config->capacity * config->max_payload_bytes);
    if (queue->slots == NULL || queue->payloads == NULL) {
        free(queue->payloads);
        free(queue->slots);
        cmeta_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }

#ifdef _WIN32
    queue->wake_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (queue->wake_event == NULL) {
        free(queue->payloads);
        free(queue->slots);
        cmeta_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
#else
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        free(queue->payloads);
        free(queue->slots);
        cmeta_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    queue->wake_read = pipe_fds[0];
    queue->wake_write = pipe_fds[1];
    if (queue->wake_read >= FD_SETSIZE ||
        !set_fd_flags(queue->wake_read) || !set_fd_flags(queue->wake_write)) {
        close(queue->wake_read);
        close(queue->wake_write);
        free(queue->payloads);
        free(queue->slots);
        cmeta_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
#endif
    queue->owner_thread = cmeta_thread_current_token();
    queue->capacity = config->capacity;
    queue->max_payload_bytes = config->max_payload_bytes;
    queue->next_sequence = 1u;
    return queue;
}

tui_async_status_t tui_async_queue_post(
    tui_async_queue_t *queue, uint32_t type, uint64_t generation,
    const void *payload, size_t payload_size, uint64_t *sequence_out) {
    tui_async_slot_t *slot;
    size_t tail;
    uint64_t sequence;

    if (queue == NULL || type == 0u ||
        (payload_size != 0u && payload == NULL) ||
        payload_size > queue->max_payload_bytes) {
        return TUI_ASYNC_INVALID_ARGUMENT;
    }

    cmeta_mutex_lock(&queue->mutex);
    if (queue->closed) {
        increment_saturated(&queue->rejected_closed);
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_CLOSED;
    }
    if (queue->count == queue->capacity) {
        increment_saturated(&queue->rejected_full);
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_FULL;
    }
    if (queue->next_sequence == UINT64_MAX) {
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_SEQUENCE_EXHAUSTED;
    }

    const bool empty_before = queue->count == 0u;
    tail = queue->count >= queue->capacity - queue->head
        ? queue->count - (queue->capacity - queue->head)
        : queue->head + queue->count;
    sequence = queue->next_sequence++;
    slot = &queue->slots[tail];
    slot->type = type;
    slot->generation = generation;
    slot->sequence = sequence;
    slot->payload_size = payload_size;
    if (payload_size != 0u) {
        memcpy(queue->payloads + tail * queue->max_payload_bytes,
               payload, payload_size);
    }

    ++queue->count;
    increment_saturated(&queue->accepted);
    if (queue->count > queue->high_water) queue->high_water = queue->count;
    if (empty_before) wake_locked(queue);
    cmeta_mutex_unlock(&queue->mutex);

    if (sequence_out != NULL) *sequence_out = sequence;
    return TUI_ASYNC_OK;
}

tui_async_status_t tui_async_queue_try_pop(
    tui_async_queue_t *queue, tui_async_event_t *event_out,
    void *payload_buffer, size_t payload_capacity) {
    tui_async_slot_t *slot;
    tui_async_event_t event;
    size_t index;

    if (queue == NULL || event_out == NULL) {
        return TUI_ASYNC_INVALID_ARGUMENT;
    }
    if (!queue_owner(queue)) return TUI_ASYNC_NOT_OWNER;

    cmeta_mutex_lock(&queue->mutex);
    if (queue->count == 0u) {
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_EMPTY;
    }

    index = queue->head;
    slot = &queue->slots[index];
    if (slot->payload_size != 0u &&
        (payload_buffer == NULL || payload_capacity < slot->payload_size)) {
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_BUFFER_TOO_SMALL;
    }

    event.type = slot->type;
    event.generation = slot->generation;
    event.sequence = slot->sequence;
    event.payload_size = slot->payload_size;
    if (slot->payload_size != 0u) {
        memcpy(payload_buffer,
               queue->payloads + index * queue->max_payload_bytes,
               slot->payload_size);
    }

    queue->head = queue->head + 1u == queue->capacity
        ? 0u : queue->head + 1u;
    --queue->count;
    cmeta_mutex_unlock(&queue->mutex);

    *event_out = event;
    return TUI_ASYNC_OK;
}

tui_async_status_t tui_async_queue_close(tui_async_queue_t *queue) {
    if (queue == NULL) return TUI_ASYNC_INVALID_ARGUMENT;
    cmeta_mutex_lock(&queue->mutex);
    if (!queue->closed) {
        queue->closed = true;
        wake_locked(queue);
    }
    cmeta_mutex_unlock(&queue->mutex);
    return TUI_ASYNC_OK;
}

tui_async_status_t tui_async_queue_get_stats(
    tui_async_queue_t *queue, tui_async_queue_stats_t *out) {
    tui_async_queue_stats_t stats;

    if (queue == NULL || out == NULL) return TUI_ASYNC_INVALID_ARGUMENT;

    cmeta_mutex_lock(&queue->mutex);
    stats.capacity = queue->capacity;
    stats.max_payload_bytes = queue->max_payload_bytes;
    stats.queued = queue->count;
    stats.high_water = queue->high_water;
    stats.accepted = queue->accepted;
    stats.rejected_full = queue->rejected_full;
    stats.rejected_closed = queue->rejected_closed;
    stats.closed = queue->closed;
    cmeta_mutex_unlock(&queue->mutex);

    *out = stats;
    return TUI_ASYNC_OK;
}

bool tui_async_queue_is_owner(const tui_async_queue_t *queue) {
    return queue_owner(queue);
}

tui_async_status_t tui_async_queue_destroy(tui_async_queue_t *queue) {
    if (queue == NULL) return TUI_ASYNC_INVALID_ARGUMENT;
    if (!queue_owner(queue)) return TUI_ASYNC_NOT_OWNER;

    cmeta_mutex_lock(&queue->mutex);
    if (!queue->closed || queue->count != 0u) {
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_BUSY;
    }
    cmeta_mutex_unlock(&queue->mutex);

    /* All API callers, especially producers, must be quiescent here. */
#ifdef _WIN32
    CloseHandle(queue->wake_event);
#else
    close(queue->wake_read);
    close(queue->wake_write);
#endif
    cmeta_mutex_destroy(&queue->mutex);
    free(queue->payloads);
    free(queue->slots);
    free(queue);
    return TUI_ASYNC_OK;
}

/* An actual kernel wait: input and the queue's wake endpoint are observed
 * together. A producer commits and signals under the same Platform mutex.
 * Rechecking the predicate before entering the wait prevents lost wake. */
tui_async_wait_result_t tui_async_wait_io(
    tui_async_queue_t *queue, intptr_t native_input, int timeout_ms) {
    if (!queue || timeout_ms < -1 || !queue_owner(queue))
        return TUI_ASYNC_WAIT_ERROR;

    cmeta_mutex_lock(&queue->mutex);
    if (queue->wake_failed) {
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_WAIT_ERROR;
    }
    if (queue->count != 0u || queue->closed) {
        cmeta_mutex_unlock(&queue->mutex);
        return TUI_ASYNC_WAIT_MESSAGE;
    }
    cmeta_mutex_unlock(&queue->mutex);

#ifdef _WIN32
    HANDLE handles[2];
    DWORD count = 1u;
    handles[0] = queue->wake_event;
    if (native_input != 0) handles[count++] = (HANDLE)native_input;
    const DWORD timeout = timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
    const DWORD result = WaitForMultipleObjects(count, handles, FALSE, timeout);
    if (result == WAIT_TIMEOUT) return TUI_ASYNC_WAIT_TIMEOUT;
    if (count == 2u && result == WAIT_OBJECT_0 + 1u)
        return TUI_ASYNC_WAIT_INPUT;
    if (result != WAIT_OBJECT_0) return TUI_ASYNC_WAIT_ERROR;

    cmeta_mutex_lock(&queue->mutex);
    const bool pending = queue->count != 0u || queue->closed;
    if (!pending && !ResetEvent(queue->wake_event)) queue->wake_failed = true;
    const bool failed = queue->wake_failed;
    cmeta_mutex_unlock(&queue->mutex);
    if (failed) return TUI_ASYNC_WAIT_ERROR;
    return pending ? TUI_ASYNC_WAIT_MESSAGE : TUI_ASYNC_WAIT_INTERRUPTED;
#else
    if (native_input >= FD_SETSIZE) return TUI_ASYNC_WAIT_ERROR;
    fd_set readers;
    FD_ZERO(&readers);
    FD_SET(queue->wake_read, &readers);
    int max_fd = queue->wake_read;
    if (native_input >= 0) {
        FD_SET((int)native_input, &readers);
        if ((int)native_input > max_fd) max_fd = (int)native_input;
    }
    struct timeval tv;
    struct timeval *timeout = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        timeout = &tv;
    }
    const int result = select(max_fd + 1, &readers, NULL, NULL, timeout);
    if (result == 0) return TUI_ASYNC_WAIT_TIMEOUT;
    if (result < 0)
        return errno == EINTR ? TUI_ASYNC_WAIT_INTERRUPTED
                              : TUI_ASYNC_WAIT_ERROR;
    if (native_input >= 0 && FD_ISSET((int)native_input, &readers))
        return TUI_ASYNC_WAIT_INPUT;

    cmeta_mutex_lock(&queue->mutex);
    unsigned char bytes[64];
    for (;;) {
        ssize_t n = read(queue->wake_read, bytes, sizeof(bytes));
        if (n > 0) continue;
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        queue->wake_failed = true;
        break;
    }
    const bool pending = queue->count != 0u || queue->closed;
    const bool failed = queue->wake_failed;
    cmeta_mutex_unlock(&queue->mutex);
    if (failed) return TUI_ASYNC_WAIT_ERROR;
    return pending ? TUI_ASYNC_WAIT_MESSAGE : TUI_ASYNC_WAIT_INTERRUPTED;
#endif
}
