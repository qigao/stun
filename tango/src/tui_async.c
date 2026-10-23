/*
 * Tango bounded asynchronous UI mailbox.
 *
 * Queue state is protected by the canonical Salts::Platform mutex. Producer
 * threads only copy bounded event data; dequeue is restricted to the captured
 * UI-owner thread. No callback is invoked while the lock is held.
 */
#include "tui_async.h"

#include <salts/thread.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

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
};

static void increment_saturated(uint64_t *value) {
    if (*value != UINT64_MAX) ++*value;
}

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

    tail = (queue->head + queue->count) % queue->capacity;
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
    ++queue->accepted;
    if (queue->count > queue->high_water) queue->high_water = queue->count;
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

    queue->head = (queue->head + 1u) % queue->capacity;
    --queue->count;
    cmeta_mutex_unlock(&queue->mutex);

    *event_out = event;
    return TUI_ASYNC_OK;
}

tui_async_status_t tui_async_queue_close(tui_async_queue_t *queue) {
    if (queue == NULL) return TUI_ASYNC_INVALID_ARGUMENT;
    cmeta_mutex_lock(&queue->mutex);
    queue->closed = true;
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
    cmeta_mutex_destroy(&queue->mutex);
    free(queue->payloads);
    free(queue->slots);
    free(queue);
    return TUI_ASYNC_OK;
}
