#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test_) do { \
    if (!(test_)) { \
        fprintf(stderr,"%s:%d: failed: %s\n",__FILE__,__LINE__,#test_); \
        return 1; \
    } \
} while (0)

#define TICK "\x60"
typedef struct {
    const char *markdown;
    const char *html;
} html_fixture;

static const html_fixture cases[] = {
    { "## Hello\n", "<h2>Hello</h2>\n" },
    { "Normal **Bold** text\n",
      "<p>Normal <strong>Bold</strong> text</p>\n" },
    { "A *bright* day.\n",
      "<p>A <em>bright</em> day.</p>\n" },
    { "> quote\n", "<blockquote>\n<p>quote</p>\n</blockquote>\n" },
    { "1. alpha\n2. beta\n",
      "<ol>\n<li>alpha</li>\n<li>beta</li>\n</ol>\n" },
    { "- one\n- two\n",
      "<ul>\n<li>one</li>\n<li>two</li>\n</ul>\n" },
    { TICK TICK TICK "c\nint v;\n" TICK TICK TICK "\n",
      "<pre><code class=\"language-c\">int v;\n</code></pre>\n" },
    { "A [B](dest)\n", "<p>A <a href=\"dest\">B</a></p>\n" },
    { "a " TICK "code" TICK " b\n",
      "<p>a <code>code</code> b</p>\n" },
    { "---\n", "<hr />\n" },
    { "first\n===\n", "<h1>first</h1>\n" },
    { "first\nsecond\n", "<p>first\nsecond</p>\n" },
    { "a \\*b\n", "<p>a *b</p>\n" },
    { "中😀 👩‍💻\n", "<p>中😀 👩‍💻</p>\n" },
    { "a ~~red~~ day\n", "<p>a <del>red</del> day</p>\n" },
    { "| A | B |\n| --- | --- |\n| one | two |\n",
      "<table>\n<thead>\n<tr>\n<th>A</th>\n<th>B</th>\n</tr>\n"
      "</thead>\n<tbody>\n<tr>\n<td>one</td>\n<td>two</td>\n</tr>\n"
      "</tbody>\n</table>\n" },
    { "", "" },
};

_Static_assert(sizeof(cases)/sizeof(cases[0])==17,
               "Explicit HTML fixture denominator changed");

static int test_golden(const html_fixture *f) {
    stun_md_document *doc=NULL;
    size_t source_bytes=strlen(f->markdown);
    CHECK(stun_md_parse(f->markdown,source_bytes,256u,
                        source_bytes+1u,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    size_t need=SIZE_MAX;
    CHECK(stun_md_render_html(&v,256u,NULL,0u,&need)==STUN_UI_OK);
    const size_t expect=strlen(f->html);
    CHECK(need==expect);
    char *out=(char *)malloc(need+2u);
    CHECK(out!=NULL);
    memset(out,0xa5,need+2u);

    /* Prove atomic rejection of insufficient output capacity. */
    size_t required=SIZE_MAX;
    if(need) {
        CHECK(stun_md_render_html(&v,256u,out,need,&required)==
              STUN_UI_CAPACITY_EXCEEDED);
        CHECK(required==need);
        for(size_t k=0;k<need+2u;++k)
            CHECK((unsigned char)out[k]==0xa5);
    }
    CHECK(stun_md_render_html(&v,256u,out,need+1u,&required)==STUN_UI_OK);
    CHECK(required==need);
    CHECK(!memcmp(out,f->html,need+1u));
    CHECK((unsigned char)out[need+1u]==0xa5);
    free(out);
    stun_md_document_destroy(doc);
    return 0;
}

static int test_unsafe_unsupported(void) {
    /* Balanced-parenthesis destinations are not implemented by the C11 parser
     * yet. Use a syntactically admitted link to test the actual HTML URL gate. */
    const char bad_url[]="[bad](javascript:alert)\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(bad_url,sizeof(bad_url)-1u,64,256,&doc)==STUN_UI_OK);
    stun_ui_rich_document_view v=stun_md_document_view(doc);
    bool found_link=false;
    for(size_t i=0;i<v.document.block_count;++i)
        if(v.document.blocks[i].kind==STUN_UI_BLOCK_LINK) found_link=true;
    CHECK(found_link);  /* Must exercise URL admission, not plain-text fallback. */
    char out[128];
    memset(out,0x4d,sizeof(out));
    size_t needed=0;
    CHECK(stun_md_render_html(&v,64u,out,sizeof(out),&needed)==
          STUN_UI_INVALID_ARGUMENT);
    for(size_t i=0;i<sizeof(out);++i)
        CHECK((unsigned char)out[i]==0x4d);
    stun_md_document_destroy(doc);

    /* A second supported link scheme rejection keeps data: fail-closed. */
    const char data_url[]="[bad](data:text/html)\n";
    CHECK(stun_md_parse(data_url,sizeof(data_url)-1u,64,256,&doc)==STUN_UI_OK);
    v=stun_md_document_view(doc);
    found_link=false;
    for(size_t i=0;i<v.document.block_count;++i)
        if(v.document.blocks[i].kind==STUN_UI_BLOCK_LINK) found_link=true;
    CHECK(found_link);
    memset(out,0x4d,sizeof(out));
    CHECK(stun_md_render_html(&v,64u,out,sizeof(out),&needed)==
          STUN_UI_INVALID_ARGUMENT);
    for(size_t i=0;i<sizeof(out);++i)
        CHECK((unsigned char)out[i]==0x4d);
    stun_md_document_destroy(doc);

    const char image[]="![alt](https://example.org/x.png)\n";
    CHECK(stun_md_parse(image,sizeof(image)-1u,64,256,&doc)==STUN_UI_OK);
    v=stun_md_document_view(doc);
    CHECK(stun_md_render_html(&v,64u,NULL,0u,&needed)==
          STUN_UI_INVALID_ARGUMENT);
    stun_md_document_destroy(doc);

    CHECK(stun_md_render_html(NULL,64u,NULL,0u,&needed)==
          STUN_UI_INVALID_ARGUMENT);
    return 0;
}

int main(void) {
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        if(test_golden(&cases[i])) {
            fprintf(stderr,"HTML fixture %zu failed: %s\n",i,cases[i].markdown);
            return 1;
        }
    }
    if(test_unsafe_unsupported()) return 1;
    puts("Stun C11 RichDoc HTML: 17/17 bounded subset fixtures passed");
    return 0;
}
