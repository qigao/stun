/*
 * Tango C11 - immutable, bounded cell-grid snapshot for explicit presenters.
 * The TTY renderer remains independent of GL. Capture on the UI owner after
 * the source frame has settled; no concurrent mutation is permitted.
 */
#ifndef TANGO_TUI_GRID_SNAPSHOT_H
#define TANGO_TUI_GRID_SNAPSHOT_H

#include "tui.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tui_grid_snapshot tui_grid_snapshot_t;

typedef enum {
    TUI_GRID_SNAPSHOT_OK = 0,
    TUI_GRID_SNAPSHOT_INVALID_ARGUMENT,
    TUI_GRID_SNAPSHOT_INVALID_SOURCE,
    TUI_GRID_SNAPSHOT_RESOURCE_LIMIT,
    TUI_GRID_SNAPSHOT_INVALID_CELL,
    TUI_GRID_SNAPSHOT_ALLOCATION_FAILED
} tui_grid_snapshot_status_t;

/*
 * Prepare an independent, immutable deep copy with its caller-owned,
 * nonzero source revision. The caller explicitly bounds *both* cell count
 * and total requested allocation bytes (snapshot struct + cell storage).
 *
 * A successful capture writes *out; *out MUST initially be NULL. Failure
 * never publishes a partial snapshot, changes *out, or clears source dirty
 * state. The snapshot does not borrow the source or its backing allocation.
 *
 * Capture is UI-owner synchronous; the caller must exclude concurrent
 * writers and identify any resize/refresh with its own source revision.
 * This is not an atomic multi-thread read of an arbitrary mutable buffer.
 */
tui_grid_snapshot_status_t tui_grid_snapshot_capture(
    const tui_buffer_t* source,
    uint64_t source_revision,
    size_t max_cells,
    size_t max_bytes,
    tui_grid_snapshot_t** out);

void tui_grid_snapshot_destroy(tui_grid_snapshot_t* snapshot);
int tui_grid_snapshot_width(const tui_grid_snapshot_t* snapshot);
int tui_grid_snapshot_height(const tui_grid_snapshot_t* snapshot);
uint64_t tui_grid_snapshot_revision(const tui_grid_snapshot_t* snapshot);
size_t tui_grid_snapshot_count(const tui_grid_snapshot_t* snapshot);

/* A borrowed read-only view; invalidated only by snapshot destruction. */
const tui_cell_t* tui_grid_snapshot_cells(const tui_grid_snapshot_t* snapshot);
const tui_cell_t* tui_grid_snapshot_cell_at(
    const tui_grid_snapshot_t* snapshot, int x, int y);

#ifdef __cplusplus
}
#endif
#endif /* TANGO_TUI_GRID_SNAPSHOT_H */
