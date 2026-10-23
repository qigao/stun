#include "tui_async.h"

#include <salts/thread.h>

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
                    __FILE__, __LINE__, #condition);                         \
            return 1;                                                        \
        }                                                                    \
    } while (0)

typedef struct producer_payload {
    uint32_t producer_id;
    uint32_t ordinal;
} producer_payload_t;

typedef struct producer_context {
    tui_async_queue_t *queue;
    uint32_t producer_id;
    uint32_t count;
    tui_async_status_t status;
    tui_async_status_t pop_status;
} producer_context_t;

enum {
    TEST_EVENT_TEXT = 1u,
    TEST_PRODUCERS = 4u,
    TEST_EVENTS_PER_PRODUCER = 32u,
    TEST_TOTAL_EVENTS = TEST_PRODUCERS * TEST_EVENTS_PER_PRODUCER
};

static void producer_entry(void *arg) {
    producer_context_t *context = (producer_context_t *)arg;
    tui_async_event_t event = {0};
    producer_payload_t payload = {0};

    context->pop_status = tui_async_queue_try_pop(
        context->queue, &event, &payload, sizeof(payload));
    if (context->pop_status != TUI_ASYNC_NOT_OWNER) {
        context->status = TUI_ASYNC_INVALID_ARGUMENT;
        return;
    }

    context->status = TUI_ASYNC_OK;
    for (uint32_t i = 0; i < context->count; ++i) {
        producer_payload_t message = {context->producer_id, i};
        uint64_t sequence = 0u;
        context->status = tui_async_queue_post(
            context->queue, TEST_EVENT_TEXT, 17u, &message,
            sizeof(message), &sequence);
        if (context->status != TUI_ASYNC_OK || sequence == 0u) {
            if (context->status == TUI_ASYNC_OK) {
                context->status = TUI_ASYNC_SEQUENCE_EXHAUSTED;
            }
            return;
        }
    }
}

static int test_fifo_capacity_and_close(void) {
    const tui_async_queue_config_t invalid_capacity = {0u, 8u};
    const tui_async_queue_config_t invalid_payload = {2u, 0u};
    const tui_async_queue_config_t config = {2u, 8u};
    tui_async_queue_t *queue;
    tui_async_event_t event = {99u, 99u, 99u, 99u};
    tui_async_queue_stats_t stats;
    char first[] = "abc";
    const char second[] = "xy";
    char output[8] = {0};
    uint64_t sequence = 777u;

    CHECK(tui_async_queue_create(&invalid_capacity) == NULL);
    CHECK(tui_async_queue_create(&invalid_payload) == NULL);
    queue = tui_async_queue_create(&config);
    CHECK(queue != NULL);
    CHECK(tui_async_queue_is_owner(queue));
    CHECK(tui_async_queue_post(queue, 0u, 1u, first, 3u, NULL) ==
          TUI_ASYNC_INVALID_ARGUMENT);
    CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 1u, NULL, 1u, NULL) ==
          TUI_ASYNC_INVALID_ARGUMENT);
    CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 1u, first, 9u, NULL) ==
          TUI_ASYNC_INVALID_ARGUMENT);

    CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 7u, first, 3u,
                               &sequence) == TUI_ASYNC_OK);
    CHECK(sequence == 1u);
    first[0] = 'z'; /* The accepted payload is owned by the queue. */
    CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 8u, second, 2u,
                               &sequence) == TUI_ASYNC_OK);
    CHECK(sequence == 2u);
    sequence = 777u;
    CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 9u, second, 2u,
                               &sequence) == TUI_ASYNC_FULL);
    CHECK(sequence == 777u);

    CHECK(tui_async_queue_try_pop(queue, &event, output, 2u) ==
          TUI_ASYNC_BUFFER_TOO_SMALL);
    CHECK(event.type == 99u && event.sequence == 99u);
    CHECK(tui_async_queue_get_stats(queue, &stats) == TUI_ASYNC_OK);
    CHECK(stats.queued == 2u && stats.high_water == 2u);
    CHECK(stats.accepted == 2u && stats.rejected_full == 1u);
    CHECK(!stats.closed);

    CHECK(tui_async_queue_close(queue) == TUI_ASYNC_OK);
    CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 10u, second, 2u,
                               NULL) == TUI_ASYNC_CLOSED);
    CHECK(tui_async_queue_destroy(queue) == TUI_ASYNC_BUSY);

    CHECK(tui_async_queue_try_pop(queue, &event, output, sizeof(output)) ==
          TUI_ASYNC_OK);
    CHECK(event.type == TEST_EVENT_TEXT && event.generation == 7u);
    CHECK(event.sequence == 1u && event.payload_size == 3u);
    CHECK(memcmp(output, "abc", 3u) == 0);
    CHECK(tui_async_queue_try_pop(queue, &event, output, sizeof(output)) ==
          TUI_ASYNC_OK);
    CHECK(event.generation == 8u && event.sequence == 2u);
    CHECK(event.payload_size == 2u && memcmp(output, "xy", 2u) == 0);
    CHECK(tui_async_queue_try_pop(queue, &event, output, sizeof(output)) ==
          TUI_ASYNC_EMPTY);

    CHECK(tui_async_queue_get_stats(queue, &stats) == TUI_ASYNC_OK);
    CHECK(stats.closed && stats.queued == 0u);
    CHECK(stats.accepted == 2u && stats.rejected_full == 1u);
    CHECK(stats.rejected_closed == 1u);
    CHECK(tui_async_queue_destroy(queue) == TUI_ASYNC_OK);
    return 0;
}

static int test_concurrent_producers_and_owner_affinity(void) {
    const tui_async_queue_config_t config = {
        TEST_TOTAL_EVENTS, sizeof(producer_payload_t)};
    tui_async_queue_t *queue = tui_async_queue_create(&config);
    cmeta_thread_t threads[TEST_PRODUCERS] = {0};
    producer_context_t contexts[TEST_PRODUCERS];
    bool seen[TEST_PRODUCERS][TEST_EVENTS_PER_PRODUCER] = {{false}};
    uint32_t last_ordinal[TEST_PRODUCERS] = {0};
    bool have_last[TEST_PRODUCERS] = {false};
    tui_async_event_t event;
    producer_payload_t payload;
    tui_async_queue_stats_t stats;

    CHECK(queue != NULL);
    for (uint32_t i = 0; i < TEST_PRODUCERS; ++i) {
        contexts[i].queue = queue;
        contexts[i].producer_id = i;
        contexts[i].count = TEST_EVENTS_PER_PRODUCER;
        contexts[i].status = TUI_ASYNC_INVALID_ARGUMENT;
        contexts[i].pop_status = TUI_ASYNC_OK;
        CHECK(cmeta_thread_create(&threads[i], producer_entry,
                                  &contexts[i]) == 0);
    }
    for (uint32_t i = 0; i < TEST_PRODUCERS; ++i) {
        CHECK(cmeta_thread_join(&threads[i]) == 0);
        CHECK(contexts[i].status == TUI_ASYNC_OK);
        CHECK(contexts[i].pop_status == TUI_ASYNC_NOT_OWNER);
    }

    for (uint64_t expected_sequence = 1u;
         expected_sequence <= TEST_TOTAL_EVENTS; ++expected_sequence) {
        memset(&payload, 0, sizeof(payload));
        CHECK(tui_async_queue_try_pop(queue, &event, &payload,
                                      sizeof(payload)) == TUI_ASYNC_OK);
        CHECK(event.sequence == expected_sequence);
        CHECK(event.type == TEST_EVENT_TEXT && event.generation == 17u);
        CHECK(event.payload_size == sizeof(payload));
        CHECK(payload.producer_id < TEST_PRODUCERS);
        CHECK(payload.ordinal < TEST_EVENTS_PER_PRODUCER);
        CHECK(!seen[payload.producer_id][payload.ordinal]);
        if (have_last[payload.producer_id]) {
            CHECK(payload.ordinal > last_ordinal[payload.producer_id]);
        }
        seen[payload.producer_id][payload.ordinal] = true;
        last_ordinal[payload.producer_id] = payload.ordinal;
        have_last[payload.producer_id] = true;
    }

    CHECK(tui_async_queue_try_pop(queue, &event, &payload, sizeof(payload)) ==
          TUI_ASYNC_EMPTY);
    CHECK(tui_async_queue_get_stats(queue, &stats) == TUI_ASYNC_OK);
    CHECK(stats.accepted == TEST_TOTAL_EVENTS);
    CHECK(stats.high_water == TEST_TOTAL_EVENTS && stats.queued == 0u);
    CHECK(stats.rejected_full == 0u && !stats.closed);

    CHECK(tui_async_queue_close(queue) == TUI_ASYNC_OK);
    CHECK(tui_async_queue_destroy(queue) == TUI_ASYNC_OK);
    return 0;
}

int main(void) {
    if (test_fifo_capacity_and_close() != 0) return 1;
    if (test_concurrent_producers_and_owner_affinity() != 0) return 1;
    puts("Tango async mailbox tests passed");
    return 0;
}
