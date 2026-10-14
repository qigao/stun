#include <stun/ui_core.h>
#include <cmeta/data_reflect.h>

#include <limits.h>
#include <math.h>

cmeta_reflect_data(stun_ui_runtime_state, "stun.ui.runtime_state",
    cmeta_field(float, viewport_width)
    cmeta_field(float, viewport_height)
    cmeta_field(int, scroll_row)
    cmeta_field(int, focused_index)
);

const cmeta_data_desc *stun_ui_runtime_state_data(void) {
    return cmeta_reflected_data(stun_ui_runtime_state);
}

cmeta_status stun_ui_runtime_state_borrow(
    stun_ui_runtime_state *state, cmeta_object_ref *out) {
    if (!state || !out) return CMETA_INVALID_ARGUMENT;
    return cmeta_object_borrow(out, state,
                               stun_ui_runtime_state_data(), NULL);
}

static bool valid_viewport(float width, float height) {
    return isfinite(width) && isfinite(height) && width > 0 && height > 0;
}

stun_ui_status stun_ui_runtime_state_init(
    stun_ui_runtime_state *state, float width, float height) {
    if (!state || !valid_viewport(width, height))
        return STUN_UI_INVALID_ARGUMENT;
    *state = (stun_ui_runtime_state){
        .viewport_width = width,
        .viewport_height = height,
        .scroll_row = 0,
        .focused_index = -1,
        .revision = 1u
    };
    return STUN_UI_OK;
}

static stun_ui_status advance_revision(stun_ui_runtime_state *state) {
    if (state->revision == UINT64_MAX) return STUN_UI_CAPACITY_EXCEEDED;
    ++state->revision;
    return STUN_UI_OK;
}

stun_ui_status stun_ui_runtime_state_set_viewport(
    stun_ui_runtime_state *state, float width, float height) {
    if (!state || !valid_viewport(width, height))
        return STUN_UI_INVALID_ARGUMENT;
    if (state->viewport_width == width && state->viewport_height == height)
        return STUN_UI_OK;
    const stun_ui_status status = advance_revision(state);
    if (status != STUN_UI_OK) return status;
    state->viewport_width = width;
    state->viewport_height = height;
    return STUN_UI_OK;
}

stun_ui_status stun_ui_runtime_state_set_scroll(
    stun_ui_runtime_state *state, int scroll_row) {
    if (!state || scroll_row < 0) return STUN_UI_INVALID_ARGUMENT;
    if (state->scroll_row == scroll_row) return STUN_UI_OK;
    const stun_ui_status status = advance_revision(state);
    if (status != STUN_UI_OK) return status;
    state->scroll_row = scroll_row;
    return STUN_UI_OK;
}

stun_ui_status stun_ui_runtime_state_set_focus(
    stun_ui_runtime_state *state, int focused_index) {
    if (!state || focused_index < -1) return STUN_UI_INVALID_ARGUMENT;
    if (state->focused_index == focused_index) return STUN_UI_OK;
    const stun_ui_status status = advance_revision(state);
    if (status != STUN_UI_OK) return status;
    state->focused_index = focused_index;
    return STUN_UI_OK;
}

stun_ui_status stun_ui_rect_view_validate(
    const stun_ui_rect_view *view, size_t max_nodes) {
    if (!view || !view->nodes || !view->count)
        return STUN_UI_INVALID_ARGUMENT;
    if (!max_nodes || view->count > max_nodes || view->count > UINT32_MAX)
        return STUN_UI_CAPACITY_EXCEEDED;
    for (size_t i = 0; i < view->count; ++i) {
        const stun_ui_rect *node = &view->nodes[i];
        if (!node->stable_id ||
            (i == 0 ? node->parent_index != STUN_UI_PARENT_NONE
                    : node->parent_index >= i) ||
            !isfinite(node->x) || !isfinite(node->y) ||
            !isfinite(node->width) || !isfinite(node->height) ||
            node->width < 0 || node->height < 0)
            return STUN_UI_INVALID_ARGUMENT;
        for (size_t j = 0; j < i; ++j) {
            if (view->nodes[j].stable_id == node->stable_id)
                return STUN_UI_INVALID_ARGUMENT;
        }
    }
    return STUN_UI_OK;
}

static bool valid_block_kind(stun_ui_block_kind kind) {
    return kind >= STUN_UI_BLOCK_DOCUMENT &&
           kind <= STUN_UI_BLOCK_URI_AUTOLINK;
}

stun_ui_status stun_ui_document_validate(
    const stun_ui_document_view *view, size_t max_blocks) {
    if (!view || !view->blocks || !view->block_count ||
        (!view->source && view->source_bytes))
        return STUN_UI_INVALID_ARGUMENT;
    if (!max_blocks || view->block_count > max_blocks ||
        view->block_count > UINT32_MAX)
        return STUN_UI_CAPACITY_EXCEEDED;
    for (size_t i = 0; i < view->block_count; ++i) {
        const stun_ui_block *block = &view->blocks[i];
        if (!block->stable_id || !valid_block_kind(block->kind) ||
            (i == 0 ? block->parent_index != STUN_UI_PARENT_NONE
                    : block->parent_index >= i) ||
            block->byte_begin > block->byte_end ||
            block->byte_end > view->source_bytes ||
            (block->kind == STUN_UI_BLOCK_HEADING &&
             (block->heading_level < 1 || block->heading_level > 6)) ||
            (block->kind != STUN_UI_BLOCK_HEADING &&
             block->heading_level != 0))
            return STUN_UI_INVALID_ARGUMENT;
        /* IMAGE_TITLE is metadata only, never visible content. */
        if (block->kind == STUN_UI_BLOCK_IMAGE_TITLE &&
            (i == 0 || block->parent_index != i-1u ||
             view->blocks[i-1u].kind != STUN_UI_BLOCK_IMAGE ||
             (i+1u < view->block_count &&
              view->blocks[i+1u].parent_index == i)))
            return STUN_UI_INVALID_ARGUMENT;
        if ((block->kind==STUN_UI_BLOCK_RAW_HTML_INLINE ||
             block->kind==STUN_UI_BLOCK_RAW_HTML_BLOCK ||
             block->kind==STUN_UI_BLOCK_URI_AUTOLINK) &&
            i+1u<view->block_count &&
            view->blocks[i+1u].parent_index==i)
            return STUN_UI_INVALID_ARGUMENT;
        for (size_t j = 0; j < i; ++j) {
            if (view->blocks[j].stable_id == block->stable_id)
                return STUN_UI_INVALID_ARGUMENT;
        }
    }
    if (view->blocks[0].kind != STUN_UI_BLOCK_DOCUMENT)
        return STUN_UI_INVALID_ARGUMENT;
    return STUN_UI_OK;
}

stun_ui_status stun_ui_document_visit(
    const stun_ui_document_view *view, size_t max_blocks,
    stun_ui_document_visit_fn visit, void *context) {
    if (!visit) return STUN_UI_INVALID_ARGUMENT;
    const stun_ui_status status = stun_ui_document_validate(view, max_blocks);
    if (status != STUN_UI_OK) return status;
    for (size_t i = 0; i < view->block_count; ++i) {
        const stun_ui_block *block = &view->blocks[i];
        const char *span = view->source
            ? view->source + block->byte_begin : NULL;
        const stun_ui_status result = visit(
            block, span, block->byte_end - block->byte_begin, context);
        if (result != STUN_UI_OK) return result;
    }
    return STUN_UI_OK;
}

/* A RichDoc contains exact semantic text ranges and a separate borrowed
 * metadata byte arena. Both must remain valid throughout a traversal. */
stun_ui_status stun_ui_rich_document_validate(
    const stun_ui_rich_document_view *view, size_t max_blocks) {
    if (!view || view->block_metadata_count != view->document.block_count ||
        !view->block_metadata ||
        (!view->metadata && view->metadata_bytes) ||
        (!view->raw_markdown && view->raw_markdown_bytes))
        return STUN_UI_INVALID_ARGUMENT;
    stun_ui_status status =
        stun_ui_document_validate(&view->document, max_blocks);
    if (status != STUN_UI_OK) return status;
    for (size_t i=0; i<view->block_metadata_count; ++i) {
        const stun_ui_block_metadata *m = &view->block_metadata[i];
        if (m->byte_begin > m->byte_end ||
            m->byte_end > view->metadata_bytes)
            return STUN_UI_INVALID_ARGUMENT;
    }
    return STUN_UI_OK;
}

stun_ui_status stun_ui_rich_document_metadata(
    const stun_ui_rich_document_view *view, size_t node_index,
    const char **out_bytes, size_t *out_length) {
    if (!view || !out_bytes || !out_length ||
        node_index >= view->block_metadata_count ||
        !view->block_metadata)
        return STUN_UI_INVALID_ARGUMENT;
    const stun_ui_block_metadata range = view->block_metadata[node_index];
    if (range.byte_begin > range.byte_end ||
        range.byte_end > view->metadata_bytes ||
        (!view->metadata && view->metadata_bytes))
        return STUN_UI_INVALID_ARGUMENT;
    *out_bytes = view->metadata
        ? view->metadata + range.byte_begin : NULL;
    *out_length = range.byte_end - range.byte_begin;
    return STUN_UI_OK;
}
