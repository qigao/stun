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
    STUN_UI_CAPACITY_EXCEEDED = -2,
    STUN_UI_OUT_OF_MEMORY = -3
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
    STUN_UI_BLOCK_TABLE_CELL,
    STUN_UI_BLOCK_ORDERED_LIST,
    STUN_UI_BLOCK_EMPHASIS,
    STUN_UI_BLOCK_STRONG,
    STUN_UI_BLOCK_LINK,
    STUN_UI_BLOCK_IMAGE,
    STUN_UI_BLOCK_STRIKETHROUGH,
    STUN_UI_BLOCK_UNDERLINE,
    STUN_UI_BLOCK_RULE,
    STUN_UI_BLOCK_ENTITY,
    STUN_UI_BLOCK_LINE_BREAK,
    STUN_UI_BLOCK_TASK_ITEM,
    STUN_UI_BLOCK_CHECKBOX,
    STUN_UI_BLOCK_MATH,
    STUN_UI_BLOCK_MATH_BLOCK,
    STUN_UI_BLOCK_HIGHLIGHT,
    STUN_UI_BLOCK_SUPERSCRIPT,
    STUN_UI_BLOCK_SUBSCRIPT,
    STUN_UI_BLOCK_FOOTNOTE,
    STUN_UI_BLOCK_FOOTNOTE_DEF,
    STUN_UI_BLOCK_DEFINITION_LIST,
    STUN_UI_BLOCK_DEFINITION_TERM,
    STUN_UI_BLOCK_DEFINITION_DESC,
    STUN_UI_BLOCK_DIAGRAM,
    /* Appended: keep all existing public enum values stable. */
    STUN_UI_BLOCK_CODE_INDENTED,
    /* Distinct from the existing SOFT STUN_UI_BLOCK_LINE_BREAK node. */
    STUN_UI_BLOCK_HARD_LINE_BREAK,
    /* Optional decoded title child of STUN_UI_BLOCK_LINK; metadata-owned. */
    STUN_UI_BLOCK_LINK_TITLE,
    /* Decoded IMAGE title, always the first direct IMAGE child. */
    STUN_UI_BLOCK_IMAGE_TITLE
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

/* RichDoc bridge: the existing Markdown AST does not record raw source offsets.
 * Therefore text.source is an OWNED-BY-PROJECTOR concatenation of decoded
 * semantic text nodes; byte_begin/end do NOT index into raw Markdown. Block
 * kinds retain structural semantics. Node metadata (fence language, link or
 * image target, footnote label) is borrowed separately from the projector.
 * The raw Markdown is preserved for explicit diagnostics only.
 *
 * Lifetime: all pointers are borrowed for the call; the producer must keep
 * source, metadata and both arrays alive until the last consumer finishes.
 * A projector re-build, move or release invalidates all these views. Stable
 * node IDs are deterministic only within a snapshot; streaming reconciliation
 * must use a separate identity policy rather than relying on ordinal IDs.
 */
/* Owned eight-byte metadata payload for list semantics. */
typedef struct stun_ui_list_semantics {
    uint32_t start; /* 0 for bullet, 0..999999999 for ordered */
    uint8_t marker; /* '-', '+', '*', '.', ')' */
    uint8_t loose; /* 0 tight; 1 loose */
    uint8_t reserved[2];
} stun_ui_list_semantics;

/* Hard-break source origin is owned as four metadata bytes on a
 * STUN_UI_BLOCK_HARD_LINE_BREAK node, never re-read from decoded text or
 * inferred by an HTML consumer. Soft breaks carry no origin metadata.
 * Additional origins require explicit admission and validation. */
typedef enum stun_ui_line_break_origin {
    STUN_UI_LINE_BREAK_TRAILING_SPACES = 1,
    STUN_UI_LINE_BREAK_BACKSLASH = 2
} stun_ui_line_break_origin;
typedef struct stun_ui_line_break_semantics {
    uint8_t origin; /* stun_ui_line_break_origin */
    uint8_t reserved[3];
} stun_ui_line_break_semantics;

typedef struct stun_ui_block_metadata {
    size_t byte_begin;
    size_t byte_end;
} stun_ui_block_metadata;

typedef struct stun_ui_rich_document_view {
    stun_ui_document_view document;
    const char *metadata;
    size_t metadata_bytes;
    const stun_ui_block_metadata *block_metadata;
    size_t block_metadata_count;
    const char *raw_markdown;
    size_t raw_markdown_bytes;
} stun_ui_rich_document_view;

stun_ui_status stun_ui_rich_document_validate(
    const stun_ui_rich_document_view *view, size_t max_blocks);

stun_ui_status stun_ui_rich_document_metadata(
    const stun_ui_rich_document_view *view, size_t node_index,
    const char **out_bytes, size_t *out_length);

#ifdef __cplusplus
}
#endif

#endif /* STUN_UI_CORE_H */
