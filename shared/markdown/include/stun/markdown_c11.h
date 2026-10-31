#ifndef STUN_MARKDOWN_C11_H
#define STUN_MARKDOWN_C11_H

#include <stun/ui_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stun compatibility API over the single SaltsUtils C11 Markdown grammar.
 *
 * Since Stun PR #231, MarkdownWidget calls stun_md_parse() directly. There
 * is no alternate C++/MD4C parser or error fallback in that production path.
 * Other consumers (including Tango) retain separate migration boundaries.
 *
 * A successful parse owns the raw Markdown, decoded semantic text, metadata
 * and nodes. The returned view borrows that document and expires on destroy.
 * Failed parses leave *out NULL and release partial state; callers MUST NOT
 * publish a partial or legacy-parser substitute.
 *
 * CommonMark/GFM feature and compatibility claims must be established by
 * independent fixtures; this API does not promise universal conformance.
 */
typedef salts_md_document stun_md_document;

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
