#include "tui_async.h"
#include "tui.h"
#include "tui_async_wait_internal.h"

#ifdef _WIN32
#include <windows.h>
#define NO_INPUT_HANDLE ((intptr_t)0)
#else
#include <unistd.h>
#define NO_INPUT_HANDLE ((intptr_t)-1)
#endif


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

/* A wake is a kernel notification, not an additional application queue. */
typedef struct delayed_post {
    tui_async_queue_t *queue;
    uint32_t delay_ms;
    tui_async_status_t result;
} delayed_post_t;

static void delayed_post_worker(void *arg) {
    delayed_post_t *ctx = (delayed_post_t *)arg;
    cmeta_sleep_ms(ctx->delay_ms);
    const uint32_t payload = 0x12345678u;
    ctx->result = tui_async_queue_post(ctx->queue, TEST_EVENT_TEXT, 4u,
                                       &payload, sizeof(payload), NULL);
}

static void delayed_close_worker(void *arg) {
    delayed_post_t *ctx = (delayed_post_t *)arg;
    cmeta_sleep_ms(ctx->delay_ms);
    ctx->result = tui_async_queue_close(ctx->queue);
}

static int test_real_kernel_wake_and_close(void) {
    const tui_async_queue_config_t config = {8u, 16u};
    tui_async_queue_t *queue = tui_async_queue_create(&config);
    tui_async_event_t event = {0};
    uint32_t payload = 0u;
    cmeta_thread_t thread = NULL;
    delayed_post_t producer = {queue, 5u, TUI_ASYNC_INVALID_ARGUMENT};
    CHECK(queue != NULL);
    CHECK(tui_async_wait_io(queue, NO_INPUT_HANDLE, 0) ==
          TUI_ASYNC_WAIT_TIMEOUT);

    CHECK(cmeta_thread_create(&thread, delayed_post_worker, &producer) == 0);
    CHECK(tui_async_wait_io(queue, NO_INPUT_HANDLE, 1000) ==
          TUI_ASYNC_WAIT_MESSAGE);
    CHECK(cmeta_thread_join(&thread) == 0);
    CHECK(producer.result == TUI_ASYNC_OK);
    CHECK(tui_async_queue_try_pop(queue, &event, &payload, sizeof(payload)) ==
          TUI_ASYNC_OK);
    CHECK(event.sequence == 1u && payload == 0x12345678u);

    /* Producer commits while the owner has not yet entered kernel wait. */
    producer.delay_ms = 0u;
    CHECK(cmeta_thread_create(&thread, delayed_post_worker, &producer) == 0);
    CHECK(cmeta_thread_join(&thread) == 0);
    CHECK(tui_async_wait_io(queue, NO_INPUT_HANDLE, 0) ==
          TUI_ASYNC_WAIT_MESSAGE);
    CHECK(tui_async_queue_try_pop(queue, &event, &payload, sizeof(payload)) ==
          TUI_ASYNC_OK);

    /* A closing producer must also wake a completely idle owner. */
    delayed_post_t closer = {queue, 5u, TUI_ASYNC_INVALID_ARGUMENT};
    CHECK(cmeta_thread_create(&thread, delayed_close_worker, &closer) == 0);
    CHECK(tui_async_wait_io(queue, NO_INPUT_HANDLE, 1000) ==
          TUI_ASYNC_WAIT_MESSAGE);
    CHECK(cmeta_thread_join(&thread) == 0);
    CHECK(closer.result == TUI_ASYNC_OK);
    CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 4u,
                               &payload, sizeof(payload), NULL) ==
          TUI_ASYNC_CLOSED);
    CHECK(tui_async_queue_destroy(queue) == TUI_ASYNC_OK);
    return 0;
}

static int test_native_input_and_wake_multiplex(void) {
    const tui_async_queue_config_t config = {2u, 8u};
    tui_async_queue_t *queue = tui_async_queue_create(&config);
    CHECK(queue != NULL);
#ifdef _WIN32
    HANDLE input = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(input != NULL);
    CHECK(SetEvent(input));
    CHECK(tui_async_wait_io(queue, (intptr_t)input, 1000) ==
          TUI_ASYNC_WAIT_INPUT);
    CHECK(ResetEvent(input));
    CHECK(CloseHandle(input));
#else
    int input[2];
    CHECK(pipe(input) == 0);
    const unsigned char byte = 'x';
    CHECK(write(input[1], &byte, 1u) == 1);
    CHECK(tui_async_wait_io(queue, (intptr_t)input[0], 1000) ==
          TUI_ASYNC_WAIT_INPUT);
    unsigned char read_byte = 0u;
    CHECK(read(input[0], &read_byte, 1u) == 1 && read_byte == byte);
    CHECK(close(input[0]) == 0);
    CHECK(close(input[1]) == 0);
#endif
    CHECK(tui_async_queue_close(queue) == TUI_ASYNC_OK);
    CHECK(tui_async_queue_destroy(queue) == TUI_ASYNC_OK);
    return 0;
}

typedef struct app_model {
    size_t updates;
    uint32_t last;
} app_model_t;

static bool app_keyboard(int msg, void *model) {
    (void)msg;
    (void)model;
    return true;
}
static void app_view(const void *model, tui_buffer_t *buffer) {
    (void)model;
    (void)buffer;
}
static int app_map(const tui_event_t *event) {
    (void)event;
    return -1;
}
static bool app_async_update(const tui_async_event_t *event,
                             const void *payload, void *model) {
    app_model_t *app = (app_model_t *)model;
    uint32_t value = 0u;
    if (event->payload_size != sizeof(value)) return false;
    memcpy(&value, payload, sizeof(value));
    app->updates++;
    app->last = value;
    return true;
}

static int test_owner_message_dispatch_batches(void) {
    app_model_t model = {0};
    tui_app_t *app = tui_app_create(&model, app_keyboard, app_view, app_map);
    CHECK(app != NULL);
    CHECK(tui_app_set_async_update(app, app_async_update));
    tui_async_queue_t *queue = tui_app_async_queue(app);
    CHECK(queue != NULL);
    app->needs_redraw = false;
    for (uint32_t i = 1u; i <= 5u; ++i) {
        CHECK(tui_async_queue_post(queue, TEST_EVENT_TEXT, 2u,
                    &i, sizeof(i), NULL) == TUI_ASYNC_OK);
    }
    CHECK(tui_app_pump_messages(app, 3u) == 3u);
    CHECK(model.updates == 3u && model.last == 3u);
    CHECK(app->needs_redraw);
    CHECK(tui_app_pump_messages(app, 32u) == 2u);
    CHECK(model.updates == 5u && model.last == 5u);
    CHECK(tui_app_pump_messages(app, 32u) == 0u);
    tui_app_destroy(app);
    return 0;
}

int main(void) {
    if (test_fifo_capacity_and_close() != 0) return 1;
    if (test_concurrent_producers_and_owner_affinity() != 0) return 1;
    if (test_real_kernel_wake_and_close() != 0) return 1;
    if (test_native_input_and_wake_multiplex() != 0) return 1;
    if (test_owner_message_dispatch_batches() != 0) return 1;
    puts("Tango async mailbox tests passed");
    return 0;
}
