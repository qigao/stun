/*
 * Tango's UI-owner event loop. Business producers copy messages into the
 * bounded mailbox; input, model transitions, and rendering have one owner.
 */
#include "tui.h"
#include "tui_terminal_internal.h"
#include "tui_app_internal.h"
#include "tui_async_wait_internal.h"

#include <salts/thread.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

enum {
    TUI_APP_QUEUE_CAPACITY = 128u,
    TUI_APP_MAX_PAYLOAD = 1024u,
    TUI_APP_INPUT_BUDGET = 32u,
    TUI_APP_MESSAGE_BUDGET = 32u
};

tui_app_t* tui_app_create(void* model, tui_update_fn update,
                          tui_view_fn view, tui_event_map_fn event_map) {
    tui_app_t* app;
    const tui_async_queue_config_t config = {
        TUI_APP_QUEUE_CAPACITY, TUI_APP_MAX_PAYLOAD
    };
    if (!update || !view || !event_map) return NULL;
    app = (tui_app_t*)calloc(1u, sizeof(*app));
    if (!app) return NULL;
    app->model = model;
    app->update = update;
    app->view = view;
    app->event_map = event_map;
    app->owns_terminal = true;
    app->needs_redraw = true;
    app->running = true;
    app->async_capacity = config.max_payload_bytes;
    app->terminal = tui_terminal_create();
    app->async_queue = tui_async_queue_create(&config);
    app->async_buffer = (unsigned char*)malloc(config.max_payload_bytes);
    if (!app->terminal || !app->async_queue || !app->async_buffer) {
        if (app->async_queue) {
            (void)tui_async_queue_close(app->async_queue);
            (void)tui_async_queue_destroy(app->async_queue);
        }
        tui_terminal_destroy(app->terminal);
        free(app->async_buffer);
        free(app);
        return NULL;
    }
    return app;
}

bool tui_app_set_async_update(tui_app_t* app, tui_async_update_fn update) {
    if (!app || !app->async_queue ||
        !tui_async_queue_is_owner(app->async_queue))
        return false;
    /* No reconfiguration once messages have been admitted. */
    tui_async_queue_stats_t stats;
    if (tui_async_queue_get_stats(app->async_queue, &stats) != TUI_ASYNC_OK ||
        stats.accepted != 0u || stats.closed)
        return false;
    app->async_update = update;
    return true;
}

tui_async_queue_t* tui_app_async_queue(tui_app_t* app) {
    return app && app->async_update ? app->async_queue : NULL;
}

size_t tui_app_pump_messages(tui_app_t* app, size_t budget) {
    size_t count = 0u;
    if (!app || !app->async_queue || !app->async_update ||
        !tui_async_queue_is_owner(app->async_queue))
        return 0u;
    while (count < budget && app->running) {
        tui_async_event_t event = {0};
        const tui_async_status_t status = tui_async_queue_try_pop(
            app->async_queue, &event, app->async_buffer, app->async_capacity);
        if (status != TUI_ASYNC_OK) {
            if (status != TUI_ASYNC_EMPTY) app->running = false;
            break;
        }
        ++count;
        app->needs_redraw = true;
        if (!app->async_update(&event, app->async_buffer, app->model))
            app->running = false;
    }
    return count;
}

void tui_app_destroy(tui_app_t* app) {
    if (!app) return;
    /* No producer or post/wake tail may still be running here. */
    if (app->async_queue) {
        if (!tui_async_queue_is_owner(app->async_queue)) return;
        (void)tui_async_queue_close(app->async_queue);
        tui_async_event_t event;
        while (tui_async_queue_try_pop(app->async_queue, &event,
                   app->async_buffer, app->async_capacity) == TUI_ASYNC_OK) {}
        (void)tui_async_queue_destroy(app->async_queue);
    }
    if (app->owns_terminal) tui_terminal_destroy(app->terminal);
    free(app->async_buffer);
    free(app);
}

bool tui_app_dispatch_once(tui_app_t *app) {
    if (!app || !app->async_queue ||
        !tui_async_queue_is_owner(app->async_queue) || !app->terminal)
        return false;
    /* Bound keyboard processing to avoid starving streaming traffic. */
    for (size_t i = 0u; i < TUI_APP_INPUT_BUDGET && app->running; ++i) {
        tui_event_t event;
        if (!tui_terminal_poll(app->terminal, &event)) break;
        if (event.type == TUI_EVENT_RESIZE) {
            app->needs_redraw = true;
            continue;
        }
        const int msg = app->event_map(&event);
        if (msg >= 0) {
            app->running = app->update(msg, app->model);
            app->needs_redraw = true;
        }
    }
    (void)tui_app_pump_messages(app, TUI_APP_MESSAGE_BUDGET);
    if (!app->running) return false;

    /* Exactly one render for this batch of input + async updates. */
    if (app->needs_redraw) {
        // A resize can replace and clear both native cell buffers. Prepare
        // them before owner view/render, never after Box has painted a frame.
        if (!tui_terminal_prepare_frame(app->terminal)) {
            app->running = false;
            return false;
        }
        tui_buffer_t* back = tui_terminal_buffer(app->terminal);
        app->view(app->model, back);
        if (!tui_terminal_render(app->terminal)) {
            app->running = false;
            return false;
        }
        app->needs_redraw = false;
    }

    return app->running;
}

void tui_app_run(tui_app_t* app) {
    if (!app || !app->async_queue ||
        !tui_async_queue_is_owner(app->async_queue))
        return;
    if (!tui_terminal_init(app->terminal)) return;
#ifndef _WIN32
    /* Register the private existing wake pipe before entering the Owner
     * loop. A process-directed SIGWINCH on ANY thread must wake the Owner. */
    tui_terminal_sigwinch_set_wake_fd(
        tui_async_queue_signal_wake_fd(app->async_queue));
#endif
    if (!tui_terminal_enable_mouse(app->terminal)) {
        (void)tui_terminal_cleanup(app->terminal);
        return;
    }

    app->running = true;
    app->needs_redraw = true;
    int old_w = tui_terminal_width(app->terminal);
    int old_h = tui_terminal_height(app->terminal);

    while (app->running) {
        tui_terminal_query_size(app->terminal);
        const int width = tui_terminal_width(app->terminal);
        const int height = tui_terminal_height(app->terminal);
        if (width != old_w || height != old_h) {
            old_w = width;
            old_h = height;
            app->needs_redraw = true;
        }

        if (!tui_app_dispatch_once(app)) break;

        /* Queue state is authoritative, not the wake byte/event. The wait
         * routine checks under the queue mutex immediately before sleeping;
         * a concurrent publisher always signals after publication. */
        tui_async_queue_stats_t stats;
        if (tui_async_queue_get_stats(app->async_queue, &stats) != TUI_ASYNC_OK)
            break;
        if (stats.queued != 0u) continue;
        if (stats.closed) break;

#ifdef _WIN32
        const intptr_t input = (intptr_t)app->terminal->h_in;
#else
        const intptr_t input = STDIN_FILENO;
#endif
        const tui_async_wait_result_t outcome = tui_async_wait_io(
            app->async_queue, input,
#ifndef _WIN32
            app->terminal->input_len ? 10 : -1
#else
            -1
#endif
        );
        if (outcome == TUI_ASYNC_WAIT_ERROR) {
            app->running = false;
            break;
        }
        /* INPUT/MESSAGE/INTERRUPTED/TIMEOUT all restart owner dispatch.
         * Partial escape bytes have the existing 10 ms disambiguation. */
    }
    (void)tui_async_queue_close(app->async_queue);
    (void)tui_terminal_disable_mouse(app->terminal);
    (void)tui_terminal_cleanup(app->terminal);
}

void* tui_app_model(tui_app_t* app) {
    return app ? app->model : NULL;
}

tui_terminal_t* tui_app_terminal(tui_app_t* app) {
    return app ? app->terminal : NULL;
}
