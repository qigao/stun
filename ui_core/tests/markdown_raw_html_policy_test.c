#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do {if(!(expr)) { \
    fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr);return 1; \
} } while(0)

typedef struct raw_fixture {
    unsigned official;
    const char *source,*spec,*safe;
    size_t raw_nodes;
} raw_fixture;
static size_t raw_nodes(stun_ui_rich_document_view v) {
    size_t found=0u;
    for(size_t i=1u;i<v.document.block_count;++i)
        found+=v.document.blocks[i].kind==STUN_UI_BLOCK_RAW_HTML_INLINE;
    return found;
}
static int verify(const raw_fixture *f) {
    const size_t n=strlen(f->source);
    char *input=(char*)malloc(n+1u);CHECK(input!=NULL);
    memcpy(input,f->source,n+1u);
    stun_md_document *doc=NULL,*rejected=NULL;
    CHECK(stun_md_parse(input,n,256u,n+1u,&doc)==STUN_UI_OK);
    CHECK(doc!=NULL);memset(input,'!',n);free(input);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
    CHECK(v.raw_markdown_bytes==n &&
          !memcmp(v.raw_markdown,f->source,n));
    CHECK(raw_nodes(v)==f->raw_nodes);
    for(unsigned policy=0u;policy<2u;++policy) {
        const stun_md_html_policy mode=policy?
            STUN_MD_HTML_SPEC_CONFORMANCE:STUN_MD_HTML_SAFE_APPLICATION;
        const char *expected=policy?f->spec:f->safe;
        const size_t bytes=strlen(expected);
        size_t required=SIZE_MAX,reported=SIZE_MAX;
        CHECK(stun_md_render_html_with_policy(
            &v,256u,mode,NULL,0u,&required)==STUN_UI_OK);
        CHECK(required==bytes);
        char *out=(char*)malloc(bytes+3u);CHECK(out!=NULL);
        memset(out,0x55,bytes+3u);
        CHECK(stun_md_render_html_with_policy(
            &v,256u,mode,out,bytes,&reported)==STUN_UI_CAPACITY_EXCEEDED);
        CHECK(reported==bytes);
        for(size_t i=0u;i<bytes+3u;++i)
            CHECK((unsigned char)out[i]==0x55u);
        CHECK(stun_md_render_html_with_policy(
            &v,256u,mode,out,bytes+1u,&reported)==STUN_UI_OK);
        CHECK(reported==bytes && !memcmp(out,expected,bytes+1u));
        CHECK((unsigned char)out[bytes+1u]==0x55u);
        if(!policy) {
            size_t measured=SIZE_MAX;
            CHECK(stun_md_render_html(&v,256u,out,bytes+1u,&measured)
                  ==STUN_UI_OK);
            CHECK(measured==bytes && !memcmp(out,expected,bytes+1u));
        }
        free(out);
    }
    char unchanged[64];memset(unchanged,0x47,sizeof(unchanged));
    size_t needed=SIZE_MAX;
    CHECK(stun_md_render_html_with_policy(&v,256u,
          (stun_md_html_policy)42,unchanged,sizeof(unchanged),&needed)
          ==STUN_UI_INVALID_ARGUMENT);
    for(size_t i=0u;i<sizeof(unchanged);++i)
        CHECK((unsigned char)unchanged[i]==0x47u);
    const size_t count=v.document.block_count;
    stun_md_document_destroy(doc);
    CHECK(stun_md_parse(f->source,n,count-1u,n+1u,&rejected)
          ==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(rejected==NULL);
    return 0;
}
static const raw_fixture fixtures[] = {
    {613u,"<a><bab><c2c>\n",
     "<p><a><bab><c2c></p>\n",
     "<p>&lt;a&gt;&lt;bab&gt;&lt;c2c&gt;</p>\n",
     3u},
    {614u,"<a/><b2/>\n",
     "<p><a/><b2/></p>\n",
     "<p>&lt;a/&gt;&lt;b2/&gt;</p>\n",
     2u},
    {617u,"Foo <responsive-image src=\"foo.jpg\" />\n",
     "<p>Foo <responsive-image src=\"foo.jpg\" /></p>\n",
     "<p>Foo &lt;responsive-image src=&quot;foo.jpg&quot; /&gt;</p>\n",
     1u},
    {623u,"</a></foo >\n",
     "<p></a></foo ></p>\n",
     "<p>&lt;/a&gt;&lt;/foo &gt;</p>\n",
     2u},
    {630u,"foo <a href=\"&ouml;\">\n",
     "<p>foo <a href=\"&ouml;\"></p>\n",
     "<p>foo &lt;a href=&quot;&amp;ouml;&quot;&gt;</p>\n",
     1u},
    {631u,"foo <a href=\"\\*\">\n",
     "<p>foo <a href=\"\\*\"></p>\n",
     "<p>foo &lt;a href=&quot;\\*&quot;&gt;</p>\n",
     1u},
    {0u,"before <script>alert(1)</script> after\n",
     "<p>before <script>alert(1)</script> after</p>\n",
     "<p>before &lt;script&gt;alert(1)&lt;/script&gt; after</p>\n",2u},
    {0u,"x <a foo=\"unclosed\n",
     "<p>x &lt;a foo=&quot;unclosed</p>\n",
     "<p>x &lt;a foo=&quot;unclosed</p>\n",0u}
};
int main(void) {
    _Static_assert(sizeof(fixtures)/sizeof(fixtures[0])==8u,
                   "six official examples plus hostile/malformed controls");
    for(size_t i=0u;i<sizeof(fixtures)/sizeof(fixtures[0]);++i)
        if(verify(&fixtures[i])) {
            fprintf(stderr,"RAW HTML fixture %u failed\n",
                    fixtures[i].official);
            return 1;
        }
    puts("RAW_HTML_INLINE C11: six official spec outputs, safe escaping, "
         "caller-source mutation, hostile tokens, atomic node/buffer limits");
    return 0;
}
