#ifndef STUN_MARKDOWN_C11_H
#define STUN_MARKDOWN_C11_H

#include <stun/ui_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* New, native C11 Markdown parser candidate.
 *
 * This parses Markdown input directly; it is NOT an adapter over md::parse().
 * The existing C++ parser remains the production authority until semantic
 * parity has been qualified and callers are switched atomically. Do not run
 * two parsers to produce one UI document.
 *
 * A successful parse owns the raw Markdown, the decoded/semantic text buffer,
 * metadata, and all nodes. The returned view borrows the document; it becomes
 * invalid on destroy. On failure *out remains NULL and partial state is freed.
 *
 * This initial implementation covers the explicitly tested CommonMark subset:
 * ATX/Setext headings, paragraphs, fenced code, quotes, lists, thematic rules,
 * emphasis/strong, inline code, links/images, escaped punctuation and line
 * breaks. It does not claim full CommonMark/GFM compliance yet.
 */
typedef struct stun_md_document stun_md_document;

/* Bounded node and source admission; zero budgets are invalid.
 * Input is an exact-length byte sequence, not a NUL-terminated string.
 * ASCII syntax delimiters are recognized without modifying UTF-8 payloads.
 * Strict Unicode validation remains at the Salts::Unicode admission boundary.
 */
stun_ui_status stun_md_parse(const char *input, size_t length,
                            size_t max_nodes, size_t max_source_bytes,
                            stun_md_document **out);

/* No-fail cleanup; NULL is accepted. */
void stun_md_document_destroy(stun_md_document *doc);

/* Borrowed immutable view. Empty view returned for NULL. */
stun_ui_rich_document_view stun_md_document_view(const stun_md_document *doc);

#ifdef __cplusplus
}
#endif

#endif /* STUN_MARKDOWN_C11_H */
