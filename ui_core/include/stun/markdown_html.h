#ifndef STUN_MARKDOWN_HTML_H
#define STUN_MARKDOWN_HTML_H

#include <stun/ui_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Render an already-parsed native C11 RichDoc into HTML.
 *
 * The caller retains the view (including source and metadata). The renderer
 * parses nothing, stores no runtime state, and does not touch the Tango or
 * gCanvas backends. It is an explicit supported subset, not full CommonMark.
 *
 * This API is transactional:
 *   buffer == NULL, capacity == 0: count output bytes without writing;
 *   buffer != NULL: emit only after fully validating output and capacity.
 *   required receives exact output byte count, excluding terminating NUL.
 *   on failure, supplied buffer bytes are unchanged.
 *
 * Unsupported semantic kinds and invalid/unsafe link targets fail closed with
 * STUN_UI_INVALID_ARGUMENT, never silently fall back to raw HTML output.
 * max_nodes caps validation/traversal effort and recursion depth is bounded.
 */
stun_ui_status stun_md_render_html(const stun_ui_rich_document_view *view,
                                   size_t max_nodes,
                                   char *buffer, size_t capacity,
                                   size_t *required);

#ifdef __cplusplus
}
#endif

#endif /* STUN_MARKDOWN_HTML_H */
