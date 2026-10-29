#include "tui_grid_snapshot.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct tui_grid_snapshot {
    int width;
    int height;
    uint64_t revision;
    size_t count;
    tui_cell_t* cells;
};

/* Use the ONE Tango C11 normalization/Unicode policy, not a second parser.
 * Every canonical leading cell must reproduce itself through tui_buffer_set
 * on a two-column scratch grid. A wide cell must also own its exact canonical
 * continuation cell; orphan continuations or forged span/attributes fail.
 */
static bool valid_canonical_grid(const tui_cell_t* cells, int width, int height) {
    const tui_cell_t empty = TUI_CELL_DEFAULT;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const tui_cell_t* cell = &cells[(size_t)y * (size_t)width + (size_t)x];
            if (cell->span == TUI_CELL_SPAN_CONTINUATION ||
                cell->span > TUI_CELL_SPAN_CONTINUATION ||
                cell->grapheme_len > TUI_CELL_GRAPHEME_CAPACITY) return false;

            tui_cell_t scratch_cells[2] = {empty, empty};
            tui_buffer_t scratch = {0};
            scratch.width = 2;
            scratch.height = 1;
            scratch.cells = scratch_cells;
            scratch.capacity = 2;
            tui_buffer_set(&scratch, 0, 0, *cell);

            if (!tui_cell_eq(&scratch_cells[0], cell)) return false;
            if (cell->span == TUI_CELL_SPAN_WIDE) {
                if (x + 1 >= width) return false;
                const tui_cell_t* continuation =
                    &cells[(size_t)y * (size_t)width + (size_t)x + 1u];
                if (!tui_cell_eq(&scratch_cells[1], continuation)) return false;
                ++x;
            }
        }
    }
    return true;
}

tui_grid_snapshot_status_t tui_grid_snapshot_capture(
    const tui_buffer_t* source, uint64_t source_revision, size_t max_cells,
    size_t max_bytes, tui_grid_snapshot_t** out) {
    if (!source || !out || *out || source_revision == 0u ||
        max_cells == 0u || max_bytes == 0u) {
        return TUI_GRID_SNAPSHOT_INVALID_ARGUMENT;
    }
    if (source->width <= 0 || source->height <= 0 || !source->cells) {
        return TUI_GRID_SNAPSHOT_INVALID_SOURCE;
    }

    const size_t width = (size_t)source->width;
    const size_t height = (size_t)source->height;
    if (width > SIZE_MAX / height) {
        return TUI_GRID_SNAPSHOT_RESOURCE_LIMIT;
    }
    const size_t count = width * height;
    if (count > source->capacity) {
        return TUI_GRID_SNAPSHOT_INVALID_SOURCE;
    }
    if (count > max_cells ||
        count > (SIZE_MAX - sizeof(tui_grid_snapshot_t)) / sizeof(tui_cell_t)) {
        return TUI_GRID_SNAPSHOT_RESOURCE_LIMIT;
    }
    const size_t storage_bytes = count * sizeof(tui_cell_t);
    if (sizeof(tui_grid_snapshot_t) + storage_bytes > max_bytes) {
        return TUI_GRID_SNAPSHOT_RESOURCE_LIMIT;
    }

    tui_grid_snapshot_t* candidate =
        (tui_grid_snapshot_t*)malloc(sizeof(tui_grid_snapshot_t));
    if (!candidate) return TUI_GRID_SNAPSHOT_ALLOCATION_FAILED;
    candidate->cells = (tui_cell_t*)malloc(storage_bytes);
    if (!candidate->cells) {
        free(candidate);
        return TUI_GRID_SNAPSHOT_ALLOCATION_FAILED;
    }
    candidate->width = source->width;
    candidate->height = source->height;
    candidate->count = count;
    candidate->revision = source_revision;
    memcpy(candidate->cells, source->cells, storage_bytes);

    if (!valid_canonical_grid(candidate->cells, candidate->width,
                              candidate->height)) {
        tui_grid_snapshot_destroy(candidate);
        return TUI_GRID_SNAPSHOT_INVALID_CELL;
    }
    *out = candidate;
    return TUI_GRID_SNAPSHOT_OK;
}

void tui_grid_snapshot_destroy(tui_grid_snapshot_t* snapshot) {
    if (!snapshot) return;
    free(snapshot->cells);
    free(snapshot);
}
int tui_grid_snapshot_width(const tui_grid_snapshot_t* snapshot) {
    return snapshot ? snapshot->width : 0;
}
int tui_grid_snapshot_height(const tui_grid_snapshot_t* snapshot) {
    return snapshot ? snapshot->height : 0;
}
uint64_t tui_grid_snapshot_revision(const tui_grid_snapshot_t* snapshot) {
    return snapshot ? snapshot->revision : 0u;
}
size_t tui_grid_snapshot_count(const tui_grid_snapshot_t* snapshot) {
    return snapshot ? snapshot->count : 0u;
}
const tui_cell_t* tui_grid_snapshot_cells(const tui_grid_snapshot_t* snapshot) {
    return snapshot ? snapshot->cells : NULL;
}
const tui_cell_t* tui_grid_snapshot_cell_at(
    const tui_grid_snapshot_t* snapshot, int x, int y) {
    if (!snapshot || x < 0 || y < 0 ||
        x >= snapshot->width || y >= snapshot->height) return NULL;
    return &snapshot->cells[(size_t)y * (size_t)snapshot->width + (size_t)x];
}
