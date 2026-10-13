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
    {627u,"foo <?php echo $a; ?>\n",
     "<p>foo <?php echo $a; ?></p>\n",
     "<p>foo &lt;?php echo $a; ?&gt;</p>\n",1u},
    {628u,"foo <!ELEMENT br EMPTY>\n",
     "<p>foo <!ELEMENT br EMPTY></p>\n",
     "<p>foo &lt;!ELEMENT br EMPTY&gt;</p>\n",1u},
    {629u,"foo <![CDATA[>&<]]>\n",
     "<p>foo <![CDATA[>&<]]></p>\n",
     "<p>foo &lt;![CDATA[&gt;&amp;&lt;]]&gt;</p>\n",1u},
    {0u,"foo <!-- simple -->\n",
     "<p>foo <!-- simple --></p>\n",
     "<p>foo &lt;!-- simple --&gt;</p>\n",1u},
    {0u,"before <script>alert(1)</script> after\n",
     "<p>before <script>alert(1)</script> after</p>\n",
     "<p>before &lt;script&gt;alert(1)&lt;/script&gt; after</p>\n",2u},
    {0u,"x <a foo=\"unclosed\n",
     "<p>x &lt;a foo=&quot;unclosed</p>\n",
     "<p>x &lt;a foo=&quot;unclosed</p>\n",0u}
};
/* Real ROOT HTML type-6 blocks use the same canonical block parser.
 * The SAFE profile still escapes all raw HTML; SPEC emits only admitted
 * literal source tokens. HTML code/fence/definition interactions remain
 * bounded and may not create Reference bindings inside raw block content. */
static const raw_fixture block_fixtures[]={
    {149u,"<table>\n  <tr>\n    <td>\n           hi\n    </td>\n  </tr>\n</table>\n\nokay.\n",
     "<table>\n  <tr>\n    <td>\n           hi\n    </td>\n  </tr>\n</table>\n<p>okay.</p>\n",
     "&lt;table&gt;\n  &lt;tr&gt;\n    &lt;td&gt;\n           hi\n    &lt;/td&gt;\n  &lt;/tr&gt;\n&lt;/table&gt;\n<p>okay.</p>\n",0u},
    {150u," <div>\n  *hello*\n         <foo><a>\n",
     " <div>\n  *hello*\n         <foo><a>\n",
     " &lt;div&gt;\n  *hello*\n         &lt;foo&gt;&lt;a&gt;\n",0u},
    {151u,"</div>\n*foo*\n",
     "</div>\n*foo*\n",
     "&lt;/div&gt;\n*foo*\n",0u},
    {160u,"<table><tr><td>\nfoo\n</td></tr></table>\n",
     "<table><tr><td>\nfoo\n</td></tr></table>\n",
     "&lt;table&gt;&lt;tr&gt;&lt;td&gt;\nfoo\n&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;\n",0u},
    {185u,"Foo\n<div>\nbar\n</div>\n",
     "<p>Foo</p>\n<div>\nbar\n</div>\n",
     "<p>Foo</p>\n&lt;div&gt;\nbar\n&lt;/div&gt;\n",0u},
    {190u,"<table>\n\n<tr>\n\n<td>\nHi\n</td>\n\n</tr>\n\n</table>\n",
     "<table>\n<tr>\n<td>\nHi\n</td>\n</tr>\n</table>\n",
     "&lt;table&gt;\n&lt;tr&gt;\n&lt;td&gt;\nHi\n&lt;/td&gt;\n&lt;/tr&gt;\n&lt;/table&gt;\n",0u},
    {0u,"<div>\n[ref]: /evil\n\n[ref]\n",
     "<div>\n[ref]: /evil\n<p>[ref]</p>\n",
     "&lt;div&gt;\n[ref]: /evil\n<p>[ref]</p>\n",0u}
};

static int verify_block(const raw_fixture *f) {
    if(verify(f))return 1;
    const size_t n=strlen(f->source);
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(f->source,n,256u,n+1u,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    size_t blocks=0u;
    for(size_t i=0u;i<v.document.block_count;++i)
        blocks+=v.document.blocks[i].kind==STUN_UI_BLOCK_RAW_HTML_BLOCK;
    CHECK(blocks>0u);
    stun_md_document_destroy(doc);
    return 0;
}

/* HTML block type-6 has an exact CommonMark 0.31.2 tag vocabulary
 * and delimiter rule. Names outside that vocabulary, plus a raw slash
 * that is not '/>', must remain inline/paragraph grammar and not swallow
 * the next Markdown emphasis line as an HTML block. */
static int test_type6_exact_admission(void) {
    static const char *const nonblocks[]={
        "<source>x\n*foo*\n",
        "<output>x\n*foo*\n",
        "<menuitem>x\n*foo*\n",
        "<table/foo>x\n*foo*\n"
    };
    stun_md_document *doc=NULL;
    for(size_t k=0u;k<sizeof(nonblocks)/sizeof(nonblocks[0]);++k) {
        const char *input=nonblocks[k];size_t n=strlen(input);
        CHECK(stun_md_parse(input,n,128u,n+1u,&doc)==STUN_UI_OK);
        CHECK(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        size_t raw_blocks=0u,emphasis=0u;
        for(size_t i=0u;i<v.document.block_count;++i) {
            raw_blocks+=v.document.blocks[i].kind==
                        STUN_UI_BLOCK_RAW_HTML_BLOCK;
            emphasis+=v.document.blocks[i].kind==
                      STUN_UI_BLOCK_EMPHASIS;
        }
        CHECK(raw_blocks==0u && emphasis==1u);
        stun_md_document_destroy(doc);doc=NULL;
    }
    return 0;
}

int main(void) {
    if(test_type6_exact_admission())return 1;
    _Static_assert(sizeof(fixtures)/sizeof(fixtures[0])==12u,
                   "nine official cases, one comment and two hostile/malformed tests");
    for(size_t i=0u;i<sizeof(fixtures)/sizeof(fixtures[0]);++i)
        if(verify(&fixtures[i])) {
            fprintf(stderr,"RAW HTML fixture %u failed\n",
                    fixtures[i].official);
            return 1;
        }
    _Static_assert(sizeof(block_fixtures)/sizeof(block_fixtures[0])==7u,
                   "six official type-6 blocks + definition opacity");
    for(size_t i=0u;i<sizeof(block_fixtures)/sizeof(block_fixtures[0]);++i)
        if(verify_block(&block_fixtures[i])) {
            fprintf(stderr,"ROOT RAW_HTML_BLOCK official #%u failed\n",
                    block_fixtures[i].official);
            return 1;
        }
    puts("C11 typed RAW_HTML: nine official inline, six official block "
         "spec outputs; unchanged SAFE rendering and atomic limits");
    return 0;
}
