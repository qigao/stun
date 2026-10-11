/* Shared C11 UI semantic projection. This is not a new CSS/Markdown parser. */
#ifndef STUN_UI_CORE_H
#define STUN_UI_CORE_H

#include <cmeta/object.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum stun_ui_status {
    STUN_UI_OK = 0,
    STUN_UI_INVALID_ARGUMENT = -1,
    STUN_UI_CAPACITY_EXCEEDED = -2
} stun_ui_status;

/* Owned by the application. Reflection borrows this live native record.
 * Mutations must go through the application owner thread, using explicit
 * revision notifications. Reflection does not observe changes itself. */
typedef struct stun_ui_runtime_state {
    float viewport_width;
    float viewport_height;
    int scroll_row;
    int focused_index;
    uint64_t revision;
} stun_ui_runtime_state;

stun_ui_status stun_ui_runtime_state_init(
    stun_ui_runtime_state *state, float width, float height);
stun_ui_status stun_ui_runtime_state_set_viewport(
    stun_ui_runtime_state *state, float width, float height);
stun_ui_status stun_ui_runtime_state_set_scroll(
    stun_ui_runtime_state *state, int scroll_row);
stun_ui_status stun_ui_runtime_state_set_focus(
    stun_ui_runtime_state *state, int focused_index);

/* Canonical CMeta read-only view over the four primitive native fields.
 * The revision remains an application-owned explicit invalidation token.
 * The returned descriptor has static lifetime, but does not own state. */
const cmeta_data_desc *stun_ui_runtime_state_data(void);
cmeta_status stun_ui_runtime_state_borrow(
    stun_ui_runtime_state *state, cmeta_object_ref *out);

/* Immutable borrowed snapshots from the one active layout engine. Values
 * are float-space positions, not terminal cells or font glyph advances. */
#define STUN_UI_PARENT_NONE UINT32_MAX

typedef struct stun_ui_rect {
    uint64_t stable_id;
    uint32_t parent_index;
    float x, y, width, height;
} stun_ui_rect;

typedef struct stun_ui_rect_view {
    const stun_ui_rect *nodes;
    size_t count;
} stun_ui_rect_view;

/* No allocation. Caller limits the worst-case quadratic duplicate-ID check.
 * A node's parent must precede it in the immutable preorder projection.
 * Rectangles may legitimately extend outside their parent (scroll/clip).
 */
stun_ui_status stun_ui_rect_view_validate(
    const stun_ui_rect_view *view, size_t max_nodes);

/* Markdown syntax belongs to one existing parser. This is only its
 * renderer-independent C11 semantic projection; byte ranges borrow source.
 * The source and blocks must outlive callbacks or be retained/copied by the
 * owner before a stream operation escapes the current call. */
typedef enum stun_ui_block_kind {
    STUN_UI_BLOCK_DOCUMENT = 1,
    STUN_UI_BLOCK_PARAGRAPH,
    STUN_UI_BLOCK_HEADING,
    STUN_UI_BLOCK_TEXT,
    STUN_UI_BLOCK_CODE_FENCE,
    STUN_UI_BLOCK_CODE_SPAN,
    STUN_UI_BLOCK_LIST,
    STUN_UI_BLOCK_LIST_ITEM,
    STUN_UI_BLOCK_QUOTE,
    STUN_UI_BLOCK_TABLE,
    STUN_UI_BLOCK_TABLE_ROW,
    STUN_UI_BLOCK_TABLE_CELL
} stun_ui_block_kind;

typedef struct stun_ui_block {
    uint64_t stable_id;
    uint32_t parent_index;
    stun_ui_block_kind kind;
    uint32_t heading_level;
    size_t byte_begin;
    size_t byte_end;
} stun_ui_block;

typedef struct stun_ui_document_view {
    const char *source;
    size_t source_bytes;
    const stun_ui_block *blocks;
    size_t block_count;
} stun_ui_document_view;

stun_ui_status stun_ui_document_validate(
    const stun_ui_document_view *view, size_t max_blocks);

typedef stun_ui_status (*stun_ui_document_visit_fn)(
    const stun_ui_block *block, const char *source_span,
    size_t span_bytes, void *context);

stun_ui_status stun_ui_document_visit(
    const stun_ui_document_view *view, size_t max_blocks,
    stun_ui_document_visit_fn visit, void *context);

#ifdef __cplusplus
}
#endif

#endif /* STUN_UI_CORE_H */
