/* The only Stun Markdown entrypoint is an ABI-preserving forwarding layer.
 * The grammar, immutable ownership, RichDoc validation and HTML writer are
 * compiled exclusively inside the installed SaltsUtils C11 libraries.
 */
#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>
#include <salts/markdown.h>
#include <salts/markdown_html.h>

_Static_assert(STUN_UI_OK == SALTS_MD_OK, "status ABI changed");
_Static_assert(STUN_UI_INVALID_ARGUMENT == SALTS_MD_INVALID_ARGUMENT,
               "status ABI changed");
_Static_assert(STUN_UI_CAPACITY_EXCEEDED == SALTS_MD_CAPACITY_EXCEEDED,
               "status ABI changed");
_Static_assert(STUN_UI_OUT_OF_MEMORY == SALTS_MD_OUT_OF_MEMORY,
               "status ABI changed");

static stun_ui_status to_stun(salts_md_status code) {
    return (stun_ui_status)code;
}

stun_ui_status stun_md_parse(const char *input, size_t length,
                             size_t max_nodes, size_t max_source_bytes,
                             stun_md_document **out) {
    return to_stun(salts_md_parse(input,length,max_nodes,max_source_bytes,out));
}

void stun_md_document_destroy(stun_md_document *doc) {
    salts_md_document_destroy(doc);
}

stun_ui_rich_document_view stun_md_document_view(const stun_md_document *doc) {
    return salts_md_get_document_view(doc);
}

stun_ui_status stun_ui_document_validate(const stun_ui_document_view *view,
                                         size_t max_blocks) {
    return to_stun(salts_md_document_validate(view,max_blocks));
}

typedef struct visit_thunk {
    stun_ui_document_visit_fn visit;
    void *user;
} visit_thunk;

static salts_md_status visit_adapter(const salts_md_block *node,
                                     const char *text, size_t length,
                                     void *context) {
    visit_thunk *state = (visit_thunk *)context;
    return (salts_md_status)state->visit(node,text,length,state->user);
}

stun_ui_status stun_ui_document_visit(const stun_ui_document_view *view,
                                      size_t max_blocks,
                                      stun_ui_document_visit_fn visitor,
                                      void *context) {
    if (!visitor) return STUN_UI_INVALID_ARGUMENT;
    visit_thunk state = {visitor,context};
    return to_stun(salts_md_document_visit(view,max_blocks,visit_adapter,&state));
}

stun_ui_status stun_ui_rich_document_validate(
    const stun_ui_rich_document_view *view, size_t max_blocks) {
    return to_stun(salts_md_rich_document_validate(view,max_blocks));
}

stun_ui_status stun_ui_rich_document_metadata(
    const stun_ui_rich_document_view *view, size_t index,
    const char **out, size_t *bytes) {
    return to_stun(salts_md_rich_document_metadata(view,index,out,bytes));
}

bool stun_md_html_safe_href(const char *decoded, size_t bytes) {
    return salts_md_html_safe_href(decoded,bytes);
}

bool stun_md_html_safe_image_src(const char *decoded, size_t bytes) {
    return salts_md_html_safe_image_src(decoded,bytes);
}

bool stun_md_html_safe_title(const char *decoded, size_t bytes) {
    return salts_md_html_safe_title(decoded,bytes);
}

stun_ui_status stun_md_render_html_with_policy(
    const stun_ui_rich_document_view *view, size_t max_nodes,
    stun_md_html_policy policy, char *buffer, size_t capacity,
    size_t *required) {
    return to_stun(salts_md_render_html_with_policy(
        view,max_nodes,(salts_md_html_policy)policy,buffer,capacity,required));
}

stun_ui_status stun_md_render_html(const stun_ui_rich_document_view *view,
                                   size_t max_nodes, char *buffer,
                                   size_t capacity, size_t *required) {
    return to_stun(salts_md_render_html(
        view,max_nodes,buffer,capacity,required));
}
