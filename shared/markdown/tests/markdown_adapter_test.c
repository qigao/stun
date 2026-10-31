/* Real Stun public C11 API consumption of the single SaltsUtils Markdown
 * library. Never links the original Stun parser or a test-only grammar.
 */
#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>
#include <salts/markdown.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__,__LINE__,#x); return 1;} } while(0)

typedef struct visit_data { unsigned count; } visit_data;
static stun_ui_status visitor(const stun_ui_block *node,
                              const char *span, size_t n, void *user) {
    visit_data *v=(visit_data*)user;
    if (!node || (!span && n)) return STUN_UI_INVALID_ARGUMENT;
    ++v->count;
    return STUN_UI_OK;
}

int main(void) {
    static const char raw[]="# A &amp; B\n\n- [x] done\n";
    stun_md_document *doc=NULL;
    REQUIRE(stun_md_parse(raw,sizeof(raw)-1u,128u,sizeof(raw),&doc)==STUN_UI_OK);
    REQUIRE(doc!=NULL);
    stun_ui_rich_document_view view=stun_md_document_view(doc);
    REQUIRE(view.raw_markdown_bytes==sizeof(raw)-1u);
    REQUIRE(memcmp(view.raw_markdown,raw,sizeof(raw)-1u)==0);
    REQUIRE(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
    REQUIRE(view.document.block_count>2u);
    REQUIRE(view.document.blocks[1u].kind==STUN_UI_BLOCK_HEADING);
    REQUIRE(view.document.blocks[1u].heading_level==1u);
    visit_data user={0};
    REQUIRE(stun_ui_document_visit(&view.document,128u,visitor,&user)==STUN_UI_OK);
    REQUIRE(user.count==view.document.block_count);
    REQUIRE(stun_ui_document_visit(&view.document,128u,NULL,&user)==STUN_UI_INVALID_ARGUMENT);
    size_t required=0u;
    REQUIRE(stun_md_render_html(&view,128u,NULL,0u,&required)==STUN_UI_OK);
    REQUIRE(required>0u && required<2048u);
    char html[2048];
    memset(html,0x5a,sizeof(html));
    REQUIRE(stun_md_render_html(&view,128u,html,required,&required)==
            STUN_UI_CAPACITY_EXCEEDED);
    REQUIRE((unsigned char)html[0]==0x5au);
    REQUIRE(stun_md_render_html(&view,128u,html,sizeof(html),&required)==STUN_UI_OK);
    REQUIRE(strstr(html,"A &amp; B")!=NULL);
    REQUIRE(stun_md_html_safe_href("/docs/readme.md",15u));
    REQUIRE(!stun_md_html_safe_href("javascript:evil()",17u));
    stun_md_document_destroy(doc);

    doc=(stun_md_document*)(uintptr_t)1u;
    REQUIRE(stun_md_parse(raw,sizeof(raw)-1u,1u,sizeof(raw),&doc)==
            STUN_UI_CAPACITY_EXCEEDED);
    REQUIRE(doc==NULL);
    puts("Stun Markdown forwarding adapter: PASS");
    return 0;
}
