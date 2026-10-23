/* Native Tango core regression tests. Does not require an interactive TTY. */
#include "tui.h"
#include "tui_terminal_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int test_terminal_link(void) {
    tui_event_t event = {0};
    /* This reference pulls tui_input.o and terminal.o from the real archive.
       On Unix it catches a mismatched resize-symbol linkage. */
    CHECK(!tui_terminal_poll(NULL, &event));
    tui_terminal_t *terminal = tui_terminal_create();
    CHECK(terminal != NULL);
    tui_terminal_destroy(terminal);
    return 0;
}

static void *reject_realloc(void *old, size_t size) {
    (void)old;
    (void)size;
    return NULL;
}

static int lifecycle_enters;
static int lifecycle_leaves;
static int lifecycle_writes;
static bool lifecycle_leave_succeeds;

static bool fake_platform_enter(tui_terminal_t *terminal) {
    (void)terminal;
    ++lifecycle_enters;
    return true;
}

static bool fake_platform_leave(tui_terminal_t *terminal) {
    (void)terminal;
    ++lifecycle_leaves;
    return lifecycle_leave_succeeds;
}

static bool fake_write(tui_terminal_t *terminal, const char *data, size_t size) {
    (void)terminal;
    (void)data;
    (void)size;
    ++lifecycle_writes;
    return true;
}

static bool reject_write(tui_terminal_t *terminal, const char *data,
                         size_t size) {
    (void)terminal;
    (void)data;
    (void)size;
    ++lifecycle_writes;
    return false;
}

static int test_terminal_lifecycle_is_transactional(void) {
    tui_terminal_t *terminal = tui_terminal_create();
    CHECK(terminal != NULL);
    terminal->platform_enter = fake_platform_enter;
    terminal->platform_leave = fake_platform_leave;
    terminal->write_output = reject_write;
    lifecycle_enters = lifecycle_leaves = lifecycle_writes = 0;
    lifecycle_leave_succeeds = true;

    CHECK(!tui_terminal_init(terminal));
    CHECK(tui_terminal_last_error(terminal) == TUI_TERMINAL_ERROR_OUTPUT_IO);
    CHECK(!terminal->initialized);
    CHECK(!terminal->platform_active);
    CHECK(terminal->front == NULL && terminal->back == NULL);
    CHECK(lifecycle_enters == 1 && lifecycle_leaves == 1);
    CHECK(lifecycle_writes == 2); /* failed enter + best-effort rollback */

    terminal->write_output = fake_write;
    CHECK(tui_terminal_init(terminal));
    CHECK(terminal->initialized);
    CHECK(terminal->platform_active);
    CHECK(terminal->front != NULL && terminal->back != NULL);

    terminal->write_output = reject_write;
    CHECK(!tui_terminal_cleanup(terminal));
    CHECK(terminal->initialized); /* output restoration can be retried */
    CHECK(!terminal->platform_active); /* platform restoration still ran */
    CHECK(tui_terminal_last_error(terminal) == TUI_TERMINAL_ERROR_OUTPUT_IO);
    terminal->write_output = fake_write;
    CHECK(tui_terminal_cleanup(terminal));

    tui_buffer_t *retained_front = terminal->front;
    tui_buffer_t *retained_back = terminal->back;
    terminal->write_output = reject_write;
    CHECK(!tui_terminal_init(terminal));
    CHECK(!terminal->initialized);
    CHECK(!terminal->platform_active);
    CHECK(terminal->front == retained_front && terminal->back == retained_back);

    terminal->write_output = fake_write;
    CHECK(tui_terminal_init(terminal));
    lifecycle_leave_succeeds = false;
    CHECK(!tui_terminal_cleanup(terminal));
    CHECK(terminal->initialized); /* restoration can be retried */
    CHECK(terminal->platform_active);
    CHECK(tui_terminal_last_error(terminal) == TUI_TERMINAL_ERROR_PLATFORM);

    lifecycle_leave_succeeds = true;
    CHECK(tui_terminal_cleanup(terminal));
    CHECK(!terminal->initialized);
    CHECK(!terminal->platform_active);
    CHECK(tui_terminal_last_error(terminal) == TUI_TERMINAL_ERROR_NONE);

    /* A later successful init replaces, rather than leaks, retained buffers. */
    CHECK(tui_terminal_init(terminal));
    CHECK(tui_terminal_cleanup(terminal));
    tui_terminal_destroy(terminal);
    return 0;
}

static int test_terminal_output_limit_is_atomic(void) {
    tui_terminal_t *terminal = tui_terminal_create();
    CHECK(terminal != NULL);

    terminal->width = 128;
    terminal->height = 64;
    terminal->front = tui_buffer_create(terminal->width, terminal->height);
    terminal->back = tui_buffer_create(terminal->width, terminal->height);
    CHECK(terminal->front != NULL && terminal->back != NULL);
    terminal->initialized = true;
    terminal->color_mode = TUI_COLOR_TRUECOLOR;

    const tui_cell_t prior = *tui_buffer_at_const(terminal->front, 0, 0);
    for (int y = 0; y < terminal->height; ++y) {
        for (int x = 0; x < terminal->width; ++x) {
            const tui_color_t fg = TUI_COLOR((x + y) & 0xff, x & 0xff, y & 0xff);
            tui_buffer_set(terminal->back, x, y,
                           tui_cell_styled('X', fg, TUI_BLACK, TUI_STYLE_BOLD));
        }
    }

    /* A failed growth must publish neither bytes nor front-buffer state. */
    terminal->output_realloc = reject_realloc;
    CHECK(!tui_terminal_render(terminal));
    CHECK(tui_terminal_last_error(terminal) ==
          TUI_TERMINAL_ERROR_OUTPUT_ALLOCATION);
    CHECK(terminal->output_len == 0u);
    CHECK(tui_cell_eq(tui_buffer_at_const(terminal->front, 0, 0), &prior));
    CHECK(tui_buffer_is_dirty(terminal->back));

    /* The independent hard limit follows the same retry-safe contract. */
    terminal->output_realloc = realloc;
    terminal->output_limit = terminal->output_cap;
    CHECK(!tui_terminal_render(terminal));
    CHECK(tui_terminal_last_error(terminal) == TUI_TERMINAL_ERROR_OUTPUT_LIMIT);
    CHECK(terminal->output_len == 0u);
    CHECK(tui_cell_eq(tui_buffer_at_const(terminal->front, 0, 0), &prior));
    CHECK(tui_buffer_is_dirty(terminal->back));

    terminal->initialized = false;
    tui_terminal_destroy(terminal);
    return 0;
}

#ifndef _WIN32
static int test_terminal_input_is_incremental(void) {
    tui_terminal_t *terminal = tui_terminal_create();
    tui_event_t event;
    CHECK(terminal != NULL);

    /* A split CSI sequence stays pending until its final byte arrives. */
    CHECK(tui_terminal_input_push(terminal, "\033", 1u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "[", 1u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "A", 1u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_KEY && event.key == TUI_KEY_UP);

    /* Bytes following the first event remain queued for later polls. */
    CHECK(tui_terminal_input_push(terminal, "bc", 2u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.ch == 'b');
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.ch == 'c');
    CHECK(!tui_terminal_input_next(terminal, &event));

    /* SGR mouse fields may cross any read boundary and are parsed boundedly. */
    terminal->mouse_enabled = true;
    CHECK(tui_terminal_input_push(terminal, "\033[<0;", 5u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "12;7", 4u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "M", 1u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_MOUSE_PRESS);
    CHECK(event.key == TUI_KEY_MOUSE_LEFT);
    CHECK(event.x == 11 && event.y == 6);

    /* Unknown complete sequences are skipped without hiding later input. */
    CHECK(tui_terminal_input_push(terminal, "\033[999xq", 7u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.ch == 'q');

    /* An unterminated sequence is bounded and the decoder recovers. */
    unsigned char hostile[64];
    hostile[0] = 27u;
    hostile[1] = '[';
    memset(hostile + 2, '1', sizeof(hostile) - 2u);
    CHECK(tui_terminal_input_push(terminal, hostile, sizeof(hostile)));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(terminal->input_len == 0u);
    CHECK(tui_terminal_input_push(terminal, "z", 1u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.ch == 'z');

    /* A lone Escape is emitted only when the pending prefix is flushed. */
    CHECK(tui_terminal_input_push(terminal, "\033", 1u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_flush(terminal, &event));
    CHECK(event.key == TUI_KEY_ESCAPE);

    /* A timed-out CSI prefix is discarded as one control sequence. */
    CHECK(tui_terminal_input_push(terminal, "\033[12", 4u));
    CHECK(!tui_terminal_input_flush(terminal, &event));
    CHECK(terminal->input_len == 0u);

    tui_terminal_destroy(terminal);
    return 0;
}
#endif

static int test_buffer_and_layout(void) {
    tui_buffer_t *buffer = tui_buffer_create(10, 3);
    CHECK(buffer != NULL);
    tui_buffer_text(buffer, 1, 1, "ok", TUI_WHITE, TUI_BLACK);
    CHECK(tui_buffer_at(buffer, 1, 1)->ch == 'o');
    CHECK(tui_buffer_at(buffer, 2, 1)->ch == 'k');
    tui_buffer_destroy(buffer);

    tui_layout_t *layout = tui_layout_create();
    CHECK(layout != NULL);
    int root = tui_layout_item(layout);
    int left = tui_layout_item(layout);
    int right = tui_layout_item(layout);
    CHECK(root >= 0 && left >= 0 && right >= 0);
    tui_layout_insert(layout, root, left);
    tui_layout_insert(layout, root, right);
    tui_layout_set_flags(layout, root, TUI_LAY_ROW | TUI_LAY_GAP(1));
    tui_layout_set_size(layout, left, 4, 0);
    tui_layout_set_size(layout, right, 4, 0);
    tui_layout_compute(layout, root, 10, 3);
    int x, y, w, h;
    tui_layout_get_rect(layout, right, &x, &y, &w, &h);
    CHECK(x == 5 && y == 0 && w == 4 && h == 3);
    tui_layout_destroy(layout);
    return 0;
}

static tui_event_t key_event(tui_key_t key) {
    tui_event_t e = {0};
    e.type = TUI_EVENT_KEY;
    e.key = key;
    return e;
}

static int test_textarea_bounds(void) {
    tui_textarea_t *ta = (tui_textarea_t*)calloc(1, sizeof(*ta));
    CHECK(ta != NULL);
    tui_textarea_init(ta);
    ta->focused = true;
    tui_event_t backspace = key_event(TUI_KEY_BACKSPACE);
    tui_event_t del = key_event(TUI_KEY_DELETE);

    /* Small joins still work. */
    tui_textarea_set_text(ta, "ab\ncd");
    ta->cursor_y = 1;
    ta->cursor_x = 0;
    CHECK(tui_textarea_handle(ta, &backspace));
    CHECK(ta->line_count == 1);
    CHECK(strcmp(ta->lines[0], "abcd") == 0);

    tui_textarea_set_text(ta, "ab\ncd");
    ta->cursor_y = 0;
    ta->cursor_x = 2;
    CHECK(tui_textarea_handle(ta, &del));
    CHECK(ta->line_count == 1);
    CHECK(strcmp(ta->lines[0], "abcd") == 0);

    /* Neither join may overrun TUI_TEXTAREA_LINE_LEN. */
    char long_text[340];
    memset(long_text, 'a', 250);
    long_text[250] = '\n';
    memset(long_text + 251, 'b', 80);
    long_text[331] = 0;
    tui_textarea_set_text(ta, long_text);
    CHECK(ta->line_count == 2);
    ta->cursor_y = 1;
    ta->cursor_x = 0;
    CHECK(tui_textarea_handle(ta, &backspace));
    CHECK(ta->line_count == 2);
    CHECK(strlen(ta->lines[0]) == 250);
    CHECK(strlen(ta->lines[1]) == 80);

    ta->cursor_y = 0;
    ta->cursor_x = 250;
    CHECK(tui_textarea_handle(ta, &del));
    CHECK(ta->line_count == 2);
    CHECK(strlen(ta->lines[0]) == 250);
    CHECK(strlen(ta->lines[1]) == 80);

    /* Repeated newline input cannot produce an out-of-bounds line_count. */
    const size_t n = TUI_TEXTAREA_MAX_LINES + 2;
    char *many = (char*)malloc(2 * n + 1);
    CHECK(many != NULL);
    for (size_t j = 0; j < n; ++j) {
        many[2 * j] = 'x';
        many[2 * j + 1] = '\n';
    }
    many[2 * n] = 0;
    tui_textarea_set_text(ta, many);
    CHECK(ta->line_count == TUI_TEXTAREA_MAX_LINES);
    CHECK(ta->lines[TUI_TEXTAREA_MAX_LINES - 1][0] == 'x');
    free(many);
    free(ta);
    return 0;
}

int main(void) {
    if (test_terminal_link()) return 1;
    if (test_terminal_lifecycle_is_transactional()) return 1;
    if (test_terminal_output_limit_is_atomic()) return 1;
#ifndef _WIN32
    if (test_terminal_input_is_incremental()) return 1;
#endif
    if (test_buffer_and_layout()) return 1;
    if (test_textarea_bounds()) return 1;
    puts("Tango native-core tests passed");
    return 0;
}
