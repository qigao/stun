/* #181: real C11 RichDoc block-container semantics and exact HTML output. */
#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if(!(x)) { \
    fprintf(stderr,"line %d: %s\n",__LINE__,#x); return false; \
} } while(0)

typedef struct fixture {
    const char *name,*markdown,*expected;
    int loose; /* -1: don't inspect; otherwise outer list loose flag */
    unsigned list_count;
} fixture;

static const fixture cases[]={
    {"cm-108","  - foo\n\n    bar\n",
     "<ul>\n<li>\n<p>foo</p>\n<p>bar</p>\n</li>\n</ul>\n",1,1u},
    {"cm-109","1.  foo\n\n    - bar\n",
     "<ol>\n<li>\n<p>foo</p>\n<ul>\n<li>bar</li>\n</ul>\n</li>\n</ol>\n",1,2u},
    {"cm-254","1.  A paragraph\n    with two lines.\n\n        indented code\n\n    > A block quote.\n",
     "<ol>\n<li>\n<p>A paragraph\nwith two lines.</p>\n<pre><code>indented code\n</code></pre>\n<blockquote>\n<p>A block quote.</p>\n</blockquote>\n</li>\n</ol>\n",1,1u},
    {"cm-255","- one\n\n two\n",
     "<ul>\n<li>one</li>\n</ul>\n<p>two</p>\n",0,1u},
    {"cm-256","- one\n\n  two\n",
     "<ul>\n<li>\n<p>one</p>\n<p>two</p>\n</li>\n</ul>\n",1,1u},
    {"cm-301","- foo\n- bar\n+ baz\n",
     "<ul>\n<li>foo</li>\n<li>bar</li>\n</ul>\n<ul>\n<li>baz</li>\n</ul>\n",0,2u},
    {"cm-302","1. foo\n2. bar\n3) baz\n",
     "<ol>\n<li>foo</li>\n<li>bar</li>\n</ol>\n<ol start=\"3\">\n<li>baz</li>\n</ol>\n",0,2u},
    {"cm-304","The number of windows in my house is\n14.  The number of doors is 6.\n",
     "<p>The number of windows in my house is\n14.  The number of doors is 6.</p>\n",-1,0u},
    {"cm-305","The number of windows in my house is\n1.  The number of doors is 6.\n",
     "<p>The number of windows in my house is</p>\n<ol>\n<li>The number of doors is 6.</li>\n</ol>\n",0,1u},
    {"ordered-zero","0. zero\n",
     "<ol start=\"0\">\n<li>zero</li>\n</ol>\n",0,1u},
    {"nested-tight","- outer\n  - inner\n- next\n",
     "<ul>\n<li>outer\n<ul>\n<li>inner</li>\n</ul>\n</li>\n<li>next</li>\n</ul>\n",0,2u},
    {"tab-item","- alpha\n\tbeta\n",
     "<ul>\n<li>alpha\nbeta</li>\n</ul>\n",0,1u},
    {"unicode-loose","- \xe4\xb8\xad\xf0\x9f\x91\xa9\n\n  \xe6\x97\xa5\n",
     "<ul>\n<li>\n<p>\xe4\xb8\xad\xf0\x9f\x91\xa9</p>\n<p>\xe6\x97\xa5</p>\n</li>\n</ul>\n",1,1u},
    /* Preserve already-successful CommonMark examples, not only new gates. */
    {"cm-060","* Foo\n* * *\n* Bar\n",
     "<ul>\n<li>Foo</li>\n</ul>\n<hr />\n<ul>\n<li>Bar</li>\n</ul>\n",0,2u},
    {"cm-280","-\n\n  foo\n",
     "<ul>\n<li></li>\n</ul>\n<p>foo</p>\n",0,1u},
    {"cm-295","- foo\n - bar\n  - baz\n   - boo\n",
     "<ul>\n<li>foo</li>\n<li>bar</li>\n<li>baz</li>\n<li>boo</li>\n</ul>\n",0,1u},
    {"cm-310","- a\n - b\n  - c\n   - d\n  - e\n - f\n- g\n",
     "<ul>\n<li>a</li>\n<li>b</li>\n<li>c</li>\n<li>d</li>\n<li>e</li>\n<li>f</li>\n<li>g</li>\n</ul>\n",0,1u},
    {"cm-312","- a\n - b\n  - c\n   - d\n    - e\n",
     "<ul>\n<li>a</li>\n<li>b</li>\n<li>c</li>\n<li>d\n- e</li>\n</ul>\n",0,1u},
    {"cm-325","* foo\n  * bar\n\n  baz\n",
     "<ul>\n<li>\n<p>foo</p>\n<ul>\n<li>bar</li>\n</ul>\n<p>baz</p>\n</li>\n</ul>\n",1,2u},
    /* #181: quote paragraphs inside a list item are owned and continued
     * within one quote, without admitting synthetic blank paragraphs. */
    {"cm-320","* a\n  > b\n  >\n* c\n",
     "<ul>\n<li>a\n<blockquote>\n<p>b</p>\n</blockquote>\n</li>\n<li>c</li>\n</ul>\n",0,1u},
    {"list-quote-marked-continuation","* a\n  > b\n  > c\n* d\n",
     "<ul>\n<li>a\n<blockquote>\n<p>b\nc</p>\n</blockquote>\n</li>\n<li>d</li>\n</ul>\n",0,1u},
    {"list-quote-explicit-blank","* a\n  > b\n  >\n  > c\n* d\n",
     "<ul>\n<li>a\n<blockquote>\n<p>b</p>\n<p>c</p>\n</blockquote>\n</li>\n<li>d</li>\n</ul>\n",0,1u},
    {"cm-264","- Foo\n\n      bar\n\n\n      baz\n",
     "<ul>\n<li>\n<p>Foo</p>\n<pre><code>bar\n\n\nbaz\n</code></pre>\n</li>\n</ul>\n",1,1u},
    {"quote-list-code-interior-blanks","> - Foo\n>\n>       bar\n>\n>\n>       baz\n",
     "<blockquote>\n<ul>\n<li>\n<p>Foo</p>\n<pre><code>bar\n\n\nbaz\n</code></pre>\n</li>\n</ul>\n</blockquote>\n",1,1u},
    {"list-code-trailing-blanks","- Foo\n\n      bar\n\n\n",
     "<ul>\n<li>\n<p>Foo</p>\n<pre><code>bar\n</code></pre>\n</li>\n</ul>\n",1,1u}
};
_Static_assert(sizeof(cases)/sizeof(cases[0])==25u,
               "Scoped list container fixture denominator changed");

static bool check_case(const fixture *f) {
    const size_t source_length=strlen(f->markdown);
    stun_md_document *owner=NULL;
    REQUIRE(stun_md_parse(f->markdown,source_length,512u,
                          source_length+1u,&owner)==STUN_UI_OK);
    REQUIRE(owner!=NULL);
    const stun_ui_rich_document_view view=stun_md_document_view(owner);
    REQUIRE(stun_ui_rich_document_validate(&view,512u)==STUN_UI_OK);
    REQUIRE(view.raw_markdown_bytes==source_length);
    REQUIRE(memcmp(view.raw_markdown,f->markdown,source_length)==0);

    unsigned lists=0;
    int loose=-1;
    for(size_t i=0;i<view.document.block_count;++i) {
        const stun_ui_block *node=&view.document.blocks[i];
        REQUIRE(node->stable_id==i+1u);
        if(node->kind==STUN_UI_BLOCK_LIST||
           node->kind==STUN_UI_BLOCK_ORDERED_LIST) {
            const char *meta=NULL;size_t bytes=0;
            REQUIRE(stun_ui_rich_document_metadata(&view,i,&meta,&bytes)==STUN_UI_OK);
            REQUIRE(bytes==sizeof(stun_ui_list_semantics));
            stun_ui_list_semantics info;
            memcpy(&info,meta,sizeof(info));
            REQUIRE(info.loose<=1u);
            REQUIRE(info.reserved[0]==0u&&info.reserved[1]==0u);
            if(lists==0) loose=info.loose;
            if(node->kind==STUN_UI_BLOCK_ORDERED_LIST)
                REQUIRE(info.start<=999999999u);
            else REQUIRE(info.start==0u);
            ++lists;
        }
    }
    REQUIRE(lists==f->list_count);
    if(f->loose!=-1) REQUIRE(loose==f->loose);
    size_t required=0;
    REQUIRE(stun_md_render_html(&view,512u,NULL,0,&required)==STUN_UI_OK);
    REQUIRE(required==strlen(f->expected));
    char *out=(char*)malloc(required+2u);
    REQUIRE(out!=NULL);
    memset(out,0x7d,required+2u);
    size_t checked=0;
    REQUIRE(stun_md_render_html(&view,512u,out,required,&checked)==
            STUN_UI_CAPACITY_EXCEEDED);
    REQUIRE(checked==required);
    for(size_t i=0;i<required+2u;++i) REQUIRE(out[i]==0x7d);
    REQUIRE(stun_md_render_html(&view,512u,out,required+1u,&checked)==STUN_UI_OK);
    REQUIRE(checked==required);
    if(strcmp(out,f->expected)!=0)
        fprintf(stderr,"fixture %s\nactual: %s\nexpected: %s\n",
                f->name,out,f->expected);
    REQUIRE(strcmp(out,f->expected)==0);
    REQUIRE((unsigned char)out[required+1u]==0x7d);
    free(out);
    stun_md_document_destroy(owner);
    return true;
}

int main(void) {
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        if(!check_case(&cases[i])) {
            fprintf(stderr,"FAILED: %s\n",cases[i].name);
            return 1;
        }
    }
    const char *limited="- one\n\n  two\n";
    stun_md_document *d=(stun_md_document*)(uintptr_t)1u;
    const stun_ui_status status=stun_md_parse(limited,strlen(limited),2u,
                                             strlen(limited)+1u,&d);
    if(status!=STUN_UI_CAPACITY_EXCEEDED||d!=NULL) {
        fprintf(stderr,"node exhaustion must roll back exactly once\n");
        return 1;
    }
    /* The quote-continuation soft-break allocation must also be failure
     * atomic. Eight nodes admit the first quote text but not its next
     * LINE_BREAK, and the caller must never receive a partial RichDoc. */
    const char *quote_limit="* a\n  > b\n  > c\n";
    d=(stun_md_document*)(uintptr_t)1u;
    if(stun_md_parse(quote_limit,strlen(quote_limit),8u,
                     strlen(quote_limit)+1u,&d)!=STUN_UI_CAPACITY_EXCEEDED ||
       d!=NULL) {
        fprintf(stderr,"quote continuation node exhaustion must roll back\n");
        return 1;
    }
    /* Interior blank line publication must also fail atomically. With
     * eight nodes, the second deferred newline exceeds the node budget. */
    const char *code_blanks="- Foo\n\n      bar\n\n\n      baz\n";
    d=(stun_md_document*)(uintptr_t)1u;
    if(stun_md_parse(code_blanks,strlen(code_blanks),8u,
                     strlen(code_blanks)+1u,&d)!=STUN_UI_CAPACITY_EXCEEDED ||
       d!=NULL) {
        fprintf(stderr,"indented code blank-node exhaustion must roll back\n");
        return 1;
    }
    /* The container nesting limit is executable, not just documented:
     * forcing a 33rd frame must fail atomically without returning a view. */
    char deep[4096];
    size_t used=0u;
    for(size_t depth=0u;depth<34u;++depth) {
        for(size_t j=0u;j<depth*2u;++j) deep[used++]=' ';
        deep[used++]='-'; deep[used++]=' '; deep[used++]='x';
        deep[used++]='\n';
    }
    deep[used]='\0';
    d=(stun_md_document*)(uintptr_t)1u;
    if(stun_md_parse(deep,used,2048u,used+1u,&d)!=
       STUN_UI_CAPACITY_EXCEEDED||d!=NULL) {
        fprintf(stderr,"container depth overflow must roll back\n");
        return 1;
    }
    printf("Native CommonMark list container subset: %zu/%zu passed\n",
        sizeof(cases)/sizeof(cases[0]),sizeof(cases)/sizeof(cases[0]));
    return 0;
}
