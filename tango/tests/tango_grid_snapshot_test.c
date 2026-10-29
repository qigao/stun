/* Tango's canonical cell-copy qualification: no TTY and no GPU needed. */
#include "tui_grid_snapshot.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static size_t exact_budget(size_t cells) {
    /* Avoid relying on opaque snapshot layout: this is only a generous
     * positive bound, while negative tests exercise the exact reject path. */
    return cells * sizeof(tui_cell_t) + 256u;
}

static int test_owned_unicode_grid(void) {
    tui_buffer_t* source = tui_buffer_create(5, 2);
    CHECK(source != NULL);
    CHECK(tui_buffer_set_grapheme(source, 1, 0, "\xE4\xB8\xAD", 3,
                                  TUI_YELLOW, TUI_BLUE, TUI_STYLE_BOLD));
    CHECK(tui_buffer_set_grapheme(source, 3, 0, "e\xCC\x81", 3,
                                  TUI_GREEN, TUI_BLACK, TUI_STYLE_UNDERLINE));
    tui_buffer_set(source, 0, 1, tui_cell('X', TUI_RED, TUI_BLUE));
    CHECK(tui_buffer_at_const(source, 1, 0)->span == TUI_CELL_SPAN_WIDE);
    CHECK(tui_buffer_at_const(source, 2, 0)->span == TUI_CELL_SPAN_CONTINUATION);

    const int old_min_x = source->dirty_min_x;
    const int old_max_x = source->dirty_max_x;
    tui_grid_snapshot_t* frame = NULL;
    CHECK(tui_grid_snapshot_capture(source, 73, 10, exact_budget(10), &frame)
          == TUI_GRID_SNAPSHOT_OK);
    CHECK(frame != NULL);
    CHECK(tui_grid_snapshot_width(frame) == 5);
    CHECK(tui_grid_snapshot_height(frame) == 2);
    CHECK(tui_grid_snapshot_count(frame) == 10u);
    CHECK(tui_grid_snapshot_revision(frame) == 73u);
    CHECK(source->dirty_min_x == old_min_x &&
          source->dirty_max_x == old_max_x);
    CHECK(tui_grid_snapshot_cell_at(frame, -1, 0) == NULL);
    CHECK(tui_grid_snapshot_cell_at(frame, 5, 0) == NULL);
    CHECK(tui_grid_snapshot_cell_at(frame, 1, 2) == NULL);
    CHECK(tui_grid_snapshot_cell_at(frame, 1, 0)->span == TUI_CELL_SPAN_WIDE);
    CHECK(tui_grid_snapshot_cell_at(frame, 2, 0)->span == TUI_CELL_SPAN_CONTINUATION);
    CHECK(tui_grid_snapshot_cell_at(frame, 3, 0)->grapheme_len == 3);
    CHECK(memcmp(tui_grid_snapshot_cell_at(frame, 3, 0)->grapheme,
                 "e\xCC\x81", 3) == 0);

    /* The source may be changed, resized, and retired after prepare. */
    tui_buffer_resize(source, 2, 1);
    tui_buffer_destroy(source);
    CHECK(tui_grid_snapshot_cell_at(frame, 1, 0)->ch == 0x4e2du);
    CHECK(tui_grid_snapshot_cell_at(frame, 2, 0)->span == TUI_CELL_SPAN_CONTINUATION);
    CHECK(tui_grid_snapshot_cell_at(frame, 0, 1)->ch == 'X');
    CHECK(tui_grid_snapshot_cells(frame) ==
          tui_grid_snapshot_cell_at(frame, 0, 0));
    tui_grid_snapshot_destroy(frame);
    tui_grid_snapshot_destroy(NULL);
    return 0;
}

static int test_reject_forged_cell_without_publication(void) {
    tui_buffer_t* source = tui_buffer_create(3, 1);
    CHECK(source != NULL);
    tui_grid_snapshot_t* frame = NULL;
    const tui_cell_t baseline = *tui_buffer_at_const(source, 0, 0);

    /* A public mutable buffer may be forged by legacy consumers. A presenter
     * must validate every cell and continuation before allocating GPU work. */
    tui_buffer_at(source, 0, 0)->span = TUI_CELL_SPAN_CONTINUATION;
    CHECK(tui_grid_snapshot_capture(source, 1, 3, exact_budget(3), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_CELL);
    CHECK(frame == NULL);
    CHECK(tui_buffer_at_const(source, 0, 0)->span == TUI_CELL_SPAN_CONTINUATION);
    *tui_buffer_at(source, 0, 0) = baseline;

    tui_buffer_at(source, 0, 0)->ch = 0x1bu;
    CHECK(tui_grid_snapshot_capture(source, 1, 3, exact_budget(3), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_CELL);
    CHECK(frame == NULL);
    *tui_buffer_at(source, 0, 0) = baseline;

    tui_buffer_at(source, 1, 0)->grapheme_len =
        TUI_CELL_GRAPHEME_CAPACITY + 1u;
    CHECK(tui_grid_snapshot_capture(source, 1, 3, exact_budget(3), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_CELL);
    CHECK(frame == NULL);
    *tui_buffer_at(source, 1, 0) = baseline;

    tui_buffer_at(source, 1, 0)->grapheme_len = 3u;
    memcpy(tui_buffer_at(source, 1, 0)->grapheme, "\xFF\xFF\xFF", 3);
    CHECK(tui_grid_snapshot_capture(source, 1, 3, exact_budget(3), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_CELL);
    CHECK(frame == NULL);
    *tui_buffer_at(source, 1, 0) = baseline;

    /* Width-2 scalar in the last column may not bleed into the next row. */
    tui_buffer_at(source, 2, 0)->ch = 0x4e2du;
    tui_buffer_at(source, 2, 0)->span = TUI_CELL_SPAN_WIDE;
    CHECK(tui_grid_snapshot_capture(source, 1, 3, exact_budget(3), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_CELL);
    CHECK(frame == NULL);
    *tui_buffer_at(source, 2, 0) = baseline;

    /* A canonical leading cell needs a matching continuation; a fake
     * continuation's colors/styles must not alias the visible leading cell. */
    tui_buffer_set(source, 0, 0, tui_cell(0x4e2d, TUI_YELLOW, TUI_BLUE));
    tui_buffer_at(source, 1, 0)->style = TUI_STYLE_BOLD;
    CHECK(tui_grid_snapshot_capture(source, 1, 3, exact_budget(3), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_CELL);
    CHECK(frame == NULL);

    tui_buffer_destroy(source);
    return 0;
}

static int test_bounds_and_failure_atomicity(void) {
    tui_buffer_t* source = tui_buffer_create(2, 2);
    CHECK(source != NULL);
    tui_grid_snapshot_t* frame = NULL;
    CHECK(tui_grid_snapshot_capture(source, 0, 4, exact_budget(4), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_ARGUMENT);
    CHECK(tui_grid_snapshot_capture(source, 1, 0, exact_budget(4), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_ARGUMENT);
    CHECK(tui_grid_snapshot_capture(source, 1, 4, 0, &frame)
          == TUI_GRID_SNAPSHOT_INVALID_ARGUMENT);
    CHECK(tui_grid_snapshot_capture(source, 1, 3, exact_budget(4), &frame)
          == TUI_GRID_SNAPSHOT_RESOURCE_LIMIT);
    CHECK(tui_grid_snapshot_capture(source, 1, 4, 4, &frame)
          == TUI_GRID_SNAPSHOT_RESOURCE_LIMIT);
    CHECK(frame == NULL);
    const size_t capacity = source->capacity;
    source->capacity = 1u;
    CHECK(tui_grid_snapshot_capture(source, 1, 4, exact_budget(4), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_SOURCE);
    source->capacity = capacity;

    CHECK(tui_grid_snapshot_capture(source, 1, 4, exact_budget(4), &frame)
          == TUI_GRID_SNAPSHOT_OK);
    CHECK(frame != NULL);
    tui_grid_snapshot_t* const original = frame;
    CHECK(tui_grid_snapshot_capture(source, 2, 4, exact_budget(4), &frame)
          == TUI_GRID_SNAPSHOT_INVALID_ARGUMENT);
    CHECK(frame == original);
    CHECK(tui_grid_snapshot_revision(original) == 1u);

    tui_buffer_t forged = *source;
    forged.width = -1;
    tui_grid_snapshot_t* none = NULL;
    CHECK(tui_grid_snapshot_capture(&forged, 2, 4, exact_budget(4), &none)
          == TUI_GRID_SNAPSHOT_INVALID_SOURCE);
    forged = *source;
    forged.height = 0;
    CHECK(tui_grid_snapshot_capture(&forged, 2, 4, exact_budget(4), &none)
          == TUI_GRID_SNAPSHOT_INVALID_SOURCE);
    CHECK(none == NULL);
    CHECK(tui_grid_snapshot_width(NULL) == 0);
    CHECK(tui_grid_snapshot_height(NULL) == 0);
    CHECK(tui_grid_snapshot_revision(NULL) == 0);
    CHECK(tui_grid_snapshot_count(NULL) == 0);
    CHECK(tui_grid_snapshot_cells(NULL) == NULL);
    CHECK(tui_grid_snapshot_cell_at(NULL, 0, 0) == NULL);

    tui_buffer_destroy(source);
    CHECK(tui_grid_snapshot_cell_at(frame, 1, 1)->ch == ' ');
    tui_grid_snapshot_destroy(frame);
    return 0;
}

int main(void) {
    if (test_owned_unicode_grid() != 0) return 1;
    if (test_reject_forged_cell_without_publication() != 0) return 1;
    if (test_bounds_and_failure_atomicity() != 0) return 1;
    return 0;
}
