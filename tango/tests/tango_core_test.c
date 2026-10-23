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

    /* UTF-8 scalar ownership stays in Salts::Unicode across split reads. */
    CHECK(tui_terminal_input_push(terminal, "\xE4", 1u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "\xB8\xADx", 3u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_KEY && event.key == TUI_KEY_NONE);
    CHECK(event.ch == 0x4E2Du);
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.ch == 'x');

    /* Malformed UTF-8 never hides a later valid queued key. */
    CHECK(tui_terminal_input_push(terminal, "\xC0q", 2u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_flush(terminal, &event));
    CHECK(event.ch == 'q');
    CHECK(terminal->input_len == 0u);

    /* A timed-out partial scalar is discarded without emitting byte events. */
    CHECK(tui_terminal_input_push(terminal, "\xE4\xB8", 2u));
    CHECK(!tui_terminal_input_flush(terminal, &event));
    CHECK(terminal->input_len == 0u);

    /* Bracketed paste framing may split at any byte.  Payload scalars are
     * explicitly typed as paste data, never keyboard commands. */
    CHECK(tui_terminal_input_push(terminal, "\033[20", 4u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "0~", 2u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(terminal->paste_active);
    CHECK(tui_terminal_input_push(terminal, "a\xE4", 2u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_PASTE && event.ch == 'a');
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "\xB8\xAD\r\n", 4u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_PASTE && event.ch == 0x4e2du);
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_PASTE && event.ch == '\n');

    /* An escape sequence inside a paste cannot become an arrow event. */
    CHECK(tui_terminal_input_push(terminal, "\033[Aq", 4u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_PASTE && event.ch == '[');
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_PASTE && event.ch == 'A');
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_PASTE && event.ch == 'q');

    CHECK(tui_terminal_input_push(terminal, "\033[20", 4u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "1~\033[A", 5u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(!terminal->paste_active);
    CHECK(event.type == TUI_EVENT_KEY && event.key == TUI_KEY_UP);

    /* A timed-out partial paste control never falls through as Escape/key
     * input; discard mode remains bounded until a later complete end marker. */
    CHECK(tui_terminal_input_push(terminal, "\033[200~", 6u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    CHECK(tui_terminal_input_push(terminal, "\033[20", 4u));
    CHECK(!tui_terminal_input_flush(terminal, &event));
    CHECK(terminal->paste_active && terminal->paste_discarding);
    CHECK(tui_terminal_input_push(terminal, "\033[201~k", 7u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(event.type == TUI_EVENT_KEY && event.ch == 'k');

    /* Once the hard payload limit is reached, content is discarded until the
     * trusted end marker, after which ordinary keyboard decoding resumes. */
    CHECK(tui_terminal_input_push(terminal, "\033[200~", 6u));
    CHECK(!tui_terminal_input_next(terminal, &event));
    terminal->paste_bytes = TUI_TERMINAL_PASTE_LIMIT;
    CHECK(tui_terminal_input_push(terminal, "x\033[201~z", 8u));
    CHECK(tui_terminal_input_next(terminal, &event));
    CHECK(!terminal->paste_active && !terminal->paste_discarding);
    CHECK(event.type == TUI_EVENT_KEY && event.ch == 'z');

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

#define UTF8_GRIN       "\xF0\x9F\x98\x80"
#define UTF8_WOMAN_TECH "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB"
#define UTF8_FLAG_JP    "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"
#define UTF8_THUMBS_MED "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD"
#define UTF8_HEART      "\xE2\x9D\xA4\xEF\xB8\x8F"
#define UTF8_KEYCAP_ONE "1\xEF\xB8\x8F\xE2\x83\xA3"
#define UTF8_CJK        "\xE4\xB8\xAD"
#define UTF8_E_ACUTE    "e\xCC\x81"

static char rendered[4096];
static size_t rendered_len;

static bool capture_write(tui_terminal_t *terminal, const char *data,
                          size_t size) {
    (void)terminal;
    if (size > sizeof(rendered) - 1u - rendered_len) return false;
    memcpy(rendered + rendered_len, data, size);
    rendered_len += size;
    rendered[rendered_len] = 0;
    return true;
}

static int test_grapheme_cells(void) {
    static const char text[] =
        "A" UTF8_GRIN UTF8_WOMAN_TECH UTF8_FLAG_JP UTF8_THUMBS_MED
        UTF8_HEART UTF8_KEYCAP_ONE UTF8_CJK UTF8_E_ACUTE "Z";
    static const int leads[] = {0, 1, 3, 5, 7, 9, 11, 13, 15, 16};
    static const int continuations[] = {2, 4, 6, 8, 10, 12, 14};
    tui_buffer_t *buffer = tui_buffer_create(20, 2);
    CHECK(buffer != NULL);

    CHECK(tui_grapheme_width(UTF8_GRIN, sizeof(UTF8_GRIN) - 1u) == 2);
    CHECK(tui_grapheme_width(UTF8_WOMAN_TECH,
                             sizeof(UTF8_WOMAN_TECH) - 1u) == 2);
    CHECK(tui_grapheme_width(UTF8_FLAG_JP, sizeof(UTF8_FLAG_JP) - 1u) == 2);
    CHECK(tui_grapheme_width(UTF8_THUMBS_MED,
                             sizeof(UTF8_THUMBS_MED) - 1u) == 2);
    CHECK(tui_grapheme_width(UTF8_HEART, sizeof(UTF8_HEART) - 1u) == 2);
    CHECK(tui_grapheme_width(UTF8_KEYCAP_ONE,
                             sizeof(UTF8_KEYCAP_ONE) - 1u) == 2);
    CHECK(tui_grapheme_width(UTF8_CJK, sizeof(UTF8_CJK) - 1u) == 2);
    CHECK(tui_grapheme_width(UTF8_E_ACUTE,
                             sizeof(UTF8_E_ACUTE) - 1u) == 1);
    CHECK(tui_buffer_text(buffer, 0, 0, text, TUI_WHITE, TUI_BLACK));

    for (size_t i = 0; i < sizeof(leads) / sizeof(leads[0]); ++i) {
        CHECK(tui_buffer_at_const(buffer, leads[i], 0)->span !=
              TUI_CELL_SPAN_CONTINUATION);
    }
    for (size_t i = 0;
         i < sizeof(continuations) / sizeof(continuations[0]); ++i) {
        CHECK(tui_buffer_at_const(buffer, continuations[i], 0)->span ==
              TUI_CELL_SPAN_CONTINUATION);
    }
    CHECK(tui_buffer_at_const(buffer, 3, 0)->grapheme_len ==
          sizeof(UTF8_WOMAN_TECH) - 1u);
    CHECK(memcmp(tui_buffer_at_const(buffer, 3, 0)->grapheme,
                 UTF8_WOMAN_TECH, sizeof(UTF8_WOMAN_TECH) - 1u) == 0);
    CHECK(tui_buffer_at_const(buffer, 15, 0)->ch == 'e');

    /* Replacing either half clears the complete old wide-cell span. */
    tui_buffer_set(buffer, 2, 0, tui_cell('x', TUI_WHITE, TUI_BLACK));
    CHECK(tui_buffer_at_const(buffer, 1, 0)->ch == ' ');
    CHECK(tui_buffer_at_const(buffer, 1, 0)->span == TUI_CELL_SPAN_SINGLE);
    CHECK(tui_buffer_at_const(buffer, 2, 0)->ch == 'x');
    tui_buffer_set(buffer, 3, 0, tui_cell('y', TUI_WHITE, TUI_BLACK));
    CHECK(tui_buffer_at_const(buffer, 3, 0)->ch == 'y');
    CHECK(tui_buffer_at_const(buffer, 4, 0)->ch == ' ');

    /* Scalar callers are normalized through the same width policy, while
     * terminal controls and invalid scalars never enter the buffer. */
    tui_buffer_clear(buffer);
    tui_buffer_set(buffer, 0, 0, tui_cell(0x4e2du, TUI_WHITE, TUI_BLACK));
    CHECK(tui_buffer_at_const(buffer, 0, 0)->span == TUI_CELL_SPAN_WIDE);
    CHECK(tui_buffer_at_const(buffer, 1, 0)->span ==
          TUI_CELL_SPAN_CONTINUATION);
    const tui_cell_t scalar_prior = *tui_buffer_at_const(buffer, 2, 0);
    tui_buffer_set(buffer, 2, 0, tui_cell(0x1bu, TUI_WHITE, TUI_BLACK));
    CHECK(tui_cell_eq(&scalar_prior, tui_buffer_at_const(buffer, 2, 0)));
    tui_buffer_set(buffer, 2, 0,
                   tui_cell(0x110000u, TUI_WHITE, TUI_BLACK));
    CHECK(tui_cell_eq(&scalar_prior, tui_buffer_at_const(buffer, 2, 0)));

    /* Invalid, oversized, and clipped-wide writes publish no partial cell. */
    const tui_cell_t prior = *tui_buffer_at_const(buffer, 19, 0);
    CHECK(!tui_buffer_set_grapheme(buffer, 19, 0, UTF8_CJK,
                                   sizeof(UTF8_CJK) - 1u,
                                   TUI_WHITE, TUI_BLACK, TUI_STYLE_NONE));
    CHECK(tui_cell_eq(&prior, tui_buffer_at_const(buffer, 19, 0)));
    CHECK(!tui_buffer_text(buffer, 19, 0, "\xF0\x28\x8C\x28",
                           TUI_WHITE, TUI_BLACK));
    CHECK(tui_cell_eq(&prior, tui_buffer_at_const(buffer, 19, 0)));
    char oversized[TUI_CELL_GRAPHEME_CAPACITY + 2u];
    oversized[0] = 'e';
    for (size_t i = 1u; i < sizeof(oversized) - 1u; i += 2u) {
        oversized[i] = (char)0xCC;
        oversized[i + 1u] = (char)0x81;
    }
    oversized[sizeof(oversized) - 1u] = 0;
    CHECK(!tui_buffer_text(buffer, 19, 0, oversized,
                           TUI_WHITE, TUI_BLACK));
    CHECK(tui_cell_eq(&prior, tui_buffer_at_const(buffer, 19, 0)));

    tui_buffer_clear(buffer);
    tui_text_wrap(buffer, 0, 0, 2, 2, UTF8_CJK "a",
                  TUI_WHITE, TUI_BLACK);
    CHECK(tui_buffer_at_const(buffer, 0, 0)->span == TUI_CELL_SPAN_WIDE);
    CHECK(tui_buffer_at_const(buffer, 1, 0)->span ==
          TUI_CELL_SPAN_CONTINUATION);
    CHECK(tui_buffer_at_const(buffer, 0, 1)->ch == 'a');
    CHECK(tui_text_wrap_height(UTF8_CJK "a", 2) == 2);
    tui_buffer_destroy(buffer);

    /* The renderer emits owned cluster bytes once; continuation emits none. */
    tui_terminal_t *terminal = tui_terminal_create();
    CHECK(terminal != NULL);
    terminal->width = 4;
    terminal->height = 1;
    terminal->front = tui_buffer_create(4, 1);
    terminal->back = tui_buffer_create(4, 1);
    CHECK(terminal->front != NULL && terminal->back != NULL);
    terminal->initialized = true;
    terminal->color_mode = TUI_COLOR_TRUECOLOR;
    terminal->write_output = capture_write;
    rendered_len = 0u;
    CHECK(tui_buffer_text(terminal->back, 0, 0,
                          UTF8_WOMAN_TECH "X", TUI_WHITE, TUI_BLACK));
    CHECK(tui_terminal_render(terminal));
    CHECK(strstr(rendered, UTF8_WOMAN_TECH "X") != NULL);
    terminal->initialized = false;
    tui_terminal_destroy(terminal);
    return 0;
}

static int test_terminal_render_contract(void) {
    tui_terminal_t *terminal = tui_terminal_create();
    CHECK(terminal != NULL);
    terminal->width = 2;
    terminal->height = 1;
    terminal->front = tui_buffer_create(2, 1);
    terminal->back = tui_buffer_create(2, 1);
    CHECK(terminal->front != NULL && terminal->back != NULL);
    terminal->initialized = true;
    terminal->write_output = capture_write;

    /* Public structs may be mutated directly.  The final renderer boundary
     * must reject forged control bytes without writing a partial frame. */
    tui_cell_t *forged = tui_buffer_at(terminal->back, 0, 0);
    forged->ch = 0x1bu;
    forged->grapheme_len = 1u;
    forged->grapheme[0] = '\033';
    tui_buffer_mark_dirty(terminal->back, 0, 0);
    rendered_len = 0u;
    CHECK(!tui_terminal_render(terminal));
    CHECK(tui_terminal_last_error(terminal) ==
          TUI_TERMINAL_ERROR_INVALID_STATE);
    CHECK(rendered_len == 0u);
    CHECK(tui_buffer_is_dirty(terminal->back));

    /* Both supported color modes have an exact, tested SGR contract. */
    terminal->color_mode = TUI_COLOR_TRUECOLOR;
    tui_buffer_set(terminal->back, 0, 0,
                   tui_cell('X', TUI_COLOR(1, 2, 3), TUI_COLOR(4, 5, 6)));
    rendered_len = 0u;
    CHECK(tui_terminal_render(terminal));
    CHECK(strstr(rendered, "\033[38;2;1;2;3m") != NULL);
    CHECK(strstr(rendered, "\033[48;2;4;5;6m") != NULL);

    terminal->color_mode = TUI_COLOR_256;
    tui_buffer_set(terminal->back, 0, 0,
                   tui_cell('Y', TUI_COLOR(255, 0, 0),
                             TUI_COLOR(0, 255, 0)));
    rendered_len = 0u;
    CHECK(tui_terminal_render(terminal));
    CHECK(strstr(rendered, "\033[38;5;196m") != NULL);
    CHECK(strstr(rendered, "\033[48;5;46m") != NULL);

    /* A forged wide lead without its continuation is likewise rejected. */
    forged = tui_buffer_at(terminal->back, 0, 0);
    forged->span = TUI_CELL_SPAN_WIDE;
    tui_buffer_mark_dirty(terminal->back, 0, 0);
    rendered_len = 0u;
    CHECK(!tui_terminal_render(terminal));
    CHECK(tui_terminal_last_error(terminal) ==
          TUI_TERMINAL_ERROR_INVALID_STATE);
    CHECK(rendered_len == 0u);

    terminal->initialized = false;
    tui_terminal_destroy(terminal);
    return 0;
}


/* Every invalid public cell state must fail before any part of a frame is
 * written or committed. Exercise malformed cells after a valid dirty prefix
 * so a future early flush cannot leak a partial terminal frame. */
static int expect_invalid_render(tui_terminal_t *terminal,
                                 const tui_cell_t expected_front[3]) {
    rendered_len = 0u;
    CHECK(!tui_terminal_render(terminal));
    CHECK(tui_terminal_last_error(terminal) ==
          TUI_TERMINAL_ERROR_INVALID_STATE);
    CHECK(rendered_len == 0u);
    CHECK(terminal->output_len == 0u);
    CHECK(tui_buffer_is_dirty(terminal->back));
    for (int x = 0; x < 3; ++x) {
        CHECK(tui_cell_eq(&expected_front[x],
                          tui_buffer_at_const(terminal->front, x, 0)));
    }
    return 0;
}

static void prepare_invalid_render(tui_terminal_t *terminal) {
    tui_buffer_clear(terminal->back);
    tui_buffer_set(terminal->back, 0, 0,
                   tui_cell('A', TUI_WHITE, TUI_BLACK));
    tui_buffer_set(terminal->back, 1, 0,
                   tui_cell('B', TUI_WHITE, TUI_BLACK));
}

static int test_terminal_renderer_rejects_forged_cells(void) {
    tui_terminal_t *terminal = tui_terminal_create();
    CHECK(terminal != NULL);
    terminal->width = 3;
    terminal->height = 1;
    terminal->front = tui_buffer_create(3, 1);
    terminal->back = tui_buffer_create(3, 1);
    CHECK(terminal->front != NULL && terminal->back != NULL);
    terminal->initialized = true;
    terminal->color_mode = TUI_COLOR_TRUECOLOR;
    terminal->write_output = capture_write;

    tui_cell_t expected_front[3];
    for (int x = 0; x < 3; ++x) {
        expected_front[x] = *tui_buffer_at_const(terminal->front, x, 0);
    }

    /* C1 controls must be rejected even when a caller bypasses buffer APIs. */
    prepare_invalid_render(terminal);
    *tui_buffer_at(terminal->back, 2, 0) =
        tui_cell(0x85u, TUI_WHITE, TUI_BLACK);
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* Surrogate and out-of-range scalars must never reach the renderer. */
    {
        static const uint32_t invalid_scalars[] = {0xd800u, 0x110000u};
        for (size_t i = 0;
             i < sizeof(invalid_scalars) / sizeof(invalid_scalars[0]); ++i) {
            prepare_invalid_render(terminal);
            *tui_buffer_at(terminal->back, 2, 0) =
                tui_cell(invalid_scalars[i], TUI_WHITE, TUI_BLACK);
            tui_buffer_mark_dirty(terminal->back, 2, 0);
            CHECK(expect_invalid_render(terminal, expected_front) == 0);
        }
    }

    /* Unknown cell-span values cannot be interpreted as single cells. */
    prepare_invalid_render(terminal);
    tui_cell_t forged = tui_cell('X', TUI_WHITE, TUI_BLACK);
    forged.span = 0xffu;
    *tui_buffer_at(terminal->back, 2, 0) = forged;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* An isolated UTF-8 continuation byte is not a renderable grapheme. */
    prepare_invalid_render(terminal);
    forged = tui_cell('X', TUI_WHITE, TUI_BLACK);
    forged.grapheme_len = 1u;
    forged.grapheme[0] = (char)0x80;
    *tui_buffer_at(terminal->back, 2, 0) = forged;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* The owned grapheme bytes must begin with the cell's public scalar. */
    prepare_invalid_render(terminal);
    forged = tui_cell('X', TUI_WHITE, TUI_BLACK);
    forged.grapheme_len = 1u;
    forged.grapheme[0] = 'Y';
    *tui_buffer_at(terminal->back, 2, 0) = forged;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* A cell cannot claim more owned grapheme bytes than its fixed storage. */
    prepare_invalid_render(terminal);
    forged = tui_cell('X', TUI_WHITE, TUI_BLACK);
    forged.grapheme_len =
        (uint8_t)(TUI_CELL_GRAPHEME_CAPACITY + 1u);
    *tui_buffer_at(terminal->back, 2, 0) = forged;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* A single cell cannot contain two grapheme clusters. */
    prepare_invalid_render(terminal);
    forged = tui_cell('A', TUI_WHITE, TUI_BLACK);
    forged.grapheme_len = 2u;
    forged.grapheme[0] = 'A';
    forged.grapheme[1] = 'B';
    *tui_buffer_at(terminal->back, 2, 0) = forged;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* A wide scalar clipped at the right edge must not reach the terminal. */
    prepare_invalid_render(terminal);
    forged = tui_cell(0x4e2du, TUI_WHITE, TUI_BLACK);
    forged.span = TUI_CELL_SPAN_WIDE;
    *tui_buffer_at(terminal->back, 2, 0) = forged;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* A continuation cannot be orphaned from a wide leading cell. */
    prepare_invalid_render(terminal);
    forged = tui_cell(0u, TUI_WHITE, TUI_BLACK);
    forged.span = TUI_CELL_SPAN_CONTINUATION;
    *tui_buffer_at(terminal->back, 2, 0) = forged;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    /* A wide lead and continuation must have matching style and colors. */
    prepare_invalid_render(terminal);
    CHECK(tui_buffer_set_grapheme(terminal->back, 1, 0, UTF8_CJK,
                                  sizeof(UTF8_CJK) - 1u,
                                  TUI_WHITE, TUI_BLACK, TUI_STYLE_NONE));
    tui_buffer_at(terminal->back, 2, 0)->style = TUI_STYLE_BOLD;
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    prepare_invalid_render(terminal);
    CHECK(tui_buffer_set_grapheme(terminal->back, 1, 0, UTF8_CJK,
                                  sizeof(UTF8_CJK) - 1u,
                                  TUI_WHITE, TUI_BLACK, TUI_STYLE_NONE));
    tui_buffer_at(terminal->back, 2, 0)->fg = TUI_COLOR(1, 2, 3);
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    prepare_invalid_render(terminal);
    CHECK(tui_buffer_set_grapheme(terminal->back, 1, 0, UTF8_CJK,
                                  sizeof(UTF8_CJK) - 1u,
                                  TUI_WHITE, TUI_BLACK, TUI_STYLE_NONE));
    tui_buffer_at(terminal->back, 2, 0)->bg = TUI_COLOR(3, 2, 1);
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    /* The continuation background must also match its lead. */
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    prepare_invalid_render(terminal);
    CHECK(tui_buffer_set_grapheme(terminal->back, 1, 0, UTF8_CJK,
                                  sizeof(UTF8_CJK) - 1u,
                                  TUI_WHITE, TUI_BLACK, TUI_STYLE_NONE));
    tui_buffer_at(terminal->back, 2, 0)->ch = 'x';
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    prepare_invalid_render(terminal);
    CHECK(tui_buffer_set_grapheme(terminal->back, 1, 0, UTF8_CJK,
                                  sizeof(UTF8_CJK) - 1u,
                                  TUI_WHITE, TUI_BLACK, TUI_STYLE_NONE));
    tui_buffer_at(terminal->back, 2, 0)->grapheme_len = 1u;
    tui_buffer_at(terminal->back, 2, 0)->grapheme[0] = 'x';
    tui_buffer_mark_dirty(terminal->back, 2, 0);
    CHECK(expect_invalid_render(terminal, expected_front) == 0);

    terminal->initialized = false;
    tui_terminal_destroy(terminal);
    return 0;
}

static int test_layout_rejects_invalid_graphs(void) {
    int x = 7, y = 7, w = 7, h = 7;

    CHECK(tui_layout_item(NULL) == -1);
    tui_layout_reset(NULL);
    tui_layout_insert(NULL, 0, 1);
    tui_layout_set_size(NULL, 0, 1, 1);
    tui_layout_set_min_size(NULL, 0, 1, 1);
    tui_layout_set_grow(NULL, 0, 1);
    tui_layout_set_shrink(NULL, 0, 1);
    tui_layout_set_flags(NULL, 0, 0);
    tui_layout_set_margins(NULL, 0, 0, 0, 0, 0);
    tui_layout_compute(NULL, 0, 1, 1);
    tui_layout_get_rect(NULL, 0, &x, &y, &w, &h);
    CHECK(x == 0 && y == 0 && w == 0 && h == 0);

    tui_layout_t *layout = tui_layout_create();
    CHECK(layout != NULL);
    int root = tui_layout_item(layout);
    int child = tui_layout_item(layout);
    int other = tui_layout_item(layout);
    CHECK(root == 0 && child == 1 && other == 2);

    /* All out-of-range public handles are safe no-ops. */
    tui_layout_insert(layout, 99, child);
    tui_layout_insert(layout, root, 99);
    tui_layout_set_size(layout, 99, 1, 1);
    tui_layout_set_min_size(layout, 99, 1, 1);
    tui_layout_set_grow(layout, 99, 1);
    tui_layout_set_shrink(layout, 99, 1);
    tui_layout_set_flags(layout, 99, TUI_LAY_ROW);
    tui_layout_set_margins(layout, 99, 1, 1, 1, 1);
    tui_layout_compute(layout, 99, 1, 1);
    tui_layout_get_rect(layout, 99, &x, NULL, &w, NULL);
    CHECK(x == 0 && w == 0);

    tui_layout_insert(layout, root, child);
    tui_layout_insert(layout, child, root); /* cycle */
    tui_layout_insert(layout, other, child); /* reparent */
    tui_layout_set_flags(layout, root, TUI_LAY_ROW);
    tui_layout_set_min_size(layout, child, 1, 1);
    tui_layout_set_grow(layout, child, -1);
    tui_layout_insert(layout, root, other);
    tui_layout_set_min_size(layout, other, 1, 1);
    tui_layout_set_grow(layout, other, 1);
    tui_layout_compute(layout, root, 10, 2);

    tui_layout_get_rect(layout, child, &x, &y, &w, &h);
    CHECK(x == 0 && y == 0 && w == 1 && h == 2);
    tui_layout_get_rect(layout, other, &x, &y, &w, &h);
    CHECK(x == 1 && y == 0 && w == 9 && h == 2);

    /* The fixed-capacity arena refuses item 257 without writing past it. */
    for (int i = 3; i < 256; ++i) CHECK(tui_layout_item(layout) == i);
    CHECK(tui_layout_item(layout) == -1);
    tui_layout_destroy(layout);
    return 0;
}

static tui_event_t key_event(tui_key_t key) {
    tui_event_t e = {0};
    e.type = TUI_EVENT_KEY;
    e.key = key;
    return e;
}

static tui_event_t scalar_event(uint32_t scalar) {
    tui_event_t e = {0};
    e.type = TUI_EVENT_KEY;
    e.ch = scalar;
    return e;
}

static tui_event_t paste_event(uint32_t scalar) {
    tui_event_t e = {0};
    e.type = TUI_EVENT_PASTE;
    e.ch = scalar;
    return e;
}

static int test_input_grapheme_editing(void) {
    tui_input_state_t state;
    tui_input_init(&state);
    tui_input_set(&state, UTF8_WOMAN_TECH "a" UTF8_E_ACUTE);
    CHECK(state.len == (int)(sizeof(UTF8_WOMAN_TECH) - 1u + 1u +
                             sizeof(UTF8_E_ACUTE) - 1u));
    CHECK(state.cursor == state.len);

    tui_event_t left = key_event(TUI_KEY_LEFT);
    tui_event_t right = key_event(TUI_KEY_RIGHT);
    tui_event_t backspace = key_event(TUI_KEY_BACKSPACE);
    tui_event_t del = key_event(TUI_KEY_DELETE);
    CHECK(tui_input_handle(&state, &left));
    CHECK(state.cursor == (int)(sizeof(UTF8_WOMAN_TECH) - 1u + 1u));
    CHECK(tui_input_handle(&state, &backspace));
    CHECK(strcmp(state.text, UTF8_WOMAN_TECH UTF8_E_ACUTE) == 0);
    CHECK(state.cursor == (int)(sizeof(UTF8_WOMAN_TECH) - 1u));
    CHECK(tui_input_handle(&state, &del));
    CHECK(strcmp(state.text, UTF8_WOMAN_TECH) == 0);
    CHECK(tui_input_handle(&state, &backspace));
    CHECK(state.text[0] == '\0' && state.cursor == 0 && state.len == 0);

    /* Paste events insert data but cannot smuggle a navigation key. */
    tui_event_t pasted = paste_event('x');
    pasted.key = TUI_KEY_BACKSPACE;
    CHECK(tui_input_handle(&state, &pasted));
    CHECK(strcmp(state.text, "x") == 0 && state.cursor == 1);
    pasted = paste_event('\n');
    CHECK(!tui_input_handle(&state, &pasted));
    CHECK(strcmp(state.text, "x") == 0);

    /* Scalar input can merge with adjacent text; deletion still removes the
     * resulting complete extended grapheme. */
    tui_input_set(&state, "e");
    tui_event_t acute = scalar_event(0x0301u);
    CHECK(tui_input_handle(&state, &acute));
    CHECK(strcmp(state.text, UTF8_E_ACUTE) == 0);
    CHECK(state.cursor == (int)(sizeof(UTF8_E_ACUTE) - 1u));
    CHECK(tui_input_handle(&state, &backspace));
    CHECK(state.text[0] == '\0');

    tui_input_set(&state, UTF8_WOMAN_TECH "a" UTF8_E_ACUTE);
    state.cursor = 0;
    CHECK(tui_input_handle(&state, &right));
    CHECK(state.cursor == (int)(sizeof(UTF8_WOMAN_TECH) - 1u));
    CHECK(tui_input_handle(&state, &right));
    CHECK(state.cursor == (int)(sizeof(UTF8_WOMAN_TECH) - 1u + 1u));
    CHECK(tui_input_handle(&state, &right));
    CHECK(state.cursor == state.len);

    /* Capacity truncation never copies a partial UTF-8 scalar/grapheme. */
    char capacity_text[258];
    memset(capacity_text, 'a', 254u);
    memcpy(capacity_text + 254u, UTF8_CJK, sizeof(UTF8_CJK));
    tui_input_set(&state, capacity_text);
    CHECK(state.len == 254 && state.cursor == 254);
    CHECK(state.text[254] == '\0');
    tui_input_set(&state, "\xF0\x28\x8C\x28");
    CHECK(state.len == 0 && state.cursor == 0 && state.text[0] == '\0');

    /* Widget cursor placement uses display columns, not UTF-8 bytes. */
    tui_buffer_t *buffer = tui_buffer_create(8, 1);
    CHECK(buffer != NULL);
    tui_widget_input(buffer, 0, 0, 8, UTF8_CJK "a",
                     (int)(sizeof(UTF8_CJK) - 1u), true, NULL);
    CHECK(tui_buffer_at_const(buffer, 1, 0)->span == TUI_CELL_SPAN_WIDE);
    CHECK(tui_buffer_at_const(buffer, 2, 0)->span ==
          TUI_CELL_SPAN_CONTINUATION);
    CHECK(tui_buffer_at_const(buffer, 3, 0)->ch == 0x258fu);
    tui_buffer_destroy(buffer);
    return 0;
}

static int test_textarea_bounds(void) {
    tui_textarea_t *ta = (tui_textarea_t*)calloc(1, sizeof(*ta));
    CHECK(ta != NULL);
    tui_textarea_init(ta);
    ta->focused = true;
    tui_event_t backspace = key_event(TUI_KEY_BACKSPACE);
    tui_event_t del = key_event(TUI_KEY_DELETE);
    tui_event_t left = key_event(TUI_KEY_LEFT);
    tui_event_t right = key_event(TUI_KEY_RIGHT);
    tui_event_t up = key_event(TUI_KEY_UP);
    tui_event_t down = key_event(TUI_KEY_DOWN);

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

    /* Navigation and deletion operate on complete extended graphemes. */
    tui_textarea_set_text(ta, UTF8_WOMAN_TECH "a" UTF8_E_ACUTE);
    CHECK(tui_textarea_handle(ta, &right));
    CHECK(ta->cursor_x == (int)(sizeof(UTF8_WOMAN_TECH) - 1u));
    CHECK(tui_textarea_handle(ta, &right));
    CHECK(ta->cursor_x == (int)(sizeof(UTF8_WOMAN_TECH) - 1u + 1u));
    CHECK(tui_textarea_handle(ta, &right));
    CHECK(ta->cursor_x == (int)strlen(ta->lines[0]));
    CHECK(tui_textarea_handle(ta, &left));
    CHECK(ta->cursor_x == (int)(sizeof(UTF8_WOMAN_TECH) - 1u + 1u));
    CHECK(tui_textarea_handle(ta, &backspace));
    CHECK(strcmp(ta->lines[0], UTF8_WOMAN_TECH UTF8_E_ACUTE) == 0);
    CHECK(ta->cursor_x == (int)(sizeof(UTF8_WOMAN_TECH) - 1u));
    ta->cursor_x = 0;
    CHECK(tui_textarea_handle(ta, &del));
    CHECK(strcmp(ta->lines[0], UTF8_E_ACUTE) == 0);

    /* A combining scalar can merge into its neighbor, and is then deleted
     * together with that complete grapheme. */
    tui_textarea_set_text(ta, "e");
    ta->cursor_x = 1;
    tui_event_t acute = scalar_event(0x0301u);
    CHECK(tui_textarea_handle(ta, &acute));
    CHECK(strcmp(ta->lines[0], UTF8_E_ACUTE) == 0);
    CHECK(ta->cursor_x == (int)(sizeof(UTF8_E_ACUTE) - 1u));
    CHECK(tui_textarea_handle(ta, &backspace));
    CHECK(ta->lines[0][0] == '\0' && ta->cursor_x == 0);

    /* Multi-line paste content uses data events; only newline data splits. */
    tui_event_t pasted = paste_event(0x4e2du);
    pasted.key = TUI_KEY_DELETE;
    CHECK(tui_textarea_handle(ta, &pasted));
    CHECK(strcmp(ta->lines[0], UTF8_CJK) == 0);
    pasted = paste_event('\n');
    CHECK(tui_textarea_handle(ta, &pasted));
    CHECK(ta->line_count == 2 && ta->cursor_y == 1 && ta->cursor_x == 0);

    /* Vertical movement preserves terminal columns rather than byte offsets. */
    tui_textarea_set_text(ta, UTF8_CJK "a\nabc");
    ta->cursor_x = (int)(sizeof(UTF8_CJK) - 1u);
    CHECK(tui_textarea_handle(ta, &down));
    CHECK(ta->cursor_y == 1 && ta->cursor_x == 2);
    CHECK(tui_textarea_handle(ta, &up));
    CHECK(ta->cursor_y == 0 &&
          ta->cursor_x == (int)(sizeof(UTF8_CJK) - 1u));

    /* Setters accept CR/LF variants and never retain partial graphemes. */
    tui_textarea_set_text(ta, "a\r\nb\rc");
    CHECK(ta->line_count == 3);
    CHECK(strcmp(ta->lines[0], "a") == 0);
    CHECK(strcmp(ta->lines[1], "b") == 0);
    CHECK(strcmp(ta->lines[2], "c") == 0);
    char capacity_text[258];
    memset(capacity_text, 'a', 254u);
    memcpy(capacity_text + 254u, UTF8_CJK, sizeof(UTF8_CJK));
    tui_textarea_set_text(ta, capacity_text);
    CHECK(strlen(ta->lines[0]) == 254u);
    CHECK(ta->lines[0][254] == '\0');
    tui_textarea_set_text(ta, "\xF0\x28\x8C\x28");
    CHECK(ta->line_count == 1 && ta->lines[0][0] == '\0');

    /* Drawing uses display columns, including scroll normalization and the
     * continuation half of a reversed wide cursor cell. */
    tui_textarea_set_text(ta, UTF8_CJK "a");
    ta->cursor_x = (int)(sizeof(UTF8_CJK) - 1u);
    ta->scroll_x = 1;
    tui_buffer_t *buffer = tui_buffer_create(6, 1);
    CHECK(buffer != NULL);
    tui_textarea_draw(buffer, 0, 0, 6, 1, ta, NULL);
    CHECK(tui_buffer_at_const(buffer, 0, 0)->span == TUI_CELL_SPAN_WIDE);
    CHECK(tui_buffer_at_const(buffer, 1, 0)->span ==
          TUI_CELL_SPAN_CONTINUATION);
    CHECK((tui_buffer_at_const(buffer, 2, 0)->style & TUI_STYLE_REVERSE) != 0);
    ta->cursor_x = 0;
    tui_textarea_draw(buffer, 0, 0, 6, 1, ta, NULL);
    CHECK((tui_buffer_at_const(buffer, 0, 0)->style & TUI_STYLE_REVERSE) != 0);
    CHECK((tui_buffer_at_const(buffer, 1, 0)->style & TUI_STYLE_REVERSE) != 0);
    tui_buffer_destroy(buffer);

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
    if (test_grapheme_cells()) return 1;
    if (test_terminal_render_contract()) return 1;
    if (test_terminal_renderer_rejects_forged_cells()) return 1;
    if (test_layout_rejects_invalid_graphs()) return 1;
    if (test_input_grapheme_editing()) return 1;
    if (test_textarea_bounds()) return 1;
    puts("Tango native-core tests passed");
    return 0;
}
