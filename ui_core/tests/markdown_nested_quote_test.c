/* #181: bounded nested-quote RichDoc ownership and actual HTML. */
#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct sample {
    const char *name,*source,*html;
    unsigned quote_nodes,depth;
} sample;
static const sample cases[]={
    {"cm228","> # Foo\n> bar\n> baz\n",
     "<blockquote>\n<h1>Foo</h1>\n<p>bar\nbaz</p>\n</blockquote>\n",1,1},
    {"cm230","   > # Foo\n   > bar\n > baz\n",
     "<blockquote>\n<h1>Foo</h1>\n<p>bar\nbaz</p>\n</blockquote>\n",1,1},
    {"cm236",">     foo\n    bar\n",
     "<blockquote>\n<pre><code>foo\n</code></pre>\n</blockquote>\n<pre><code>bar\n</code></pre>\n",1,1},
    {"cm250","> > > foo\nbar\n",
     "<blockquote>\n<blockquote>\n<blockquote>\n<p>foo\nbar</p>\n</blockquote>\n</blockquote>\n</blockquote>\n",3,3},
    {"fenced","> ~~~c\n> hi\n> ~~~\n",
     "<blockquote>\n<pre><code class=\"language-c\">hi\n</code></pre>\n</blockquote>\n",1,1},
    {"changing-depth","> outer\n> > inner\n> after\n",
     "<blockquote>\n<p>outer</p>\n<blockquote>\n<p>inner\nafter</p>\n</blockquote>\n</blockquote>\n",2,2},
    {"cm251",">>> foo\n> bar\n>>baz\n",
     "<blockquote>\n<blockquote>\n<blockquote>\n<p>foo\nbar\nbaz</p>\n</blockquote>\n</blockquote>\n</blockquote>\n",3,3},
    {"partial-marker-interrupt",">>> foo\n> # Heading\n",
     "<blockquote>\n<blockquote>\n<blockquote>\n<p>foo</p>\n</blockquote>\n</blockquote>\n<h1>Heading</h1>\n</blockquote>\n",3,3},
    {"partial-marker-list",">>> foo\n> - bar\n",
     "<blockquote>\n<blockquote>\n<blockquote>\n<p>foo</p>\n</blockquote>\n</blockquote>\n<ul>\n<li>bar</li>\n</ul>\n</blockquote>\n",3,3}
};
_Static_assert(sizeof(cases)/sizeof(cases[0])==9u,"fixture count");
static int verify(const sample *f) {
    stun_md_document *owner=NULL;
    const size_t input_size=strlen(f->source);
    if(stun_md_parse(f->source,input_size,256u,input_size+1u,&owner)!=
       STUN_UI_OK || !owner) return 1;
    const stun_ui_rich_document_view view=stun_md_document_view(owner);
    int fail=stun_ui_rich_document_validate(&view,256u)!=STUN_UI_OK ||
       view.raw_markdown_bytes!=input_size ||
       memcmp(view.raw_markdown,f->source,input_size)!=0;
    unsigned count=0u,maxdepth=0u;
    for(size_t i=1u;i<view.document.block_count&&!fail;++i) {
        if(view.document.blocks[i].kind!=STUN_UI_BLOCK_QUOTE) continue;
        ++count;
        size_t at=i;unsigned depth=0u;
        while(at) {
            const stun_ui_block *b=&view.document.blocks[at];
            if(b->kind==STUN_UI_BLOCK_QUOTE) ++depth;
            if(b->parent_index>=at) {fail=1;break;}
            at=b->parent_index;
        }
        if(depth>maxdepth)maxdepth=depth;
    }
    if(count!=f->quote_nodes||maxdepth!=f->depth)fail=1;
    size_t needed=0u;
    if(!fail&&stun_md_render_html(&view,256u,NULL,0u,&needed)!=STUN_UI_OK)fail=1;
    if(!fail&&needed!=strlen(f->html))fail=1;
    if(!fail) {
        char *out=(char*)malloc(needed+2u);
        if(!out)fail=1;
        else {
            memset(out,0xa5,needed+2u);
            size_t measured=0u;
            if(stun_md_render_html(&view,256u,out,needed,&measured)!=
               STUN_UI_CAPACITY_EXCEEDED || measured!=needed)fail=1;
            for(size_t i=0;i<needed+2u;++i)
                if((unsigned char)out[i]!=0xa5u)fail=1;
            if(!fail&&(stun_md_render_html(&view,256u,out,needed+1u,&measured)
                !=STUN_UI_OK || measured!=needed ||
                memcmp(out,f->html,needed+1u)!=0 ||
                (unsigned char)out[needed+1u]!=0xa5u))fail=1;
            free(out);
        }
    }
    stun_md_document_destroy(owner);
    if(fail)fprintf(stderr,"Nested quote mismatch: %s\n",f->name);
    return fail;
}
int main(void) {
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(verify(&cases[i]))return 1;
    /* Every quoted fixture must reject exhausted node/source budgets without
     * publishing a partially constructed document to the caller. */
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        const size_t bytes=strlen(cases[i].source);
        stun_md_document *limited=(stun_md_document*)(uintptr_t)1u;
        if(stun_md_parse(cases[i].source,bytes,1u,bytes+1u,&limited)!=
           STUN_UI_CAPACITY_EXCEEDED || limited!=NULL) {
            fprintf(stderr,"Node budget rollback: %s\n",cases[i].name);
            return 1;
        }
        limited=(stun_md_document*)(uintptr_t)1u;
        if(stun_md_parse(cases[i].source,bytes,256u,bytes-1u,&limited)!=
           STUN_UI_CAPACITY_EXCEEDED || limited!=NULL) {
            fprintf(stderr,"Source budget rollback: %s\n",cases[i].name);
            return 1;
        }
    }
    /* The maximum permitted depth must succeed, not merely depth+1 fail. */
    char boundary[160];size_t boundary_bytes=0u;
    for(size_t i=0u;i<32u;++i) {
        boundary[boundary_bytes++]='>';
        boundary[boundary_bytes++]=' ';
    }
    boundary[boundary_bytes++]='x';
    boundary[boundary_bytes++]='\n';
    stun_md_document *accepted=NULL;
    if(stun_md_parse(boundary,boundary_bytes,256u,boundary_bytes+1u,
                     &accepted)!=STUN_UI_OK || !accepted) {
        fprintf(stderr,"Depth 32 unexpectedly rejected\n");
        return 1;
    }
    const stun_ui_rich_document_view accepted_view=
        stun_md_document_view(accepted);
    unsigned accepted_quotes=0u;
    for(size_t i=1u;i<accepted_view.document.block_count;++i)
        if(accepted_view.document.blocks[i].kind==STUN_UI_BLOCK_QUOTE)
            ++accepted_quotes;
    const int invalid_boundary=
        stun_ui_rich_document_validate(&accepted_view,256u)!=STUN_UI_OK ||
        accepted_view.raw_markdown_bytes!=boundary_bytes ||
        memcmp(accepted_view.raw_markdown,boundary,boundary_bytes)!=0 ||
        accepted_quotes!=32u;
    stun_md_document_destroy(accepted);
    if(invalid_boundary) {
        fprintf(stderr,"Depth 32 ownership/count invalid\n");
        return 1;
    }
    char deep[160];size_t n=0u;
    for(size_t i=0;i<33u;++i){deep[n++]='>';deep[n++]=' ';}
    deep[n++]='x';deep[n++]='\n';
    stun_md_document *owner=(stun_md_document*)(uintptr_t)1u;
    if(stun_md_parse(deep,n,256u,n+1u,&owner)!=
       STUN_UI_CAPACITY_EXCEEDED || owner!=NULL)return 1;
    printf("Native nested-quote fixtures %zu/%zu, node/source budgets and depth 32/33 boundary PASS\n",
           sizeof(cases)/sizeof(cases[0]),sizeof(cases)/sizeof(cases[0]));
    return 0;
}
