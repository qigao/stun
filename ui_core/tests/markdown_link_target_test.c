#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr); \
        return 1; \
    } \
} while (0)

typedef struct link_fixture {
    const char *markdown,*href,*title,*label,*expected_html;
} link_fixture;

static size_t kind_count(stun_ui_rich_document_view v,
                         stun_ui_block_kind kind) {
    size_t total=0u;
    for(size_t i=0u;i<v.document.block_count;++i)
        total+=v.document.blocks[i].kind==kind;
    return total;
}

static int verify_link(const link_fixture *f) {
    const size_t raw_size=strlen(f->markdown);
    stun_md_document *owner=NULL;
    CHECK(stun_md_parse(f->markdown,raw_size,128u,raw_size+1u,
                        &owner)==STUN_UI_OK);
    CHECK(owner!=NULL);
    const stun_ui_rich_document_view view=stun_md_document_view(owner);
    CHECK(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
    CHECK(view.raw_markdown_bytes==raw_size);
    CHECK(memcmp(view.raw_markdown,f->markdown,raw_size)==0);
    CHECK(kind_count(view,STUN_UI_BLOCK_LINK)==1u);
    CHECK(kind_count(view,STUN_UI_BLOCK_LINK_TITLE)==
          (f->title?1u:0u));

    bool found_link=false,found_title=!f->title,found_label=false;
    for(size_t i=1u;i<view.document.block_count;++i) {
        const stun_ui_block *node=&view.document.blocks[i];
        CHECK(node->parent_index<i);
        if(node->kind==STUN_UI_BLOCK_LINK) {
            const char *url=NULL;size_t bytes=SIZE_MAX;
            CHECK(stun_ui_rich_document_metadata(&view,i,&url,&bytes)==
                  STUN_UI_OK);
            CHECK(bytes==strlen(f->href));
            CHECK(!bytes || (url &&
                  !memcmp(url,f->href,bytes)));
            found_link=true;
        }
        if(node->kind==STUN_UI_BLOCK_LINK_TITLE) {
            CHECK(node->parent_index+1u==i);
            CHECK(view.document.blocks[node->parent_index].kind==
                  STUN_UI_BLOCK_LINK);
            const char *title=NULL;size_t bytes=SIZE_MAX;
            CHECK(stun_ui_rich_document_metadata(&view,i,&title,&bytes)==
                  STUN_UI_OK);
            CHECK(bytes==strlen(f->title));
            CHECK(!bytes || (title &&
                  !memcmp(title,f->title,bytes)));
            found_title=true;
        }
        if(node->kind==STUN_UI_BLOCK_TEXT &&
           view.document.blocks[node->parent_index].kind==
               STUN_UI_BLOCK_LINK) {
            const size_t size=node->byte_end-node->byte_begin;
            CHECK(node->byte_end<=view.document.source_bytes);
            if(size==strlen(f->label) &&
               !memcmp(view.document.source+node->byte_begin,
                       f->label,size))
                found_label=true;
        }
    }
    CHECK(found_link && found_title && found_label);

    size_t required=SIZE_MAX;
    CHECK(stun_md_render_html(&view,128u,NULL,0u,&required)==STUN_UI_OK);
    CHECK(required==strlen(f->expected_html));
    char buffer[256];
    CHECK(required+2u<sizeof(buffer));
    memset(buffer,0x58,sizeof(buffer));
    size_t counted=SIZE_MAX;
    CHECK(stun_md_render_html(&view,128u,buffer,required,&counted)==
          STUN_UI_CAPACITY_EXCEEDED);
    CHECK(counted==required);
    for(size_t i=0u;i<sizeof(buffer);++i)
        CHECK((unsigned char)buffer[i]==0x58u);
    CHECK(stun_md_render_html(&view,128u,buffer,required+1u,&counted)==
          STUN_UI_OK);
    CHECK(counted==required);
    CHECK(!memcmp(buffer,f->expected_html,required+1u));
    CHECK((unsigned char)buffer[required+1u]==0x58u);
    stun_md_document_destroy(owner);
    return 0;
}

static int verify_safe_rejection(const char *source) {
    const size_t n=strlen(source);
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(source,n,128u,n+1u,&doc)==STUN_UI_OK);
    CHECK(doc!=NULL);
    const stun_ui_rich_document_view view=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
    CHECK(kind_count(view,STUN_UI_BLOCK_LINK)==1u);
    char buffer[256];
    memset(buffer,0x35,sizeof(buffer));
    size_t required=SIZE_MAX;
    CHECK(stun_md_render_html(&view,128u,buffer,sizeof(buffer),
                              &required)==STUN_UI_INVALID_ARGUMENT);
    for(size_t i=0u;i<sizeof(buffer);++i)
        CHECK((unsigned char)buffer[i]==0x35u);
    stun_md_document_destroy(doc);
    return 0;
}

int main(void) {
    static const link_fixture samples[]={
        {"[foo](/bar\\* \"ti\\*tle\")\n",
         "/bar*","ti*tle","foo",
         "<p><a href=\"/bar*\" title=\"ti*tle\">foo</a></p>\n"}, /* #22 */
        {"[foo](/f&ouml;&ouml; \"f&ouml;&ouml;\")\n",
         "/föö","föö","foo",
         "<p><a href=\"/f%C3%B6%C3%B6\" title=\"föö\">foo</a></p>\n"}, /* #32 */
        {"[&copy;](dest \"Title\")\n",
         "dest","Title","©",
         "<p><a href=\"dest\" title=\"Title\">©</a></p>\n"},
        {"[x](/f\\&copy; \"a\")\n",
         "/f&copy;","a","x",
         "<p><a href=\"/f&amp;copy;\" title=\"a\">x</a></p>\n"},
        {"[x](dest \"say \\\"hi\\\"\")\n",
         "dest","say \"hi\"","x",
         "<p><a href=\"dest\" title=\"say &quot;hi&quot;\">x</a></p>\n"},
        {"[x](dest \"\")\n",
         "dest","","x",
         "<p><a href=\"dest\" title=\"\">x</a></p>\n"},
        {"[x](/f%C3%B6)\n",
         "/f%C3%B6",NULL,"x",
         "<p><a href=\"/f%C3%B6\">x</a></p>\n"}
    };
    _Static_assert(sizeof(samples)/sizeof(samples[0])==7u,
                   "link title/URI semantic fixture count");
    for(size_t i=0u;i<sizeof(samples)/sizeof(samples[0]);++i)
        if(verify_link(&samples[i]))return 1;

    /* An entity-expanded disallowed scheme must fail AFTER decoding, never
     * be normalized into an apparently safe ASCII URL. */
    CHECK(verify_safe_rejection("[x](java&#x73;cript:alert \"t\")\n")==0);
    CHECK(verify_safe_rejection("[x](data:text/html \"t\")\n")==0);
    CHECK(verify_safe_rejection("[x](safe \"a&#10;b\")\n")==0);
    /* The whole decoded href must be admitted, not only its first scheme,
     * separator or fragment. These inputs used to bypass early returns at
     * '/', '?', '#', 'https:' and 'mailto:' with a late C0 scalar. */
    CHECK(verify_safe_rejection("[x](/safe&#10;bad \"t\")\n")==0);
    CHECK(verify_safe_rejection(
        "[x](https://example.org/&#x09;bad \"t\")\n")==0);
    CHECK(verify_safe_rejection("[x](foo?bad&#10;value)\n")==0);
    CHECK(verify_safe_rejection("[x](#frag&#13;bad)\n")==0);
    CHECK(verify_safe_rejection("[x](mailto:foo&#x0A;bar \"t\")\n")==0);

    /* The entity-produced quote cannot fabricate a syntactic link TITLE. */
    const char invalid[]="[a](url &quot;tit&quot;)\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(invalid,sizeof(invalid)-1u,128u,
                        sizeof(invalid),&doc)==STUN_UI_OK);
    CHECK(doc!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    CHECK(kind_count(v,STUN_UI_BLOCK_LINK)==0u);
    stun_md_document_destroy(doc);

    /* Node budget exhaustion with a title must discard every partial node. */
    const char limited[]="[x](/f&ouml;&ouml; \"f&ouml;&ouml;\")\n";
    stun_md_document *rejected=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(limited,sizeof(limited)-1u,3u,
                        sizeof(limited),&rejected)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(rejected==NULL);

    puts("C11 link metadata: 7/7 title/URL fixtures, 8 safety rejections, "
         "invalid syntax literal and atomic node-budget rollback");
    return 0;
}
