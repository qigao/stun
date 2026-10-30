/* Shared C11 UI semantic projection. This is not a new CSS/Markdown parser. */
#ifndef STUN_UI_CORE_H
#define STUN_UI_CORE_H

#include <cmeta/object.h>
#include <salts/markdown.h>
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

/* Markdown syntax and immutable RichDoc now belong exclusively to
 * Salts::MarkdownParser. Alias the actual public C types: no duplicate AST,
 * no layout casts, no second parser and no Stun-owned parser allocations.
 * The numeric Stun enum spellings are retained for existing consumers.
 */
typedef salts_md_block_kind stun_ui_block_kind;
typedef salts_md_block stun_ui_block;
typedef salts_md_document_view stun_ui_document_view;
typedef salts_md_document_visit_fn salts_md_visit_fn;
typedef salts_md_list_semantics stun_ui_list_semantics;
typedef salts_md_table_alignment stun_ui_table_alignment;
typedef salts_md_line_break_origin stun_ui_line_break_origin;
typedef salts_md_line_break_semantics stun_ui_line_break_semantics;
typedef salts_md_block_metadata stun_ui_block_metadata;
typedef salts_md_rich_document_view stun_ui_rich_document_view;
#define STUN_UI_PARENT_NONE SALTS_MD_PARENT_NONE
#define STUN_UI_BLOCK_DOCUMENT SALTS_MD_BLOCK_DOCUMENT
#define STUN_UI_BLOCK_PARAGRAPH SALTS_MD_BLOCK_PARAGRAPH
#define STUN_UI_BLOCK_HEADING SALTS_MD_BLOCK_HEADING
#define STUN_UI_BLOCK_TEXT SALTS_MD_BLOCK_TEXT
#define STUN_UI_BLOCK_CODE_FENCE SALTS_MD_BLOCK_CODE_FENCE
#define STUN_UI_BLOCK_CODE_SPAN SALTS_MD_BLOCK_CODE_SPAN
#define STUN_UI_BLOCK_LIST SALTS_MD_BLOCK_LIST
#define STUN_UI_BLOCK_LIST_ITEM SALTS_MD_BLOCK_LIST_ITEM
#define STUN_UI_BLOCK_QUOTE SALTS_MD_BLOCK_QUOTE
#define STUN_UI_BLOCK_TABLE SALTS_MD_BLOCK_TABLE
#define STUN_UI_BLOCK_TABLE_ROW SALTS_MD_BLOCK_TABLE_ROW
#define STUN_UI_BLOCK_TABLE_CELL SALTS_MD_BLOCK_TABLE_CELL
#define STUN_UI_BLOCK_ORDERED_LIST SALTS_MD_BLOCK_ORDERED_LIST
#define STUN_UI_BLOCK_EMPHASIS SALTS_MD_BLOCK_EMPHASIS
#define STUN_UI_BLOCK_STRONG SALTS_MD_BLOCK_STRONG
#define STUN_UI_BLOCK_LINK SALTS_MD_BLOCK_LINK
#define STUN_UI_BLOCK_IMAGE SALTS_MD_BLOCK_IMAGE
#define STUN_UI_BLOCK_STRIKETHROUGH SALTS_MD_BLOCK_STRIKETHROUGH
#define STUN_UI_BLOCK_UNDERLINE SALTS_MD_BLOCK_UNDERLINE
#define STUN_UI_BLOCK_RULE SALTS_MD_BLOCK_RULE
#define STUN_UI_BLOCK_ENTITY SALTS_MD_BLOCK_ENTITY
#define STUN_UI_BLOCK_LINE_BREAK SALTS_MD_BLOCK_LINE_BREAK
#define STUN_UI_BLOCK_TASK_ITEM SALTS_MD_BLOCK_TASK_ITEM
#define STUN_UI_BLOCK_CHECKBOX SALTS_MD_BLOCK_CHECKBOX
#define STUN_UI_BLOCK_MATH SALTS_MD_BLOCK_MATH
#define STUN_UI_BLOCK_MATH_BLOCK SALTS_MD_BLOCK_MATH_BLOCK
#define STUN_UI_BLOCK_HIGHLIGHT SALTS_MD_BLOCK_HIGHLIGHT
#define STUN_UI_BLOCK_SUPERSCRIPT SALTS_MD_BLOCK_SUPERSCRIPT
#define STUN_UI_BLOCK_SUBSCRIPT SALTS_MD_BLOCK_SUBSCRIPT
#define STUN_UI_BLOCK_FOOTNOTE SALTS_MD_BLOCK_FOOTNOTE
#define STUN_UI_BLOCK_FOOTNOTE_DEF SALTS_MD_BLOCK_FOOTNOTE_DEF
#define STUN_UI_BLOCK_DEFINITION_LIST SALTS_MD_BLOCK_DEFINITION_LIST
#define STUN_UI_BLOCK_DEFINITION_TERM SALTS_MD_BLOCK_DEFINITION_TERM
#define STUN_UI_BLOCK_DEFINITION_DESC SALTS_MD_BLOCK_DEFINITION_DESC
#define STUN_UI_BLOCK_DIAGRAM SALTS_MD_BLOCK_DIAGRAM
#define STUN_UI_BLOCK_CODE_INDENTED SALTS_MD_BLOCK_CODE_INDENTED
#define STUN_UI_BLOCK_HARD_LINE_BREAK SALTS_MD_BLOCK_HARD_LINE_BREAK
#define STUN_UI_BLOCK_LINK_TITLE SALTS_MD_BLOCK_LINK_TITLE
#define STUN_UI_BLOCK_IMAGE_TITLE SALTS_MD_BLOCK_IMAGE_TITLE
#define STUN_UI_BLOCK_RAW_HTML_INLINE SALTS_MD_BLOCK_RAW_HTML_INLINE
#define STUN_UI_BLOCK_RAW_HTML_BLOCK SALTS_MD_BLOCK_RAW_HTML_BLOCK
#define STUN_UI_BLOCK_URI_AUTOLINK SALTS_MD_BLOCK_URI_AUTOLINK
#define STUN_UI_TABLE_ALIGN_NONE SALTS_MD_TABLE_ALIGN_NONE
#define STUN_UI_TABLE_ALIGN_LEFT SALTS_MD_TABLE_ALIGN_LEFT
#define STUN_UI_TABLE_ALIGN_CENTER SALTS_MD_TABLE_ALIGN_CENTER
#define STUN_UI_TABLE_ALIGN_RIGHT SALTS_MD_TABLE_ALIGN_RIGHT
#define STUN_UI_LINE_BREAK_TRAILING_SPACES SALTS_MD_LINE_BREAK_TRAILING_SPACES
#define STUN_UI_LINE_BREAK_BACKSLASH SALTS_MD_LINE_BREAK_BACKSLASH

stun_ui_status stun_ui_document_validate(const stun_ui_document_view *view,
                                         size_t max_blocks);

typedef stun_ui_status (*stun_ui_document_visit_fn)(
    const stun_ui_block *block, const char *source_span, size_t span_bytes,
    void *context);

stun_ui_status stun_ui_document_visit(const stun_ui_document_view *view,
                                      size_t max_blocks,
                                      stun_ui_document_visit_fn visit,
                                      void *context);
stun_ui_status stun_ui_rich_document_validate(
    const stun_ui_rich_document_view *view, size_t max_blocks);
stun_ui_status stun_ui_rich_document_metadata(
    const stun_ui_rich_document_view *view, size_t node_index,
    const char **out_bytes, size_t *out_length);

#ifdef __cplusplus
}
#endif

#endif /* STUN_UI_CORE_H */